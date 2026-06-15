#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uzycie: %s <adres_ipv4> <port> <liczba>\n", argv[0]);
        return 1;
    }

    const char *host = argv[1];
    int port = atoi(argv[2]);
    int liczba = atoi(argv[3]);

    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Niepoprawny port\n");
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        perror("inet_pton");
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    char msg[64];
    int msg_len = snprintf(msg, sizeof(msg), "ZADANIE %d", liczba);
    if (send(sock, msg, msg_len, 0) < 0) {
        perror("send");
        close(sock);
        return 1;
    }

    char buf[BUFFER_SIZE];
    ssize_t n;
    while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
        fwrite(buf, 1, (size_t)n, stdout);
    }

    if (n < 0) {
        perror("recv");
        close(sock);
        return 1;
    }

    printf("\n");
    close(sock);
    return 0;
}
