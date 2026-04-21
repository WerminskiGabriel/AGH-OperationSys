#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <signal.h>

#ifdef DLL
#include <dlfcn.h>
void call_func(const char *func_name) {
    void *handle = dlopen("./libsignals.so", RTLD_LAZY);
    if (!handle) {
        exit(1);
    }
    void (*func)() = dlsym(handle, func_name);
    if (func) {
        func();
    }
}

#else
#include "signals.h"
#endif

void sig_unblock() {
    printf("wykonuje unblock\n");
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

void SIGUSR2_handler(int sig, siginfo_t *info, void *ucontext) {
    int code = info->si_value.sival_int;

    printf("\nOdebrano sygnał USR2 z main.(kod: %d\n", code);

#ifdef DLL
    if (code == 1) call_func("sig_default");
    else if (code == 2) call_func("sig_mask");
    else if (code == 3) call_func("sig_ignore");
    else if (code == 4) call_func("sig_handle");
#else
    if (code == 1) sig_default();
    else if (code == 2) sig_mask();
    else if (code == 3) sig_ignore();
    else if (code == 4) sig_handle();
#endif
}

int main(int argc, char *argv[]) {
    if (argc != 2) return 1;

    struct sigaction sa;
    sa.sa_sigaction = SIGUSR2_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR2, &sa, NULL);

    for (int i = 1; i <= 20; i++) {
        printf("%d\n", i);
        if (i == 5 || i == 15) {
            printf("Wysyłam sygnał USR1\n");
            raise(SIGUSR1);
        } else if (i == 10) {
            sigset_t pending_set;
            sigpending(&pending_set);
            if (sigismember(&pending_set, SIGUSR1)) {
                printf("Odblokowuję USR1\n");
                sig_unblock();
            }
        }
        sleep(1);
    }
    printf("Pętla wykonana.\n");
    return 0;
}
