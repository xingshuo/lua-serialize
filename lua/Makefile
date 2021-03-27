CC = gcc
# 支持重载lua头文件路径
LUA_INCDIR ?= ./inc53
CFLAGS = -g -O2 -Wall -I$(LUA_INCDIR)
SHARED = -fPIC --shared
# 支持重载so路径名称
TARGET ?= lseri.so

$(TARGET): lua-seri.c seri.c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@

clean:
	rm $(TARGET)