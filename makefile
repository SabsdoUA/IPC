CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L
LDFLAGS =
PROGRAMS = zadanie proc_p1 proc_p2 proc_t proc_d proc_serv2 proc_pr proc_s proc_serv1
all: $(PROGRAMS)
%.o: %.cpp ipc_common.h
	$(CC) $(CFLAGS) -x c -c $< -o $@
zadanie: zadanie.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
proc_p1: proc_p1.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
proc_p2: proc_p2.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
proc_t: proc_t.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
proc_d: proc_d.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
proc_serv2: proc_serv2.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
proc_pr: proc_pr.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
proc_s: proc_s.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
proc_serv1: proc_serv1.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
clean:
	rm -f $(PROGRAMS) *.o
.PHONY: all clean
