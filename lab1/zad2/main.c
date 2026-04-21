#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

int main( int argc , char *argv[] ) {
    if ( argc != 3 ) {
        return 1;
    }

    int N = atoi(argv[1]);
    int M = atoi(argv[2]);


    if ( N <= 0 || M <= 0) {
        return 1;
    }

    char M_str[100];
    sprintf(M_str, "%d", M);

    for( int i = 0 ; i < N ;  i ++ ) {
        pid_t pid = fork();

        if ( pid < 0 ) {
            exit( 1 );
        }
        if ( pid == 0 ) {

            execl( "./child", "child", M_str , NULL );
            perror("err");
            exit(1);
        }
    }
    while ( wait(0) > 0 );
    printf( "Rodzic (PID: %d)\n", getpid() );
    return 0;
}