#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "client.h"
#include "pop3.h"
#include "protocols.h"
#include "selector.h"
#include "logger/logger.h"
#include "global_state/globalstate.h"

static void stm_read(struct selector_key* key);
static void stm_write(struct selector_key* key);

static const struct fd_handler stm_handler = {
    .handle_read = stm_read,
    .handle_write = stm_write,
    .handle_block = NULL,
    .handle_close = NULL,
};

static void accept_passive_sockets(struct selector_key* key, bool pop3);

void dajt_accept_passive_sockets(struct selector_key* key)
{
    accept_passive_sockets(key, false);
}


void pop3_accept_passive_sockets(struct selector_key* key)
{
    accept_passive_sockets(key, true);
}

static void accept_passive_sockets(struct selector_key* key, bool pop3) {
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

    client = new_client(client_fd, &client_addr, client_addr_len, pop3);
    if (!add_client(client)) {
        log(LOG_ERROR, "No se pudo agregar el cliente, maximo alcanzado");
        goto fail;
    }

    if (client == NULL) {
        goto fail;
    }

    if (SELECTOR_SUCCESS != selector_register(key->s, client_fd, &stm_handler, OP_WRITE, client)) {
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

static void stm_read(struct selector_key* key)
{
    struct state_machine* stm = &ATTACHMENT(key)->stm;
    // const enum pop3_state st = 
    stm_handler_read(stm, key);

    // if (ERROR == st || DONE == st) {
    //     pop3_done(key);
    // }
}

static void
stm_write(struct selector_key* key)
{
    struct state_machine* stm = &ATTACHMENT(key)->stm;
    // const enum pop3_state st = 
    stm_handler_write(stm, key);

    // if (ERROR == st || DONE == st) {
    //     pop3_done(key);
    // }
}

void register_fd(struct selector_key* key, int fd, fd_interest interest, void * data) {
    selector_register(key->s, fd, &stm_handler, interest, data);
}