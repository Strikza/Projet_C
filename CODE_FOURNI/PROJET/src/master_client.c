#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#define _XOPEN_SOURCE

#include <stdlib.h>
#include <stdio.h>

#include "myassert.h"

#include "master_client.h"

// fonctions éventuelles proposées dans le .h

void liberationTubesNommes(int fd_master_client, int fd_client_master){
    
    int ret;

    ret = close(fd_master_client);
    assert(ret != -1);

    ret = close(fd_client_master);
    assert(ret != -1);

}