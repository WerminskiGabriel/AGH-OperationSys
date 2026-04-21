#include <signal.h>
#include <stdio.h>



void sig_default() {
    signal(SIGUSR1, SIG_DFL);
}