#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "myassert.h"

#include "master_client.h"
#include "master_worker.h"

/************************************************************************
 * Données persistantes d'un master
 ************************************************************************/

typedef struct master
{
    int cli_mas;        // tube nommé du client vers le master
    int mas_cli;        // tube nommé du master vers le client
    int mas_w;          // tube anonyme du master vers le worker
    int w_mas;          // tube anonyme des workers vers le master
    int highest_prime;
    int highest_number;
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
void ouvertureTubeNommes(master * mas) {
   
    int fd = open(MASTER_CLIENT, O_WRONLY);
    assert(fd != -1);
    mas->mas_cli = fd;
   
    int fd1 = open(CLIENT_MASTER, O_RDONLY);
    assert(fd1 != -1);
    mas->cli_mas = fd1;
}


void fermetureTubeNommes(master * mas) {

    int fd = close(mas->mas_cli);
    assert(fd != -1);

    int fd1 = close(mas->cli_mas);
    assert(fd1 != -1);
}

void writeTubeMaster(int fd, int* answer) {// faire une sous fonction pour les write
    int ret;
    ret = write(fd, answer, sizeof(int));
    assert(ret != -1);
}

void readTubeMaster(int fd, int* answer) { // faire une sous fonction pour les write
    int ret;
    ret = read(fd, answer, sizeof(int));
    assert(ret != -1);
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


    while(!endwhile) {
        ouvertureTubeNommes(mas);

        // attente ordre d'un client
        int order;
        read(mas->cli_mas, &order, sizeof(int));
        assert(ret != -1);

        //si ORDER_STOP
        if(order == ORDER_STOP) {
            // envoyer ordre de fin au premier worker
            writeTubeMaster(mas->mas_w, &order);

            // et attendre sa fin
            int end;
            readTubeMaster(mas->w_mas, &end);

            // envoyer un accusé de réception au client
            writeTubeMaster(mas->mas_cli, &order);

            endwhile = 1;
        }
        
        // si ORDER_COMPUTE_PRIME
        if(order == ORDER_COMPUTE_PRIME) {
            //récupérer le nombre N à tester provenant du client
            int N;
            readTubeMaster(mas->cli_mas, &N);
            printf("M : Le master reçois le nombre '%d' à envoyer aux workers\n", N);
        
            // construire le pipeline jusqu'au nombre N-1 (si non encore fait) :
            if(N > mas->highest_number) {
                printf("M : Il faut peut-être créer de nouveau worker, car N=%d est plus grand que mas->highest=%d\n", N, mas->highest_number);
                for(int i = mas->highest_number+1; i<N; i++) {
                    writeTubeMaster(mas->mas_w, &i);
                    printf("M : J'ai écrit %d dans le pipeline pour créer les workers\n", i);
                    
                    int X;    
                    readTubeMaster(mas->w_mas, &X);
                    printf("M : J'ai reçus %d d'un worker (phase de création de worker)\n", X);
                }
                mas->highest_number = N;
            }
            // envoyer N dans le pipeline
            writeTubeMaster(mas->mas_w, &N);
            printf("M : J'ai écrit %d dans le pipeline pour le tester\n", N);
            
            // récupérer la réponse
            int M;   
            printf("M : Je vais attendre 1s que les workers finissent leur travail\n");
            sleep(1);
            readTubeMaster(mas->w_mas, &M);
            printf("M : J'ai reçus %d d'un worker (phase de test)\n", M);

            if(M == 1) { //Si N est bien premier
                if(N > mas->highest_prime) {
                    mas->highest_prime = N;
                }
                mas->howmanyprimals++;
            }

            // la transmettre au client
            writeTubeMaster(mas->mas_cli, &M);
        }
        
        // si ORDER_HOW_MANY_PRIME
        if(order == ORDER_HOW_MANY_PRIME) {
            // transmettre la réponse au client
            writeTubeMaster(mas->mas_cli, &(mas->howmanyprimals));
        }

        // - si ORDER_HIGHEST_PRIME
        if(order == ORDER_HIGHEST_PRIME) {
            // transmettre la réponse au client
            writeTubeMaster(mas->mas_cli, &(mas->highest_prime));
        }


        // fermer les tubes nommés
        fermetureTubeNommes(mas);

        // attendre ordre du client avant de continuer (sémaphore : précédence)

        ret = semop(syncsem, &wait, 1);
        assert(ret != -1);

        // revenir en début de boucle
        printf("----------------------------------------------------------\n");
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
    assert(keyCritic != -1);

    keySync = ftok(SEMKEY_SYNC, PROJ_ID);
    assert(keySync != -1);

    // - création des sémaphores
    int CriticID = semget(keyCritic, 1, 0641 | IPC_CREAT);
    assert(CriticID != -1);
    int SyncID = semget(keySync, 1, 0641 | IPC_CREAT);
    assert(SyncID != -1);

    // - initialisation des sémaphores
    int ret;
    ret = semctl(CriticID, 0, SETVAL, 1);
    assert(ret != -1);
    ret = semctl(SyncID, 0, SETVAL, 1);
    assert(ret != -1);


    // ***** création des tubes nommés *****
    ret = mkfifo(MASTER_CLIENT, 0600);
    assert(ret != -1);
    ret = mkfifo(CLIENT_MASTER, 0600);
    assert(ret != -1);

    // ***** création du premier worker *****

    // création des tubes anonymes
    int master_w2[2];                                   // tube anonyme du master vers premier worker
    int w_master[2];                                    // tube anonyme des workers vers le master (tout les workers doivent le connaitre)
    ret = pipe(master_w2);
    assert(ret != -1);
    ret = pipe(w_master);
    assert(ret != -1);

    if(fork() == 0) {
        
        printf("W2 : Je suis le premier des workers créé\n");
        char* arg1 = malloc(sizeof(char)); // = malloc(sizeof(master_w2)); 
        char* arg2 = malloc(sizeof(char)); // = malloc(sizeof(w_master));
        sprintf(arg1, "%d", master_w2[0]);
        sprintf(arg2, "%d", w_master[1]);

        char* args[] = {"./worker", "2", arg1, arg2, NULL};
        ret = execv(args[0], args);
        assert(ret != -1);
    }

    // création d'un master
    master *mas = malloc(sizeof(master));
    mas->mas_w = master_w2[1];
    mas->w_mas = w_master[0];
    mas->highest_prime = 2;
    mas->highest_number = 2;
    mas->howmanyprimals =0;

    // boucle infinie
    loop(mas, SyncID);

    // DESTRUCTION

    // destruction des tubes anonymes

    close(*master_w2);
    close(*w_master);
    
    // destruction des tubes nommés

    unlink(MASTER_CLIENT);
    unlink(CLIENT_MASTER);
    
    // destruction des sémaphores, ...

    ret = semctl(CriticID, 0, IPC_RMID);
    assert(ret != -1);
    ret = semctl(SyncID, 0, IPC_RMID);
    assert(ret != -1);

    free(mas);

    return EXIT_SUCCESS;
}

// N'hésitez pas à faire des fonctions annexes ; si les fonctions main
// et loop pouvaient être "courtes", ce serait bien
