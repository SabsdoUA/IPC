#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>

namespace {
union semun_wrapper {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

bool wait_sem(int sem_id, unsigned short idx) {
    struct sembuf op{static_cast<unsigned short>(idx), -1, 0};
    return semop(sem_id, &op, 1) == 0;
}

bool post_sem(int sem_id, unsigned short idx) {
    struct sembuf op{static_cast<unsigned short>(idx), 1, 0};
    return semop(sem_id, &op, 1) == 0;
}
}

int main(int argc, char *argv[]) {
    FILE *out_log = std::fopen("proc_t.out", "w");
    FILE *err_log = std::fopen("proc_t.err", "w");

    if (argc != 4) {
        std::fprintf(err_log ? err_log : stderr, "Usage: %s <sem_id> <shm_id> <pipe_read_fd>\n", argv[0]);
        return 1;
    }

    int sem_id = std::atoi(argv[1]);
    int shm_id = std::atoi(argv[2]);
    int pipe_fd = std::atoi(argv[3]);

    void *mem = shmat(shm_id, nullptr, 0);
    if (mem == reinterpret_cast<void *>(-1)) {
        std::perror("proc_t: shmat");
        if (err_log) std::fprintf(err_log, "shmat failed\n");
        return 1;
    }
    char *buffer = static_cast<char *>(mem);

    FILE *pipe_stream = fdopen(pipe_fd, "r");
    if (!pipe_stream) {
        std::perror("proc_t: fdopen");
        shmdt(buffer);
        if (err_log) std::fprintf(err_log, "fdopen failed\n");
        return 1;
    }

    std::string line;
    char read_buf[256];
    while (fgets(read_buf, sizeof(read_buf), pipe_stream)) {
        line.assign(read_buf);
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
        }
        if (line.size() > 150) line.resize(150);

        if (!wait_sem(sem_id, 0)) {
            if (err_log) std::fprintf(err_log, "sem wait failed\n");
            break;
        }
        std::snprintf(buffer, 256, "%s", line.c_str());
        if (!post_sem(sem_id, 1)) {
            if (err_log) std::fprintf(err_log, "sem post failed\n");
            break;
        }
        if (out_log) std::fprintf(out_log, "forwarded: %s\n", line.c_str());
    }

    fclose(pipe_stream);
    shmdt(buffer);
    if (out_log) std::fclose(out_log);
    if (err_log) std::fclose(err_log);
    return 0;
}
