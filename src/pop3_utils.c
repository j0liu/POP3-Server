#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <unistd.h>
#include "buffer.h"
#include "netutils.h"
// #include "tests.h"
#include "pop3_utils.h"
#include "parser/parser.h"
#include "socket_data.h"

Pop3Args * pop3args;


void noop_handler(SocketData * socket_data, char * commandParameters, uint8_t parameters_length) {
    printf("NOOP detected :D\n");
    socket_write(socket_data, "+OK\r\n", 6);
}

#define CAPA_MSG "+OK Capability list follows\r\nUSER\r\nPASS\r\nSTAT\r\nLIST\r\nRETR\r\nDELE\r\nNOOP\r\nRSET\r\nQUIT\r\n.\r\n"

void capa_handler(SocketData * socket_data, char * commandParameters, uint8_t parameters_length) {
    printf("CAPA detected :O\n");
    socket_write(socket_data, CAPA_MSG, sizeof CAPA_MSG - 1);
}

void user_handler(SocketData * socket_data, char * commandParameters, uint8_t parameters_length) {
    printf("USER detected >:)\n");
    for (int i = 0; i < (int) pop3args->quantity_users; i++) {
        // TODO: Validate the comparison to prevent segfaults?
        printf("%s %s\n", commandParameters, pop3args->users[i].name);
        if (strcmp(pop3args->users[i].name, commandParameters) == 0) {
            socket_write(socket_data, "+OK ;)\r\n", 9); 
            return;
        }
    }
    socket_write(socket_data, "+OK\r\n", 6);
}

command_description available_commands[] = {
    {.id = CAPA, .name = "CAPA", .handler = capa_handler},
    {.id = NOOP, .name = "NOOP", .handler = noop_handler},
    {.id = USER, .name = "USER", .handler = user_handler}
};

int consume_pop3_buffer(parser * pop3parser, SocketData * socket_data, ssize_t n) {
    for (int i=0; i<n; i++) {
        const uint8_t c = socket_data_read(socket_data);
        const parser_event * event = parser_feed(pop3parser, c);
        if (event == NULL)
            return -1;
        if (event->finished) {
           add_finished_event(pop3parser); 
        }
    }
    // TODO: Handle errors?
    return 0;
}

int process_event(parser_event * event, SocketData * socket_data) {
    for (int i = 0; i < (int) N(available_commands); i++) {
        if (strcmp(event->command, available_commands[i].name) == 0) {
            available_commands[i].handler(socket_data, event->args, event->args_length);
        }
    }
    // TODO: Handle errors?
    return 0;
}

/**
 * maneja cada conexión entrante
 *
 * @param fd   descriptor de la conexión entrante.
 * @param caddr información de la conexiónentrante.
 */

static void pop3_handle_connection(const int fd, const struct sockaddr *caddr) {
    SocketData * socket_data = initialize_socket_data(fd);
    socket_write(socket_data, "+OK POP3 server ready\r\n", 23);
   {
    ssize_t n;
    extern parser_definition pop3_parser_definition;
    parser * pop3parser = parser_init(NULL, &pop3_parser_definition);

    while(true) {
        n = socket_data_receive(socket_data);
        if(n > 0) {
            if (consume_pop3_buffer(pop3parser, socket_data, n) == 0) {
                parser_event * event = parser_get_event(pop3parser);
                if (event != NULL)
                    process_event(event, socket_data);
            }
        } else {
            break;
        }   
    } 
   } 
    close(fd);
}

/** rutina de cada hilo worker */

static void * handle_connection_pthread(void *args) {
  const struct connection *c = args;
  pthread_detach(pthread_self());
  pop3_handle_connection(c->fd, (struct sockaddr *)&c->addr);
  free(args);
  return 0;
}


int serve_pop3_concurrent_blocking(const int server, Pop3Args * receivedArgs) {
    pop3args = receivedArgs; // TODO: Improve

    // TODO: add something similar to 'done' again
    for (;;)
    {
        struct sockaddr_in6 caddr;
        socklen_t caddrlen = sizeof(caddr);
        // Wait for a client to connect
        const int client = accept(server, (struct sockaddr *)&caddr, &caddrlen);
        if (client < 0) {
            perror("unable to accept incoming socket");
        }
        else {
            // TODO(juan): limitar la cantidad de hilos concurrentes
            struct connection *c = malloc(sizeof(struct connection));
            if (c == NULL) {
                // lo trabajamos iterativamente
                pop3_handle_connection(client, (struct sockaddr *)&caddr);
            }
            else
            {
                pthread_t tid;
                c->fd = client;
                c->addrlen = caddrlen;
                memcpy(&(c->addr), &caddr, caddrlen);

                if (pthread_create(&tid, 0, handle_connection_pthread, c)) {
                    free(c);
                    // lo trabajamos iterativamente
                    pop3_handle_connection(client, (struct sockaddr *)&caddr);
                }
            }
        }
    }
    return 0;
}