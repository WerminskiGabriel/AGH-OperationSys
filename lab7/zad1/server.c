#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT         9000
#define BUFFER_SIZE  4096
#define BACKLOG      16

static int LICZNIK_ZAPYTAN = 0;

static void wyslij_odpowiedz_http(int fd, const char *body, int body_len) {
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Server: Zajeciowy serwer SO\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "Content-Length: %d\r\n"
        "\r\n",
        body_len);

    send(fd, header, header_len, 0);
    send(fd, body, body_len, 0);
}

static void obsluz_zapytanie(int fd, const char *buf) {
    char body[128];
    int body_len;

    if (strncmp(buf, "ZADANIE", 7) == 0) {
        int dodaj = 0;
        sscanf(buf + 7, "%d", &dodaj);
        LICZNIK_ZAPYTAN += dodaj;

        body_len = snprintf(body, sizeof(body), "Liczba pobrań strony: %d", LICZNIK_ZAPYTAN);
        send(fd, body, body_len, 0);
        return;
    }

    if (strncmp(buf, "GET", 3) == 0 || strncmp(buf, "POST", 4) == 0) {
        LICZNIK_ZAPYTAN++;

        body_len = snprintf(body, sizeof(body), "Liczba pobrań strony: %d", LICZNIK_ZAPYTAN);
        wyslij_odpowiedz_http(fd, body, body_len);
    }
}

int main(void) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("[SERWER] Nasluchuje na porcie %d (0.0.0.0:%d)\n", PORT, PORT);

    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        char buf[BUFFER_SIZE];
        ssize_t n = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            obsluz_zapytanie(client_fd, buf);
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
