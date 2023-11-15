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
#include "pop3.h"
#include "protocols.h"
#include "socket_data.h"

#define MAX_PENDING_CONNECTIONS 500

static bool done = false;

static void sigterm_handler(const int signal)
{
    printf("Signal %d, cleaning up and exiting\n", signal);
    done = true;
}

int main(const int argc, char** argv)
{
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    Args args;
    parse_args(argc, argv, &args);
    const char* err_msg;
    int no = 0;
    int ret;

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

    fprintf(stdout, "Listening on TCP port %d\n", args.pop3_port);

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

    fprintf(stdout, "Listening on DAJT port %d\n", args.dajt_port);

    fd_set readfds;
    int max_sd = socket_pop3 > socket_dajt ? socket_pop3 : socket_dajt;

    while (!done) {
        FD_ZERO(&readfds);
        FD_SET(socket_pop3, &readfds);
        FD_SET(socket_dajt, &readfds);

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            printf("Select error");
        }

        if (FD_ISSET(socket_pop3, &readfds)) {
            ret = serve_pop3_concurrent_blocking(socket_pop3, &args);
        }

        if (FD_ISSET(socket_dajt, &readfds)) {
            err_msg = "DAJT not implemented";
            goto finally;
        }
    }

finally:
    if (err_msg) {
        perror(err_msg);
        ret = 1;
    }
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
