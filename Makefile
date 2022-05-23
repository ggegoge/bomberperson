SHELL=/bin/sh

CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -std=c++20 -Wconversion -O2
LDFLAGS = -lboost_program_options -lpthread
LDFLAGS_STATIC = -Wl,-Bstatic -lboost_program_options -Wl,-Bdynamic -lpthread

CLIENT_SRC = robots-client.cc readers.cc
CLIENT_OBJS = $(CLIENT_SRC:%.cc=src/%.o)

SERV_SRC = robots-server.cc readers.cc
SERV_OBJS = $(SERV_SRC:%.cc=src/%.o)

.PHONY: all clean release debug opt-server dbg-server opt-client dbg-client statics

# Default target is release.
all: release

# providing these targets (release and debug) for user convenience
release: opt-server opt-client

debug: dbg-server dbg-client

opt-server: CXXFLAGS += -DNDEBUG
opt-server: robots-server

opt-client: CXXFLAGS += -DNDEBUG
opt-client: robots-client

dbg-server: CXXFLAGS += -g
dbg-server: robots-server

dbg-client: CXXFLAGS += -g
dbg-client: robots-client

# Executables
robots-client: $(CLIENT_OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)

robots-server: $(SERV_OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)

# Staticly linked targets only to help when eg someone would want to use program
# compiled elsewhere.
statics: robots-client-static robots-server-static

robots-client-static: $(CLIENT_OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS_STATIC)

robots-server-static: $(SERV_OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS_STATIC)

# OBJS
src/robots-client.o: src/robots-client.cc src/marshal.h src/readers.h src/messages.h
src/robots-server.o: src/robots-server.cc src/marshal.h src/readers.h src/messages.h
src/readers.o: src/readers.cc src/readers.h

clean:
	-rm -f $(CLIENT_OBJS) $(SERV_OBJS)
	-rm -f robots-client robots-server
	-rm -f robots-client-static robots-server-static
