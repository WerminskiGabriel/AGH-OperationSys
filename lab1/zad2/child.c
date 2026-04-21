#include <stdio.h> //prinf
#include <stdlib.h> //coversions (atoi )
#include <unistd.h> // fork getid sleep
#include <sys/wait.h>
#include <sys/types.h>


int main( int argc , char *argv[] ) {
    if ( argc != 2 ) {
        return 1;
    }
    int M = atoi(argv[1]);

    for( int j = 0 ; j < M ; j ++ ) {
        printf( "Potomek (PID: %d )\n", getpid() );
        usleep(250000);
    }
    return 0;
}