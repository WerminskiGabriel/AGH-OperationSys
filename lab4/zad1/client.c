#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define SERVER_KEY  0x1234ABCD
#define TEXT_SIZE   512

#define MSG_INIT      1
#define MSG_BROADCAST 2
#define MSG_RESPONSE  3
#define MSG_DELIVER   4

typedef struct {
    long  mtype;
    int   id;
    key_t key;
    char  text[TEXT_SIZE];
} msg_t;
#define MSG_PAYLOAD (sizeof(msg_t) - sizeof(long))

static int client_qid = -1;
static pid_t child_pid = -1;

static void cleanup(int sig) {
    (void)sig;
    if (child_pid > 0) kill(child_pid, SIGTERM);
    if (client_qid != -1) msgctl(client_qid, IPC_RMID, NULL);
    printf("\n[klient] Rozłączony.\n");
    exit(0);
}

static void receiver(void) {
    msg_t m;
    for (;;) {
        if (msgrcv(client_qid, &m, MSG_PAYLOAD, MSG_DELIVER, 0) == -1) {
            if (errno == EINTR) continue;
            break;
        }
        printf("\n  [klient %d] >> %s\n> ", m.id, m.text);
        fflush(stdout);
    }
}

int main(void) {
    signal(SIGINT,  cleanup);
    signal(SIGTERM, cleanup);

    key_t my_key = (key_t)getpid();
    client_qid = msgget(my_key, IPC_CREAT | IPC_EXCL | 0666);
    if (client_qid == -1) { perror("msgget"); exit(1); }

    int server_qid = msgget(SERVER_KEY, 0666);
    if (server_qid == -1) {
        perror("msgget serwera");
        cleanup(0);
    }

    msg_t m = { .mtype = MSG_INIT, .key = my_key };
    msgsnd(server_qid, &m, MSG_PAYLOAD, 0);

    msgrcv(client_qid, &m, MSG_PAYLOAD, MSG_RESPONSE, 0);
    int my_id = m.id;
    printf("[klient] Połączony ID = %d\n> ", my_id);
    fflush(stdout);

    child_pid = fork();
    if (child_pid == 0) { receiver(); exit(0); }

    char line[TEXT_SIZE];
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0') { printf("> "); fflush(stdout); continue; }

        msg_t out = { .mtype = MSG_BROADCAST, .id = my_id };
        strncpy(out.text, line, TEXT_SIZE - 1);
        msgsnd(server_qid, &out, MSG_PAYLOAD, 0);
        printf("> "); fflush(stdout);
    }

    cleanup(0);
}