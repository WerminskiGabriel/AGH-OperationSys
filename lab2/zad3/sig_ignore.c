#include <signal.h>
#include <stdio.h>
void sig_ignore() {
    printf("wykonuje ignore\n");
    signal(SIGUSR1, SIG_IGN);
}