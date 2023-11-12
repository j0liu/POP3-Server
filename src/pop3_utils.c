#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <netinet/in.h>
#include <sys/socket.h>

#include "buffer.h"
#include "netutils.h"
#include <unistd.h>
// #include "tests.h"
#include "client_data.h"
#include "parser/parser.h"
#include "pop3_utils.h"
#include "socket_data.h"
Pop3Args* pop3args;

#define CRLF "\r\n"
#define OK "+OK"
#define OKCRLF (OK CRLF)
#define ERR "-ERR"
#define SERVER_READY OK " POP3 Party Started" CRLF
#define CAPA_MSG_AUTHORIZATION (OK CRLF "CAPA" CRLF "USER" CRLF "PIPELINING" CRLF "." CRLF)
#define CAPA_MSG_TRANSACTION (OK CRLF "CAPA" CRLF "PIPELINING" CRLF "." CRLF)
#define NO_USERNAME_GIVEN (ERR " No username given." CRLF)
#define NO_MSG_NUMBER_GIVEN (ERR " Message number required." CRLF)
#define UNKNOWN_COMMAND (ERR " Unknown command: %s" CRLF)
#define LOGGING_IN (OK " Logged in." CRLF)
#define LOGGING_OUT (OK " Logging out." CRLF)
#define AUTH_FAILED (ERR " [AUTH] Authentication failed." CRLF)
#define DISCONNECTED_FOR_INACTIVITY (ERR " Disconnected for inactivity." CRLF)
#define ERR_NOISE (ERR " Noise after message number: %s" CRLF)
#define ERR_INVALID_NUMBER (ERR " Invalid message number: %s" CRLF)
#define OK_OCTETS (OK " %ld octets" CRLF)
#define TERMINATION ("." CRLF)
// LIST
#define OK_LIST (OK " %d messages:" CRLF)
#define NO_MESSAGE_LIST (ERR " There's no message %d." CRLF)
#define INVALID_NUMBER_LIST (ERR " Invalid message number: %d" CRLF)

static void noop_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    socket_write(client_data->socket_data, OKCRLF, sizeof OKCRLF - 1);
}

static void capa_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    if (client_data->state == AUTHORIZATION) {
        socket_write(client_data->socket_data, CAPA_MSG_AUTHORIZATION, sizeof CAPA_MSG_AUTHORIZATION - 1);
        return;
    }
    socket_write(client_data->socket_data, CAPA_MSG_TRANSACTION, sizeof CAPA_MSG_TRANSACTION - 1);
}

static void user_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    for (int i = 0; i < (int)pop3args->quantity_users; i++) {
        // TODO: Validate the comparison to prevent segfaults?
        if (strcmp(pop3args->users[i].name, commandParameters) == 0) {
            socket_write(client_data->socket_data, OKCRLF, sizeof OKCRLF - 1);
            client_data->user = &pop3args->users[i];
            return;
        }
    }
    socket_write(client_data->socket_data, OKCRLF, sizeof OKCRLF - 1);
}

static void pass_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    if (client_data->user == NULL) {
        socket_write(client_data->socket_data, NO_USERNAME_GIVEN, sizeof NO_USERNAME_GIVEN - 1);
        return;
    }
    if (strcmp(client_data->user->pass, commandParameters) == 0) {
        socket_write(client_data->socket_data, LOGGING_IN, sizeof LOGGING_IN - 1);
        client_data->state = TRANSACTION;
        client_data->mail_info_list = get_mail_info_list(pop3args->maildir_path, &client_data->mail_count);
        return;
    }
    socket_write(client_data->socket_data, AUTH_FAILED, sizeof AUTH_FAILED - 1);
    return;
}

static int first_argument_to_int(ClientData* client_data, char* commandParameters)
{
    char* endptr;
    int num = -1, len;
    if (commandParameters != NULL) {
        num = strtol(commandParameters, &endptr, 10);
        if (num > 0 && num < client_data->mail_count && *endptr == '\0' && endptr != commandParameters)
            return num;
    }
    char buff[100] = { 0 }; // TODO: Improve
    if (num <= 0 || endptr == commandParameters) {
        len = sprintf(buff, ERR_INVALID_NUMBER, commandParameters != NULL ? commandParameters : "");
    } else if (*endptr != '\0') {
        len = sprintf(buff, ERR_NOISE, endptr);
    } else if (num > client_data->mail_count) {
        len = sprintf(buff, NO_MESSAGE_LIST, num);
    }
    socket_write(client_data->socket_data, buff, len);
    return -1;
}

static void list_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    char buff[100] = { 0 }; // TODO: Improve
    while (*commandParameters == ' ') {
        commandParameters++;
        parameters_length--;
    }

    if (parameters_length == 0) {
        for (int i = 0; i < client_data->mail_count; i++) {
            int len = sprintf(buff, "%d %ld" CRLF, i + 1, client_data->mail_info_list[i].size);
            socket_write(client_data->socket_data, buff, len);
        }
        socket_write(client_data->socket_data, TERMINATION, sizeof TERMINATION - 1);
    } else {
        int num = first_argument_to_int(client_data, commandParameters);
        if (num > 0) {
            int len = sprintf(buff, OK " %d %ld" CRLF, num, client_data->mail_info_list[num - 1].size);
            socket_write(client_data->socket_data, buff, len);
        }
    }
}

static void retr_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    while (*commandParameters == ' ') {
        commandParameters++;
        parameters_length--;
    }
    if (parameters_length == 0) {
        socket_write(client_data->socket_data, NO_MSG_NUMBER_GIVEN, sizeof NO_MSG_NUMBER_GIVEN - 1);
        return;
    }
    int num = first_argument_to_int(client_data, commandParameters);
    if (num > 0) {
        char initial_message[50] = { 0 };
        int len = sprintf(initial_message, OK_OCTETS, client_data->mail_info_list[num].size);
        socket_write(client_data->socket_data, initial_message, len);
    }
}

CommandDescription available_commands[] = {
    { .id = CAPA, .name = "CAPA", .handler = capa_handler, .valid_states = AUTHORIZATION | TRANSACTION },
    { .id = NOOP, .name = "NOOP", .handler = noop_handler, .valid_states = TRANSACTION },
    { .id = USER, .name = "USER", .handler = user_handler, .valid_states = AUTHORIZATION },
    { .id = PASS, .name = "PASS", .handler = pass_handler, .valid_states = AUTHORIZATION },
    { .id = LIST, .name = "LIST", .handler = list_handler, .valid_states = TRANSACTION },
    { .id = RETR, .name = "RETR", .handler = retr_handler, .valid_states = TRANSACTION },
};

static int consume_pop3_buffer(parser* pop3parser, ClientData* client_data)
{
    for (; buffer_can_read(&client_data->socket_data->client_buffer);) {
        const uint8_t c = socket_data_read(client_data->socket_data);
        const parser_event* event = parser_feed(pop3parser, c);
        if (event == NULL)
            return -1;
        if (event->finished)
            return 0;
    }
    // TODO: Handle errors?
    return -1;
}

static int process_event(parser_event* event, ClientData* client_data)
{
    for (int i = 0; i < (int)N(available_commands); i++) {
        if (strncasecmp(event->command, available_commands[i].name, 4) == 0) {
            if ((client_data->state & available_commands[i].valid_states) == 0) {
                char initial_message[50] = { 0 };
                int len = sprintf(initial_message, UNKNOWN_COMMAND, event->command);
                socket_write(client_data->socket_data, initial_message, len);
                return -1;
            }
            available_commands[i].handler(client_data, event->args, event->args_length);

            return 0;
        }
    }

    char buff[100] = { 0 }; // TODO: Improve
    int len = sprintf(buff, UNKNOWN_COMMAND, event->command);
    socket_write(client_data->socket_data, buff, len);
    // TODO: Handle errors?
    return -1;
}

/**
 * maneja cada conexión entrante
 *
 * @param fd   descriptor de la conexión entrante.
 * @param caddr información de la conexiónentrante.
 */

static void pop3_handle_connection(const int fd, const struct sockaddr* caddr)
{
    SocketData* socket_data = initialize_socket_data(fd);
    socket_write(socket_data, SERVER_READY, sizeof SERVER_READY - 1);
    ClientData* client_data = initialize_client_data(socket_data);

    extern parser_definition pop3_parser_definition;
    parser* pop3parser = parser_init(NULL, &pop3_parser_definition);

    while (buffer_can_read(&client_data->socket_data->client_buffer) || socket_data_receive(client_data->socket_data) > 0) {
        if (consume_pop3_buffer(pop3parser, client_data) == 0) {
            parser_event* event = parser_pop_event(pop3parser);
            if (event != NULL) {
                process_event(event, client_data);
                free(event);
            }
        }
    }
    close(fd);
}

/** rutina de cada hilo worker */

static void* handle_connection_pthread(void* args)
{
    const struct connection* c = args;
    pthread_detach(pthread_self());
    pop3_handle_connection(c->fd, (struct sockaddr*)&c->addr);
    free(args);
    return 0;
}

int serve_pop3_concurrent_blocking(const int server, Pop3Args* receivedArgs)
{
    pop3args = receivedArgs; // TODO: Improve

    // TODO: add something similar to 'done' again
    for (;;) {
        struct sockaddr_in6 caddr;
        socklen_t caddrlen = sizeof(caddr);
        // Wait for a client to connect
        const int client = accept(server, (struct sockaddr*)&caddr, &caddrlen);
        if (client < 0) {
            perror("Unable to accept incoming socket");
        } else {
            // TODO(juan): limitar la cantidad de hilos concurrentes
            struct connection* c = malloc(sizeof(struct connection));
            if (c == NULL) {
                // lo trabajamos iterativamente
                pop3_handle_connection(client, (struct sockaddr*)&caddr);
            } else {
                pthread_t tid;
                c->fd = client;
                c->addrlen = caddrlen;
                memcpy(&(c->addr), &caddr, caddrlen);

                if (pthread_create(&tid, 0, handle_connection_pthread, c)) {
                    free(c);
                    // lo trabajamos iterativamente
                    pop3_handle_connection(client, (struct sockaddr*)&caddr);
                }
            }
        }
    }
    return 0;
}