#include <stdio.h> //prinf
#include <stdlib.h> //coversions (atoi )
#include <unistd.h> // fork getid sleep
#include <sys/wait.h>
#include <sys/types.h>


int zmiennaGlobalna = 0;

#define M 1

int main( int argc , char *argv[] ) {
    if ( argc != 2 ) {
        return 1;
    }

    int N = atoi(argv[1]);

    if ( N == 0 ) {
        return 1;
    }

    for( int i = 0 ; i < N ;  i ++ ) {
        pid_t pid = vfork();

        if ( pid < 0 ) {
            exit( 1 );
        }
        else if ( pid == 0 ) {

            for( int j = 0 ; j < M ; j ++ ) {
                zmiennaGlobalna++;
                printf( "Potomek (PID: %d )\n", getpid() );
                usleep(250000);
            }
            exit(0);
        }
    }
    while ( wait(0) > 0 );
    printf( "Rodzic (PID: %d) zmiennaGlobalna=%d\n", getpid(), zmiennaGlobalna );
    return 0;
}