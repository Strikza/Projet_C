#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "myassert.h"

#include "master_worker.h"

/************************************************************************
 * Données persistantes d'un worker
 ************************************************************************/

// on peut ici définir une structure stockant tout ce dont le worker
// a besoin : le nombre premier dont il a la charge, ...

struct s_worker{
    int myNumberPrime;
    int *fdIn;
    int *fdOut;
    int *fdToMaster;
};

/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message)
{
    fprintf(stderr, "usage : %s <n> <fdIn> <fdToMaster>\n", exeName);
    fprintf(stderr, "   <n> : nombre premier géré par le worker\n");
    fprintf(stderr, "   <fdIn> : canal d'entrée pour tester un nombre\n");
    fprintf(stderr, "   <fdToMaster> : canal de sortie pour indiquer si un nombre est premier ou non\n");
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}

static void parseArgs(int argc, char * argv[], struct s_worker * worker)
{
    if (argc != 4)
        usage(argv[0], "Nombre d'arguments incorrect");

    // remplir la structure
    worker->myNumberPrime = (int *) argv[1];
    worker->fdIn = (int *) argv[2];
    worker->fdToMaster = (int *) argv[3];
    worker->fdOut = NULL;

}

/************************************************************************
 * Fonction annexes
 ************************************************************************/

int lectureTube(int * fdTube){
    
    int ret;
    int res;

    ret = close(fdTube[1]);  // Fermeture côté écriture
    assert(ret != -1);

    ret = read(fdTube[0], &res, sizeof(int));
    assert(ret != -1);

    return res;
}

void ecritureTube(int * fdTube, int answer){
    
    int ret;

    ret = close(fdTube[0]);  // Fermeture côté lecture
    assert(ret != -1);

    ret = write(fdTube[1], &answer, sizeof(int));
    assert(ret != -1);
}

void libererRessources(int * fdIn, int * fdOut, int * fdToMaster){

    int ret;

    // On ferme les "file descriptors"
    ret = close(*fdIn);
    assert(ret != -1);

    ret = close(*fdOut);
    assert(ret != -1);

    ret = close(*fdToMaster);
    assert(ret != -1);

}

/************************************************************************
 * Boucle principale de traitement
 ************************************************************************/

void loop(struct s_worker *cur_worker)
{
    // boucle infinie :
    //    attendre l'arrivée d'un nombre à tester
    //    si ordre d'arrêt
    //       si il y a un worker suivant, transmettre l'ordre et attendre sa fin
    //       sortir de la boucle
    //    sinon c'est un nombre à tester, 4 possibilités :
    //           - le nombre est premier
    //           - le nombre n'est pas premier
    //           - s'il y a un worker suivant lui transmettre le nombre
    //           - s'il n'y a pas de worker suivant, le créer

    int stop = 1;
    int ret;
    int number;

    while(stop == 1){

        number = lectureTube(cur_worker->fdIn);

        // Si l'ordre du master est ORDER_STOP
        if(number == STOP){

            // Si ce worker est le dernier
            if(cur_worker->fdOut != NULL){

                // On transmet l'information au worker suivant
                ecritureTube(cur_worker->fdOut, STOP);

                // On attend une réponse de retour du worker suivant
                ret = lectureTube(cur_worker->fdOut);
                assert(ret != STOP);
            }

            // On renvoie la réponse de retour au worker précédent pour le débloquer
            ecritureTube(cur_worker->fdIn, STOP);

            stop = 0;
        }
        // Le nombre est premier
        else if(number == cur_worker->myNumberPrime){

            // On écrit notre réponse au Master
            ecritureTube(cur_worker->fdToMaster, VALID);
        }
        // Le nombre n'est pas premier
        else if((number != cur_worker->myNumberPrime) && (number % cur_worker->myNumberPrime == 0)){

            // On écrit notre réponse au Master
            ecritureTube(cur_worker->fdToMaster, INVALID);
        }
        // On envoie la donnée au worker suivant
        else if(cur_worker->fdOut != NULL){

            // On envoir la donnée au worker suivant
            ecritureTube(cur_worker->fdOut, number);
        }
        // On créé le worker suivant
        else{
            
            // On créé le tube de communication entre ces 2 worker
            int newFdOut[2];
            ret = pipe(newFdOut);
            assert(ret != -1);

            cur_worker->fdOut = newFdOut;

            // On créé un fils pour créé un nouveau worker
            if(fork() == 0){
                // Arguments en paramètres du prochain worker
                char * args[] = {"worker", (char *)number, (char *)newFdOut, (char *)(cur_worker->fdToMaster)};

                // On remplace ce processus par le nouveau worker
                execv("./worker", args);
            }

            // On envoie la donnée sur le nouveau tube
            ecritureTube(cur_worker->fdOut, number);
        }
    }
}

/************************************************************************
 * Programme principal
 ************************************************************************/

int main(int argc, char * argv[])
{
    struct s_worker cur_worker;

    parseArgs(argc, argv, &cur_worker);

    // Si on est créé c'est qu'on est un nombre premier
    // Envoyer au master un message positif pour dire
    // que le nombre testé est bien premier

    ecritureTube(cur_worker.fdToMaster, 1);

    loop(&cur_worker);

    // libérer les ressources : fermeture des files descriptors par exemple

    libererRessources(cur_worker.fdIn, cur_worker.fdOut, cur_worker.fdToMaster);

    return EXIT_SUCCESS;
}
