#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define RUN_SECONDS          20
#define CAMERA_HZ            25
#define WRITER_HZ            10
#define ROBOT_HZ             100
#define LOGGER_HZ            10
#define SYNC_THRESHOLD_MS    20
#define BUFFER_SIZE          32
#define STATS_INTERVAL_SEC   3
#define WATCHDOG_INTERVAL_MS 500
#define WATCHDOG_DEADLINE_MS 80

#define OUTPUT_DIR           "output"
#define LOGGER_FILE          "output/robot_state.log"
#define REPORT_FILE          "output/report.txt"

typedef struct {
    struct timespec timestamp;
    int frame_number;
    int camera_id;
} camera_frame_t;

typedef struct {
    camera_frame_t left;
    camera_frame_t right;
    int pair_id;
} stereo_pair_t;

typedef struct {
    double x;
    double y;
    double orientation;
    struct timespec timestamp;
} robot_state_t;

typedef struct {
    camera_frame_t frames[BUFFER_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} frame_buffer_t;

typedef struct {
    stereo_pair_t pairs[BUFFER_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    sem_t ready;
} stereo_buffer_t;

static volatile sig_atomic_t g_running = 1;

static pthread_mutex_t g_robot_mutex = PTHREAD_MUTEX_INITIALIZER;
static robot_state_t g_robot_state;
static sem_t g_robot_sem;

static frame_buffer_t g_left_buffer;
static frame_buffer_t g_right_buffer;
static stereo_buffer_t g_stereo_buffer;

static pthread_mutex_t g_stats_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_left_frames = 0;
static int g_right_frames = 0;
static int g_stereo_pairs = 0;
static int g_robot_samples = 0;
static int g_saved_pairs = 0;

static atomic_ulong g_atomic_left_frames = 0;
static atomic_ulong g_atomic_right_frames = 0;
static atomic_ulong g_atomic_robot_samples = 0;
static atomic_ulong g_atomic_stereo_pairs = 0;
static atomic_ulong g_atomic_saved_pairs = 0;

static struct timespec g_last_left_frame;
static struct timespec g_last_right_frame;
static struct timespec g_last_robot_update;
static pthread_mutex_t g_watchdog_mutex = PTHREAD_MUTEX_INITIALIZER;

static long timespec_diff_ms(const struct timespec *a, const struct timespec *b) {
    long sec = a->tv_sec - b->tv_sec;
    long nsec = a->tv_nsec - b->tv_nsec;
    return sec * 1000L + nsec / 1000000L;
}

static void timespec_now(struct timespec *ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

static void sleep_hz(int hz) {
    struct timespec req = {0, 1000000000L / hz};
    clock_nanosleep(CLOCK_MONOTONIC, 0, &req, NULL);
}

static void frame_buffer_init(frame_buffer_t *buf) {
    memset(buf, 0, sizeof(*buf));
    pthread_mutex_init(&buf->mutex, NULL);
    pthread_cond_init(&buf->not_empty, NULL);
    pthread_cond_init(&buf->not_full, NULL);
}

static void frame_buffer_destroy(frame_buffer_t *buf) {
    pthread_cond_destroy(&buf->not_empty);
    pthread_cond_destroy(&buf->not_full);
    pthread_mutex_destroy(&buf->mutex);
}

static int frame_buffer_put(frame_buffer_t *buf, const camera_frame_t *frame) {
    pthread_mutex_lock(&buf->mutex);

    while (buf->count == BUFFER_SIZE && g_running) {
        pthread_cond_wait(&buf->not_full, &buf->mutex);
    }
    if (!g_running) {
        pthread_mutex_unlock(&buf->mutex);
        return -1;
    }

    buf->frames[buf->head] = *frame;
    buf->head = (buf->head + 1) % BUFFER_SIZE;
    buf->count++;
    pthread_cond_signal(&buf->not_empty);

    pthread_mutex_unlock(&buf->mutex);
    return 0;
}

static int frame_buffer_get(frame_buffer_t *buf, camera_frame_t *frame) {
    pthread_mutex_lock(&buf->mutex);

    while (buf->count == 0 && g_running) {
        pthread_cond_wait(&buf->not_empty, &buf->mutex);
    }
    if (buf->count == 0) {
        pthread_mutex_unlock(&buf->mutex);
        return -1;
    }

    *frame = buf->frames[buf->tail];
    buf->tail = (buf->tail + 1) % BUFFER_SIZE;
    buf->count--;
    pthread_cond_signal(&buf->not_full);

    pthread_mutex_unlock(&buf->mutex);
    return 0;
}

static void stereo_buffer_init(stereo_buffer_t *buf) {
    memset(buf, 0, sizeof(*buf));
    pthread_mutex_init(&buf->mutex, NULL);
    sem_init(&buf->ready, 0, 0);
}

static void stereo_buffer_destroy(stereo_buffer_t *buf) {
    sem_destroy(&buf->ready);
    pthread_mutex_destroy(&buf->mutex);
}

static int stereo_buffer_put(stereo_buffer_t *buf, const stereo_pair_t *pair) {
    pthread_mutex_lock(&buf->mutex);

    if (buf->count == BUFFER_SIZE) {
        buf->tail = (buf->tail + 1) % BUFFER_SIZE;
        buf->count--;
    }

    buf->pairs[buf->head] = *pair;
    buf->head = (buf->head + 1) % BUFFER_SIZE;
    buf->count++;

    pthread_mutex_unlock(&buf->mutex);
    sem_post(&buf->ready);
    return 0;
}

static int stereo_buffer_get(stereo_buffer_t *buf, stereo_pair_t *pair) {
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 1;

    if (sem_timedwait(&buf->ready, &timeout) != 0) {
        return -1;
    }

    pthread_mutex_lock(&buf->mutex);

    if (buf->count == 0) {
        pthread_mutex_unlock(&buf->mutex);
        return -1;
    }

    *pair = buf->pairs[buf->tail];
    buf->tail = (buf->tail + 1) % BUFFER_SIZE;
    buf->count--;

    pthread_mutex_unlock(&buf->mutex);
    return 0;
}

static void system_init(void) {
    mkdir(OUTPUT_DIR, 0755);
    sem_init(&g_robot_sem, 0, 0);
    frame_buffer_init(&g_left_buffer);
    frame_buffer_init(&g_right_buffer);
    stereo_buffer_init(&g_stereo_buffer);
    memset(&g_robot_state, 0, sizeof(g_robot_state));
    timespec_now(&g_last_left_frame);
    timespec_now(&g_last_right_frame);
    timespec_now(&g_last_robot_update);
}

static void system_cleanup(void) {
    frame_buffer_destroy(&g_left_buffer);
    frame_buffer_destroy(&g_right_buffer);
    stereo_buffer_destroy(&g_stereo_buffer);
    sem_destroy(&g_robot_sem);
}

static void system_notify_shutdown(void) {
    pthread_mutex_lock(&g_left_buffer.mutex);
    pthread_cond_broadcast(&g_left_buffer.not_empty);
    pthread_cond_broadcast(&g_left_buffer.not_full);
    pthread_mutex_unlock(&g_left_buffer.mutex);

    pthread_mutex_lock(&g_right_buffer.mutex);
    pthread_cond_broadcast(&g_right_buffer.not_empty);
    pthread_cond_broadcast(&g_right_buffer.not_full);
    pthread_mutex_unlock(&g_right_buffer.mutex);

    sem_post(&g_robot_sem);
    sem_post(&g_stereo_buffer.ready);
}

static void request_shutdown(int sig) {
    (void)sig;
    g_running = 0;
    system_notify_shutdown();
    printf("\n[SIGNAL] Zatrzymywanie systemu (CTRL+C)...\n");
}

static void update_stats(int camera_id) {
    pthread_mutex_lock(&g_stats_mutex);
    if (camera_id == 0) {
        g_left_frames++;
    } else {
        g_right_frames++;
    }
    pthread_mutex_unlock(&g_stats_mutex);

    if (camera_id == 0) {
        atomic_fetch_add(&g_atomic_left_frames, 1);
        pthread_mutex_lock(&g_watchdog_mutex);
        timespec_now(&g_last_left_frame);
        pthread_mutex_unlock(&g_watchdog_mutex);
    } else {
        atomic_fetch_add(&g_atomic_right_frames, 1);
        pthread_mutex_lock(&g_watchdog_mutex);
        timespec_now(&g_last_right_frame);
        pthread_mutex_unlock(&g_watchdog_mutex);
    }
}

static void set_thread_priority(pthread_t thread, int priority) {
    struct sched_param param;
    memset(&param, 0, sizeof(param));
    param.sched_priority = priority;

    if (pthread_setschedparam(thread, SCHED_FIFO, &param) != 0) {
        if (pthread_setschedparam(thread, SCHED_RR, &param) != 0) {
            fprintf(stderr, "[PRIORITY] Brak uprawnien do SCHED_FIFO/RR (uruchom z sudo?)\n");
        }
    }
}

static void write_report(void) {
    FILE *report = fopen(REPORT_FILE, "w");
    if (!report) {
        perror("fopen report");
        return;
    }

    unsigned long left = atomic_load(&g_atomic_left_frames);
    unsigned long right = atomic_load(&g_atomic_right_frames);
    unsigned long robot = atomic_load(&g_atomic_robot_samples);
    unsigned long stereo = atomic_load(&g_atomic_stereo_pairs);
    unsigned long saved = atomic_load(&g_atomic_saved_pairs);

    fprintf(report, "Raport systemu robota mobilnego\n");
    fprintf(report, "==============================\n");
    fprintf(report, "Czas pracy: %d s\n", RUN_SECONDS);
    fprintf(report, "Ramki lewej kamery: %lu\n", left);
    fprintf(report, "Ramki prawej kamery: %lu\n", right);
    fprintf(report, "Pary stereo: %lu\n", stereo);
    fprintf(report, "Zapisane pary: %lu\n", saved);
    fprintf(report, "Probki stanu robota: %lu\n", robot);
    fprintf(report, "Srednia czestotliwosc kamer: %.2f Hz\n",
            (double)(left + right) / (2.0 * RUN_SECONDS));
    fprintf(report, "Srednia czestotliwosc robota: %.2f Hz\n",
            (double)robot / RUN_SECONDS);
    fprintf(report, "Srednia czestotliwosc zapisu: %.2f Hz\n",
            (double)saved / RUN_SECONDS);

    fclose(report);
    printf("[REPORT] Zapisano %s\n", REPORT_FILE);
}

static void *left_camera_thread(void *arg) {
    (void)arg;
    int frame_num = 0;

    while (g_running) {
        camera_frame_t frame;
        timespec_now(&frame.timestamp);
        frame.frame_number = ++frame_num;
        frame.camera_id = 0;

        frame_buffer_put(&g_left_buffer, &frame);
        update_stats(0);
        sleep_hz(CAMERA_HZ);
    }

    return NULL;
}

static void *right_camera_thread(void *arg) {
    (void)arg;
    int frame_num = 0;

    while (g_running) {
        camera_frame_t frame;
        timespec_now(&frame.timestamp);
        frame.frame_number = ++frame_num;
        frame.camera_id = 1;

        frame_buffer_put(&g_right_buffer, &frame);
        update_stats(1);
        sleep_hz(CAMERA_HZ);
    }

    return NULL;
}

static void *sync_thread(void *arg) {
    (void)arg;
    int pair_id = 0;
    camera_frame_t left;
    camera_frame_t right;
    int has_left = 0;
    int has_right = 0;

    while (g_running) {
        if (!has_left && frame_buffer_get(&g_left_buffer, &left) == 0) {
            has_left = 1;
        }

        if (!has_right && frame_buffer_get(&g_right_buffer, &right) == 0) {
            has_right = 1;
        }

        if (!has_left || !has_right) {
            continue;
        }

        long diff = labs(timespec_diff_ms(&left.timestamp, &right.timestamp));

        if (diff < SYNC_THRESHOLD_MS) {
            stereo_pair_t pair;
            pair.left = left;
            pair.right = right;
            pair.pair_id = ++pair_id;
            stereo_buffer_put(&g_stereo_buffer, &pair);

            pthread_mutex_lock(&g_stats_mutex);
            g_stereo_pairs++;
            pthread_mutex_unlock(&g_stats_mutex);
            atomic_fetch_add(&g_atomic_stereo_pairs, 1);

            has_left = 0;
            has_right = 0;
        } else if (timespec_diff_ms(&left.timestamp, &right.timestamp) < 0) {
            has_left = 0;
        } else {
            has_right = 0;
        }
    }

    return NULL;
}

static void *writer_thread(void *arg) {
    (void)arg;

    while (g_running) {
        stereo_pair_t pair;

        if (stereo_buffer_get(&g_stereo_buffer, &pair) == 0) {
            char left_name[64];
            char right_name[64];

            snprintf(left_name, sizeof(left_name), OUTPUT_DIR "/left_%04d.jpg", pair.pair_id);
            snprintf(right_name, sizeof(right_name), OUTPUT_DIR "/right_%04d.jpg", pair.pair_id);

            FILE *lf = fopen(left_name, "w");
            if (lf) {
                fprintf(lf, "frame=%d ts=%ld.%09ld\n",
                        pair.left.frame_number,
                        pair.left.timestamp.tv_sec,
                        pair.left.timestamp.tv_nsec);
                fclose(lf);
            }

            FILE *rf = fopen(right_name, "w");
            if (rf) {
                fprintf(rf, "frame=%d ts=%ld.%09ld\n",
                        pair.right.frame_number,
                        pair.right.timestamp.tv_sec,
                        pair.right.timestamp.tv_nsec);
                fclose(rf);
            }

            pthread_mutex_lock(&g_stats_mutex);
            g_saved_pairs++;
            pthread_mutex_unlock(&g_stats_mutex);
            atomic_fetch_add(&g_atomic_saved_pairs, 1);

            printf("[WRITER] Zapisano pare %04d\n", pair.pair_id);
        }

        sleep_hz(WRITER_HZ);
    }

    return NULL;
}

static void *robot_thread(void *arg) {
    (void)arg;
    double x = 0.0;
    double y = 0.0;
    double orientation = 0.0;

    while (g_running) {
        pthread_mutex_lock(&g_robot_mutex);
        timespec_now(&g_robot_state.timestamp);
        g_robot_state.x = x;
        g_robot_state.y = y;
        g_robot_state.orientation = orientation;
        pthread_mutex_unlock(&g_robot_mutex);

        sem_post(&g_robot_sem);

        pthread_mutex_lock(&g_stats_mutex);
        g_robot_samples++;
        pthread_mutex_unlock(&g_stats_mutex);
        atomic_fetch_add(&g_atomic_robot_samples, 1);

        pthread_mutex_lock(&g_watchdog_mutex);
        timespec_now(&g_last_robot_update);
        pthread_mutex_unlock(&g_watchdog_mutex);

        x += 0.01;
        y += 0.005;
        orientation += 0.02;

        sleep_hz(ROBOT_HZ);
    }

    return NULL;
}

static void *logger_thread(void *arg) {
    (void)arg;
    FILE *log = fopen(LOGGER_FILE, "w");
    if (!log) {
        perror("fopen logger");
        return NULL;
    }

    while (g_running) {
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 1;

        if (sem_timedwait(&g_robot_sem, &timeout) != 0) {
            continue;
        }

        robot_state_t state;
        pthread_mutex_lock(&g_robot_mutex);
        state = g_robot_state;
        pthread_mutex_unlock(&g_robot_mutex);

        fprintf(log, "ts=%ld.%09ld x=%.3f y=%.3f orient=%.3f\n",
                state.timestamp.tv_sec, state.timestamp.tv_nsec,
                state.x, state.y, state.orientation);
        fflush(log);

        sleep_hz(LOGGER_HZ);
    }

    fclose(log);
    return NULL;
}

static void *stats_thread(void *arg) {
    (void)arg;
    int elapsed = 0;

    while (g_running) {
        sleep(STATS_INTERVAL_SEC);
        if (!g_running) {
            break;
        }

        elapsed += STATS_INTERVAL_SEC;

        pthread_mutex_lock(&g_stats_mutex);
        printf("\n[STATS] Po %d s: lewa=%d prawa=%d stereo=%d robot=%d zapisane=%d\n",
               elapsed, g_left_frames, g_right_frames,
               g_stereo_pairs, g_robot_samples, g_saved_pairs);
        pthread_mutex_unlock(&g_stats_mutex);
    }

    return NULL;
}

static void *watchdog_thread(void *arg) {
    (void)arg;
    struct timespec now;
    struct timespec left_ts;
    struct timespec right_ts;
    struct timespec robot_ts;

    while (g_running) {
        timespec_now(&now);

        pthread_mutex_lock(&g_watchdog_mutex);
        left_ts = g_last_left_frame;
        right_ts = g_last_right_frame;
        robot_ts = g_last_robot_update;
        pthread_mutex_unlock(&g_watchdog_mutex);

        if (timespec_diff_ms(&now, &left_ts) > WATCHDOG_DEADLINE_MS) {
            printf("[WATCHDOG] LEFT CAMERA SLOW\n");
        }

        if (timespec_diff_ms(&now, &right_ts) > WATCHDOG_DEADLINE_MS) {
            printf("[WATCHDOG] RIGHT CAMERA SLOW\n");
        }

        if (timespec_diff_ms(&now, &robot_ts) > WATCHDOG_DEADLINE_MS / 2) {
            printf("[WATCHDOG] ROBOT STATE SLOW\n");
        }

        struct timespec sleep_time = {0, WATCHDOG_INTERVAL_MS * 1000000L};
        clock_nanosleep(CLOCK_MONOTONIC, 0, &sleep_time, NULL);
    }

    return NULL;
}

static void join_all(pthread_t *threads, int count) {
    for (int i = 0; i < count; i++) {
        pthread_join(threads[i], NULL);
    }
}

int main(void) {
    pthread_t threads[8];
    struct sigaction sa;

    printf("=== Zadanie 3: RT + watchdog + atomics ===\n");

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = request_shutdown;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    system_init();

    pthread_create(&threads[0], NULL, left_camera_thread, NULL);
    pthread_create(&threads[1], NULL, right_camera_thread, NULL);
    pthread_create(&threads[2], NULL, sync_thread, NULL);
    pthread_create(&threads[3], NULL, writer_thread, NULL);
    pthread_create(&threads[4], NULL, robot_thread, NULL);
    pthread_create(&threads[5], NULL, logger_thread, NULL);
    pthread_create(&threads[6], NULL, stats_thread, NULL);
    pthread_create(&threads[7], NULL, watchdog_thread, NULL);

    set_thread_priority(threads[4], 50);
    set_thread_priority(threads[7], 40);

    for (int i = 0; i < RUN_SECONDS && g_running; i++) {
        sleep(1);
    }

    g_running = 0;
    system_notify_shutdown();
    join_all(threads, 8);
    write_report();
    system_cleanup();

    printf("\n[KONIEC] System RT zatrzymany.\n");
    return 0;
}
