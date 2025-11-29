#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    FILE *out_log = std::fopen("proc_serv2.out", "w");
    FILE *err_log = std::fopen("proc_serv2.err", "w");

    if (argc != 3) {
        std::fprintf(err_log ? err_log : stderr, "Usage: %s <port2> <output_file>\n", argv[0]);
        return 1;
    }

    int port = std::atoi(argv[1]);
    const char *out_path = argv[2];

    int sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        std::perror("proc_serv2: socket");
        if (err_log) std::fprintf(err_log, "socket creation failed\n");
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == -1) {
        std::perror("proc_serv2: bind");
        if (err_log) std::fprintf(err_log, "bind failed\n");
        close(sock);
        return 1;
    }

    FILE *out = std::fopen(out_path, "w");
    if (!out) {
        std::perror("proc_serv2: fopen");
        if (err_log) std::fprintf(err_log, "cannot open output file\n");
        close(sock);
        return 1;
    }

    char buffer[512];
    sockaddr_in peer{};
    socklen_t peer_len = sizeof(peer);

    while (true) {
        ssize_t received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                                    reinterpret_cast<sockaddr *>(&peer), &peer_len);
        if (received < 0) {
            std::perror("proc_serv2: recvfrom");
            if (err_log) std::fprintf(err_log, "recvfrom failed\n");
            break;
        }
        buffer[received] = '\0';

        std::fprintf(out, "%s\n", buffer);
        std::fflush(out);

        if (std::string(buffer) == "Koncim" || std::string(buffer) == "Koncim\n") {
            break;
        }
        if (out_log) std::fprintf(out_log, "received: %s\n", buffer);
    }

    std::fclose(out);
    close(sock);
    if (out_log) std::fclose(out_log);
    if (err_log) std::fclose(err_log);
    return 0;
}
