#include <iostream>
#include <fstream>
#include <string>
#include <csignal>
#include <unistd.h>
#include <cstring>

volatile sig_atomic_t signal_received = 0;

void signal_handler(int signum) {
    if (signum == SIGUSR1) signal_received = 1;
}

int main(int argc, char* argv[]) {
    if (argc != 3) return 1;

    int pipe_fd = std::stoi(argv[1]);
    std::string filename = argv[2];

    std::ifstream infile(filename);
    if (!infile.is_open()) {
        perror("Proc_p1: Failed to open input file");
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, nullptr);

    std::string word;

    while (true) {
        while (!signal_received) pause();
        signal_received = 0;

        if (infile >> word) {
            word += "\n";
            write(pipe_fd, word.c_str(), word.length());
        } else {
            break;
        }
    }

    infile.close();
    close(pipe_fd);
    return 0;
}
