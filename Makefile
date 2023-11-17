CFLAGS = -std=c11 -pedantic -pedantic-errors -g -Wall -Werror -Wextra -Wno-unused-parameter -Wno-newline-eof -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200112L -fsanitize=address -g
OBJS = src/*.o src/parser/*.o src/stm/*.o
SOURCES = src/*.c src/parser/*.c src/stm/*.c
LIBS = -l pthread
TARGET = server

COMPILER = ${CC}

all: server

clean:
	rm -f $(OBJS) $(TARGET)

server:
	$(COMPILER) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)