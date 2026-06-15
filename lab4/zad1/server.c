#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <signal.h>
#include <errno.h>

#define SERVER_KEY  0x1234ABCD
#define MAX_CLIENTS 16
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

static int server_qid = -1;

static struct { int id; int qid; } clients[MAX_CLIENTS];
static int n_clients = 0;

static void cleanup(int sig) {
    (void)sig;
    if (server_qid != -1) msgctl(server_qid, IPC_RMID, NULL);
    printf("\n[serwer] Zamknięty.\n");
    exit(0);
}

static void broadcast(int sender_id, const char *text) {
    msg_t m = { .mtype = MSG_DELIVER, .id = sender_id };
    strncpy(m.text, text, TEXT_SIZE - 1);
    for (int i = 0; i < n_clients; i++) {
        if (clients[i].id != sender_id)
            msgsnd(clients[i].qid, &m, MSG_PAYLOAD, IPC_NOWAIT);
    }
}

int main(void) {
    signal(SIGINT,  cleanup);
    signal(SIGTERM, cleanup);

    int old = msgget(SERVER_KEY, 0666);
    if (old != -1) msgctl(old, IPC_RMID, NULL);

    server_qid = msgget(SERVER_KEY, IPC_CREAT | 0666);
    if (server_qid == -1) { perror("msgget"); exit(1); }
    printf("[serwer] Gotowy (qid=%d)\n", server_qid);

    msg_t m;
    for (;;) {
        if (msgrcv(server_qid, &m, MSG_PAYLOAD, 0, 0) == -1) {
            if (errno == EINTR) continue;
            perror("msgrcv"); continue;
        }

        if (m.mtype == MSG_INIT) {
            if (n_clients >= MAX_CLIENTS) continue;
            int cqid = msgget(m.key, 0666);
            if (cqid == -1) { perror("msgget klienta"); continue; }

            int new_id = n_clients + 1;
            clients[n_clients].id  = new_id;
            clients[n_clients].qid = cqid;
            n_clients++;
            printf("[serwer] Klient %d połączony\n", new_id);

            msg_t resp = { .mtype = MSG_RESPONSE, .id = new_id };
            msgsnd(cqid, &resp, MSG_PAYLOAD, 0);

        } else if (m.mtype == MSG_BROADCAST) {
            printf("[serwer] Klient %d: %s\n", m.id, m.text);
            broadcast(m.id, m.text);
        }
    }
}