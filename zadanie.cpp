#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
volatile sig_atomic_t ready_signals = 0;

void ready_handler(int signo) {
    if (signo == SIGUSR1) {
        ++ready_signals;
    }
}

union semun_wrapper {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

pid_t spawn_process(const std::string &path, const std::vector<std::string> &args, const std::vector<int> &fds_to_close) {
    pid_t pid = fork();
    if (pid != 0) {
        return pid;
    }

    for (int fd : fds_to_close) {
        ::close(fd);
    }

    std::vector<char *> argv;
    argv.reserve(args.size() + 2);
    argv.push_back(const_cast<char *>(path.c_str()));
    for (const auto &arg : args) {
        argv.push_back(const_cast<char *>(arg.c_str()));
    }
    argv.push_back(nullptr);

    execvp(path.c_str(), argv.data());
    std::perror("execvp");
    _exit(127);
}
}

int main(int argc, char *argv[]) {
    FILE *out_log = std::fopen("zadanie.out", "w");
    FILE *err_log = std::fopen("zadanie.err", "w");

    if (argc != 3) {
        std::fprintf(err_log ? err_log : stderr, "Usage: %s <port1> <port2>\n", argv[0]);
        return 1;
    }

    int port1 = std::atoi(argv[1]);
    int port2 = std::atoi(argv[2]);

    struct sigaction sa{};
    sa.sa_handler = ready_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa, nullptr) == -1) {
        std::perror("zadanie: sigaction");
        if (err_log) std::fprintf(err_log, "sigaction failed\n");
        return 1;
    }

    int r1[2];
    int r2[2];
    if (pipe(r1) == -1 || pipe(r2) == -1) {
        std::perror("zadanie: pipe");
        if (err_log) std::fprintf(err_log, "pipe creation failed\n");
        return 1;
    }

    int shm1 = shmget(IPC_PRIVATE, 256, IPC_CREAT | 0666);
    int shm2 = shmget(IPC_PRIVATE, 256, IPC_CREAT | 0666);
    if (shm1 == -1 || shm2 == -1) {
        std::perror("zadanie: shmget");
        if (err_log) std::fprintf(err_log, "shmget failed\n");
        return 1;
    }

    int sem1 = semget(IPC_PRIVATE, 2, IPC_CREAT | 0666);
    int sem2 = semget(IPC_PRIVATE, 2, IPC_CREAT | 0666);
    if (sem1 == -1 || sem2 == -1) {
        std::perror("zadanie: semget");
        if (err_log) std::fprintf(err_log, "semget failed\n");
        return 1;
    }

    semun_wrapper s{};
    unsigned short init1[2] = {1, 0};
    s.array = init1;
    if (semctl(sem1, 0, SETALL, s) == -1) {
        std::perror("zadanie: semctl SETALL sem1");
        if (err_log) std::fprintf(err_log, "semctl sem1 failed\n");
        return 1;
    }
    unsigned short init2[2] = {1, 0};
    s.array = init2;
    if (semctl(sem2, 0, SETALL, s) == -1) {
        std::perror("zadanie: semctl SETALL sem2");
        if (err_log) std::fprintf(err_log, "semctl sem2 failed\n");
        return 1;
    }

    pid_t p1 = spawn_process("./proc_p1", {std::to_string(r1[1]), "p1.txt"}, {r1[0], r2[0], r2[1]});
    pid_t p2 = spawn_process("./proc_p2", {std::to_string(r1[1]), "p2.txt"}, {r1[0], r2[0], r2[1]});
    pid_t t = spawn_process("./proc_t", {std::to_string(sem1), std::to_string(shm1), std::to_string(r2[0])}, {r1[0], r1[1], r2[1]});
    pid_t d = spawn_process("./proc_d", {std::to_string(sem2), std::to_string(shm2), std::to_string(port1)}, {r1[0], r1[1], r2[0], r2[1]});
    pid_t serv2 = spawn_process("./proc_serv2", {std::to_string(port2), "serv2.txt"}, {r1[0], r1[1], r2[0], r2[1]});

    pid_t sproc = spawn_process("./proc_s", {std::to_string(shm1), std::to_string(sem1), std::to_string(shm2), std::to_string(sem2)}, {r1[1], r2[0]});
    pid_t serv1 = spawn_process("./proc_serv1", {std::to_string(port1), std::to_string(port2)}, {r1[0], r1[1], r2[0], r2[1]});
    pid_t pr = spawn_process("./proc_pr", {std::to_string(p1), std::to_string(p2), std::to_string(r1[0]), std::to_string(r2[1])}, {r1[1], r2[0]});

    if (out_log) {
        std::fprintf(out_log, "spawned p1=%d p2=%d t=%d d=%d serv2=%d s=%d serv1=%d pr=%d\n", p1, p2, t, d, serv2, sproc, serv1, pr);
    }

    (void)t;
    (void)d;
    (void)serv2;
    (void)sproc;
    (void)serv1;
    (void)pr;

    ::close(r1[0]);
    ::close(r1[1]);
    ::close(r2[0]);
    ::close(r2[1]);

    while (ready_signals < 3) {
        pause();
    }

    if (out_log) std::fprintf(out_log, "readiness signals received\n");

    int status = 0;
    waitpid(pr, &status, 0);

    std::vector<pid_t> to_terminate = {p1, p2, t, d, serv2, sproc, serv1};
    for (pid_t pid : to_terminate) {
        if (pid > 0) kill(pid, SIGTERM);
    }

    for (pid_t pid : to_terminate) {
        if (pid > 0) waitpid(pid, nullptr, 0);
    }

    shmctl(shm1, IPC_RMID, nullptr);
    shmctl(shm2, IPC_RMID, nullptr);
    semctl(sem1, 0, IPC_RMID);
    semctl(sem2, 0, IPC_RMID);

    if (out_log) std::fclose(out_log);
    if (err_log) std::fclose(err_log);

    return 0;
}
