#include <limits.h>
#include "ipc_common.h"
#include <signal.h>
static volatile sig_atomic_t request_flag;
static int pipe_fd = -1;
static FILE *input_file = NULL;
static void signal_handler(int sig) {
    (void)sig;
    request_flag = 1;
}
int main(int argc, char **argv) {
    if (argc != 3) {
        return EXIT_FAILURE;
    }
    long fd_value = 0;
    if (parse_int(argv[1], &fd_value) != 0 || fd_value < 0 || fd_value > INT_MAX) {
        return EXIT_FAILURE;
    }
    pipe_fd = (int)fd_value;
    input_file = fopen(argv[2], "r");
    if (!input_file) {
        return EXIT_FAILURE;
    }
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        fclose(input_file);
        return EXIT_FAILURE;
    }
    struct sigaction ignore_act;
    memset(&ignore_act, 0, sizeof(ignore_act));
    ignore_act.sa_handler = SIG_IGN;
    sigemptyset(&ignore_act.sa_mask);
    if (sigaction(SIGPIPE, &ignore_act, NULL) == -1) {
        fclose(input_file);
        return EXIT_FAILURE;
    }
    sigset_t block_set;
    sigemptyset(&block_set);
    sigaddset(&block_set, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &block_set, NULL) == -1) {
        fclose(input_file);
        return EXIT_FAILURE;
    }
    sigset_t wait_set;
    sigemptyset(&wait_set);
    if (kill(getppid(), SIGUSR1) == -1) {
        fclose(input_file);
        close(pipe_fd);
        return EXIT_FAILURE;
    }
    char line[IPC_TEXT_SIZE];
    for (;;) {
        while (!request_flag) {
            if (sigsuspend(&wait_set) == -1 && errno != EINTR) {
                fclose(input_file);
                return EXIT_FAILURE;
            }
        }
        request_flag = 0;
        if (!fgets(line, (int)sizeof(line), input_file)) {
            fclose(input_file);
            close(pipe_fd);
            return EXIT_SUCCESS;
        }
        size_t len = strcspn(line, "\r\n");
        line[len] = '\0';
        if (len >= IPC_TEXT_SIZE) {
            fclose(input_file);
            close(pipe_fd);
            return EXIT_FAILURE;
        }
        char tagged[IPC_TEXT_SIZE];
        int written = snprintf(tagged, sizeof(tagged), "P2: %s", line);
        if (written < 0 || (size_t)written >= sizeof(tagged)) {
            fclose(input_file);
            close(pipe_fd);
            return EXIT_FAILURE;
        }
        if (write_all(pipe_fd, tagged, (size_t)written) != 0) {
            fclose(input_file);
            close(pipe_fd);
            return EXIT_FAILURE;
        }
        const char newline = '\n';
        if (write_all(pipe_fd, &newline, 1) != 0) {
            fclose(input_file);
            close(pipe_fd);
            return EXIT_FAILURE;
        }
    }
}
