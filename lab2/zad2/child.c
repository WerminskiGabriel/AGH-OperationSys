#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

void sig_default() {
    signal(SIGUSR1, SIG_DFL);
}

void handler(int signum) {
    printf("Wywołano handler dla sygnału <%d>\n", signum);
}

void sig_handle() {
    printf("wykonuje handle\n");
    signal(SIGUSR1, handler);
}

void sig_ignore() {
    printf("wykonuje ignore\n");
    signal(SIGUSR1, SIG_IGN);
}

void sig_mask() {
    printf("wykonuje mask\n");
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set,SIGUSR1);
    sigprocmask(SIG_BLOCK, &set,NULL);
}

void sig_unblock() {
    printf("wykonuje unblock\n");
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set,SIGUSR1);
    sigprocmask(SIG_UNBLOCK, &set,NULL);
}

void SIGUSR2_handler(int sig, siginfo_t *info, void *ucontext) {
    int code = info->si_value.sival_int;
    printf("\nOdebrano sygnał USR2 z main.(kod: %d)\n", code);

    if (code == 1)  sig_default();
    else if (code == 2)  sig_mask();
    else if (code == 3)  sig_ignore();
    else if (code == 4)  sig_handle();


}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        return 1;
    }

    struct sigaction sa;
    sa.sa_sigaction = SIGUSR2_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR2, &sa, NULL);
    sleep(1);

    for (int i = 1; i <= 20; i++) {
        printf("%d\n", i);
        if (i == 5 || i == 15) {
            printf("Wysyłam sygnał USR2\n");
            raise(SIGUSR1);
        } else if (i == 10) {
            sigset_t pending_set;
            sigpending(&pending_set);
            if (sigismember(&pending_set, SIGUSR1)) {
                printf("Odblokowuję USR1");
                sig_unblock();
            }
        }
        sleep(1);

    }
    printf("Pętla została wykonana w całości.");
    return 0;
}