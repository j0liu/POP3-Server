#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "client.h"
#include "pop3.h"
#include "protocols.h"
#include "selector.h"

static void pop3_read(struct selector_key* key);
static void pop3_write(struct selector_key* key);
static void pop3_close(struct selector_key* key);

static const struct fd_handler pop3_handler = {
    .handle_read = pop3_read,
    .handle_write = pop3_write,
    .handle_block = NULL,
    .handle_close = pop3_close,
};

void dajt_accept_passive_sockets(struct selector_key* key)
{
    printf("Not implemented yet\n");
}

void pop3_accept_passive_sockets(struct selector_key* key)
{
    struct sockaddr_in6 client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    const int client_fd = accept(key->fd, (struct sockaddr*)&client_addr, &client_addr_len);
    Client* client = NULL;

    if (client_fd == -1) {
        goto fail;
    }
    if (selector_fd_set_nio(client_fd) == -1) {
        goto fail;
    }

    client = new_client(client_fd, &client_addr, client_addr_len);
    if (client == NULL) {
        goto fail;
    }

    if (SELECTOR_SUCCESS != selector_register(key->s, client_fd, &pop3_handler, OP_WRITE, client)) {
        goto fail;
    }

    return;
fail:
    if (client_fd != -1) {
        close(client_fd);
    }
    free_client(client);
}

// Handlers top level de la conexión pasiva.
// son los que emiten los eventos a la maquina de estados.
// static void pop3_done(struct selector_key* key);

static void pop3_read(struct selector_key* key)
{
    struct state_machine* stm = &ATTACHMENT(key)->stm;
    // const enum pop3_state st = 
    stm_handler_read(stm, key);

    // if (ERROR == st || DONE == st) {
    //     pop3_done(key);
    // }
}

static void
pop3_write(struct selector_key* key)
{
    struct state_machine* stm = &ATTACHMENT(key)->stm;
    // const enum pop3_state st = 
    stm_handler_write(stm, key);

    // if (ERROR == st || DONE == st) {
    //     pop3_done(key);
    // }
}

static void
pop3_close(struct selector_key* key)
{
    // free_client(ATTACHMENT(key));
}

// static void
// pop3_done(struct selector_key* key)
// {
//     int fd = ATTACHMENT(key)->connection->fd;

//     if (fd != -1) {
//         if (SELECTOR_SUCCESS != selector_unregister_fd(key->s, fd)) {
//             abort();
//         }
//         close(fd);
//     }
//     // Free
// }

void register_fd(struct selector_key* key, int fd, fd_interest interest, void * data) {
    selector_register(key->s, fd, &pop3_handler, interest, data);
}