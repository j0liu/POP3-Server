#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include <netinet/in.h>
#include <sys/socket.h>

#include "buffer.h"
#include "netutils.h"
#include <unistd.h>
// #include "tests.h"
#include "client_data.h"
#include "mail.h"
#include "parser/parser.h"
#include "pop3_utils.h"
#include "socket_data.h"
Args* args;

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
#define ERR_INACTIVITY_TIMEOUT (ERR " Disconnected for inactivity." CRLF)
#define ERR_LOCKED_MAILDROP (ERR " Unable to lock maildrop." CRLF)
#define OK_OCTETS (OK " %ld octets" CRLF)
#define TERMINATION ("." CRLF)
// LIST
#define OK_LIST (OK " %d messages:" CRLF)
#define NO_MESSAGE_LIST (ERR " There's no message %d." CRLF)
#define INVALID_NUMBER_LIST (ERR " Invalid message number: %d" CRLF)
// RETR
#define NO_SUCH_MESSAGE (ERR " No such message" CRLF)
#define RETR_TERMINATION (CRLF "." CRLF)
// DELE
#define OK_DELE (OK " Marked to be deleted." CRLF)
#define MESSAGE_IS_DELETED (ERR " Message is deleted." CRLF)
// QUIT
#define NOT_REMOVED (ERR " Some messages may not have been deleted." CRLF)
#define OK_QUIT_NO_AUTH (OK " POP3 Party over" CRLF)
#define OK_QUIT (OK " POP3 Party over (%d messages left)" CRLF)
#define OK_QUIT_EMPTY (OK " POP3 Party over (maildrop empty)" CRLF)

static bool capa_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    if (client_data->state == AUTHORIZATION) {
        socket_write(client_data->socket_data, CAPA_MSG_AUTHORIZATION, sizeof CAPA_MSG_AUTHORIZATION - 1);
        return false;
    }
    socket_write(client_data->socket_data, CAPA_MSG_TRANSACTION, sizeof CAPA_MSG_TRANSACTION - 1);
    return false;
}

static bool quit_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    if (client_data->state == AUTHORIZATION) {
        socket_write(client_data->socket_data, OK_QUIT_NO_AUTH, sizeof OK_QUIT_NO_AUTH - 1);
        free_client_data(client_data);
        return true;
    }

    client_data->state = UPDATE;

    for (int i = 0; i < client_data->mail_count; i++) {
        if (client_data->mail_info_list[i].deleted) {
            if (remove(client_data->mail_info_list[i].filename) != 0) {
                socket_write(client_data->socket_data, NOT_REMOVED, sizeof NOT_REMOVED - 1);
                free_mail_info_list(client_data->mail_info_list, client_data->mail_count);
                free_client_data(client_data);
                return true;
            }
        }
    }

    if (client_data->mail_count_not_deleted == 0) {
        socket_write(client_data->socket_data, OK_QUIT_EMPTY, sizeof OK_QUIT_EMPTY - 1);
    } else {
        char buff[100] = { 0 }; // TODO: Improve
        int len = sprintf(buff, OK_QUIT, client_data->mail_count_not_deleted);
        socket_write(client_data->socket_data, buff, len);
    }

    free_mail_info_list(client_data->mail_info_list, client_data->mail_count);
    free_client_data(client_data);

    return true;
}

static bool user_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    for (int i = 0; i < (int)args->quantity_users; i++) {
        if (strncmp(args->users[i].name, commandParameters, parameters_length) == 0 && args->users[i].name[parameters_length] == '\0') {
            socket_write(client_data->socket_data, OKCRLF, sizeof OKCRLF - 1);
            client_data->user = &args->users[i];
            return false;
        }
    }
    socket_write(client_data->socket_data, OKCRLF, sizeof OKCRLF - 1);
    return false;
}

static bool pass_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    if (client_data->user == NULL) {
        socket_write(client_data->socket_data, NO_USERNAME_GIVEN, sizeof NO_USERNAME_GIVEN - 1);
        return false;
    }
    if (strcmp(client_data->user->pass, commandParameters) == 0) {
        client_data->state = TRANSACTION;
        client_data->mail_info_list = get_mail_info_list(args->maildir_path, &client_data->mail_count, client_data->user->name);
        if (client_data->mail_info_list == NULL) {
            socket_write(client_data->socket_data, ERR_LOCKED_MAILDROP, sizeof ERR_LOCKED_MAILDROP - 1);
            free_client_data(client_data);
            return true;
        }
        socket_write(client_data->socket_data, LOGGING_IN, sizeof LOGGING_IN - 1);
        client_data->mail_count_not_deleted = client_data->mail_count;
        return false;
    }
    socket_write(client_data->socket_data, AUTH_FAILED, sizeof AUTH_FAILED - 1);
    return false;
}

static int first_argument_to_int(ClientData* client_data, char* commandParameters)
{
    char* endptr;
    int num = -1, len;
    if (commandParameters != NULL) {
        num = strtol(commandParameters, &endptr, 10);
        if (num > 0 && num <= client_data->mail_count && *endptr == '\0' && endptr != commandParameters) {
            return num;
        }
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

static bool stat_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    int count = 0;
    size_t size = 0;
    for (int i = 0; i < client_data->mail_count; i++) {
        if (!client_data->mail_info_list[i].deleted) {
            count++;
            size += client_data->mail_info_list[i].size;
        }
    }
    char buff[100] = { 0 }; // TODO: Improve
    int len = sprintf(buff, OK " %d %ld" CRLF, count, size);
    socket_write(client_data->socket_data, buff, len);

    return false;
}

static bool list_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    char buff[100] = { 0 }; // TODO: Improve
    while (*commandParameters == ' ') {
        commandParameters++;
        parameters_length--;
    }

    if (parameters_length == 0) {
        int len = sprintf(buff, OK_LIST, client_data->mail_count_not_deleted);
        socket_write(client_data->socket_data, buff, len);

        for (int i = 0; i < client_data->mail_count; i++) {
            if (!client_data->mail_info_list[i].deleted) {
                int len = sprintf(buff, "%d %ld" CRLF, i + 1, client_data->mail_info_list[i].size);
                socket_write(client_data->socket_data, buff, len);
            }
        }
        socket_write(client_data->socket_data, TERMINATION, sizeof TERMINATION - 1);
    } else {
        int num = first_argument_to_int(client_data, commandParameters);
        if (num > 0) {
            int len = sprintf(buff, OK " %d %ld" CRLF, num, client_data->mail_info_list[num - 1].size);
            socket_write(client_data->socket_data, buff, len);
        }
    }
    return false;
}

static bool retr_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    while (*commandParameters == ' ') {
        commandParameters++;
        parameters_length--;
    }

    if (parameters_length == 0) {
        socket_write(client_data->socket_data, NO_MSG_NUMBER_GIVEN, sizeof NO_MSG_NUMBER_GIVEN - 1);
        return false;
    }

    int num = first_argument_to_int(client_data, commandParameters);
    if (num > 0) {
        if (client_data->mail_info_list[num - 1].deleted) {
            socket_write(client_data->socket_data, MESSAGE_IS_DELETED, sizeof MESSAGE_IS_DELETED - 1);
            return false;
        }

        char initial_message[50] = { 0 };
        int len = sprintf(initial_message, OK_OCTETS, client_data->mail_info_list[num - 1].size);
        socket_write(client_data->socket_data, initial_message, len);

        // Open the email file
        FILE* email_file = fopen(client_data->mail_info_list[num - 1].filename, "r"); //TODO: use the file descriptor already opened
        if (email_file == NULL) {
            // Handle file open error
            socket_write(client_data->socket_data, NO_SUCH_MESSAGE, sizeof NO_SUCH_MESSAGE - 1);
            return false;
        }

        // -- Add . if CRLF. is found
        char line[1024];
        char last_char = '\0';
        while (fgets(line, sizeof(line), email_file) != NULL) {
            size_t line_len = strlen(line);
            if (line_len > 0) {
                last_char = line[line_len - 1];
            }

            if (line[0] == '.' && (line[1] == '\r' && line[2] == '\n')) {
                socket_write(client_data->socket_data, ".", 1);
            }
            socket_write(client_data->socket_data, line, line_len);
        }

        if (last_char == '.') {
            socket_write(client_data->socket_data, ".", 1);
        }
        // --

        // Close the file
        fclose(email_file);

        // Send terminating sequence
        socket_write(client_data->socket_data, RETR_TERMINATION, sizeof RETR_TERMINATION - 1);
    }
    return false;
}

static bool dele_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    while (*commandParameters == ' ') {
        commandParameters++;
        parameters_length--;
    }

    if (parameters_length == 0) {
        socket_write(client_data->socket_data, NO_MSG_NUMBER_GIVEN, sizeof NO_MSG_NUMBER_GIVEN - 1);
        return false;
    }

    int num = first_argument_to_int(client_data, commandParameters);
    if (num > 0) {
        if (client_data->mail_info_list[num - 1].deleted) {
            socket_write(client_data->socket_data, MESSAGE_IS_DELETED, sizeof MESSAGE_IS_DELETED - 1);
        } else {
            client_data->mail_info_list[num - 1].deleted = true;
            client_data->mail_count_not_deleted--;
            socket_write(client_data->socket_data, OK_DELE, sizeof OK_DELE - 1);
        }
    }
    return false;
}

static bool noop_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    socket_write(client_data->socket_data, OKCRLF, sizeof OKCRLF - 1);
    return false;
}

static bool rset_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    for (int i = 0; i < client_data->mail_count; i++) {
        client_data->mail_info_list[i].deleted = false;
    }
    client_data->mail_count_not_deleted = client_data->mail_count;
    socket_write(client_data->socket_data, OKCRLF, sizeof OKCRLF - 1);
    return false;
}

CommandDescription available_commands[] = {
    { .id = CAPA, .name = "CAPA", .handler = capa_handler, .valid_states = AUTHORIZATION | TRANSACTION },
    { .id = QUIT, .name = "QUIT", .handler = quit_handler, .valid_states = AUTHORIZATION | TRANSACTION },
    { .id = USER, .name = "USER", .handler = user_handler, .valid_states = AUTHORIZATION },
    { .id = PASS, .name = "PASS", .handler = pass_handler, .valid_states = AUTHORIZATION },
    { .id = STAT, .name = "STAT", .handler = stat_handler, .valid_states = TRANSACTION },
    { .id = LIST, .name = "LIST", .handler = list_handler, .valid_states = TRANSACTION },
    { .id = RETR, .name = "RETR", .handler = retr_handler, .valid_states = TRANSACTION },
    { .id = DELE, .name = "DELE", .handler = dele_handler, .valid_states = TRANSACTION },
    { .id = NOOP, .name = "NOOP", .handler = noop_handler, .valid_states = TRANSACTION },
    { .id = RSET, .name = "RSET", .handler = rset_handler, .valid_states = TRANSACTION },
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
            client_data->last_activity_time = time(NULL);
            bool close_connection = available_commands[i].handler(client_data, event->args, event->args_length);
            if (close_connection)
                return 1;

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

static void pop3_handle_connection(const int fd, const struct sockaddr_in6* caddr)
{
    SocketData* socket_data = initialize_socket_data(fd);
    socket_write(socket_data, SERVER_READY, sizeof SERVER_READY - 1);
    ClientData* client_data = initialize_client_data(socket_data);

    extern parser_definition pop3_parser_definition;
    parser* pop3parser = parser_init(NULL, &pop3_parser_definition);
    int result;

    while (buffer_can_read(&client_data->socket_data->client_buffer) || socket_data_receive(client_data->socket_data) > 0) {
        if (difftime(time(NULL), client_data->last_activity_time) > INACTIVITY_TIMEOUT) {
            socket_write(client_data->socket_data, ERR_INACTIVITY_TIMEOUT, sizeof ERR_INACTIVITY_TIMEOUT - 1);
            break;
        }

        if (consume_pop3_buffer(pop3parser, client_data) == 0) {
            parser_event* event = parser_pop_event(pop3parser);
            if (event != NULL) {
                result = process_event(event, client_data);
                free(event);
                if (result == 1)
                    break;
            }
        }
    }
    if (result != 1) {
        if (client_data->state == TRANSACTION) {
            free_mail_info_list(client_data->mail_info_list, client_data->mail_count);
        }
        free_client_data(client_data);
    }
    close(fd);
}

/** rutina de cada hilo worker */

static void* handle_connection_pthread(void* args)
{
    const struct connection* c = args;
    pthread_detach(pthread_self());
    pop3_handle_connection(c->fd, (struct sockaddr_in6*)&c->addr);
    free(args);
    return 0;
}

int serve_pop3_concurrent_blocking(const int server, Args* receivedArgs)
{
    args = receivedArgs;

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
                pop3_handle_connection(client, (struct sockaddr_in6*)&caddr);
            } else {
                pthread_t tid;
                c->fd = client;
                c->addrlen = caddrlen;
                memcpy(&(c->addr), &caddr, caddrlen);

                if (pthread_create(&tid, 0, handle_connection_pthread, c)) {
                    free(c);
                    // lo trabajamos iterativamente
                    pop3_handle_connection(client, (struct sockaddr_in6*)&caddr);
                }
            }
        }
    }
    return 0;
}