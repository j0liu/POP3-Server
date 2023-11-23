CFLAGS = -std=c11 -pedantic -pedantic-errors -g -Wall -Werror -Wextra -Wno-unused-parameter -Wno-newline-eof -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200112L -fsanitize=address -g -O3
OBJS = kerl.o
SOURCES = kerl.c
TARGET = kerl

COMPILER = ${CC}

all: kerl

clean:
	rm -f $(OBJS) $(TARGET)

pop3d:
	$(COMPILER) $(CFLAGS) -o $(TARGET) $(SOURCES)