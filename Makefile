CFLAGS = -std=c11 -pedantic -pedantic-errors -g -Wall -Werror -Wextra -Wno-unused-parameter -Wno-newline-eof -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200112L -fsanitize=address -g -O3
OBJS = src/*.o src/parser/*.o src/stm/*.o src/dajt/*.o src/global_state/*.o src/logger/*.o
SOURCES = src/*.c src/parser/*.c src/stm/*.c  src/dajt/*.c src/global_state/*.c src/logger/*.c
LIBS = -l pthread
TARGET = pop3d

COMPILER = ${CC}

all: pop3d

clean:
	rm -f $(OBJS) $(TARGET)

pop3d:
	$(COMPILER) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)