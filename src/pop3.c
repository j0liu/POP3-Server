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
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "buffer.h"
#include "client.h"
#include "mail.h"
#include "netutils.h"
#include "parser/parser.h"
#include "pop3.h"
#include "protocols.h"
#include "socket_data.h"

Args* args;

#define FINISH_CONNECTION true
#define CONTINUE_CONNECTION false

static bool capa_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    // if (client_data->state == AUTHORIZATION) {
    //     socket_write(client_data->socket_data, CAPA_MSG_AUTHORIZATION, sizeof CAPA_MSG_AUTHORIZATION - 1);
    //     return CONTINUE_CONNECTION;
    // }
    // socket_write(client_data->socket_data, CAPA_MSG_TRANSACTION, sizeof CAPA_MSG_TRANSACTION - 1);
    printf("CAPA\n");
    return CONTINUE_CONNECTION;
}
/*
static bool quit_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    if (client_data->state == AUTHORIZATION) {
        socket_write(client_data->socket_data, OK_QUIT_NO_AUTH, sizeof OK_QUIT_NO_AUTH - 1);
        return FINISH_CONNECTION;
    }

    client_data->state = UPDATE;

    for (int i = 0; i < client_data->mail_count; i++) {
        if (client_data->mail_info_list[i].deleted) {
            if (remove(client_data->mail_info_list[i].filename) != 0) {
                socket_write(client_data->socket_data, NOT_REMOVED, sizeof NOT_REMOVED - 1);
                free_mail_info_list(client_data->mail_info_list, client_data->mail_count);
                return FINISH_CONNECTION;
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

    return FINISH_CONNECTION;
}

static bool user_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    for (int i = 0; i < (int)args->quantity_users; i++) {
        if (strncmp(args->users[i].name, commandParameters, parameters_length) == 0 && args->users[i].name[parameters_length] == '\0') {
            socket_write(client_data->socket_data, OKCRLF, sizeof OKCRLF - 1);
            client_data->user = &args->users[i];
            return CONTINUE_CONNECTION;
        }
    }
    socket_write(client_data->socket_data, OKCRLF, sizeof OKCRLF - 1);
    return CONTINUE_CONNECTION;
}

static bool pass_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    if (client_data->user == NULL) {
        socket_write(client_data->socket_data, NO_USERNAME_GIVEN, sizeof NO_USERNAME_GIVEN - 1);
        return CONTINUE_CONNECTION;
    }
    if (strcmp(client_data->user->pass, commandParameters) == 0) {
        client_data->state = TRANSACTION;
        client_data->mail_info_list = get_mail_info_list(args->maildir_path, &client_data->mail_count, client_data->user->name);
        if (client_data->mail_info_list == NULL) {
            socket_write(client_data->socket_data, ERR_LOCKED_MAILDROP, sizeof ERR_LOCKED_MAILDROP - 1);
            free_client_data(client_data);
            return FINISH_CONNECTION;
        }
        socket_write(client_data->socket_data, LOGGING_IN, sizeof LOGGING_IN - 1);
        client_data->mail_count_not_deleted = client_data->mail_count;
        return CONTINUE_CONNECTION;
    }
    socket_write(client_data->socket_data, AUTH_FAILED, sizeof AUTH_FAILED - 1);
    return CONTINUE_CONNECTION;
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

    return CONTINUE_CONNECTION;
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
    return CONTINUE_CONNECTION;
}

static bool retr_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    while (*commandParameters == ' ') {
        commandParameters++;
        parameters_length--;
    }

    if (parameters_length == 0) {
        socket_write(client_data->socket_data, NO_MSG_NUMBER_GIVEN, sizeof NO_MSG_NUMBER_GIVEN - 1);
        return CONTINUE_CONNECTION;
    }

    int num = first_argument_to_int(client_data, commandParameters);
    if (num > 0) {
        if (client_data->mail_info_list[num - 1].deleted) {
            socket_write(client_data->socket_data, MESSAGE_IS_DELETED, sizeof MESSAGE_IS_DELETED - 1);
            return CONTINUE_CONNECTION;
        }

        char initial_message[50] = { 0 };
        int len = sprintf(initial_message, OK_OCTETS, client_data->mail_info_list[num - 1].size);
        socket_write(client_data->socket_data, initial_message, len);

        // Open the email file
        FILE* email_file = fopen(client_data->mail_info_list[num - 1].filename, "r"); // TODO: use the file descriptor already opened
        if (email_file == NULL) {
            // Handle file open error
            socket_write(client_data->socket_data, NO_SUCH_MESSAGE, sizeof NO_SUCH_MESSAGE - 1);
            return CONTINUE_CONNECTION;
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
    return CONTINUE_CONNECTION;
}

static bool dele_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    while (*commandParameters == ' ') {
        commandParameters++;
        parameters_length--;
    }

    if (parameters_length == 0) {
        socket_write(client_data->socket_data, NO_MSG_NUMBER_GIVEN, sizeof NO_MSG_NUMBER_GIVEN - 1);
        return CONTINUE_CONNECTION;
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
    return CONTINUE_CONNECTION;
}

static bool noop_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    socket_write(client_data->socket_data, OKCRLF, sizeof OKCRLF - 1);
    return CONTINUE_CONNECTION;
}

static bool rset_handler(ClientData* client_data, char* commandParameters, uint8_t parameters_length)
{
    for (int i = 0; i < client_data->mail_count; i++) {
        client_data->mail_info_list[i].deleted = false;
    }
    client_data->mail_count_not_deleted = client_data->mail_count;
    socket_write(client_data->socket_data, OKCRLF, sizeof OKCRLF - 1);
    return CONTINUE_CONNECTION;
}
*/
CommandDescription available_commands[] = {
    { .name = "CAPA", .handler = capa_handler, .valid_states = AUTHORIZATION | TRANSACTION },
    { .name = "USER", .handler = capa_handler, .valid_states = AUTHORIZATION },
};
    /*
    { .name = "CAPA", .handler = capa_handler, .valid_states = AUTHORIZATION | TRANSACTION },
    { .name = "QUIT", .handler = quit_handler, .valid_states = AUTHORIZATION | TRANSACTION },
    { .name = "USER", .handler = user_handler, .valid_states = AUTHORIZATION },
    { .name = "PASS", .handler = pass_handler, .valid_states = AUTHORIZATION },
    { .name = "STAT", .handler = stat_handler, .valid_states = TRANSACTION },
    { .name = "LIST", .handler = list_handler, .valid_states = TRANSACTION },
    { .name = "RETR", .handler = retr_handler, .valid_states = TRANSACTION },
    { .name = "DELE", .handler = dele_handler, .valid_states = TRANSACTION },
    { .name = "NOOP", .handler = noop_handler, .valid_states = TRANSACTION },
    { .name = "RSET", .handler = rset_handler, .valid_states = TRANSACTION },
};
*/

static int consume_pop3_buffer(parser* pop3parser, ClientData* client_data)
{
    for (; buffer_can_read(&client_data->socket_data->read_buffer);) {
        const uint8_t c = socket_data_read(client_data->socket_data);
        const parser_event* event = parser_feed(pop3parser, c);
        if (event == NULL) {
            printf("Event null\n");
            return -1;
        }

        if (event->finished) {
            printf("Event finished\n");
            return 0;
        }
    }

    printf("No more data\n");
    return -1;
}

static bool process_event(parser_event* event, ClientData* client_data)
{
    if (event->command_length + event->args_length > MAX_COMMAND_LENGTH) {
        socket_buffer_write(client_data->socket_data, ERR_COMMAND_TOO_LONG, sizeof ERR_COMMAND_TOO_LONG - 1);
        return FINISH_CONNECTION;
    }

    client_data->last_activity_time = time(NULL);
    for (int i = 0; i < (int)N(available_commands); i++) {
        if (event->command_length == 4 && strncasecmp(event->command, available_commands[i].name, event->command_length) == 0) {
            if ((client_data->state & available_commands[i].valid_states) == 0) {
                socket_buffer_write(client_data->socket_data, UNKNOWN_COMMAND, sizeof UNKNOWN_COMMAND - 1);
                return CONTINUE_CONNECTION;
            }
            return available_commands[i].handler(client_data, event->args, event->args_length);
        }
    }

    socket_buffer_write(client_data->socket_data, UNKNOWN_COMMAND, sizeof UNKNOWN_COMMAND - 1);

    return CONTINUE_CONNECTION;
}

/**
 * maneja cada conexión entrante
 *
 * @param fd   descriptor de la conexión entrante.
 * @param caddr información de la conexiónentrante.
 */
/* static void pop3_handle_connection(const int fd, const struct sockaddr_in6* caddr)
{
    SocketData* socket_data = initialize_socket_data(fd);
    socket_write(socket_data, SERVER_READY, sizeof SERVER_READY - 1);
    ClientData* client_data = initialize_client_data(socket_data);

    extern parser_definition pop3_parser_definition;
    parser* pop3parser = parser_init(NULL, &pop3_parser_definition); // TODO: ponerlo en Client
    int result;

    while (buffer_can_read(&client_data->socket_data->read_buffer) || socket_data_receive(client_data->socket_data) > 0) {
        if (difftime(time(NULL), client_data->last_activity_time) > INACTIVITY_TIMEOUT) {
            socket_write(client_data->socket_data, ERR_INACTIVITY_TIMEOUT, sizeof ERR_INACTIVITY_TIMEOUT - 1);
            break;
        }
        printf("buffer_can_read: %d\n", buffer_can_read(&client_data->socket_data->read_buffer));

        if (consume_pop3_buffer(pop3parser, client_data) == 0) {
            printf("Consume pop3 buffer\n");

            parser_event* event = parser_pop_event(pop3parser);
            if (event != NULL) {
                printf("Event not null\n");
                result = process_event(event, client_data);
                free(event);
                if (result == FINISH_CONNECTION)
                    break;
            }
        }
    }

    if (client_data->state == TRANSACTION) {
        free_mail_info_list(client_data->mail_info_list, client_data->mail_count);
    }
    free_client_data(client_data);
    close(fd);
} */

void welcome_init(const unsigned state, struct selector_key* key)
{
    // extern parser_definition pop3_parser_definition;
    // parser* pop3parser = parser_init(NULL, &pop3_parser_definition);

    // CommandState* d = &ATTACHMENT(key).command_st;
    // buffer* rb = &(ATTACHMENT(key)->client_data->socket_data->read_buffer);
    buffer* wb = &(ATTACHMENT(key)->client_data->socket_data->write_buffer);

    // Agregamos el mensaje de bienvenida
    size_t len = 0;
    uint8_t* ptr = buffer_write_ptr(wb, &len);
    strncpy((char*)ptr, SERVER_READY, len);
    buffer_write_adv(wb, strlen(SERVER_READY));
}

void welcome_close(const unsigned state, struct selector_key* key)
{
    /* CommandState* d = &ATTACHMENT(key).command_st; */
    buffer *rb = &(ATTACHMENT(key)->client_data->socket_data->read_buffer), *wb = &(ATTACHMENT(key)->client_data->socket_data->write_buffer);
    buffer_reset(rb);
    buffer_reset(wb);
}

unsigned welcome_write(struct selector_key* key)
{
    /* CommandState* d = &ATTACHMENT(key).command_st; */
    buffer* wb = &(ATTACHMENT(key)->client_data->socket_data->write_buffer);
    size_t len = 0;
    uint8_t* wbPtr = buffer_read_ptr(wb, &len);
    ssize_t sent_count = send(key->fd, wbPtr, len, MSG_NOSIGNAL);

    if (sent_count == -1) {
        return ERROR;
    }

    // Para las estadisticas, despues usar un struct
    // bytes_sent += sent_count;

    buffer_read_adv(wb, sent_count);
    // Si no pude mandar el mensaje de bienvenida completo, vuelve a intentar
    if (buffer_can_read(wb)) {
        return WELCOME;
    }

    // Si no hay mas para escribir
    if (selector_set_interest(key->s, key->fd, OP_READ) != SELECTOR_SUCCESS) {
        return ERROR;
    }

    // return WELCOME;
    return COMMAND_READ;
}

void command_read_init(const unsigned state, struct selector_key* key) {
    Client * client = ATTACHMENT(key);
    extern parser_definition pop3_parser_definition;
    client->pop3parser = parser_init(NULL, &pop3_parser_definition); 
}

// void command_read_close(const unsigned state, struct selector_key* key)
// {
// }

unsigned command_read(struct selector_key* key)
{
    Client * client = ATTACHMENT(key);
    ClientData * client_data = client->client_data;

    buffer* rb = &client_data->socket_data->read_buffer;
    size_t len;
    uint8_t *rbPtr = buffer_write_ptr(rb, &len);
    ssize_t read_count = recv(key->fd, rbPtr, len, 0); // TODO: Ver flags?

    if (read_count == -1) {
        return ERROR;
    } else if (read_count == 0) {
        return DONE;
    }

    buffer_write_adv(rb, read_count);
    rbPtr = buffer_read_ptr(rb, &len);

    
    int result;

    while (buffer_can_read(rb)) {
        // if (difftime(time(NULL), client_data->last_activity_time) > INACTIVITY_TIMEOUT) {
        //     socket_write(client_data->socket_data, ERR_INACTIVITY_TIMEOUT, sizeof ERR_INACTIVITY_TIMEOUT - 1);
        //     break;
        // }
        //printf("buffer_can_read: %d\n", buffer_can_read(&client_data->socket_data->read_buffer));

        if (consume_pop3_buffer(client->pop3parser, client_data) == 0) {
            printf("Consume pop3 buffer\n");

            parser_event* event = parser_pop_event(client->pop3parser);
            if (event != NULL) {
                printf("Event not null\n");
                result = process_event(event, client_data);
                free(event);
                if (result == FINISH_CONNECTION)
                    break;
            }
        }
    }
    

    return COMMAND_READ;
}

void command_write_init(const unsigned state, struct selector_key* key)
{
}

void command_write_close(const unsigned state, struct selector_key* key)
{
}

unsigned command_write(struct selector_key* key)
{
    return 0;
}