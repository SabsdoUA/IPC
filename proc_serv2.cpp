#include <limits.h>
#include "ipc_common.h"
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
int main(int argc, char **argv) {
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    long port_value = 0;
    if (parse_int(argv[1], &port_value) != 0 || port_value <= 0 || port_value > 65535) {
        return EXIT_FAILURE;
    }
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        return EXIT_FAILURE;
    }
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        close(sock);
        return EXIT_FAILURE;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port_value);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        close(sock);
        return EXIT_FAILURE;
    }
    FILE *output = fopen("serv2.txt", "w");
    if (!output) {
        close(sock);
        return EXIT_FAILURE;
    }
    if (kill(getppid(), SIGUSR1) == -1) {
        fclose(output);
        close(sock);
        return EXIT_FAILURE;
    }
    char buffer[IPC_TEXT_SIZE];
    for (;;) {
        ssize_t received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, NULL, NULL);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            fclose(output);
            close(sock);
            return EXIT_FAILURE;
        }
        if (received == 0) {
            break;
        }
        size_t len = (size_t)received;
        buffer[len] = '\0';
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
            buffer[--len] = '\0';
        }
        if (fwrite(buffer, 1, len, output) != len) {
            fclose(output);
            close(sock);
            return EXIT_FAILURE;
        }
        if (fputc('\n', output) == EOF) {
            fclose(output);
            close(sock);
            return EXIT_FAILURE;
        }
        if (fflush(output) == EOF) {
            fclose(output);
            close(sock);
            return EXIT_FAILURE;
        }
    }
    fclose(output);
    close(sock);
    return EXIT_SUCCESS;
}
