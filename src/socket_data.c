#include "socket_data.h"
#include "buffer.h"
#include "netutils.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#define BUFFER_SIZE 1024

SocketData* initialize_socket_data(const int fd)
{
    SocketData* new_socket_data = malloc(sizeof(SocketData));
    new_socket_data->fd = fd;
    buffer_init(&new_socket_data->client_buffer, BUFFER_SIZE, malloc(BUFFER_SIZE));
    buffer_init(&new_socket_data->server_buffer, BUFFER_SIZE, malloc(BUFFER_SIZE));
    return new_socket_data;
}

ssize_t socket_data_receive(SocketData* socket_data)
{
    size_t buffSize;
    ssize_t n;
    uint8_t* clientBufWritePtr = buffer_write_ptr(&socket_data->client_buffer, &buffSize);
    // TODO: Do something with buffsize?
    n = recv(socket_data->fd, clientBufWritePtr, buffSize, 0);
    if (n > 0) {
        buffer_write_adv(&socket_data->client_buffer, n);
    }
    return n;
}

int socket_write(SocketData* socket_data, const char* msg, const ssize_t len)
{
    size_t n_bytes_available;
    uint8_t* serverBufWritePtr = buffer_write_ptr(&socket_data->server_buffer, &n_bytes_available);

    memcpy(serverBufWritePtr, msg, len);
    buffer_write_adv(&socket_data->server_buffer, len);
    // TODO: Make not blocking
    return sock_blocking_write(socket_data->fd, &socket_data->server_buffer);
}

uint8_t socket_data_read(SocketData* socket_data)
{
    return buffer_read(&socket_data->client_buffer);
}

void free_socket_data(SocketData* socket_data)
{
    free(socket_data->client_buffer.data);
    free(socket_data->server_buffer.data);
    free(socket_data);
}