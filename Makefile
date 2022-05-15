SHELL=/bin/sh

CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++20  -O2

CLIENT_SRC = robots-client.cc sockets.cc netio.cc
CLIENT_OBJS = $(CLIENT_SRC:%.cc=src/%.o)

.PHONY: all clean

all: robots-client

robots-client: $(CLIENT_OBJS)
	$(CXX) $^ -o $@

src/robots-client.o: src/robots-client.cc src/sockets.h src/serialise.h src/netio.h
src/sockets.o: src/sockets.cc src/sockets.h

clean:
	-rm -f $(OBJS)
	-rm -f robots-client
