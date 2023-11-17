#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "buffer.h"
#include "netutils.h"
#include "socket_data.h"

#define BUFFER_SIZE 1024

SocketData* initialize_socket_data(const int fd)
{
    SocketData* new_socket_data = malloc(sizeof(SocketData));
    if (new_socket_data == NULL) {
        return NULL;
    }
    new_socket_data->fd = fd;
    buffer_init(&new_socket_data->read_buffer, BUFFER_SIZE, malloc(BUFFER_SIZE));
    buffer_init(&new_socket_data->write_buffer, BUFFER_SIZE, malloc(BUFFER_SIZE));
    return new_socket_data;
}

ssize_t socket_data_receive(SocketData* socket_data)
{
    size_t buffSize;
    ssize_t n;
    uint8_t* clientBufWritePtr = buffer_write_ptr(&socket_data->read_buffer, &buffSize);
    // TODO: Do something with buffsize?
    n = recv(socket_data->fd, clientBufWritePtr, buffSize, 0);
    if (n > 0) {
        buffer_write_adv(&socket_data->read_buffer, n);
    }
    return n;
}

bool socket_buffer_write(SocketData* socket_data, const char* msg, const ssize_t len)
{
    size_t n_bytes_available;
    uint8_t* serverBufWritePtr = buffer_write_ptr(&socket_data->write_buffer, &n_bytes_available);
    size_t message_len = strlen(msg);
    if (n_bytes_available < message_len) {
        // No se puede escribir el mensaje completo
        return false;
    }

    memcpy(serverBufWritePtr, msg, len);
    buffer_write_adv(&socket_data->write_buffer, len);
    return true;
}

uint8_t socket_data_read(SocketData* socket_data)
{
    return buffer_read(&socket_data->read_buffer);
}

void free_socket_data(SocketData* socket_data)
{
    free(socket_data->read_buffer.data);
    free(socket_data->write_buffer.data);
    free(socket_data);
}