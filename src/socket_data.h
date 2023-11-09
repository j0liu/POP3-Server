#ifndef SOCKET_DATA_H
#define SOCKET_DATA_H

#include "buffer.h"
#include <stdint.h>

typedef struct SocketData {
    buffer client_buffer;
    buffer server_buffer;
    int fd;
} SocketData;

SocketData * initialize_socket_data(const int fd);
ssize_t socket_data_receive(SocketData * socket_data);
uint8_t socket_data_read(SocketData * socket_data);
void free_socket_data(SocketData * socket_data);
int socket_write(SocketData * socket_data, const char * msg, const ssize_t len);

#endif