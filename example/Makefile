CC = gcc
PWD = $(shell pwd)
LUA_INCDIR ?= $(PWD)/../lua/inc53
CFLAGS = -g -O2 -Wall -I$(LUA_INCDIR)
SHARED = -fPIC --shared
CLIENT_DIR = client
SOCKET_LIB = $(CLIENT_DIR)/lsocket.so
SERIALIZE_LIB = $(CLIENT_DIR)/lseri.so
SERVER_DIR = server
SERVER_EXE = $(SERVER_DIR)/main.exe

all: $(SOCKET_LIB) $(SERIALIZE_LIB) $(SERVER_EXE)

$(SOCKET_LIB): $(CLIENT_DIR)/lsocket.c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@

$(SERIALIZE_LIB):
	$(MAKE) -C ../lua/ TARGET=$(PWD)/$(SERIALIZE_LIB) LUA_INCDIR=$(LUA_INCDIR)

$(SERVER_EXE): $(SERVER_DIR)/main.go
	go build -o $@ $^

clean:
	rm -f $(SOCKET_LIB) $(SERIALIZE_LIB) $(SERVER_EXE)