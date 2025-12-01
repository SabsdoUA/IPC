#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>

volatile sig_atomic_t ready_signals_count = 0;

void sigusr1_handler_parent(int signo) {
    if (signo == SIGUSR1) ready_signals_count++;
}

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

void cleanup_resources(int shm1, int shm2, int sem1, int sem2) {
    if (shm1 != -1) shmctl(shm1, IPC_RMID, NULL);
    if (shm2 != -1) shmctl(shm2, IPC_RMID, NULL);
    if (sem1 != -1) semctl(sem1, 0, IPC_RMID);
    if (sem2 != -1) semctl(sem2, 0, IPC_RMID);
}

void terminate_process(pid_t pid) {
    if (pid > 0) {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <tcp_port> <udp_port>" << std::endl;
        return 1;
    }

    const char* tcp_port = argv[1];
    const char* udp_port = argv[2];

    struct sigaction sa;
    sa.sa_handler = sigusr1_handler_parent;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("Sigaction failed");
        return 1;
    }

    int pipe_r1[2], pipe_r2[2];
    if (pipe(pipe_r1) == -1 || pipe(pipe_r2) == -1) {
        perror("Pipe creation failed");
        return 1;
    }

    int shm1_id = shmget(IPC_PRIVATE, 256, IPC_CREAT | 0666);
    int shm2_id = shmget(IPC_PRIVATE, 256, IPC_CREAT | 0666);
    int sem1_id = semget(IPC_PRIVATE, 2, IPC_CREAT | 0666);
    int sem2_id = semget(IPC_PRIVATE, 2, IPC_CREAT | 0666);

    if (shm1_id == -1 || shm2_id == -1 || sem1_id == -1 || sem2_id == -1) {
        perror("IPC Creation failed");
        cleanup_resources(shm1_id, shm2_id, sem1_id, sem2_id);
        return 1;
    }

    union semun arg;
    unsigned short values[2] = {1, 0};
    arg.array = values;
    if (semctl(sem1_id, 0, SETALL, arg) == -1 || semctl(sem2_id, 0, SETALL, arg) == -1) {
        perror("Semaphore initialization failed");
        cleanup_resources(shm1_id, shm2_id, sem1_id, sem2_id);
        return 1;
    }

    pid_t serv1_pid = fork();
    if (serv1_pid == 0) {
        execl("./proc_serv1", "proc_serv1", tcp_port, udp_port, (char*)NULL);
        perror("Exec Serv1 failed");
        exit(1);
    } else if (serv1_pid < 0) {
        perror("Fork Serv1 failed");
        cleanup_resources(shm1_id, shm2_id, sem1_id, sem2_id);
        return 1;
    }

    while (ready_signals_count < 1) pause();

    pid_t s_pid = fork();
    if (s_pid == 0) {
        char shm1[16], sem1[16], shm2[16], sem2[16];
        std::sprintf(shm1, "%d", shm1_id);
        std::sprintf(sem1, "%d", sem1_id);
        std::sprintf(shm2, "%d", shm2_id);
        std::sprintf(sem2, "%d", sem2_id);
        execl("./proc_s", "proc_s", shm1, sem1, shm2, sem2, (char*)NULL);
        perror("Exec S failed");
        exit(1);
    } else if (s_pid < 0) {
        perror("Fork S failed");
        cleanup_resources(shm1_id, shm2_id, sem1_id, sem2_id);
        return 1;
    }

    while (ready_signals_count < 2) pause();

    pid_t p1_pid = fork();
    if (p1_pid == 0) {
        close(pipe_r1[0]);
        char fd[16];
        std::sprintf(fd, "%d", pipe_r1[1]);
        execl("./proc_p1", "proc_p1", fd, "p1.txt", (char*)NULL);
        perror("Exec P1 failed");
        exit(1);
    } else if (p1_pid < 0) {
        perror("Fork P1 failed");
        cleanup_resources(shm1_id, shm2_id, sem1_id, sem2_id);
        return 1;
    }

    pid_t p2_pid = fork();
    if (p2_pid == 0) {
        close(pipe_r1[0]);
        char fd[16];
        std::sprintf(fd, "%d", pipe_r1[1]);
        execl("./proc_p2", "proc_p2", fd, "p2.txt", (char*)NULL);
        perror("Exec P2 failed");
        exit(1);
    } else if (p2_pid < 0) {
        perror("Fork P2 failed");
        cleanup_resources(shm1_id, shm2_id, sem1_id, sem2_id);
        return 1;
    }

    pid_t t_pid = fork();
    if (t_pid == 0) {
        close(pipe_r2[1]);
        char fd[16], shm[16], sem[16];
        std::sprintf(fd, "%d", pipe_r2[0]);
        std::sprintf(shm, "%d", shm1_id);
        std::sprintf(sem, "%d", sem1_id);
        execl("./proc_t", "proc_t", fd, shm, sem, (char*)NULL);
        perror("Exec T failed");
        exit(1);
    } else if (t_pid < 0) {
        perror("Fork T failed");
        cleanup_resources(shm1_id, shm2_id, sem1_id, sem2_id);
        return 1;
    }

    pid_t d_pid = fork();
    if (d_pid == 0) {
        char shm[16], sem[16];
        std::sprintf(shm, "%d", shm2_id);
        std::sprintf(sem, "%d", sem2_id);
        execl("./proc_d", "proc_d", sem, shm, tcp_port, (char*)NULL);
        perror("Exec D failed");
        exit(1);
    } else if (d_pid < 0) {
        perror("Fork D failed");
        cleanup_resources(shm1_id, shm2_id, sem1_id, sem2_id);
        return 1;
    }

    pid_t serv2_pid = fork();
    if (serv2_pid == 0) {
        execl("./proc_serv2", "proc_serv2", udp_port, "serv2.txt", (char*)NULL);
        perror("Exec Serv2 failed");
        exit(1);
    } else if (serv2_pid < 0) {
        perror("Fork Serv2 failed");
        cleanup_resources(shm1_id, shm2_id, sem1_id, sem2_id);
        return 1;
    }

    pid_t pr_pid = fork();
    if (pr_pid == 0) {
        close(pipe_r1[1]);
        close(pipe_r2[0]);
        char p1[16], p2[16], r1[16], r2[16];
        std::sprintf(p1, "%d", p1_pid);
        std::sprintf(p2, "%d", p2_pid);
        std::sprintf(r1, "%d", pipe_r1[0]);
        std::sprintf(r2, "%d", pipe_r2[1]);
        execl("./proc_pr", "proc_pr", p1, p2, r1, r2, (char*)NULL);
        perror("Exec Pr failed");
        exit(1);
    } else if (pr_pid < 0) {
        perror("Fork Pr failed");
        cleanup_resources(shm1_id, shm2_id, sem1_id, sem2_id);
        return 1;
    }

    while (ready_signals_count < 3) pause();

    close(pipe_r1[0]);
    close(pipe_r1[1]);
    close(pipe_r2[0]);
    close(pipe_r2[1]);

    waitpid(pr_pid, NULL, 0);
    sleep(2);

    terminate_process(p1_pid);
    terminate_process(p2_pid);
    terminate_process(t_pid);
    terminate_process(s_pid);
    terminate_process(d_pid);
    terminate_process(serv1_pid);
    terminate_process(serv2_pid);

    cleanup_resources(shm1_id, shm2_id, sem1_id, sem2_id);
    return 0;
}
