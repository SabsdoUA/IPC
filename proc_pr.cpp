#include <limits.h>
#include "ipc_common.h"
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char **argv) {
    if (argc != 5) {
        return EXIT_FAILURE;
    }
    long pid1_value = 0;
    long pid2_value = 0;
    long read_fd_value = 0;
    long write_fd_value = 0;
    if (parse_int(argv[1], &pid1_value) != 0 || pid1_value <= 0 || pid1_value > INT_MAX) {
        return EXIT_FAILURE;
    }
    if (parse_int(argv[2], &pid2_value) != 0 || pid2_value <= 0 || pid2_value > INT_MAX) {
        return EXIT_FAILURE;
    }
    if (parse_int(argv[3], &read_fd_value) != 0 || read_fd_value < 0 || read_fd_value > INT_MAX) {
        return EXIT_FAILURE;
    }
    if (parse_int(argv[4], &write_fd_value) != 0 || write_fd_value < 0 || write_fd_value > INT_MAX) {
        return EXIT_FAILURE;
    }
    pid_t producers[2];
    int active[2];
    producers[0] = (pid_t)pid1_value;
    producers[1] = (pid_t)pid2_value;
    active[0] = 1;
    active[1] = 1;
    int read_fd = (int)read_fd_value;
    int write_fd = (int)write_fd_value;
    struct sigaction ignore_act;
    memset(&ignore_act, 0, sizeof(ignore_act));
    ignore_act.sa_handler = SIG_IGN;
    sigemptyset(&ignore_act.sa_mask);
    if (sigaction(SIGPIPE, &ignore_act, NULL) == -1) {
        close(read_fd);
        close(write_fd);
        return EXIT_FAILURE;
    }
    if (kill(getppid(), SIGUSR1) == -1) {
        close(read_fd);
        close(write_fd);
        return EXIT_FAILURE;
    }
    char input[IPC_TEXT_SIZE];
    char output[IPC_TEXT_SIZE];
    int next = 0;
    for (;;) {
        for (int i = 0; i < 2; ++i) {
            int idx = (next + i) % 2;
            if (!active[idx]) {
                continue;
            }
            if (kill(producers[idx], SIGUSR1) == -1) {
                if (errno == ESRCH) {
                    active[idx] = 0;
                    continue;
                }
                close(read_fd);
                close(write_fd);
                return EXIT_FAILURE;
            }
        }
        if (!active[0] && !active[1]) {
            break;
        }
        ssize_t len = read_line_fd(read_fd, input, sizeof(input));
        if (len < 0) {
            close(read_fd);
            close(write_fd);
            return EXIT_FAILURE;
        }
        if (len == 0) {
            break;
        }
        int source = -1;
        if (strncmp(input, "P1: ", 4) == 0) {
            source = 0;
        } else if (strncmp(input, "P2: ", 4) == 0) {
            source = 1;
        }
        if (source == -1) {
            close(read_fd);
            close(write_fd);
            return EXIT_FAILURE;
        }
        int written = snprintf(output, sizeof(output), "Pr: %s", input);
        if (written < 0 || (size_t)written >= sizeof(output)) {
            close(read_fd);
            close(write_fd);
            return EXIT_FAILURE;
        }
        if (write_all(write_fd, output, (size_t)written) != 0) {
            close(read_fd);
            close(write_fd);
            return EXIT_FAILURE;
        }
        if (write_all(write_fd, "\n", 1) != 0) {
            close(read_fd);
            close(write_fd);
            return EXIT_FAILURE;
        }
        next = 1 - source;
    }
    close(read_fd);
    close(write_fd);
    return EXIT_SUCCESS;
}
