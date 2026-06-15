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

#define N 3
#define M 2
#define K 2
#define T 2
#define STR_LEN 10

#define SHM_NAME "/buffer_shm_task3"
#define SEM_MUTEX "/sem_mutex"
#define SEM_EMPTY_NORM "/sem_empty_norm"
#define SEM_EMPTY_PRIO "/sem_empty_prio"
#define SEM_FULL_TOTAL "/sem_full_total"

struct SharedData {
    char norm_data[K][STR_LEN + 1];
    char prio_data[K][STR_LEN + 1];
    int norm_in, norm_out, norm_count;
    int prio_in, prio_out, prio_count;
};

void generate_string(char *str) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (int i = 0; i < STR_LEN; i++) {
        str[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    str[STR_LEN] = '\0';
}

void manager() {
    sem_t *mutex = sem_open(SEM_MUTEX, 0);
    sem_t *e_norm = sem_open(SEM_EMPTY_NORM, 0);
    sem_t *e_prio = sem_open(SEM_EMPTY_PRIO, 0);

    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    struct SharedData *sd = mmap(NULL, sizeof(struct SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    while (1) {
        sleep(5);
        sem_wait(mutex);

        printf("\n[MANAGER] Monitor: NORMAL [%d/%d], PRIORITY [%d/%d]\n",
               sd->norm_count, K, sd->prio_count, K);

        if (sd->norm_count > 0 && sd->prio_count < K) {
            char temp[STR_LEN + 1];
            strcpy(temp, sd->norm_data[sd->norm_out]);
            sd->norm_out = (sd->norm_out + 1) % K;
            sd->norm_count--;

            strcpy(sd->prio_data[sd->prio_in], temp);
            sd->prio_in = (sd->prio_in + 1) % K;
            sd->prio_count++;

            sem_post(e_norm);
            sem_wait(e_prio);

            printf("[MANAGER] Awansowano zadanie: %s (NORMAL -> PRIORITY)\n\n", temp);
        } else if (sd->norm_count > 0 && sd->prio_count == K) {
            printf("[MANAGER] Brak miejsca w PRIORITY na awans.\n\n");
        }
        sem_post(mutex);
    }
}

void producer(int id) {
    srand(time(NULL) ^ (getpid() << 1));
    sem_t *mutex = sem_open(SEM_MUTEX, 0);
    sem_t *e_norm = sem_open(SEM_EMPTY_NORM, 0);
    sem_t *e_prio = sem_open(SEM_EMPTY_PRIO, 0);
    sem_t *f_total = sem_open(SEM_FULL_TOTAL, 0);
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    struct SharedData *sd = mmap(NULL, sizeof(struct SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    for (int i = 0; i < T; i++) {
        char task[STR_LEN + 1];
        generate_string(task);
        int is_priority = (rand() % 100 < 30);

        if (is_priority) {
            sem_wait(e_prio);
            sem_wait(mutex);
            strcpy(sd->prio_data[sd->prio_in], task);
            sd->prio_in = (sd->prio_in + 1) % K;
            sd->prio_count++;
            sem_post(mutex);
        } else {
            sem_wait(e_norm);
            sem_wait(mutex);
            strcpy(sd->norm_data[sd->norm_in], task);
            sd->norm_in = (sd->norm_in + 1) % K;
            sd->norm_count++;
            sem_post(mutex);
        }
        sem_post(f_total);
        sleep(rand() % 3);
    }
    exit(0);
}

void consumer(int id) {
    sem_t *mutex = sem_open(SEM_MUTEX, 0);
    sem_t *e_norm = sem_open(SEM_EMPTY_NORM, 0);
    sem_t *e_prio = sem_open(SEM_EMPTY_PRIO, 0);
    sem_t *f_total = sem_open(SEM_FULL_TOTAL, 0);
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    struct SharedData *sd = mmap(NULL, sizeof(struct SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    while (1) {
        sem_wait(f_total);
        sem_wait(mutex);

        char task[STR_LEN + 1];
        int was_prio = 0;

        if (sd->prio_count > 0) {
            strcpy(task, sd->prio_data[sd->prio_out]);
            sd->prio_out = (sd->prio_out + 1) % K;
            sd->prio_count--;
            was_prio = 1;
        } else {
            strcpy(task, sd->norm_data[sd->norm_out]);
            sd->norm_out = (sd->norm_out + 1) % K;
            sd->norm_count--;
        }


        printf("[Konsument %d] [%s]: %s\n", id, was_prio ? "PRIO" : "NORM", task);
        for (int i = 0; i < STR_LEN; i++) {
            usleep(300000);
        }
        sem_post(mutex);
        if (was_prio) sem_post(e_prio);
        else sem_post(e_norm);
    }
}

int main() {
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(struct SharedData));
    struct SharedData *sd = mmap(NULL, sizeof(struct SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    memset(sd, 0, sizeof(struct SharedData));

    sem_unlink(SEM_MUTEX);
    sem_unlink(SEM_EMPTY_NORM);
    sem_unlink(SEM_EMPTY_PRIO);
    sem_unlink(SEM_FULL_TOTAL);

    sem_open(SEM_MUTEX, O_CREAT, 0666, 1);
    sem_open(SEM_EMPTY_NORM, O_CREAT, 0666, K);
    sem_open(SEM_EMPTY_PRIO, O_CREAT, 0666, K);
    sem_open(SEM_FULL_TOTAL, O_CREAT, 0666, 0);

    pid_t manager_pid = fork();
    if (manager_pid == 0) {
        manager();
        exit(0);
    }
    for (int i = 0; i < M; i++) if (fork() == 0) consumer(i + 1);
    for (int i = 0; i < N; i++) if (fork() == 0) producer(i + 1);

    for (int i = 0; i < N; i++) wait(NULL);

    printf("Wszyscy producenci zakonczyli prace.\n");
    sleep(5);

    kill(manager_pid, SIGTERM);

    sem_unlink(SEM_MUTEX);
    sem_unlink(SEM_EMPTY_NORM);
    sem_unlink(SEM_EMPTY_PRIO);
    sem_unlink(SEM_FULL_TOTAL);
    shm_unlink(SHM_NAME);

    return 0;
}
