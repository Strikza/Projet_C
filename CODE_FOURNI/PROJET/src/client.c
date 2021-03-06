#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <unistd.h>
#include <pthread.h>
#include <math.h>

#include "myassert.h"

#include "master_client.h"

typedef struct
{
    bool result;
    int n;    
    int i;

} ThreadData;

// chaines possibles pour le premier paramètre de la ligne de commande
#define TK_STOP      "stop"
#define TK_COMPUTE   "compute"
#define TK_HOW_MANY  "howmany"
#define TK_HIGHEST   "highest"
#define TK_LOCAL     "local"

/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message)
{
    fprintf(stderr, "usage : %s <ordre> [<number>]\n", exeName);
    fprintf(stderr, "   ordre \"" TK_STOP  "\" : arrêt master\n");
    fprintf(stderr, "   ordre \"" TK_COMPUTE  "\" : calcul de nombre premier\n");
    fprintf(stderr, "                       <nombre> doit être fourni\n");
    fprintf(stderr, "   ordre \"" TK_HOW_MANY "\" : combien de nombres premiers calculés\n");
    fprintf(stderr, "   ordre \"" TK_HIGHEST "\" : quel est le plus grand nombre premier calculé\n");
    fprintf(stderr, "   ordre \"" TK_LOCAL  "\" : calcul de nombre premier en local\n");
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}

static int parseArgs(int argc, char * argv[], int *number)
{
    int order = ORDER_NONE;

    if ((argc != 2) && (argc != 3))
        usage(argv[0], "Nombre d'arguments incorrect");

    if (strcmp(argv[1], TK_STOP) == 0)
        order = ORDER_STOP;
    else if (strcmp(argv[1], TK_COMPUTE) == 0)
        order = ORDER_COMPUTE_PRIME;
    else if (strcmp(argv[1], TK_HOW_MANY) == 0)
        order = ORDER_HOW_MANY_PRIME;
    else if (strcmp(argv[1], TK_HIGHEST) == 0)
        order = ORDER_HIGHEST_PRIME;
    else if (strcmp(argv[1], TK_LOCAL) == 0)
        order = ORDER_COMPUTE_PRIME_LOCAL;
    
    if (order == ORDER_NONE)
        usage(argv[0], "ordre incorrect");
    if ((order == ORDER_STOP) && (argc != 2))
        usage(argv[0], TK_STOP" : il ne faut pas de second argument");
    if ((order == ORDER_COMPUTE_PRIME) && (argc != 3))
        usage(argv[0], TK_COMPUTE " : il faut le second argument");
    if ((order == ORDER_HOW_MANY_PRIME) && (argc != 2))
        usage(argv[0], TK_HOW_MANY" : il ne faut pas de second argument");
    if ((order == ORDER_HIGHEST_PRIME) && (argc != 2))
        usage(argv[0], TK_HIGHEST " : il ne faut pas de second argument");
    if ((order == ORDER_COMPUTE_PRIME_LOCAL) && (argc != 3))
        usage(argv[0], TK_LOCAL " : il faut le second argument");
    if ((order == ORDER_COMPUTE_PRIME) || (order == ORDER_COMPUTE_PRIME_LOCAL))
    {
        *number = strtol(argv[2], NULL, 10);
        if (*number < 2)
             usage(argv[0], "le nombre doit être >= 2");
    }       
    
    return order;
}


/************************************************************************
 * Fonction annexes
 ************************************************************************/

void ouvertureSemaphores(key_t key_crit, key_t key_sync, int * semid_crit, int * semid_sync){

    *semid_crit = semget(key_crit, 0, 0);
    assert(*semid_crit != -1);

    *semid_sync = semget(key_sync, 0, 0);
    assert(*semid_sync != -1);

}

void ouvertureTubeNommes(int * fd_client_master, int * fd_master_client){

    *fd_master_client = open(MASTER_CLIENT, O_RDONLY);
    assert(*fd_master_client != -1);

    *fd_client_master = open(CLIENT_MASTER, O_WRONLY);
    assert(*fd_client_master != -1);

}

void envoieDonneesMaster(int fd, int order, int number){
    
    int ret;
    
    if(order == ORDER_COMPUTE_PRIME){
        printf("Le client n°%d envoie l'ordre %d avec comme nombre '%d'\n", getpid(), order, number);
    }
    else{
        printf("Le client n°%d envoie l'ordre %d\n", getpid(), order);
    }

    ret = write(fd, &order, sizeof(int));
    assert(ret != -1);

    if(order == ORDER_COMPUTE_PRIME){
        ret = write(fd, &number, sizeof(int));
        assert(ret != -1);
        printf("Le client envoie le numéro %d au master\n", number);
    }

}

void lectureDonneeRenvoie(int fd, int * answer){

    int ret;

    ret = read(fd, answer, sizeof(int));
    assert(ret != -1);

    // Permet au programme d'attendre que tous les workers aient bien reçus l'ordre d'arrêt
    if(*answer == -1){
        sleep(2);  
    }

}

void displayAnswer(int order, int answer, int number){

    if(order == ORDER_HOW_MANY_PRIME){
        printf("Le master a trouvé un total INCROYABLE de seulement %d nombre(s) premier(s). :)\n", answer);
    }
    else if(order == ORDER_HIGHEST_PRIME){
        printf("Le master a réussi le SURPRENANT exploit à trouver un nombre premier égale à '%d' !!!!!\nQuelle grandeur exceptionnel ! O.O\n", answer);
    }
    else if(answer == 1){
       printf("Mon corps est prêt, le nombre '%d' est un nombre premier !\n", number);
    }
    else if(answer == 0){
        printf("Mon corps n'est pas prêt, le nombre '%d' n'est pas un nombre premier !\n", number);
    }
    else{
        printf("J'ai bien reçu l'accusé de réception de la part du Master\n");
    }

}

void deblocageMaster(int semid_sync){

    int ret;

    ret = semop(semid_sync, &take, 1);
    assert(ret != -1);

    ret = semop(semid_sync, &sell, 1);
    assert(ret != -1);
}

void *fonctionThread(void * arg){
    ThreadData *data = (ThreadData *) arg;

    printf("THREAD : n=%d, i=%d, n modulo i = %d \n",data->n, data->i, (data->n % data->i));

    if((data->n % data->i) == 0) {
        data->result = false;
        printf("THREAD : data=%d\n", data->result);
    } else
    {
        data->result = true;
        printf("THREAD : data=%d\n", data->result);
    }
    return NULL;
}

void communicationClientMaster(int order, int number){

    key_t key_crit = ftok(SEMKEY_CRITICAL, PROJ_ID);
    key_t key_sync = ftok(SEMKEY_SYNC, PROJ_ID);
    int semid_crit, semid_sync;
    int fd_master_client, fd_client_master;
    int answer;
    int ret;

    // Ouverture des sémaphores
    ouvertureSemaphores(key_crit, key_sync, &semid_crit, &semid_sync);

    // Entrée en section critique
    ret = semop(semid_crit, &take, 1);
    assert(ret != -1);

    // Ouverture des 2 tubes nommés
    ouvertureTubeNommes(&fd_client_master, &fd_master_client);
    // Envoie des données au master
    envoieDonneesMaster(fd_client_master, order, number);
        
    // Lit la réponse du master sur le 2e tube (se bloque en attendant la réponse)
    lectureDonneeRenvoie(fd_master_client, &answer);

    // Affichage de la réponse attendue par le client
    displayAnswer(order, answer, number);

    // Fermeture des tubes
    liberationTubesNommes(fd_master_client, fd_client_master);
                
    // Libération de la section critique
    ret = semop(semid_crit, &sell, 1);
    assert(ret != -1);

    // Déblocage du master
    deblocageMaster(semid_sync);
}

/************************************************************************
 * Fonction principale
 ************************************************************************/

int main(int argc, char * argv[])
{
    int number = 0;
    int order = parseArgs(argc, argv, &number);

    // order peut valoir 5 valeurs (cf. master_client.h) :
    //      - ORDER_COMPUTE_PRIME_LOCAL
    //      - ORDER_STOP
    //      - ORDER_COMPUTE_PRIME
    //      - ORDER_HOW_MANY_PRIME
    //      - ORDER_HIGHEST_PRIME
    //
    // si c'est ORDER_COMPUTE_PRIME_LOCAL
    //    alors c'est un code complètement à part multi-thread
    // sinon
    //    - entrer en section critique :
    //           . pour empêcher que 2 clients communiquent simultanément
    //           . le mutex est déjà créé par le master
    //    - ouvrir les tubes nommés (ils sont déjà créés par le master)
    //           . les ouvertures sont bloquantes, il faut s'assurer que
    //             le master ouvre les tubes dans le même ordre
    //    - envoyer l'ordre et les données éventuelles au master
    //    - attendre la réponse sur le second tube
    //    - sortir de la section critique
    //    - libérer les ressources (fermeture des tubes, ...)
    //    - débloquer le master grâce à un second sémaphore (cf. ci-dessous)
    // 
    // Une fois que le master a envoyé la réponse au client, il se bloque
    // sur un sémaphore ; le dernier point permet donc au master de continuer
    //
    // N'hésitez pas à faire des fonctions annexes ; si la fonction main
    // ne dépassait pas une trentaine de lignes, ce serait bien.

    if(order == ORDER_COMPUTE_PRIME_LOCAL){
        pthread_t threadId[number];
        ThreadData datas[number];
        int ret;

        // Initialise les structures pour chaque case
        for(int i = 2; i<number; i++) {
            datas[i].result = true;
            datas[i].n = number;
            datas[i].i = i;
        }

        // Lance chaque thread
        for(int i = 2; i<number; i++) {
            ret = pthread_create(&(threadId[i]), NULL, fonctionThread, &(datas[i]));
            printf("data=%d\n", datas[i].result);
            assert(ret == 0);
        }

        // Vérifie si le nombre est premier, ou non
        bool res = true;
        for(int i = 2; i<number; i++) {
            if(datas[i].result == false) {
                res = false;
                i = number;
            }
        }

        if(res) {
            printf("%d est premier\n", number);
        }
        else{
            printf("%d n'est pas premier\n", number);
        }
    }
    else{
        communicationClientMaster(order, number);
    }
    
    return EXIT_SUCCESS;
}
