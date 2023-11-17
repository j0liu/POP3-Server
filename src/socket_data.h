#ifndef SOCKET_DATA_H
#define SOCKET_DATA_H

#include <stdint.h>

#include "buffer.h"

typedef struct SocketData {
    buffer read_buffer;
    buffer write_buffer;
    int fd;
} SocketData;

SocketData* initialize_socket_data(const int fd);
ssize_t socket_data_receive(SocketData* socket_data);
uint8_t socket_data_read(SocketData* socket_data);
void free_socket_data(SocketData* socket_data);
void socket_buffer_write(SocketData* socket_data, const char* msg, const ssize_t len);

#endif