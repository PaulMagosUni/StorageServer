SHELL = /bin/sh
CC = gcc
INCLUDES = -I ./headers
CFLAGS += -Wall -Werror -Wextra -pedantic -g $(INCLUDES)


CLIENTOUTPUT = ./bin/client
SERVEROUTPUT = ./bin/server
CLIENTDIR = ./src/Client
SERVERDIR = ./src/Server
DATASTRUCTURES = ./src/DataStructures
APIDIR = ./src/Api
OBJS = $(DATAOBJ) $(CLIENTOBJ) $(SERVEROBJ) $(APIOBJ) $(MULTIOBJ) $(LOGOBJ)

TARGETS = $(CLIENTOUTPUT) $(SERVEROUTPUT)
all : $(TARGETS)

# ---------------------------- DATA STRUCTURES ----------------------------- #
DATASRC = $(wildcard $(DATASTRUCTURES)/*.c)
DATAOBJ = $(patsubst $(DATASTRUCTURES)/%.c, $(DATASTRUCTURES)/%.o, $(DATASRC))

$(DATASTRUCTURES)/%.o: $(DATASTRUCTURES)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# --------------------------------- CLIENT --------------------------------- #
CLIENTSRC = $(wildcard $(CLIENTDIR)/*.c)
CLIENTOBJ = $(patsubst $(CLIENTDIR)/%.c, $(CLIENTDIR)/%.o, $(CLIENTSRC))
APISRC = $(APIDIR)/api.c
APIOBJ = $(APIDIR)/api.o

$(CLIENTOUTPUT): $(CLIENTOBJ) $(DATAOBJ) $(APIOBJ)
	$(CC) $(CLIENTOBJ) $(DATAOBJ) $(APIOBJ) -o $@

$(CLIENTDIR)/%.o: $(CLIENTDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(APIOBJ): $(APISRC)
	$(CC) $(CFLAGS) -c $< -o $@

# --------------------------------- SERVER --------------------------------- #
SERVERSRC = $(wildcard $(SERVERDIR)/*.c)
MULTISRC = $(wildcard $(SERVERDIR)/multithread/*.c)
LOGSRC = $(wildcard $(SERVERDIR)/utility/*.c)
SERVEROBJ = $(patsubst $(SERVERDIR)/%.c, $(SERVERDIR)/%.o, $(SERVERSRC))
MULTIOBJ = $(patsubst $(SERVERDIR)/multithread/%.c, $(SERVERDIR)/multithread/%.o, $(MULTISRC))
LOGOBJ = $(patsubst $(SERVERDIR)/utility/%.c, $(SERVERDIR)/utility/%.o, $(LOGSRC))

$(SERVEROUTPUT): $(SERVEROBJ) $(DATAOBJ) $(MULTIOBJ) $(LOGOBJ)
	$(CC) $(SERVEROBJ) $(DATAOBJ) $(MULTIOBJ) $(LOGOBJ) -o $@

$(SERVERDIR)/%.o: $(SERVERDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(SERVERDIR)/multithread/%.o: $(SERVERDIR)/multithread/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(SERVERDIR)/utility/%.o: $(SERVERDIR)/utility/%.c
	$(CC) $(CFLAGS) -c $< -o $@



.PHONY: all clean
.SUFFIXES:
.SUFFIXES: .c .o .h

clean :
	-rm -f $(OBJS)
cleanall :
	-rm -f $(TARGETS) ./log/