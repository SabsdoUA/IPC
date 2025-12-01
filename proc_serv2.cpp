#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>

#define BUFFER_SIZE 1024

int main(int argc, char* argv[]) {
    if (argc != 3) return 1;

    int port = std::stoi(argv[1]);
    std::string outfile = argv[2];

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return 1;

    struct sockaddr_in serv_addr, cli_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Proc_serv2: Bind failed");
        return 1;
    }

    std::ofstream file(outfile, std::ios::app);
    char buffer[BUFFER_SIZE];
    socklen_t len = sizeof(cli_addr);

    while (true) {
        ssize_t n = recvfrom(sock, buffer, BUFFER_SIZE - 1, 0,
                             (struct sockaddr*)&cli_addr, &len);

        if (n > 0) {
            buffer[n] = '\0';
            file << buffer << std::endl;
            file.flush();
        }
    }

    close(sock);
    return 0;
}