#include <iostream>
#include <omp.h>
#define N 10
#define TNUM 2

using namespace std;

int main()
{

    // Operazioni preliminari - preparazione del data environment

    // creo il buffer dinamico

    char *const cyclebuffer = new char[N]; // volendo posso farmi dare N da utente

    // mi muovo nel buffer con 2 pointer: head e tail. Introduco inoltre un counter per gli elementi nel buffer
    char *tail = cyclebuffer;
    char *head = cyclebuffer;

    int count = 0; // inizialmente buffer vuoto

    /* ANALISI CONCORRENZE:
    - count -> entrambi i thread vogliono leggerci e scriverci. Noto che questa deve essere condivisa tra i 2 thread
    - cyclebuffer -> struttura su cui i thread leggono e scrivono. Devo proteggere le letture e le scritture. Dopo ognuna
    di queste operazioni devo inoltre fare un flush per aggiornare l'array (USO I LOCK PER ASSICURARE MUTUA ESCLUSIONE)
    -  head e tail -> il producer si gestisce head in autonomia, il consumer gestisce tail -> no concorrenza su queste
    PROBLEMA: head e tail non vanno in concorrenza, sono privati dei thread. Ma i valori a cui accedo usandoli possono
    andare in concorrenza -> I LOCK VANNO QUINDI SUI VALORI PUNTATI DA HEAD E TAIL, NON SUL PUNTATORE CYCLEBUFFER
    NOTA: se mando '0' al producer, questo si ferma. Devo segnalarlo al consumer, altrimenti continua ad
    aspettare che il buffer si riempia anche quando il producer ha terminato la sua attività
    */

    // variabili che mi servono per gestire la pairwise sync dei 2 thread

    bool flag = 0; // per segnalare che il producer ha finito

    omp_set_num_threads(TNUM); // setto il numero di thread a 2

    // 2 POSSIBILITA':
    //  - creare un array di locks, così associo ogni lock a una cella del buffer
    //  - mantenere un lock unico. In questo modo però faccio un lock sull'intera struttura o cosa succede???

    omp_lock_t lock_buffer[N]; // array di locks
    // #pragma omp parallel for (volendo posso parallelizzare)
    for (int i = 0; i < N; i++)
    {
        omp_init_lock(&lock_buffer[i]);
    }

#pragma omp parallel sections // shared(count, head, tail, cyclebuffer, flag) default(none)
    {

// INIZIA IL PRODUCER
#pragma omp section
        {
            // lettura dati:
            // termina se mando zero. Se non cè spazio ATTENDE, altrimenti continua all'infinito

            while (true)
            {

                // il buffer è ciclico. Se head arriva alla fine, devo rimetterla all'inizio
                if (head == &(cyclebuffer[N - 1]))
                    head = cyclebuffer; // nessun problema su head e cyclebuffer

#pragma omp flush(count) // aggiorno count (main memory -> producer)
                // condizione di attesa. Se non cè posto rimango in questo loop infinito
                while (!(N - count))
                {
                    std::cout << "\nIl producer sta attendendo che il consumer svuoti il buffer";
#pragma omp flush(count) // main memory -> producer
                };

                // setto il lock sulla cella dell'array che vado a modificare -> ci faccio i controlli, se li passa inserisco il valore nell'array

                int position = head - (cyclebuffer); // non serve nessun flush su queste 2 variabili (head è privato, cyclebuffer non cambia mai valore (char* const cyclebuffer))

                omp_set_lock(&lock_buffer[position]); // NOTA: mi sto prendendo il lock della posizione di head. Se il consumer vuole accedere ad altre posizioni
                                                      // può farlo in contemporanea. Solo se tentato di entrare nella stessa posizione c'è attesa

                cin >> *head;

                // controlli
                if (*head == '0') // tmp
                {
#pragma omp atomic write
                    flag = 1;
#pragma omp flush(flag)

                    break; // condizione di termine del producer
                }

                // potrei fare qui l'unset del lock, ma aspetto
                head++;
#pragma omp atomic update
                count++;

                omp_unset_lock(&lock_buffer[position]); // Metto qui l'unset del lock invece che prima per sfruttare il fatto che quando lo faccio mando anche un flush implicito
                // voglio che cyclebuffer (inteso come l'array, la struttura dati) e count aggiornati vadano in memoria

                // NOTA: sono certo di non andare in segm fault  perchè se buffer è pieno faccio puntare a head
                // il primo elemento (faccio girare head sul buffer)

            } // fine del ciclo. Il producer ha finito

            std::cout << "Il producer ha finito. Il consumer sta ancora lavorando??\n";
        } // end of producer section

        // CONSUMER
#pragma omp section
        {

            char flg_tmp = 0;
            while (true)
            {

                // controlli sulla presenza di valori nel buffer e sull'eventuale termine del producer

#pragma omp flush(count) // mi prendo il valore di count più aggiornato

                // se non ci sono caratteri nel buffer, attendo. Il producer dovrebbe crearmeli! Nota che se non ci sono caratteri E IL PRODUCER HA TERMINATO, DEVE TERMINARE ANCHE IL CONSUMER
                while (count == 0)
                {
                    // cout << "\nIl consumer sta attendendo che il producer riempia il buffer\n";
#pragma omp flush(count, flag) // continua a fare il refresh del valore di count e di flag

#pragma omp atomic read
                    flg_tmp = flag;
                    if (flg_tmp == 1)
                    {
                        std::cout << "\n\nProducer has finished, consumer has finished. All work done\n";
                        break; // se non ho elementi nel buffer e il producer ha finito, ho fatto tutto. Termino il programma
                    };
                }

                if (flg_tmp) // NOTA: DEVO USARE UN DOPPIO BREAK PERCHE' NON POSSO USARE RETURN NELLA ZONA PARALLELA (o c'è un metodo per farlo?)
                    break;

                // il buffer è ciclico. Se tail arriva alla fine, devo rimetterlo all'inizio
                if (tail == &(cyclebuffer[N - 1]))
                    tail = cyclebuffer; // anche qui non ho data race su questi 2 dati

                int position = tail - (cyclebuffer);

                omp_set_lock(&lock_buffer[position]);

                std::cout << "Consumer read " << *tail << "\n\n";

                omp_unset_lock(&lock_buffer[position]);

                tail++;

#pragma omp atomic update
                count--;
            }
            // non serve flushare qui alla fine. Infatti ogni volta che un thread usa una variabile che ha concorrenza, se la flusha prima di usarla
        } // end of consumer region

    } // end of parallel region

    return 0;
}