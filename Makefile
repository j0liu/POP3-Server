CFLAGS = -std=c11 -pedantic -pedantic-errors -g -Wall -Werror -Wextra -Wno-unused-parameter -Wno-newline-eof -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200112L -fsanitize=address -g
OBJS = src/*.o src/parser/*.o
SOURCES = src/*.c src/parser/*.c
LIBS = -l pthread
TARGET = pop3d

COMPILER = ${CC}

all: pop3d

clean:
	rm -f $(OBJS) $(TARGET)

pop3d:
	$(COMPILER) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)