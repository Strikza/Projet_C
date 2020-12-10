#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "myassert.h"

#include "master_client.h"

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
    assert(semid_crit != -1);

    *semid_sync = semget(key_sync, 0, 0);
    assert(semid_sync != -1);

}

void ouvertureTubeNommes(int * fd_client_master, int * fd_master_client){

    *fd_master_client = open(MASTER_CLIENT, O_RDONLY);
    assert(fd_master_client != -1);

    *fd_client_master = open(CLIENT_MASTER, O_WRONLY);
    assert(fd_client_master != -1);

}

void envoieDonneesMaster(int fd, int order, int n){
    
    int ret;

    ret = write(fd, &order, sizeof(int));
    assert(ret != -1);

    if(order == ORDER_COMPUTE_PRIME){
        ret = write(fd, &n, sizeof(int));
        assert(ret != -1);
    }
}

void lectureDonneeRenvoie(int fd, bool * answer){

    int ret;

    ret = read(fd, answer, sizeof(bool));
    assert(ret != -1);
}

void déblocageMaster(int semid_sync){

    int ret;

    ret = semop(semid_sync, &take, 1);
    assert(ret != -1);

    ret = semop(semid_sync, &sell, 1);
    assert(ret != -1);
}

/************************************************************************
 * Fonction principale
 ************************************************************************/

int main(int argc, char * argv[])
{
    key_t key_crit = ftok(SEMKEY_CRITICAL, PROJ_ID);
    key_t key_sync = ftok(SEMKEY_SYNC, PROJ_ID);
    int semid_crit;
    int semid_sync;
    int fd_master_client;
    int fd_client_master;
    int ret;
    bool answer;


    int number = 0;
    int order = parseArgs(argc, argv, &number);
    printf("%d\n", order); // pour éviter le warning

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

    
    // Ouverture des sémaphores
    ouvertureSemaphores(key_crit, key_sync, &semid_crit, &semid_sync);

    if(order == ORDER_COMPUTE_PRIME_LOCAL){

    }
    else{

        // Entrée en section critique
        ret = semop(semid_crit, &take, 1);
        assert(ret != -1);

        // Ouverture des 2 tubes nommés
        ouvertureTubeNommes(&fd_client_master, &fd_master_client);

        // Envoie des données au master
        envoieDonneesMaster(fd_client_master, order, number);
        
        // Lit la réponse du master sur le 2e tube (se bloque en attendant la réponse)
        lectureDonneeRenvoie(fd_master_client, &answer);

        if(answer == true){
            printf("Mon corp est prêt, le nombre '%d' est un nombre premier !\n", number);
        }
        else{
            printf("Mon corp n'est pas prêt, le nombre '%d' n'est pas un nombre premier !\n", number);
        }

        // Libération de la section critique
        ret = semop(semid_crit, &sell, 1);
        assert(ret != -1);

        // Fermeture des tubes
        liberationRessource(fd_master_client, fd_client_master);

        // Déblocage du master
        déblocageMaster(semid_sync);
    }
    
    return EXIT_SUCCESS;
}
