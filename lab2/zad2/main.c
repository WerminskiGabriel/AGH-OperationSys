#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        return 1;
    }
    union sigval signal_type;
    if (strcmp(argv[1], "default") == 0) {
        signal_type.sival_int = 1;
    } else if (strcmp(argv[1], "mask") == 0) {
        signal_type.sival_int = 2;
    } else if (strcmp(argv[1], "ignore") == 0) {
        signal_type.sival_int = 3;
    } else if (strcmp(argv[1], "handle") == 0) {
        signal_type.sival_int = 4;
    } else {
        signal_type.sival_int = 0;
    }

    pid_t pid = fork();

    if (pid < 0) {
        exit(1);
    }

    if (pid == 0) {
        execl("./child", "child", argv[1], NULL);
        perror("error");
        exit(1);
    }
    sleep(1);
    sigqueue(pid,SIGUSR2, signal_type);
    wait(NULL);

    return 0;
}