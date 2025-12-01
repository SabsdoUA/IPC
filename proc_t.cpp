#include <iostream>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <cstring>
#include <vector>

void sem_lock_write(int semid) {
    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_op = -1;
    sb.sem_flg = 0;
    semop(semid, &sb, 1);
}

void sem_unlock_read(int semid) {
    struct sembuf sb;
    sb.sem_num = 1;
    sb.sem_op = 1;
    sb.sem_flg = 0;
    semop(semid, &sb, 1);
}

int main(int argc, char* argv[]) {
    if (argc != 4) return 1;

    int r2_fd = std::stoi(argv[1]);
    int shm_id = std::stoi(argv[2]);
    int sem_id = std::stoi(argv[3]);

    char* shm_ptr = (char*)shmat(shm_id, nullptr, 0);
    if (shm_ptr == (char*)-1) return 1;

    char buffer[1];
    std::string current_msg;

    while (read(r2_fd, buffer, 1) > 0) {
        if (buffer[0] == '\n') {
            sem_lock_write(sem_id);
            strncpy(shm_ptr, current_msg.c_str(), 255);
            shm_ptr[255] = '\0';
            sem_unlock_read(sem_id);
            current_msg.clear();
        } else {
            current_msg += buffer[0];
        }
    }

    shmdt(shm_ptr);
    close(r2_fd);
    return 0;
}
