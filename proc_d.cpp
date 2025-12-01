#include <iostream>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>

void sem_wait_read(int semid) {
    struct sembuf sb;
    sb.sem_num = 1;
    sb.sem_op = -1;
    sb.sem_flg = 0;
    semop(semid, &sb, 1);
}

void sem_signal_write(int semid) {
    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_op = 1;
    sb.sem_flg = 0;
    semop(semid, &sb, 1);
}

int main(int argc, char* argv[]) {
    if (argc != 4) return 1;

    int sem_id = std::stoi(argv[1]);
    int shm_id = std::stoi(argv[2]);
    int port = std::stoi(argv[3]);

    char* shm_ptr = (char*)shmat(shm_id, nullptr, 0);
    if (shm_ptr == (char*)-1) return 1;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    while (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        usleep(100000);
    }

    while (true) {
        sem_wait_read(sem_id);

        char buffer[256];
        strncpy(buffer, shm_ptr, 255);
        buffer[255] = '\0';

        send(sock, buffer, strlen(buffer) + 1, 0);

        sem_signal_write(sem_id);
    }

    close(sock);
    shmdt(shm_ptr);
    return 0;
}
