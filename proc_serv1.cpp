#include <limits.h>
#include "ipc_common.h"
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        return EXIT_FAILURE;
    }
    long port1_value = 0;
    long port2_value = 0;
    if (parse_int(argv[1], &port1_value) != 0 || port1_value <= 0 || port1_value > 65535) {
        return EXIT_FAILURE;
    }
    if (parse_int(argv[2], &port2_value) != 0 || port2_value <= 0 || port2_value > 65535) {
        return EXIT_FAILURE;
    }
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock == -1) {
        return EXIT_FAILURE;
    }
    int opt = 1;
    if (setsockopt(tcp_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        close(tcp_sock);
        return EXIT_FAILURE;
    }
    struct sockaddr_in addr1;
    memset(&addr1, 0, sizeof(addr1));
    addr1.sin_family = AF_INET;
    addr1.sin_port = htons((uint16_t)port1_value);
    addr1.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(tcp_sock, (struct sockaddr *)&addr1, sizeof(addr1)) == -1) {
        close(tcp_sock);
        return EXIT_FAILURE;
    }
    if (listen(tcp_sock, 1) == -1) {
        close(tcp_sock);
        return EXIT_FAILURE;
    }
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock == -1) {
        close(tcp_sock);
        return EXIT_FAILURE;
    }
    struct sockaddr_in addr2;
    memset(&addr2, 0, sizeof(addr2));
    addr2.sin_family = AF_INET;
    addr2.sin_port = htons((uint16_t)port2_value);
    addr2.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (kill(getppid(), SIGUSR1) == -1) {
        close(udp_sock);
        close(tcp_sock);
        return EXIT_FAILURE;
    }
    int client = accept(tcp_sock, NULL, NULL);
    if (client == -1) {
        close(udp_sock);
        close(tcp_sock);
        return EXIT_FAILURE;
    }
    struct sigaction ignore_act;
    memset(&ignore_act, 0, sizeof(ignore_act));
    ignore_act.sa_handler = SIG_IGN;
    sigemptyset(&ignore_act.sa_mask);
    if (sigaction(SIGPIPE, &ignore_act, NULL) == -1) {
        close(client);
        close(udp_sock);
        close(tcp_sock);
        return EXIT_FAILURE;
    }
    char buffer[IPC_TEXT_SIZE];
    for (;;) {
        ssize_t len = read_line_fd(client, buffer, sizeof(buffer));
        if (len < 0) {
            close(client);
            close(udp_sock);
            close(tcp_sock);
            return EXIT_FAILURE;
        }
        if (len == 0) {
            if (sendto(udp_sock, "", 0, 0, (struct sockaddr *)&addr2, sizeof(addr2)) == -1) {
                close(client);
                close(udp_sock);
                close(tcp_sock);
                return EXIT_FAILURE;
            }
            break;
        }
        char tagged[IPC_TEXT_SIZE];
        int written = snprintf(tagged, sizeof(tagged), "Serv1: %s", buffer);
        if (written < 0 || (size_t)written >= sizeof(tagged)) {
            close(client);
            close(udp_sock);
            close(tcp_sock);
            return EXIT_FAILURE;
        }
        if (sendto(udp_sock, tagged, (size_t)written, 0, (struct sockaddr *)&addr2, sizeof(addr2)) == -1) {
            close(client);
            close(udp_sock);
            close(tcp_sock);
            return EXIT_FAILURE;
        }
    }
    close(client);
    close(udp_sock);
    close(tcp_sock);
    return EXIT_SUCCESS;
}
