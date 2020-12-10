#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "myassert.h"

#include "master_client.h"
#include "master_worker.h"

#define TUBE_MASTER_CLIENT "master_client"
#define TUBE_CLIENT_MASTER "client_master"

/************************************************************************
 * Données persistantes d'un master
 ************************************************************************/

// on peut ici définir une structure stockant tout ce dont le master
// a besoin


/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message)
{
    fprintf(stderr, "usage : %s\n", exeName);
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}


/************************************************************************
 * boucle principale de communication avec le client
 ************************************************************************/
void loop(/* paramètres */)
{
    // boucle infinie :
    // - ouverture des tubes (cf. rq client.c)
    // - attente d'un ordre du client (via le tube nommé)
    // - si ORDER_STOP
    //       . envoyer ordre de fin au premier worker et attendre sa fin
    //       . envoyer un accusé de réception au client
    // - si ORDER_COMPUTE_PRIME
    //       . récupérer le nombre N à tester provenant du client
    //       . construire le pipeline jusqu'au nombre N-1 (si non encore fait) :
    //             il faut connaître le plus nombre (M) déjà envoyé aux workers
    //             on leur envoie tous les nombres entre M+1 et N-1
    //             note : chaque envoie déclenche une réponse des workers
    //       . envoyer N dans le pipeline
    //       . récupérer la réponse
    //       . la transmettre au client
    // - si ORDER_HOW_MANY_PRIME
    //       . transmettre la réponse au client
    // - si ORDER_HIGHEST_PRIME
    //       . transmettre la réponse au client
    // - fermer les tubes nommés
    // - attendre ordre du client avant de continuer (sémaphore : précédence)
    // - revenir en début de boucle
    //
    // il est important d'ouvrir et fermer les tubes nommés à chaque itération
    // voyez-vous pourquoi ?
}


/************************************************************************
 * Fonction principale
 ************************************************************************/

int main(int argc, char * argv[])
{
    if (argc != 1)
        usage(argv[0], NULL);

    // ***** création des sémaphores *****
    
    // - création des clés
    key_t keySync, keyCritic;
    
    keyCritic = ftok(SEMKEY_CRITICAL, PROJ_ID);
    assert(keyCritic != 1);

    keySync = ftok(SEMKEY_SYNC, PROJ_ID);
    assert(keySync != 1);

    // - création des sémaphores
    int CriticID, SyncID;
    CriticID = semget(keyCritic, 1, IPC_CREAT | IPC_EXCL | 0641);
    assert(CriticID != 1);
    SyncID = semget(keySync, 1, IPC_CREAT | IPC_EXCL | 0641);
    assert(SyncID != 1);

    // - initialisation des sémaphores
    int ret;
    ret = semctl(CriticID, 0, SETVAL, 0);
    assert(ret != 1);
    ret = semctl(SyncID, 0, SETVAL, 1);
    assert(ret != 1);


    // ***** création des tubes nommés *****
    mkfifo(TUBE_MASTER_CLIENT, 0600);
    mkfifo(TUBE_CLIENT_MASTER, 0600);

    // ***** création du premier worker *****

    // création des tubes anonymes
    int master_w2[2];                                   // tube anonyme du master vers premier worker
    int w_master[2];                                    // tube anonyme des workers vers le master (tout les workers doivent le connaitre)
    pipe(master_w2);
    pipe(w_master);

    if(fork() == 0) {
        ret = exec("worker", 2, master_w2, w_master);
        assert(ret != -1);
        printf("le master a crée le premier work !\n");
    }


    // boucle infinie
    loop(/* paramètres */);

    // situation de synchronisation

    ret = semop(SyncID, &wait, 1);

    // DESTRUCTION

    // destruction des tubes anonymes

    unlink(master_w2);
    unlink(w_master);
    
    // destruction des tubes nommés

    unlink(TUBE_MASTER_CLIENT);
    unlink(TUBE_CLIENT_MASTER);
    
    // destruction des sémaphores, ...

    ret = semctl(CriticID, 0, IPC_RMID);
    assert(ret != -1);
    ret = semctl(SyncID, 0, IPC_RMID);
    assert(ret != -1);

    return EXIT_SUCCESS;
}

// N'hésitez pas à faire des fonctions annexes ; si les fonctions main
// et loop pouvaient être "courtes", ce serait bien
