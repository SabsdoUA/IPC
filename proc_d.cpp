#include <limits.h>
#include "ipc_common.h"
#include <signal.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
int main(int argc, char **argv) {
    if (argc != 4) {
        return EXIT_FAILURE;
    }
    long shm_value = 0;
    long sem_value = 0;
    long port_value = 0;
    if (parse_int(argv[1], &shm_value) != 0) {
        return EXIT_FAILURE;
    }
    if (parse_int(argv[2], &sem_value) != 0) {
        return EXIT_FAILURE;
    }
    if (parse_int(argv[3], &port_value) != 0 || port_value <= 0 || port_value > 65535) {
        return EXIT_FAILURE;
    }
    int shm_id = (int)shm_value;
    int sem_id = (int)sem_value;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        return EXIT_FAILURE;
    }
    struct sigaction ignore_act;
    memset(&ignore_act, 0, sizeof(ignore_act));
    ignore_act.sa_handler = SIG_IGN;
    sigemptyset(&ignore_act.sa_mask);
    if (sigaction(SIGPIPE, &ignore_act, NULL) == -1) {
        close(sock);
        return EXIT_FAILURE;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port_value);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        close(sock);
        return EXIT_FAILURE;
    }
    struct ipc_message *segment = (struct ipc_message *)shmat(shm_id, NULL, 0);
    if (segment == (void *)-1) {
        close(sock);
        return EXIT_FAILURE;
    }
    if (kill(getppid(), SIGUSR1) == -1) {
        shmdt(segment);
        close(sock);
        return EXIT_FAILURE;
    }
    for (;;) {
        if (sem_acquire(sem_id, 1) != 0) {
            shmdt(segment);
            close(sock);
            return EXIT_FAILURE;
        }
        size_t len = segment->len;
        int finished = segment->finished;
        char data[IPC_TEXT_SIZE];
        if (len >= IPC_TEXT_SIZE) {
            if (sem_release(sem_id, 0) != 0) {
                shmdt(segment);
                close(sock);
                return EXIT_FAILURE;
            }
            shmdt(segment);
            close(sock);
            return EXIT_FAILURE;
        }
        memcpy(data, segment->data, len + 1);
        if (sem_release(sem_id, 0) != 0) {
            shmdt(segment);
            close(sock);
            return EXIT_FAILURE;
        }
        if (finished) {
            break;
        }
        if (write_all(sock, data, len) != 0) {
            shmdt(segment);
            close(sock);
            return EXIT_FAILURE;
        }
        const char newline = '\n';
        if (write_all(sock, &newline, 1) != 0) {
            shmdt(segment);
            close(sock);
            return EXIT_FAILURE;
        }
    }
    shutdown(sock, SHUT_WR);
    shmdt(segment);
    close(sock);
    return EXIT_SUCCESS;
}
