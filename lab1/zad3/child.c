#include "definitions.h"
#include <sys/file.h>

int main( int argc , char *argv[] ) {
    if ( argc != 2 ) {
        return 1;
    }
    int M = atoi(argv[1]);

    FILE* file = fopen(OUTPUT_FILE, "a");
    int fd = fileno(file);

    if (flock(fd, LOCK_EX) == -1){
        perror("Flock");
        return 1;
    }

    for( int j = 0 ; j < M ; j ++ ) {
        char buffer[100];
        int len = sprintf(buffer, "Potomek (PID: %d)\n", getpid());
        fwrite(buffer, sizeof(char), len, file);
        fflush(file);
        usleep(250000);
    }
    if (flock(fd, LOCK_UN) == -1)
    perror("Flock");

    fclose(file);
    return 0;
}