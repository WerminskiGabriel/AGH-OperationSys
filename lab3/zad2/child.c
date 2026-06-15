#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define PIPE_REQ  "/tmp/integral_req"
#define PIPE_RES  "/tmp/integral_res"

static double f(double x) {
    return 4.0 / (x * x + 1.0);
}

static double integrate(double a, double b, double h) {
    double sum = 0.0;
    for (double x = a; x < b; x += h)
        sum += f(x) * h;
    return sum;
}

int main(void) {
    int fd_req = open(PIPE_REQ, O_RDONLY);
    double buf[3];
    read(fd_req, buf, sizeof(buf));
    close(fd_req);

    double result = integrate(buf[0], buf[1], buf[2]);

    int fd_res = open(PIPE_RES, O_WRONLY);
    write(fd_res, &result, sizeof(double));
    close(fd_res);

    return 0;
}