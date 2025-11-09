#include <limits.h>
#include "ipc_common.h"
#include <signal.h>
#include <sys/shm.h>
#include <sys/sem.h>

int main(int argc, char **argv) {
    if (argc != 5) {
        return EXIT_FAILURE;
    }
    long shm1_value = 0;
    long sem1_value = 0;
    long shm2_value = 0;
    long sem2_value = 0;
    if (parse_int(argv[1], &shm1_value) != 0) {
        return EXIT_FAILURE;
    }
    if (parse_int(argv[2], &sem1_value) != 0) {
        return EXIT_FAILURE;
    }
    if (parse_int(argv[3], &shm2_value) != 0) {
        return EXIT_FAILURE;
    }
    if (parse_int(argv[4], &sem2_value) != 0) {
        return EXIT_FAILURE;
    }
    int shm1 = (int)shm1_value;
    int sem1 = (int)sem1_value;
    int shm2 = (int)shm2_value;
    int sem2 = (int)sem2_value;
    struct ipc_message *segment1 = (struct ipc_message *)shmat(shm1, NULL, 0);
    if (segment1 == (void *)-1) {
        return EXIT_FAILURE;
    }
    struct ipc_message *segment2 = (struct ipc_message *)shmat(shm2, NULL, 0);
    if (segment2 == (void *)-1) {
        shmdt(segment1);
        return EXIT_FAILURE;
    }
    if (kill(getppid(), SIGUSR1) == -1) {
        shmdt(segment2);
        shmdt(segment1);
        return EXIT_FAILURE;
    }
    for (;;) {
        if (sem_acquire(sem1, 1) != 0) {
            shmdt(segment2);
            shmdt(segment1);
            return EXIT_FAILURE;
        }
        size_t len = segment1->len;
        int finished = segment1->finished;
        char data[IPC_TEXT_SIZE];
        if (len >= IPC_TEXT_SIZE) {
            if (sem_release(sem1, 0) != 0) {
                shmdt(segment2);
                shmdt(segment1);
                return EXIT_FAILURE;
            }
            shmdt(segment2);
            shmdt(segment1);
            return EXIT_FAILURE;
        }
        memcpy(data, segment1->data, len + 1);
        if (sem_release(sem1, 0) != 0) {
            shmdt(segment2);
            shmdt(segment1);
            return EXIT_FAILURE;
        }
        char tagged[IPC_TEXT_SIZE];
        int written = snprintf(tagged, sizeof(tagged), "S: %s", data);
        if (written < 0 || (size_t)written >= sizeof(tagged)) {
            shmdt(segment2);
            shmdt(segment1);
            return EXIT_FAILURE;
        }
        if (sem_acquire(sem2, 0) != 0) {
            shmdt(segment2);
            shmdt(segment1);
            return EXIT_FAILURE;
        }
        segment2->len = (size_t)written;
        segment2->finished = finished;
        memcpy(segment2->data, tagged, (size_t)written + 1);
        if (sem_release(sem2, 1) != 0) {
            shmdt(segment2);
            shmdt(segment1);
            return EXIT_FAILURE;
        }
        if (finished) {
            break;
        }
    }
    shmdt(segment2);
    shmdt(segment1);
    return EXIT_SUCCESS;
}
