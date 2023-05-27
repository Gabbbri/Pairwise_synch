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
    di queste operazioni devo inoltre fare un flush per aggiornare l'array (USO ATOMIC INVECE CHE LOCK IN QUESTO CASO)
    -  head e tail -> il producer si gestisce head in autonomia, il consumer gestisce tail -> no concorrenza su queste
    PROBLEMA: head e tail non vanno in concorrenza, sono privati dei thread. Ma i valori a cui accedo usandoli possono
    andare in concorrenza 
    NOTA: se mando '0' al producer, questo si ferma. Devo segnalarlo al consumer, altrimenti continua ad
    aspettare che il buffer si riempia anche quando il producer ha terminato la sua attività
    */

    // variabili che mi servono per gestire la pairwise sync dei 2 thread

    bool flag = 0; // per segnalare che il producer ha finito

    omp_set_num_threads(TNUM); // setto il numero di thread a 2

#pragma omp parallel sections // shared(count, head, tail, cyclebuffer, flag) default(none)
    {

// INIZIA IL PRODUCER
#pragma omp section
        {
            // lettura dati:
            // termina se mando zero. Se non cè spazio ATTENDE, altrimenti continua all'infinito

            char tmp;

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

                int position = head - (cyclebuffer); // non serve nessun flush

                cin >> tmp;

                // controlli
                if (tmp == '0') // tmp
                {
#pragma omp atomic write
                    flag = 1;
#pragma omp flush(flag)

                    break; // condizione di termine del producer
                }
#pragma omp atomic read      // proteggo la cella puntata da head, che potrebbe potenzialmente
                *head = tmp; // essere in race con una lettura fatta dal consumer (volendo posso usare lock)
                head++;
#pragma omp atomic update
                count++;

#pragma omp flush // voglio che cyclebuffer e count aggiornati vadano in memoria

                // NOTA: sono certo di non andare in segm fault  perchè se buffer è pieno faccio puntare a head
                // il primo elemento

            } // fine del ciclo. Il producer ha finito

            std::cout << "Il producer ha finito. Il consumer sta ancora lavorando??\n";
        } // end of producer section

        // CONSUMER
#pragma omp section
        {

            char tmp, flg_tmp = 0;
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

                if (flg_tmp) // NOTA: DEVO USARE UN DOPPIO BREAK PERCHE' NON POSSO USARE RETURN NELLA ZONA PARALLELA
                    break;

                // il buffer è ciclico. Se tail arriva alla fine, devo rimetterlo all'inizio
                if (tail == &(cyclebuffer[N - 1]))
                    tail = cyclebuffer; // anche qui non ho data race su questi 2 dati

                int position = tail - (cyclebuffer);

#pragma omp atomic read      // proteggo la cella puntata da tail (meglio un lock?)
                tmp = *tail; // non devo flushare perchè non ho aggiornato niente in realtà. Faccio una copia del valore e basta

                std::cout << "Consumer read " << tmp << "\n\n";

                tail++;

#pragma omp atomic update
                count--;
            }
        } // end of consumer region

    } // end of parallel region

    return 0;
}