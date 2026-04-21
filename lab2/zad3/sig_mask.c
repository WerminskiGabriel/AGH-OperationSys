#include <signal.h>
#include <stddef.h>
#include <stdio.h>
void sig_mask() {
    printf("wykonuje mask\n");
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set,SIGUSR1);
    sigprocmask(SIG_BLOCK, &set,NULL);
}