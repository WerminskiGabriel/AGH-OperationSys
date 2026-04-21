#include <stdio.h>
#include <signal.h>


void handler(int signum) {
    printf("Wywołano handler dla sygnału <%d>\n", signum);
}

void sig_handle() {
    printf("wykonuje handle\n");
    signal(SIGUSR1, handler);
}
