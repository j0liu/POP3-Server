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
#include "client_data.h"
Pop3Args * pop3args;


void noop_handler(ClientData * client_data, char * commandParameters, uint8_t parameters_length) {
    printf("NOOP detected :D\n");
    socket_write(client_data->socket_data, "+OK\r\n", 6);
}

#define CAPA_MSG "+OK Capability list follows\r\nUSER\r\nPASS\r\nSTAT\r\nLIST\r\nRETR\r\nDELE\r\nNOOP\r\nRSET\r\nQUIT\r\n.\r\n"
#define MALFORMED_COMMAND "-ERR Comando malformado\r\n"

void capa_handler(ClientData * client_data, char * commandParameters, uint8_t parameters_length) {
    printf("CAPA detected :O\n");
    socket_write(client_data->socket_data, CAPA_MSG, sizeof CAPA_MSG - 1);
}

void user_handler(ClientData * client_data, char * commandParameters, uint8_t parameters_length) {
    printf("USER detected >:)\n");
    for (int i = 0; i < (int) pop3args->quantity_users; i++) {
        // TODO: Validate the comparison to prevent segfaults?
        if (strcmp(pop3args->users[i].name, commandParameters) == 0) {
            socket_write(client_data->socket_data, "+OK ;)\r\n", 9); 
            client_data->user = &pop3args->users[i];
            return;
        }
    }
    socket_write(client_data->socket_data, "+OK\r\n", 6);
}

void pass_handler(ClientData * client_data, char * commandParameters, uint8_t parameters_length) {
    printf("PASS detected :P\n");
    if (client_data->user == NULL) {
        socket_write(client_data->socket_data, MALFORMED_COMMAND, sizeof MALFORMED_COMMAND - 1);
        return;
    }
    if (strcmp(client_data->user->pass, commandParameters) == 0) {
        socket_write(client_data->socket_data, "+OK 8)\r\n", 9);
        client_data->state = TRANSACTION;
        client_data->mail_info_list = get_mail_info_list(pop3args->maildir_path, &client_data->mail_count);
        return;
    }
    socket_write(client_data->socket_data, "-ERR 8(\r\n", 9);
    return;
    
}

void list_handler(ClientData * client_data, char * commandParameters, uint8_t parameters_length) {
    // TODO: Ver size constante
    char buffer[100] = {0};
    for (int i=0; i < client_data->mail_count; i++) {
        int len = sprintf(buffer, "%d %ld %s\r\n", i+1, client_data->mail_info_list[i].size, client_data->mail_info_list[i].filename);
        // printf("n: %s", client_data->mail_info_list[i].filename);
        socket_write(client_data->socket_data, buffer, len);
    }
}

CommandDescription available_commands[] = {
    {.id = CAPA, .name = "CAPA", .handler = capa_handler, .valid_states = AUTHORIZATION | TRANSACTION },
    {.id = NOOP, .name = "NOOP", .handler = noop_handler, .valid_states = TRANSACTION },
    {.id = USER, .name = "USER", .handler = user_handler, .valid_states = AUTHORIZATION },
    {.id = PASS, .name = "PASS", .handler = pass_handler, .valid_states = AUTHORIZATION },
    {.id = PASS, .name = "LIST", .handler = list_handler, .valid_states = TRANSACTION },

};

int consume_pop3_buffer(parser * pop3parser, ClientData * client_data, ssize_t n) {
    for (int i=0; i<n; i++) {
        const uint8_t c = socket_data_read(client_data->socket_data);
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

int process_event(parser_event * event, ClientData * client_data) {
    for (int i = 0; i < (int) N(available_commands); i++) {
        if (strcmp(event->command, available_commands[i].name) == 0) {
            if((client_data->state & available_commands[i].valid_states) == 0) {
                socket_write(client_data->socket_data, "-ERR Comando a destiempo\r\n", 27);
                return -1;
            }
            available_commands[i].handler(client_data, event->args, event->args_length);

            return 0;
        }
    }
    // TODO: Handle errors?
    return -1;
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
    ClientData * client_data = initialize_client_data(socket_data);

    ssize_t n;
    extern parser_definition pop3_parser_definition;
    parser * pop3parser = parser_init(NULL, &pop3_parser_definition);

    while(true) {
        n = socket_data_receive(client_data->socket_data);
        if(n > 0) {
            if (consume_pop3_buffer(pop3parser, client_data, n) == 0) {
                parser_event * event = parser_get_event(pop3parser);
                if (event != NULL)
                    process_event(event, client_data);
            }
        } else {
            break;
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