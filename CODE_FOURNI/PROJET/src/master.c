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

typedef struct master
{
    int cli_mas;        // tube nommé du client vers le master
    int mas_cli;        // tube nommé du master vers le client
    int* mas_w;          // tube anonyme du master vers le worker
    int* w_mas;          // tube anonyme des workers vers le master
    int highest;
    int howmanyprimals;
} master;



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
 * Fonctions Annexes
 ************************************************************************/
void ouvertureTubeNommes(int *mas_cli, int* cli_mas) {

    *mas_cli = open(MASTER_CLIENT, O_WRONLY);
    assert(mas_cli != -1);

    *cli_mas = open(CLIENT_MASTER, O_RDONLY);
    assert(cli_mas != -1);
}

void fermetureTubeNommes(int *mas_cli, int* cli_mas) {

    *mas_cli = close(*mas_cli);
    assert(mas_cli != -1);

    *cli_mas = close(*cli_mas);
    assert(cli_mas != -1);
}


/************************************************************************
 * boucle principale de communication avec le client
 ************************************************************************/
void loop(master* mas, int syncsem)
{
    int ret;
    int endwhile = 0;
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

    // ouverture des tubes
    ouvertureTubeNommes(mas->mas_cli, mas->cli_mas);

    // attente ordre d'un client
    int order;
    read(mas->cli_mas, &order, sizeof(int));
    assert(ret != -1);

    while(!endwhile) {
        //si ORDER_STOP
        if(order == ORDER_STOP) {
            // envoyer ordre de fin au premier worker
            ret = write(mas->mas_w[1], &order, sizeof(int));
            assert(ret != -1);
            printf("J'ai bien envoyé l'ordre %d (stop normalement) au workers\n", order);

            // et attendre sa fin
            int end;
            ret = read(mas->w_mas[0], &end, sizeof(int));
            assert(ret != -1);
            printf("J'ai bien reçu la fin des workers : %d\n", end);

            // envoyer un accusé de réception au client
            ret = write(mas->mas_cli, &end, sizeof(int));
            assert(ret != -1);
            printf("J'ai bien envoyé l'accusé de réception au client : %d", end);

            endwhile = 1;
        }
        // si ORDER_COMPUTE_PRIME
        if(order == ORDER_COMPUTE_PRIME) {
            //récupérer le nombre N à tester provenant du client
            int N;
            ret = read(mas->cli_mas, &N, sizeof(int));
            assert(ret != -1);
            printf("J'ai bien reçu le nombre N : %d", N);
        
            // construire le pipeline jusqu'au nombre N-1 (si non encore fait) :
            if(N > mas->highest) {
                for(int i = N-mas->highest; i=N-1; i++) {
                    ret = write(mas->mas_w[1], &i, sizeof(int));
                    assert(ret != -1);
                    printf("%d envoyé avec succès\n", i);

                    int M;    
                    ret = read(mas->w_mas[0], &M, sizeof(int));
                    assert(ret != -1);
                    // on ignore M
                }
            }
            // envoyer N dans le pipeline
            ret = write(mas->mas_w[1], &N, sizeof(int));
            assert(ret != -1);
            printf("%d envoyé avec succès\n", N);
            
            // récupérer la réponse
            int M;    
            ret = read(mas->w_mas[0], &M, sizeof(int));
            assert(ret != -1);

            if(M == 1) { //Si N est bien premier
                if(M > mas->highest) {
                    mas->highest = M;
                }
                mas->howmanyprimals++;
            }

            // la transmettre au client
            ret = write(mas->mas_cli, &M, sizeof(int));
            assert(ret != -1);
            printf("J'ai bien envoyé la réponse avec succès : %d\n", N);
        }
        // si ORDER_HOW_MANY_PRIME
        if(order == ORDER_HOW_MANY_PRIME) {
            // transmettre la réponse au client
            ret = write(mas->mas_cli, &(mas->howmanyprimals), sizeof(int));
            assert(ret != -1);
            printf("J'ai bein envoyé le howmany au client avec succès : %d\n", mas->howmanyprimals);
        }

        // - si ORDER_HIGHEST_PRIME
        if(order == ORDER_HIGHEST_PRIME) {
            // transmettre la réponse au client
            ret = write(mas->mas_cli, &(mas->highest), sizeof(int));
            assert(ret != -1);
            printf("J'ai bien envoyé le highest au client avec succès : %d\n", mas->highest);
        }


        // fermer les tubes nommés
        fermetureTubeNommes(mas->mas_cli, mas->cli_mas);

        // attendre ordre du client avant de continuer (sémaphore : précédence)

        ret = semop(syncsem, &wait, 1);
        assert(ret != -1);

        // revenir en début de boucle
    }
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
    ret = pipe(master_w2);
    assert(ret != -1);
    ret = pipe(w_master);
    assert(ret != -1);

    if(fork() == 0) {
        ret = exec("worker", 2, master_w2, w_master);
        assert(ret != -1);
        printf("le master a crée le premier work !\n");
    }

    // création d'un master
    master *mas = malloc(sizeof(master));
    mas->mas_w = &master_w2;
    mas->w_mas = &w_master;
    mas->highest = 0;
    mas->howmanyprimals =0;

    // boucle infinie
    loop(&mas, SyncID);

    // DESTRUCTION

    // destruction des tubes anonymes

    close(master_w2);
    close(w_master);
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
