SHELL=/bin/sh

CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -std=c++20 -Wconversion
LDFLAGS = -lboost_program_options -lpthread

CLIENT_SRC = robots-client.cc readers.cc
CLIENT_OBJS = $(CLIENT_SRC:%.cc=src/%.o)

SERV_SRC = robots-server.cc readers.cc
SERV_OBJS = $(SERV_SRC:%.cc=src/%.o)

.PHONY: all clean release debug

all: release

# providing these two targets (release and debug) for user convenience
release: CXXFLAGS += -DNDEBUG -O2
release: robots-client robots-server

debug: CXXFLAGS += -g
debug: robots-client robots-server

# EXECUTABLES

# The client
robots-client: $(CLIENT_OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)

robots-client-static: $(CLIENT_OBJS)
	$(CXX) $^ -o $@ -Wl,-Bstatic -lboost_program_options -Wl,-Bdynamic -lpthread

# The server
robots-server: $(SERV_OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)

robots-server-static: $(SERV_OBJS)
	$(CXX) $^ -o $@ -Wl,-Bstatic -lboost_program_options -Wl,-Bdynamic -lpthread

# OBJS
src/robots-client.o: src/robots-client.cc src/marshal.h src/readers.h src/messages.h
src/robots-server.o: src/robots-server.cc src/marshal.h src/readers.h src/messages.h

clean:
	-rm -f $(CLIENT_OBJS)
	-rm -f robots-client
