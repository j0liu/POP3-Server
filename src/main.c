#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "args.h"
#include "buffer.h"
#include "netutils.h"
#include "parser/parser.h"
#include "global_state/globalstate.h"
#include "protocols.h"
#include "selector.h"
#include "socket_data.h"
#include "logger/logger.h"
#define MAX_PENDING_CONNECTIONS 500

bool done = false;

static void sigterm_handler(const int signal)
{
    logf(LOG_DEBUG, "Signal %d, cleaning up and exiting", signal);
    done = true;
}

int main(const int argc, char** argv)
{

    log(LOG_DEBUG, "log debugging enabled");

    const char* err_msg;
    int ret;

    // ---- Signals ---
    struct sigaction handler;
    handler.sa_handler = sigterm_handler;
    if (sigfillset(&handler.sa_mask) < 0) {
        err_msg = "sigfillset() failed";
        goto finally;
    }

    handler.sa_flags = 0;

    if (sigaction(SIGINT, &handler, 0) < 0) {
        err_msg = "sigaction() failed for SIGINT";
        goto finally;
    }

    if (sigaction(SIGTERM, &handler, 0) < 0) {
        err_msg = "sigaction() failed for SIGTERM";
        goto finally;
    }
    // ---- Signals End ---

    extern Args args;
    parse_args(argc, argv, &args);
    int no = 0;
    selector_status ss = SELECTOR_SUCCESS;
    fd_selector selector = NULL;

    // POP3 SOCKET setup
    struct sockaddr_in6 addr_pop3;
    memset(&addr_pop3, 0, sizeof(addr_pop3));
    addr_pop3.sin6_family = AF_INET6;
    addr_pop3.sin6_addr = in6addr_any;
    addr_pop3.sin6_port = htons(args.pop3_port);

    const int socket_pop3 = socket(AF_INET6, SOCK_STREAM, 0);
    if (socket_pop3 < 0) {
        err_msg = "Unable to create POP3 socket";
        goto finally;
    }

    if (setsockopt(socket_pop3, SOL_SOCKET, SO_REUSEADDR, &(int) { 1 }, sizeof(int)) < 0) {
        err_msg = "Unable to set SO_REUSEADDR on POP3 socket";
        goto finally;
    }

    if (setsockopt(socket_pop3, IPPROTO_IPV6, IPV6_V6ONLY, (void*)&no, sizeof(no)) < 0) {
        err_msg = "Set POP3 socket to IPv6 and IPv4 failed";
        goto finally;
    }

    if (bind(socket_pop3, (struct sockaddr*)&addr_pop3, sizeof(addr_pop3)) < 0) {
        err_msg = "Unable to bind POP3 socket";
        goto finally;
    }

    if (listen(socket_pop3, MAX_PENDING_CONNECTIONS) < 0) {
        err_msg = "Unable to listen on POP3 socket";
        goto finally;
    }

    if (selector_fd_set_nio(socket_pop3) == -1) {
        err_msg = "Error getting POP3 socket flags";
        goto finally;
    }

    logf(LOG_DEBUG, "Listening on TCP (POP 3) port %d", args.pop3_port);

    // DAJT socket setup
    struct sockaddr_in6 addr_dajt;
    memset(&addr_dajt, 0, sizeof(addr_dajt));
    addr_dajt.sin6_family = AF_INET6;
    addr_dajt.sin6_addr = in6addr_any;
    addr_dajt.sin6_port = htons(args.dajt_port);

    const int socket_dajt = socket(AF_INET6, SOCK_STREAM, 0);
    if (socket_dajt < 0) {
        err_msg = "Unable to create DAJT socket";
        goto finally;
    }

    if (setsockopt(socket_dajt, SOL_SOCKET, SO_REUSEADDR, &(int) { 1 }, sizeof(int)) < 0) {
        err_msg = "Unable to set SO_REUSEADDR on DAJT socket";
        goto finally;
    }

    if (setsockopt(socket_dajt, IPPROTO_IPV6, IPV6_V6ONLY, (const void*)&no, sizeof(no)) < 0) {
        err_msg = "Set DAJT socket to IPv6 and IPv4 failed";
        goto finally;
    }

    if (bind(socket_dajt, (struct sockaddr*)&addr_dajt, sizeof(addr_dajt)) < 0) {
        err_msg = "Unable to bind DAJT socket";
        goto finally;
    }

    if (listen(socket_dajt, MAX_PENDING_CONNECTIONS) < 0) {
        err_msg = "Unable to listen on DAJT socket";
        goto finally;
    }

    if (selector_fd_set_nio(socket_dajt) == -1) {
        err_msg = "Error getting DAJT socket flags";
        goto finally;
    }

    logf(LOG_DEBUG, "Listening on TCP (DAJT) port %d", args.dajt_port);

    // Selector setup
    const struct selector_init conf = {
        .signal = SIGALRM,
        .select_timeout = {
            .tv_sec = 10,
            .tv_nsec = 0,
        },
    };

    if (0 != selector_init(&conf)) {
        err_msg = "Error initializing selector";
        goto finally;
    }

    selector = selector_new(1024);
    if (selector == NULL) {
        err_msg = "Unable to create selector";
        goto finally;
    }

    // TODO: Maybe unify this with the other handlers,
    // they are the same for now, only changes the handler registered inside the accept
    const struct fd_handler pop3_handler_passive_sockets = {
        .handle_read = pop3_accept_passive_sockets,
        .handle_write = NULL,
        .handle_close = NULL,
    };

    ss = selector_register(selector, socket_pop3, &pop3_handler_passive_sockets, OP_READ, NULL);
    if (ss != SELECTOR_SUCCESS) {
        err_msg = "Error registering POP3 passive socket";
        goto finally;
    }

    const struct fd_handler dajt_handler_passive_sockets = {
        .handle_read = dajt_accept_passive_sockets,
        .handle_write = NULL,
        .handle_close = NULL,
    };

    ss = selector_register(selector, socket_dajt, &dajt_handler_passive_sockets, OP_READ, NULL);
    if (ss != SELECTOR_SUCCESS) {
        err_msg = "Error registering DAJT passive socket";
        goto finally;
    }

    while (!done) {
        err_msg = NULL;
        ss = selector_select(selector);

        if (ss != SELECTOR_SUCCESS) {
            err_msg = "Error serving";
            goto finally;
        }
    }

    if (err_msg == NULL) {
        err_msg = "Closing...";
    }

finally:
    if (ss != SELECTOR_SUCCESS) {
        logf(LOG_INFO, "Error: %s %s", err_msg, ss == SELECTOR_IO ? strerror(errno) : selector_error(ss));
        ret = 2;
    } else if (err_msg) {
        perror(err_msg);
        ret = 1;
    }
    if (selector != NULL) {
        selector_destroy(selector);
    }

    selector_close();

    if (socket_pop3 >= 0) {
        close(socket_pop3);
        // TODO: Free others
    }
    if (socket_dajt >= 0) {
        close(socket_dajt);
        // TODO: Free others
    }
    return ret;
}
