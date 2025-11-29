#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {
bool write_all(int fd, const char *data, size_t len) {
    size_t written = 0;
    while (written < len) {
        ssize_t rc = ::write(fd, data + written, len - written);
        if (rc <= 0) return false;
        written += static_cast<size_t>(rc);
    }
    return true;
}
}

int main(int argc, char *argv[]) {
    FILE *out_log = std::fopen("proc_p2.out", "w");
    FILE *err_log = std::fopen("proc_p2.err", "w");

    if (argc != 3) {
        std::fprintf(err_log ? err_log : stderr, "Usage: %s <pipe_write_fd> <input_file>\n", argv[0]);
        return 1;
    }

    int pipe_fd = std::atoi(argv[1]);
    const char *file_path = argv[2];

    std::ifstream input(file_path);
    if (!input.is_open()) {
        std::perror("proc_p2: fopen");
        if (err_log) std::fprintf(err_log, "proc_p2: cannot open %s\n", file_path);
        return 1;
    }

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, nullptr);

    std::string word;
    while (true) {
        int sig = 0;
        sigwait(&mask, &sig);
        if (sig != SIGUSR1) continue;

        if (!(input >> word)) {
            if (out_log) std::fprintf(out_log, "EOF reached\n");
            break;
        }

        if (word.size() > 150) {
            word.resize(150);
        }
        std::string line = word + "\n";
        if (!write_all(pipe_fd, line.c_str(), line.size())) {
            std::perror("proc_p2: write");
            if (err_log) std::fprintf(err_log, "write failure\n");
            break;
        }
        if (out_log) std::fprintf(out_log, "wrote word: %s\n", word.c_str());
    }

    input.close();
    ::close(pipe_fd);
    if (out_log) std::fclose(out_log);
    if (err_log) std::fclose(err_log);
    return 0;
}
