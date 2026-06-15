#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <time.h>
#include <sys/wait.h>

#define N 2
#define M 5
#define K 2
#define T 1

#define STR_LEN 10

#define SHM_NAME "/buffer_shm"
#define SEM_EMPTY "/sem_empty"
#define SEM_FULL "/sem_full"
#define SEM_MUTEX "/sem_mutex"

struct SharedBuffer {
    char data[K][STR_LEN + 1];
    int in;
    int out;
};

void generate_string(char *str) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (int i = 0; i < STR_LEN; i++) {
        str[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    str[STR_LEN] = '\0';
}

void producer(int id) {
    srand(time(NULL) ^ (getpid() << 1));

    sem_t *empty = sem_open(SEM_EMPTY, 0);
    sem_t *full = sem_open(SEM_FULL, 0);
    sem_t *mutex = sem_open(SEM_MUTEX, 0);

    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    struct SharedBuffer *buf = mmap(NULL, sizeof(struct SharedBuffer), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    for (int i = 0; i < T; i++) {
        char task[STR_LEN + 1];
        generate_string(task);

        sem_wait(empty);
        sem_wait(mutex);

        strcpy(buf->data[buf->in], task);
        printf("[Producent %d] Zapisał: %s\n", id, task);
        buf->in = (buf->in + 1) % K;

        sem_post(mutex);
        sem_post(full);

        sleep(1);
    }

    munmap(buf, sizeof(struct SharedBuffer));
    exit(0);
}

void consumer(int id) {
    sem_t *empty = sem_open(SEM_EMPTY, 0);
    sem_t *full = sem_open(SEM_FULL, 0);
    sem_t *mutex = sem_open(SEM_MUTEX, 0);

    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    struct SharedBuffer *buf = mmap(NULL, sizeof(struct SharedBuffer), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    while (1) {
        sem_wait(full);
        sem_wait(mutex);

        char task[STR_LEN + 1];
        strcpy(task, buf->data[buf->out]);
        buf->out = (buf->out + 1) % K;

        printf("[Konsument %d] Pobiera: ", id);
        fflush(stdout);
        for (int i = 0; i < STR_LEN; i++) {
            printf("%c", task[i]);
            fflush(stdout);
            usleep(300000);
        }
        printf("\n");

        sem_post(mutex);
        sem_post(empty);
    }
}

int main() {
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(struct SharedBuffer));
    struct SharedBuffer *buf = mmap(NULL, sizeof(struct SharedBuffer), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    buf->in = 0;
    buf->out = 0;

    sem_unlink(SEM_EMPTY);
    sem_unlink(SEM_FULL);
    sem_unlink(SEM_MUTEX);

    sem_t *empty = sem_open(SEM_EMPTY, O_CREAT, 0666, K);
    sem_t *full = sem_open(SEM_FULL, O_CREAT, 0666, 0);
    sem_t *mutex = sem_open(SEM_MUTEX, O_CREAT, 0666, 1);

    for (int i = 0; i < M; i++) {
        if (fork() == 0) consumer(i + 1);
    }

    for (int i = 0; i < N; i++) {
        if (fork() == 0) producer(i + 1);
    }

    for (int i = 0; i < N; i++) wait(NULL);

    sleep(5);

    sem_close(empty);
    sem_close(full);
    sem_close(mutex);
    sem_unlink(SEM_EMPTY);
    sem_unlink(SEM_FULL);
    sem_unlink(SEM_MUTEX);
    munmap(buf, sizeof(struct SharedBuffer));
    shm_unlink(SHM_NAME);

    return 0;
}
