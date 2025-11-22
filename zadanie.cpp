#include <limits.h>
#include "ipc_common.h"
#include <signal.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};
static void safe_close(int *fd) {
    if (*fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}
static volatile sig_atomic_t ready_signals;
static void ready_handler(int sig) {
    (void)sig;
    ++ready_signals;
}
static int wait_for_ready(sig_atomic_t target) {
    sigset_t mask;
    sigemptyset(&mask);
    while (ready_signals < target) {
        if (sigsuspend(&mask) == -1 && errno != EINTR) {
            return -1;
        }
    }
    return 0;
}
static pid_t spawn_process(const char *path, char *const argv[], const int *close_fds, size_t close_count) {
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        for (size_t i = 0; i < close_count; ++i) {
            if (close_fds[i] >= 0) {
                close(close_fds[i]);
            }
        }
        execv(path, argv);
        _exit(EXIT_FAILURE);
    }
    return pid;
}
static int init_semaphore(int semid, unsigned short index, int value) {
    union semun arg;
    arg.val = value;
    return semctl(semid, index, SETVAL, arg);
}
static int init_shared_segment(int shmid) {
    struct ipc_message *segment = (struct ipc_message *)shmat(shmid, NULL, 0);
    if (segment == (void *)-1) {
        return -1;
    }
    memset(segment, 0, sizeof(*segment));
    if (shmdt(segment) == -1) {
        return -1;
    }
    return 0;
}
int main(int argc, char **argv) {
    if (argc != 3) {
        return EXIT_FAILURE;
    }
    struct sigaction ready_act;
    memset(&ready_act, 0, sizeof(ready_act));
    ready_act.sa_handler = ready_handler;
    sigemptyset(&ready_act.sa_mask);
    ready_act.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &ready_act, NULL) == -1) {
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
    int pipe_r1[2] = { -1, -1 };
    int pipe_r2[2] = { -1, -1 };
    if (pipe(pipe_r1) == -1) {
        return EXIT_FAILURE;
    }
    if (pipe(pipe_r2) == -1) {
        safe_close(&pipe_r1[0]);
        safe_close(&pipe_r1[1]);
        return EXIT_FAILURE;
    }
    int shm1 = shmget(IPC_PRIVATE, sizeof(struct ipc_message), IPC_CREAT | 0600);
    if (shm1 == -1) {
        safe_close(&pipe_r1[0]);
        safe_close(&pipe_r1[1]);
        safe_close(&pipe_r2[0]);
        safe_close(&pipe_r2[1]);
        return EXIT_FAILURE;
    }
    int shm2 = shmget(IPC_PRIVATE, sizeof(struct ipc_message), IPC_CREAT | 0600);
    if (shm2 == -1) {
        shmctl(shm1, IPC_RMID, NULL);
        safe_close(&pipe_r1[0]);
        safe_close(&pipe_r1[1]);
        safe_close(&pipe_r2[0]);
        safe_close(&pipe_r2[1]);
        return EXIT_FAILURE;
    }
    if (init_shared_segment(shm1) != 0 || init_shared_segment(shm2) != 0) {
        shmctl(shm1, IPC_RMID, NULL);
        shmctl(shm2, IPC_RMID, NULL);
        safe_close(&pipe_r1[0]);
        safe_close(&pipe_r1[1]);
        safe_close(&pipe_r2[0]);
        safe_close(&pipe_r2[1]);
        return EXIT_FAILURE;
    }
    int sem1 = semget(IPC_PRIVATE, 2, IPC_CREAT | 0600);
    if (sem1 == -1) {
        shmctl(shm1, IPC_RMID, NULL);
        shmctl(shm2, IPC_RMID, NULL);
        safe_close(&pipe_r1[0]);
        safe_close(&pipe_r1[1]);
        safe_close(&pipe_r2[0]);
        safe_close(&pipe_r2[1]);
        return EXIT_FAILURE;
    }
    int sem2 = semget(IPC_PRIVATE, 2, IPC_CREAT | 0600);
    if (sem2 == -1) {
        semctl(sem1, 0, IPC_RMID);
        shmctl(shm1, IPC_RMID, NULL);
        shmctl(shm2, IPC_RMID, NULL);
        safe_close(&pipe_r1[0]);
        safe_close(&pipe_r1[1]);
        safe_close(&pipe_r2[0]);
        safe_close(&pipe_r2[1]);
        return EXIT_FAILURE;
    }
    if (init_semaphore(sem1, 0, 1) == -1 || init_semaphore(sem1, 1, 0) == -1 || init_semaphore(sem2, 0, 1) == -1 || init_semaphore(sem2, 1, 0) == -1) {
        semctl(sem1, 0, IPC_RMID);
        semctl(sem2, 0, IPC_RMID);
        shmctl(shm1, IPC_RMID, NULL);
        shmctl(shm2, IPC_RMID, NULL);
        safe_close(&pipe_r1[0]);
        safe_close(&pipe_r1[1]);
        safe_close(&pipe_r2[0]);
        safe_close(&pipe_r2[1]);
        return EXIT_FAILURE;
    }
    char port1_str[16];
    char port2_str[16];
    char shm1_str[32];
    char shm2_str[32];
    char sem1_str[32];
    char sem2_str[32];
    char fd_r1_read_str[32];
    char fd_r1_write_str[32];
    char fd_r2_read_str[32];
    char fd_r2_write_str[32];
    snprintf(port1_str, sizeof(port1_str), "%ld", port1_value);
    snprintf(port2_str, sizeof(port2_str), "%ld", port2_value);
    snprintf(shm1_str, sizeof(shm1_str), "%d", shm1);
    snprintf(shm2_str, sizeof(shm2_str), "%d", shm2);
    snprintf(sem1_str, sizeof(sem1_str), "%d", sem1);
    snprintf(sem2_str, sizeof(sem2_str), "%d", sem2);
    snprintf(fd_r1_read_str, sizeof(fd_r1_read_str), "%d", pipe_r1[0]);
    snprintf(fd_r1_write_str, sizeof(fd_r1_write_str), "%d", pipe_r1[1]);
    snprintf(fd_r2_read_str, sizeof(fd_r2_read_str), "%d", pipe_r2[0]);
    snprintf(fd_r2_write_str, sizeof(fd_r2_write_str), "%d", pipe_r2[1]);
    pid_t children[16];
    size_t child_count = 0;
    int status_code = EXIT_SUCCESS;
    sig_atomic_t expected_ready = 0;
    int close_all_fds[] = { pipe_r1[0], pipe_r1[1], pipe_r2[0], pipe_r2[1] };
    char *serv2_args[] = { (char *)"./proc_serv2", port2_str, NULL };
    pid_t pid = spawn_process("./proc_serv2", serv2_args, close_all_fds, sizeof(close_all_fds) / sizeof(close_all_fds[0]));
    if (pid == -1) {
        status_code = EXIT_FAILURE;
        goto cleanup;
    }
    children[child_count++] = pid;
    if (wait_for_ready(++expected_ready) != 0) {
        status_code = EXIT_FAILURE;
        goto cleanup;
    }
    char *serv1_args[] = { (char *)"./proc_serv1", port1_str, port2_str, NULL };
    pid = spawn_process("./proc_serv1", serv1_args, close_all_fds, sizeof(close_all_fds) / sizeof(close_all_fds[0]));
    if (pid == -1) {
        status_code = EXIT_FAILURE;
        goto cleanup;
    }
    children[child_count++] = pid;
    if (wait_for_ready(++expected_ready) != 0) {
        status_code = EXIT_FAILURE;
        goto cleanup;
    }
    char *s_args[] = { (char *)"./proc_s", shm1_str, sem1_str, shm2_str, sem2_str, NULL };
    pid = spawn_process("./proc_s", s_args, close_all_fds, sizeof(close_all_fds) / sizeof(close_all_fds[0]));
    if (pid == -1) {
        status_code = EXIT_FAILURE;
        goto cleanup;
    }
    children[child_count++] = pid;
    if (wait_for_ready(++expected_ready) != 0) {
        status_code = EXIT_FAILURE;
        goto cleanup;
    }
    int close_t_fds[] = { pipe_r1[0], pipe_r1[1], pipe_r2[1] };
    char *t_args[] = { (char *)"./proc_t", fd_r2_read_str, shm1_str, sem1_str, NULL };
    pid = spawn_process("./proc_t", t_args, close_t_fds, sizeof(close_t_fds) / sizeof(close_t_fds[0]));
    if (pid == -1) {
        status_code = EXIT_FAILURE;
        goto cleanup;
    }
    children[child_count++] = pid;
    if (wait_for_ready(++expected_ready) != 0) {
        status_code = EXIT_FAILURE;
        goto cleanup;
    }
    char *d_args[] = { (char *)"./proc_d", shm2_str, sem2_str, port1_str, NULL };
    pid = spawn_process("./proc_d", d_args, close_all_fds, sizeof(close_all_fds) / sizeof(close_all_fds[0]));
    if (pid == -1) {
        status_code = EXIT_FAILURE;
        goto cleanup;
    }
    children[child_count++] = pid;
    if (wait_for_ready(++expected_ready) != 0) {
        status_code = EXIT_FAILURE;
        goto cleanup;
    }
    int close_p_fds[] = { pipe_r1[0], pipe_r2[0], pipe_r2[1] };
    char *p1_args[] = { (char *)"./proc_p1", fd_r1_write_str, (char *)"p1.txt", NULL };
    pid = spawn_process("./proc_p1", p1_args, close_p_fds, sizeof(close_p_fds) / sizeof(close_p_fds[0]));
    if (pid == -1) {
        status_code = EXIT_FAILURE;
        goto cleanup;
    }
    children[child_count++] = pid;
    if (wait_for_ready(++expected_ready) != 0) {
        status_code = EXIT_FAILURE;
        goto cleanup;
    }
    char *p2_args[] = { (char *)"./proc_p2", fd_r1_write_str, (char *)"p2.txt", NULL };
    pid = spawn_process("./proc_p2", p2_args, close_p_fds, sizeof(close_p_fds) / sizeof(close_p_fds[0]));
    if (pid == -1) {
        status_code = EXIT_FAILURE;
        goto cleanup;
    }
    children[child_count++] = pid;
    if (wait_for_ready(++expected_ready) != 0) {
        status_code = EXIT_FAILURE;
        goto cleanup;
    }
    int close_pr_fds[] = { pipe_r1[1], pipe_r2[0] };
    char pid_p1_str[32];
    char pid_p2_str[32];
    snprintf(pid_p1_str, sizeof(pid_p1_str), "%ld", (long)children[child_count - 2]);
    snprintf(pid_p2_str, sizeof(pid_p2_str), "%ld", (long)children[child_count - 1]);
    char *pr_args[] = { (char *)"./proc_pr", pid_p1_str, pid_p2_str, fd_r1_read_str, fd_r2_write_str, NULL };
    pid = spawn_process("./proc_pr", pr_args, close_pr_fds, sizeof(close_pr_fds) / sizeof(close_pr_fds[0]));
    if (pid == -1) {
        status_code = EXIT_FAILURE;
        goto cleanup;
    }
    children[child_count++] = pid;
    if (wait_for_ready(++expected_ready) != 0) {
        status_code = EXIT_FAILURE;
        goto cleanup;
    }
    safe_close(&pipe_r1[0]);
    safe_close(&pipe_r1[1]);
    safe_close(&pipe_r2[0]);
    safe_close(&pipe_r2[1]);
    for (;;) {
        pid_t waited = wait(NULL);
        if (waited == -1) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
    }
cleanup:
    if (status_code != EXIT_SUCCESS) {
        for (size_t i = 0; i < child_count; ++i) {
            kill(children[i], SIGTERM);
        }
    }
    while (waitpid(-1, NULL, 0) > 0) {
    }
    semctl(sem1, 0, IPC_RMID);
    semctl(sem2, 0, IPC_RMID);
    shmctl(shm1, IPC_RMID, NULL);
    shmctl(shm2, IPC_RMID, NULL);
    safe_close(&pipe_r1[0]);
    safe_close(&pipe_r1[1]);
    safe_close(&pipe_r2[0]);
    safe_close(&pipe_r2[1]);
    return status_code;
}
