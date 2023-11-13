/**
 * main.c - servidor proxy socks concurrente
 *
 * Interpreta los argumentos de línea de comandos, y monta un socket
 * pasivo. Por cada nueva conexión lanza un hilo que procesará de
 * forma bloqueante utilizando el protocolo SOCKS5.
 */
#include <errno.h>
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

#include "buffer.h"
#include "netutils.h"
#include <unistd.h>
// #include "tests.h"
#include "args.h"
#include "parser/parser.h"
#include "pop3_utils.h"
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

    const int server_pop3 = socket(AF_INET6, SOCK_STREAM, 0);
    if (server_pop3 < 0) {
        err_msg = "Unable to create POP3 socket";
        goto finally;
    }

    fprintf(stdout, "Listening on TCP port %d\n", args.pop3_port);

    if (setsockopt(server_pop3, IPPROTO_IPV6, IPV6_V6ONLY, (void*)&no, sizeof(no)) < 0) {
        err_msg = "Unable to bin POP3 socket to IPv6 and IPv4";
        goto finally;
    }

    if (listen(server_pop3, MAX_PENDING_CONNECTIONS) < 0) {
        err_msg = "Unable to listen on POP3 socket";
        goto finally;
    }

    // DAJT socket setup
    struct sockaddr_in6 addr_dajt;
    memset(&addr_dajt, 0, sizeof(addr_dajt));
    addr_dajt.sin6_family = AF_INET6;
    addr_dajt.sin6_addr = in6addr_any;
    addr_dajt.sin6_port = htons(args.dajt_port);

    const int server_dajt = socket(AF_INET6, SOCK_STREAM, 0);
    if (server_dajt < 0) {
        err_msg = "Unable to create DAJT socket";
        goto finally;
    }

    fprintf(stdout, "Listening on DAJT port %d\n", args.dajt_port);

    if (setsockopt(server_dajt, IPPROTO_IPV6, IPV6_V6ONLY, (void*)&no, sizeof(no)) < 0) {
        err_msg = "Unable to bind DAJT socket to IPv6 and IPv4";
        goto finally;
    }

    if (listen(server_dajt, MAX_PENDING_CONNECTIONS) < 0) {
        err_msg = "Unable to listen on DAJT socket";
        goto finally;
    }

    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);
    err_msg = 0;

    fd_set readfds;
    int max_sd = server_pop3 > server_dajt ? server_pop3 : server_dajt;

    while (!done) {
        FD_ZERO(&readfds);
        FD_SET(server_pop3, &readfds);
        FD_SET(server_dajt, &readfds);

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            printf("Select error");
        }

        if (FD_ISSET(server_pop3, &readfds)) {
            ret = serve_pop3_concurrent_blocking(server_pop3, &args);
        }

        if (FD_ISSET(server_dajt, &readfds)) {
            err_msg = "DAJT not implemented";
            goto finally;
        }
    }

finally:
    if (err_msg) {
        perror(err_msg);
        ret = 1;
    }
    if (server_pop3 >= 0) {
        close(server_pop3);
    }
    if (server_dajt >= 0) {
        close(server_dajt);
    }
    return ret;
}
