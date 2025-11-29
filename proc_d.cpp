#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
bool wait_sem(int sem_id, unsigned short idx) {
    struct sembuf op{static_cast<unsigned short>(idx), -1, 0};
    return semop(sem_id, &op, 1) == 0;
}

bool post_sem(int sem_id, unsigned short idx) {
    struct sembuf op{static_cast<unsigned short>(idx), 1, 0};
    return semop(sem_id, &op, 1) == 0;
}

bool send_all(int sock, const char *data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t rc = ::send(sock, data + sent, len - sent, 0);
        if (rc <= 0) return false;
        sent += static_cast<size_t>(rc);
    }
    return true;
}
}

int main(int argc, char *argv[]) {
    FILE *out_log = std::fopen("proc_d.out", "w");
    FILE *err_log = std::fopen("proc_d.err", "w");

    if (argc != 4) {
        std::fprintf(err_log ? err_log : stderr, "Usage: %s <sem_id> <shm_id> <port1>\n", argv[0]);
        return 1;
    }

    int sem_id = std::atoi(argv[1]);
    int shm_id = std::atoi(argv[2]);
    int port = std::atoi(argv[3]);

    void *mem = shmat(shm_id, nullptr, 0);
    if (mem == reinterpret_cast<void *>(-1)) {
        std::perror("proc_d: shmat");
        if (err_log) std::fprintf(err_log, "shmat failed\n");
        return 1;
    }
    char *shared = static_cast<char *>(mem);

    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::perror("proc_d: socket");
        shmdt(shared);
        if (err_log) std::fprintf(err_log, "socket creation failed\n");
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == -1) {
        std::perror("proc_d: connect");
        if (err_log) std::fprintf(err_log, "connect failed\n");
        close(sock);
        shmdt(shared);
        return 1;
    }

    while (true) {
        if (!wait_sem(sem_id, 1)) {
            if (err_log) std::fprintf(err_log, "sem wait failed\n");
            break;
        }
        std::string payload(shared);
        if (!post_sem(sem_id, 0)) {
            if (err_log) std::fprintf(err_log, "sem post failed\n");
            break;
        }

        payload.push_back('\0');
        if (!send_all(sock, payload.c_str(), payload.size())) {
            std::perror("proc_d: send");
            if (err_log) std::fprintf(err_log, "send failed\n");
            break;
        }
        if (out_log) std::fprintf(out_log, "sent: %s\n", payload.c_str());
    }

    close(sock);
    shmdt(shared);
    if (out_log) std::fclose(out_log);
    if (err_log) std::fclose(err_log);
    return 0;
}
