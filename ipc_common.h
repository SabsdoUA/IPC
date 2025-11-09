#ifndef IPC_COMMON_H
#define IPC_COMMON_H
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define IPC_TEXT_SIZE 256
struct ipc_message {
    size_t len;
    int finished;
    char data[IPC_TEXT_SIZE];
};
static inline int parse_int(const char *s, long *out) {
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') {
        return -1;
    }
    *out = v;
    return 0;
}
static inline int write_all(int fd, const void *buf, size_t count) {
    const char *p = (const char *)buf;
    size_t total = 0;
    while (total < count) {
        ssize_t w = write(fd, p + total, count - total);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        total += (size_t)w;
    }
    return 0;
}
static inline ssize_t read_all(int fd, void *buf, size_t count) {
    char *p = (char *)buf;
    size_t total = 0;
    while (total < count) {
        ssize_t r = read(fd, p + total, count - total);
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (r == 0) {
            break;
        }
        total += (size_t)r;
    }
    return (ssize_t)total;
}
static inline ssize_t read_line_fd(int fd, char *buf, size_t size) {
    size_t pos = 0;
    while (pos + 1 < size) {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (r == 0) {
            if (pos == 0) {
                return 0;
            }
            break;
        }
        if (c == '\n') {
            break;
        }
        buf[pos++] = c;
    }
    buf[pos] = '\0';
    return (ssize_t)pos;
}
static inline int sem_acquire(int semid, unsigned short index) {
    struct sembuf op;
    op.sem_num = index;
    op.sem_op = -1;
    op.sem_flg = 0;
    while (semop(semid, &op, 1) == -1) {
        if (errno == EINTR) {
            continue;
        }
        return -1;
    }
    return 0;
}
static inline int sem_release(int semid, unsigned short index) {
    struct sembuf op;
    op.sem_num = index;
    op.sem_op = 1;
    op.sem_flg = 0;
    while (semop(semid, &op, 1) == -1) {
        if (errno == EINTR) {
            continue;
        }
        return -1;
    }
    return 0;
}
#endif
