#ifndef CLIENT_CRIBLE
#define CLIENT_CRIBLE

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <assert.h>

// On peut mettre ici des éléments propres au couple master/client :
//    - des constantes pour rendre plus lisible les comunications
//    - des fonctions communes (création tubes, écriture dans un tube,
//      manipulation de sémaphores, ...)

// ordres possibles pour le master
#define ORDER_NONE                0
#define ORDER_STOP               -1
#define ORDER_COMPUTE_PRIME       1
#define ORDER_HOW_MANY_PRIME      2
#define ORDER_HIGHEST_PRIME       3
#define ORDER_COMPUTE_PRIME_LOCAL 4   // ne concerne pas le master

// bref n'hésitez à mettre nombre de fonctions avec des noms explicites
// pour masquer l'implémentation

#define MASTER_CLIENT "PIPE_master_client"
#define CLIENT_MASTER "PIPE_client_master"

#define SEMKEY_CRITICAL "client.c"
#define SEMKEY_SYNC "master.c"

#define PROJ_ID 5

struct sembuf take;
struct sembuf sell;
struct sembuf wait;

void liberationTubesNommes(int fd_master_client, int fd_client_master);

#endif
