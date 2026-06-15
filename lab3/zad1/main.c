#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

static double f(double x) {
    return 4.0 / (x * x + 1.0);
}

static double integrate_range(double a, double b, double h) {
    double sum = 0.0;
    for (double x = a; x < b; x += h)
        sum += f(x) * h;
    return sum;
}

static void run_child(int write_fd, double a, double b, double h) {
    double result = integrate_range(a, b, h);
    if (write(write_fd, &result, sizeof(double)) < 0) perror("write");

    close(write_fd);
    exit(0);
}

static double run_for_k(double h, int k) {
    int pipes[k][2];

    for (int i = 0; i < k; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            exit(1);
        }
    }

    double chunk = 1.0 / k;

    for (int i = 0; i < k; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {
            for (int j = 0; j < k; j++) {
                close(pipes[j][0]);
                if (j != i) close(pipes[j][1]);
            }
            double a = i * chunk;
            double b = (i == k - 1) ? 1.0 : a + chunk;
            run_child(pipes[i][1], a, b, h);
        }

        close(pipes[i][1]);
    }

    double total = 0.0;
    for (int i = 0; i < k; i++) {
        double part;
        if (read(pipes[i][0], &part, sizeof(double)) < 0) perror("read");
        close(pipes[i][0]);
        total += part;
    }

    for (int i = 0; i < k; i++) wait(NULL);

    return total;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        return 1;
    }

    setbuf(stdout, NULL);

    double h = atof(argv[1]);
    int n = atoi(argv[2]);

    if (h <= 0 || n < 1) {
        return 1;
    }

    for (int k = 1; k <= n; k++) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);

        double result = run_for_k(h, k);

        clock_gettime(CLOCK_MONOTONIC, &t1);
        double elapsed = (t1.tv_sec - t0.tv_sec)
                         + (t1.tv_nsec - t0.tv_nsec) * 1e-9;

        printf("k=%-3d  result=%.10f  time=%.4fs\n", k, result, elapsed);
    }
    return 0;
}
