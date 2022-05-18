SHELL=/bin/sh

CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -std=c++20 -g -Wconversion
LDFLAGS = -lboost_program_options -lpthread

CLIENT_SRC = robots-client.cc readers.cc
CLIENT_OBJS = $(CLIENT_SRC:%.cc=src/%.o)

.PHONY: all clean

all: robots-client

robots-client: $(CLIENT_OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)

robots-client-static: $(CLIENT_OBJS)
	$(CXX) $^ -o $@ -Wl,-Bstatic -lboost_program_options -Wl,-Bdynamic -lpthread

src/robots-client.o: src/robots-client.cc src/marshal.h src/readers.h src/messages.h

clean:
	-rm -f $(CLIENT_OBJS)
	-rm -f robots-client
