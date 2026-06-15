#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define PIPE_REQ  "/tmp/integral_req"
#define PIPE_RES  "/tmp/integral_res"
#define STEP      0.000001

int main(void) {
    double a, b;
    printf("Podaj poczatek przedzialu: ");
    scanf("%lf", &a);
    printf("Podaj koniec przedzialu:   ");
    scanf("%lf", &b);

    mkfifo(PIPE_REQ, 0666);
    mkfifo(PIPE_RES, 0666);

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (pid == 0) {
        execl("./child", "child", NULL);
        perror("execl");
        exit(1);
    }

    int fd_req = open(PIPE_REQ, O_WRONLY);
    double buf[3] = { a, b, STEP };
    write(fd_req, buf, sizeof(buf));
    close(fd_req);

    int fd_res = open(PIPE_RES, O_RDONLY);
    double result;
    read(fd_res, &result, sizeof(double));
    close(fd_res);

    wait(NULL);

    printf("Wynik calki na [%.6f, %.6f]: %.10f\n", a, b, result);

    unlink(PIPE_REQ);
    unlink(PIPE_RES);
    return 0;
}