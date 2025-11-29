CXX=g++
CXXFLAGS=-std=c++17 -Wall -Wextra -O2

all: zadanie proc_p1 proc_p2 proc_t proc_d proc_serv2

zadanie: zadanie.cpp
	$(CXX) $(CXXFLAGS) zadanie.cpp -o zadanie

proc_p1: proc_p1.cpp
	$(CXX) $(CXXFLAGS) proc_p1.cpp -o proc_p1

proc_p2: proc_p2.cpp
	$(CXX) $(CXXFLAGS) proc_p2.cpp -o proc_p2

proc_t: proc_t.cpp
	$(CXX) $(CXXFLAGS) proc_t.cpp -o proc_t

proc_d: proc_d.cpp
	$(CXX) $(CXXFLAGS) proc_d.cpp -o proc_d

proc_serv2: proc_serv2.cpp
	$(CXX) $(CXXFLAGS) proc_serv2.cpp -o proc_serv2

clean:
	rm -f zadanie proc_p1 proc_p2 proc_t proc_d proc_serv2

.PHONY: all clean
