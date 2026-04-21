#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

void sig_default() {
    signal(SIGUSR1, SIG_DFL);
}
void handler(int signum) {
    printf("Wywołano handler dla sygnału <%d>\n",signum);
}
void sig_handle() {
    signal(SIGUSR1, handler);
}

void sig_ignore() {
    signal(SIGUSR1, SIG_IGN);
}

void sig_mask() {
    sigset_t set;
    sigemptyset( &set);
    sigaddset( &set,SIGUSR1);
    sigprocmask( SIG_BLOCK, &set,NULL);
}
void sig_unblock() {
    sigset_t set;
    sigemptyset( &set);
    sigaddset( &set,SIGUSR1);
    sigprocmask( SIG_UNBLOCK, &set,NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        return 1;
    }

    if (strcmp(argv[1], "default") == 0) {
        sig_default();
    } else if (strcmp(argv[1], "mask") == 0) {
        sig_mask();
    } else if (strcmp(argv[1], "ignore") == 0) {
        sig_ignore();
    } else if (strcmp(argv[1], "handle") == 0) {
        sig_handle();
    }

    for (int i = 1; i <= 20; i++) {
        if (i == 5 || i == 15) {
            printf("Wysyłam sygnał USR1\n");
            raise(SIGUSR1);
        } else if (i == 10 ) {
            sigset_t pending_set;
            sigpending( &pending_set);
            if (sigismember(&pending_set, SIGUSR1)) {
                printf("Odblokowuję USR1");
                sig_unblock();
            }
        }
        printf("%d\n", i);
        sleep(1);
    }
    printf("Pętla została wykonana w całości.");
    return 0;
}