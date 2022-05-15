SHELL=/bin/sh

CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++20  -O2

CLIENT_SRC = 
CLIENT_OBJS = $(CLIENT_SRC:%.c=src/%.o)

.PHONY: all clean

all: robots-client

robots-client: $(CLIENT_OBJS)
	$(CXX) $^ -o $@

src/robots-client.o: src/robots-client.c src/utils.h

clean:
	-rm -f $(OBJS)
	-rm -f robots-client
