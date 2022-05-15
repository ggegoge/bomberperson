SHELL=/bin/sh

CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -std=c++20 -g # -Wconversion

CLIENT_SRC = robots-client.cc sockets.cc netio.cc messages.cc
CLIENT_OBJS = $(CLIENT_SRC:%.cc=src/%.o)

.PHONY: all clean

all: robots-client

robots-client: $(CLIENT_OBJS)
	$(CXX) $^ -o $@

src/robots-client.o: src/robots-client.cc src/sockets.h src/serialise.h src/netio.h src/messages.h
src/sockets.o: src/sockets.cc src/sockets.h
src/messages.o: src/messages.cc src/messages.h src/serialise.h

clean:
	-rm -f $(CLIENT_OBJS)
	-rm -f robots-client
