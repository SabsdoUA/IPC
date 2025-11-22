#include <limits.h>
#include "ipc_common.h"
#include <signal.h>
#include <sys/shm.h>
#include <sys/sem.h>
static int send_message(int semid, unsigned short release_index) {
    return sem_release(semid, release_index);
}
int main(int argc, char **argv) {
    if (argc != 4) {
        return EXIT_FAILURE;
    }
    long fd_value = 0;
    long shm_value = 0;
    long sem_value = 0;
    if (parse_int(argv[1], &fd_value) != 0 || fd_value < 0 || fd_value > INT_MAX) {
        return EXIT_FAILURE;
    }
    if (parse_int(argv[2], &shm_value) != 0) {
        return EXIT_FAILURE;
    }
    if (parse_int(argv[3], &sem_value) != 0) {
        return EXIT_FAILURE;
    }
    int read_fd = (int)fd_value;
    int shm_id = (int)shm_value;
    int sem_id = (int)sem_value;
    struct sigaction ignore_act;
    memset(&ignore_act, 0, sizeof(ignore_act));
    ignore_act.sa_handler = SIG_IGN;
    sigemptyset(&ignore_act.sa_mask);
    if (sigaction(SIGPIPE, &ignore_act, NULL) == -1) {
        close(read_fd);
        return EXIT_FAILURE;
    }
    struct ipc_message *segment = (struct ipc_message *)shmat(shm_id, NULL, 0);
    if (segment == (void *)-1) {
        close(read_fd);
        return EXIT_FAILURE;
    }
    FILE *stream = fdopen(read_fd, "r");
    if (!stream) {
        shmdt(segment);
        close(read_fd);
        return EXIT_FAILURE;
    }
    if (kill(getppid(), SIGUSR1) == -1) {
        fclose(stream);
        shmdt(segment);
        return EXIT_FAILURE;
    }
    char buffer[IPC_TEXT_SIZE];
    for (;;) {
        if (!fgets(buffer, (int)sizeof(buffer), stream)) {
            if (feof(stream)) {
                if (sem_acquire(sem_id, 0) != 0) {
                    fclose(stream);
                    shmdt(segment);
                    return EXIT_FAILURE;
                }
                segment->len = 0;
                segment->finished = 1;
                segment->data[0] = '\0';
                if (send_message(sem_id, 1) != 0) {
                    fclose(stream);
                    shmdt(segment);
                    return EXIT_FAILURE;
                }
                break;
            }
            if (ferror(stream)) {
                fclose(stream);
                shmdt(segment);
                return EXIT_FAILURE;
            }
            continue;
        }
        size_t len = strcspn(buffer, "\r\n");
        buffer[len] = '\0';
        if (sem_acquire(sem_id, 0) != 0) {
            fclose(stream);
            shmdt(segment);
            return EXIT_FAILURE;
        }
        segment->len = len;
        segment->finished = 0;
        memcpy(segment->data, buffer, len + 1);
        if (send_message(sem_id, 1) != 0) {
            fclose(stream);
            shmdt(segment);
            return EXIT_FAILURE;
        }
    }
    fclose(stream);
    shmdt(segment);
    return EXIT_SUCCESS;
}
