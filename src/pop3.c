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

#include "logger/logger.h"
#include "buffer.h"
#include "client.h"
#include "mail.h"
#include "netutils.h"
#include "parser/parser.h"
#include "pop3.h"
#include "protocols.h"
#include "socket_data.h"
#include "global_state/globalstate.h"
extern Args args;
extern GlobalState global_state;


#define REGISTER_PENDING -2

static int capa_handler(Client * client, char* commandParameters, uint8_t parameters_length)
{
    //ClientData* client_data = client->client_data;
    if (client->state == AUTHORIZATION) {
        // TODO: Manejar escrituras 
        socket_buffer_write(client->socket_data, CAPA_MSG_AUTHORIZATION, sizeof CAPA_MSG_AUTHORIZATION - 1); 
    } else {
        socket_buffer_write(client->socket_data, CAPA_MSG_TRANSACTION, sizeof CAPA_MSG_TRANSACTION - 1);
    }
    client->command_state.finished = true; 
    return COMMAND_WRITE; 
}

static int quit_handler(Client * client, char* commandParameters, uint8_t parameters_length)
{
    ClientData* client_data = client->client_data;
    if (client->state == AUTHORIZATION) {
        socket_buffer_write(client->socket_data, OK_QUIT_NO_AUTH, sizeof OK_QUIT_NO_AUTH - 1);
        return DONE;
    }

    client->state = UPDATE;

    for (int i = 0; i < client_data->mail_count; i++) {
        if (client_data->mail_info_list[i].deleted) {
            if (remove(client_data->mail_info_list[i].filename) != 0) {
                socket_buffer_write(client->socket_data, NOT_REMOVED, sizeof NOT_REMOVED - 1);
                free_mail_info_list(client_data->mail_info_list, client_data->mail_count);
                return ERROR;
            }
        }
    }

    if (client_data->mail_count_not_deleted == 0) {
        socket_buffer_write(client->socket_data, OK_QUIT_EMPTY, sizeof OK_QUIT_EMPTY - 1);
    } else {
        char buff[100] = { 0 }; // TODO: Improve
        int len = sprintf(buff, OK_QUIT, client_data->mail_count_not_deleted);
        socket_buffer_write(client->socket_data, buff, len);
    }

    return DONE;
}

static int user_handler(Client * client, char* commandParameters, uint8_t parameters_length)
{
    //ClientData* client_data = client->client_data;
    for (int i = 0; i < (int)args.quantity_users; i++) {
        if (strncmp(args.users[i].name, commandParameters, parameters_length) == 0 && args.users[i].name[parameters_length] == '\0') {
            socket_buffer_write(client->socket_data, OKCRLF, sizeof OKCRLF - 1);
            client->user = &args.users[i];
            client->command_state.finished = true; 
            return COMMAND_WRITE; 
        }
    }
    client->command_state.finished = true; 
    socket_buffer_write(client->socket_data, OKCRLF, sizeof OKCRLF - 1);
    return COMMAND_WRITE; 
}

static int pass_handler(Client * client, char* commandParameters, uint8_t parameters_length)
{
    ClientData* client_data = client->client_data;
    if (client->user == NULL) {
        socket_buffer_write(client->socket_data, NO_USERNAME_GIVEN, sizeof NO_USERNAME_GIVEN - 1);
        return ERROR;
    }
    if (strcmp(client->user->pass, commandParameters) == 0) {
        client->state = TRANSACTION;
        client_data->mail_info_list = get_mail_info_list(args.maildir_path, &client_data->mail_count, client->user->name);
        // if (client_data->mail_info_list == NULL) {
            // socket_buffer_write(client_data->socket_data, ERR_LOCKED_MAILDROP, sizeof ERR_LOCKED_MAILDROP - 1);
            // free_client_data(client_data);
            // return C; 
        // }
        socket_buffer_write(client->socket_data, LOGGING_IN, sizeof LOGGING_IN - 1);
        client_data->mail_count_not_deleted = client_data->mail_count;
        client->command_state.finished = true; 
        return COMMAND_WRITE; 
    }
    socket_buffer_write(client->socket_data, AUTH_FAILED, sizeof AUTH_FAILED - 1);
    client->command_state.finished = true; 
    return COMMAND_WRITE; 
}

static int first_argument_to_int(Client* client, char* commandParameters)
{
    char* endptr;
    int num = -1, len = 0;
    if (commandParameters != NULL) {
        num = strtol(commandParameters, &endptr, 10);
        if (num > 0 && num <= client->client_data->mail_count && *endptr == '\0' && endptr != commandParameters) {
            return num;
        }
    }
    char buff[100] = { 0 }; // TODO: Improve
    if (num <= 0 || endptr == commandParameters) {
        len = sprintf(buff, ERR_INVALID_NUMBER, commandParameters != NULL ? commandParameters : "");
    } else if (*endptr != '\0') {
        len = sprintf(buff, ERR_NOISE, endptr);
    } else if (num > client->client_data->mail_count) {
        len = sprintf(buff, NO_MESSAGE_LIST, num);
    }
    socket_buffer_write(client->socket_data, buff, len);
    return -1;
}

static int stat_handler(Client* client, char* commandParameters, uint8_t parameters_length)
{
    ClientData * client_data = client->client_data;
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
    socket_buffer_write(client->socket_data, buff, len);
    client->command_state.finished = true; 
    return COMMAND_WRITE;
}

static int list_handler(Client* client, char* commandParameters, uint8_t parameters_length)
{
    ClientData * client_data = client->client_data;
    char buff[100] = { 0 }; // TODO: Improve
    if (client_data->list_current_mail == 0) {
        while (*commandParameters == ' ') {
            commandParameters++;
            parameters_length--;
        }
    }

    if (parameters_length == 0) {
        if (client_data->list_current_mail == 0) {
            int len = sprintf(buff, OK_LIST, client_data->mail_count_not_deleted);
            socket_buffer_write(client->socket_data, buff, len);
        }

        if (client_data->mail_count > 0 && !client_data->mail_info_list[client_data->list_current_mail].deleted) {
            int len = sprintf(buff, "%d %ld" CRLF, client_data->list_current_mail + 1, client_data->mail_info_list[client_data->list_current_mail].size);
            socket_buffer_write(client->socket_data, buff, len);
            client_data->list_current_mail++;
        }
        if (client_data->list_current_mail == client_data->mail_count) {
            client_data->list_current_mail = 0;
            socket_buffer_write(client->socket_data, TERMINATION, sizeof TERMINATION - 1);
            client->command_state.finished = true;
        }
    } else {
        int num = first_argument_to_int(client, commandParameters);
        if (num > 0) {
            int len = sprintf(buff, OK " %d %ld" CRLF, num, client_data->mail_info_list[num - 1].size);
            socket_buffer_write(client->socket_data, buff, len);
        }
        client->command_state.finished = true;
    }
    return COMMAND_WRITE;
}

static int retr_handler(Client* client, char* commandParameters, uint8_t parameters_length)
{
    ClientData * client_data = client->client_data;

    int num = client_data->current_mail;
    if (num == NO_EMAIL) {
        while (*commandParameters == ' ') {
            commandParameters++;
            parameters_length--;
        }

        if (parameters_length == 0) {
            socket_buffer_write(client->socket_data, NO_MSG_NUMBER_GIVEN, sizeof NO_MSG_NUMBER_GIVEN - 1);
            return ERROR; 
        }

        num = first_argument_to_int(client, commandParameters);
        if (num == -1) {
            return ERROR;
        }
        client_data->current_mail = num;
        client_data->byte_stuffing = true;

        if (client_data->mail_info_list[num - 1].deleted) {
            socket_buffer_write(client->socket_data, MESSAGE_IS_DELETED, sizeof MESSAGE_IS_DELETED - 1);
            return ERROR; 
        }
        client_data->current_mail = num;

        // Aca iria algo de la transformacion...?
        client_data->mail_fd = open(client_data->mail_info_list[num - 1].filename, O_RDONLY);
        
        selector_fd_set_nio(client_data->mail_fd);
        if (global_state.transformations_enabled) {
            int pop3_to_transf_fds[2];
            int transf_to_pop3_fds[2];
            if (pipe(pop3_to_transf_fds) == -1)
                return ERROR;
            if (pipe(transf_to_pop3_fds) == -1)
                return ERROR;
            pid_t pid = fork();
            if(pid == -1)
                return ERROR;
            if(pid == 0) {
                //soy el transformador
                dup2(pop3_to_transf_fds[0], STDIN_FILENO);
                dup2(transf_to_pop3_fds[1], STDOUT_FILENO);
                for (int fd = STDERR_FILENO + 1; fd <= transf_to_pop3_fds[1]; fd++)
                    close(fd);
                execl("/bin/sh", "sh", "-c", global_state.transformation_path, (char *) 0);
                perror("execv");
                exit(1);
            }
            close(pop3_to_transf_fds[0]); // TODO: Ver si puede fallar
            close(transf_to_pop3_fds[1]);
            client_data->pop3_to_transf_fd = pop3_to_transf_fds[1];
            selector_fd_set_nio(client_data->pop3_to_transf_fd); // TODO: Ver si esta bien esto
            client_data->transf_to_pop3_fd = transf_to_pop3_fds[0];
            selector_fd_set_nio(client_data->transf_to_pop3_fd);
        }

        // char initial_message[50] = { 0 };
        // socket_buffer_write(client_data->socket_data, initial_message, len);
        size_t bufferLen = 0;
        uint8_t * ptr = buffer_write_ptr(&(client->socket_data->write_buffer), &bufferLen);
        int len = sprintf((char *) ptr, OK_OCTETS, client_data->mail_info_list[num - 1].size);
        buffer_write_adv(&(client->socket_data->write_buffer), len);
        return REGISTER_PENDING;
    }  else if (num == EMAIL_FINISHED) {
        close(client_data->mail_fd);
        log(LOG_DEBUG, "Closing mail");
        // Send terminating sequence
        client_data->current_mail = NO_EMAIL;
        client_data->mail_fd = -1;
        client_data->byte_stuffing = false;
        if (client_data->pop3_to_transf_fd != -1) {
            close(client_data->pop3_to_transf_fd);
            close(client_data->transf_to_pop3_fd);
            client_data->pop3_to_transf_fd = -1;
            client_data->transf_to_pop3_fd = -1;
        }
        
        socket_buffer_write(client->socket_data, RETR_TERMINATION, sizeof RETR_TERMINATION - 1);
        client->command_state.finished = true;
    }

    return COMMAND_WRITE;
}

static int dele_handler(Client * client, char* commandParameters, uint8_t parameters_length)
{
    ClientData* client_data = client->client_data;
    
    while (*commandParameters == ' ') {
        commandParameters++;
        parameters_length--;
    }

    if (parameters_length == 0) {
        socket_buffer_write(client->socket_data, NO_MSG_NUMBER_GIVEN, sizeof NO_MSG_NUMBER_GIVEN - 1);
        return ERROR;
    }

    int num = first_argument_to_int(client, commandParameters);
    if (num > 0) {
        if (client_data->mail_info_list[num - 1].deleted) {
            socket_buffer_write(client->socket_data, MESSAGE_IS_DELETED, sizeof MESSAGE_IS_DELETED - 1);
        } else {
            client_data->mail_info_list[num - 1].deleted = true;
            client_data->mail_count_not_deleted--;
            socket_buffer_write(client->socket_data, OK_DELE, sizeof OK_DELE - 1);
        }
    }

    client->command_state.finished = true; 
    return COMMAND_WRITE;    
}

static int noop_handler(Client* client, char* commandParameters, uint8_t parameters_length)
{
    //ClientData * client_data = client->client_data;
    socket_buffer_write(client->socket_data, OKCRLF, sizeof OKCRLF - 1);
    
    client->command_state.finished = true; 
    return COMMAND_WRITE;
}

static int rset_handler(Client* client, char* commandParameters, uint8_t parameters_length)
{
    ClientData * client_data = client->client_data;
    for (int i = 0; i < client_data->mail_count; i++) {
        client_data->mail_info_list[i].deleted = false;
    }
    client_data->mail_count_not_deleted = client_data->mail_count;
    socket_buffer_write(client->socket_data, OKCRLF, sizeof OKCRLF - 1);
    client->command_state.finished = true; 
    return COMMAND_WRITE;
}

CommandDescription available_commands[] = {
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


static int consume_pop3_buffer(parser* command_parser, SocketData* socket_data)
{
    for (; buffer_can_read(&socket_data->read_buffer);) {
        const uint8_t c = socket_data_read(socket_data);
        const parser_event* event = parser_feed(command_parser, c);
        if (event == NULL) {
            return -1;
        }

        if (event->finished) {
            return 0;
        }
    }

    log(LOG_DEBUG, "No more data");
    return -1;
}

static bool process_event(parser_event* event, Client* client)
{
    // if (event->command_length + event->args_length > MAX_COMMAND_LENGTH) {
    //     socket_buffer_write(client_data->socket_data, ERR_COMMAND_TOO_LONG, sizeof ERR_COMMAND_TOO_LONG - 1);
    //     return wTION;
    // }

    for (int i = 0; i < (int)N(available_commands); i++) {
        if (event->command_length == 4 && strncasecmp(event->command, available_commands[i].name, event->command_length) == 0) {
            if ((client->state & available_commands[i].valid_states) == 0) {
                break;
            }
            client->command_state.command_index = i;
            client->command_state.argLen = event->args_length;
            strcpy(client->command_state.arguments, event->args);
            client->command_state.finished = false;
            return CONTINUE_CONNECTION;
        }
    }

    socket_buffer_write(client->socket_data, UNKNOWN_COMMAND, sizeof UNKNOWN_COMMAND - 1);
    client->command_state.command_index = -1;
    client->command_state.argLen = 0;
    client->command_state.finished = false;
    return FINISH_CONNECTION; 
}

void welcome_init(const unsigned prev_state, const unsigned state, struct selector_key* key)
{
    buffer* wb = &(ATTACHMENT(key)->socket_data->write_buffer);
    global_state.total_connections++;
    global_state.current_connections++;

    // Agregamos el mensaje de bienvenida
    // Asumimos que el buffer no puede estar lleno en este punto
    size_t len = 0;
    uint8_t* ptr = buffer_write_ptr(wb, &len);
    strncpy((char*)ptr, SERVER_READY, len);
    buffer_write_adv(wb, sizeof SERVER_READY - 1);
    log(LOG_INFO, NEW_POP3_CONNECTION);
}

// TODO: Ver de sacar?
void welcome_close(const unsigned state, struct selector_key* key)
{
    /* CommandState* d = &ATTACHMENT(key).command_st; */
    buffer *rb = &(ATTACHMENT(key)->socket_data->read_buffer), *wb = &(ATTACHMENT(key)->socket_data->write_buffer);
    buffer_reset(rb);
    buffer_reset(wb);
}

unsigned welcome_write(struct selector_key* key)
{
    buffer* wb = &(ATTACHMENT(key)->socket_data->write_buffer);
    size_t len = 0;
    uint8_t* wbPtr = buffer_read_ptr(wb, &len);
    ssize_t sent_count = send(key->fd, wbPtr, len, MSG_NOSIGNAL);

    if (sent_count == -1) {
        return ERROR;
    }

    // Estadistica de bytes transferidos
    global_state.total_bytes_sent += sent_count;

    buffer_read_adv(wb, sent_count);
    // Si no pude mandar el mensaje de bienvenida completo, vuelve a intentar
    if (buffer_can_read(wb)) {
        return WELCOME;
    }

    // Si no hay mas para escribir
    // if (selector_set_interest(key->s, key->fd, OP_READ) != SELECTOR_SUCCESS) {
    //     return ERROR;
    // }

    // return WELCOME;
    return COMMAND_READ;
}

void command_read_arrival(const unsigned prev_state, const unsigned state, struct selector_key* key) {
    if(prev_state != state) {
        logf(LOG_DEBUG, "command read arrival %d", key->fd);
        Client * client = ATTACHMENT(key);
        selector_set_interest(key->s, client->socket_data->fd, OP_READ);
    }
}

static int process_read_buffer(Client* client) {
    int result = COMMAND_READ;
    buffer* rb = &client->socket_data->read_buffer;
    while (buffer_can_read(rb)) {
        logf(LOG_DEBUG, "buffer_can_read: %d", buffer_can_read(&client->socket_data->read_buffer));

        if (consume_pop3_buffer(client->command_parser, client->socket_data) == 0) {

            parser_event* event = parser_pop_event(client->command_parser);
            if (event != NULL) {
                result = process_event(event, client);
                free(event);
                parser_reset(client->command_parser);
                if (result == CONTINUE_CONNECTION) {
                    log(LOG_DEBUG, "Continue connection");
                    return COMMAND_WRITE; 
                }

                // TODO: Manejar errores
                if (result == FINISH_CONNECTION) {
                    log(LOG_ERROR, "Error!");
                    return ERROR; 
                }
            }
        }
    }
    return result; 
}


unsigned command_read(struct selector_key* key)
{
    Client * client = ATTACHMENT(key);
    logf(LOG_DEBUG, "command read loop %d", key->fd);

    buffer* rb = &client->socket_data->read_buffer;
    size_t len;
    uint8_t *rbPtr = buffer_write_ptr(rb, &len);
    ssize_t read_count = recv(key->fd, rbPtr, len, 0); // TODO: Ver flags?

    logf(LOG_DEBUG, "read %ld", read_count);
    if (!buffer_can_read(&client->socket_data->read_buffer) && read_count == 0) {
        return DONE;
    } else if (read_count < 0) {
        if (errno == EAGAIN) {
            return COMMAND_READ;
        }
        // Ver error
        return ERROR;
    }

    buffer_write_adv(rb, read_count);
    // rbPtr = buffer_read_ptr(rb, &len);
    
    int result = process_read_buffer(client);

    return result;
}

void command_write_arrival(const unsigned prev_state, const unsigned state, struct selector_key* key) {
    if(prev_state != state) {
        logf(LOG_DEBUG, "command write arrival! %d", key->fd);
        Client * client = ATTACHMENT(key);
        selector_set_interest(key->s, client->socket_data->fd, OP_WRITE);
    }
}

static void register_mail_fds(struct selector_key* key, Client* client) {
    register_fd(key, client->client_data->mail_fd, OP_READ, client);
    if (global_state.transformations_enabled) {
        register_fd(key, client->client_data->transf_to_pop3_fd, OP_READ, client);
        register_fd(key, client->client_data->pop3_to_transf_fd, OP_WRITE, client);
        // register_fd(key, client->client_data->pop3_to_transf_fd, OP_NOOP, client);
        log(LOG_DEBUG, "registered pipes");
    }
} 

static void mail_buffer_to_client_buffer(Client* client){
    ClientData* client_data = client->client_data;
    log(LOG_DEBUG, "Byte stuffing");
    size_t mail_len = 0;
    uint8_t * mbPtr = buffer_read_ptr(&(client_data->mail_buffer), &mail_len);

    uint8_t * newline;
    do {
        newline = memchr(mbPtr, '\n', mail_len);

        if (newline != NULL) {
            size_t offset = newline - mbPtr;
            ssize_t copied = buffer_ncopy(&(client->socket_data->write_buffer), mbPtr, offset); // Excluimos el \n para mantener el contexto
            mbPtr = buffer_read_adv(&(client_data->mail_buffer), copied);
            size_t write_capacity = buffer_get_write_len(&(client->socket_data->write_buffer));;
            mail_len -= copied;
            if (mail_len == 1 || write_capacity == 0) {
                    // Casos caos
                break;
            } else if (mail_len >= 2 && newline && newline[1] == '.') {
                if (write_capacity > 2) {
                    // Caso byte stuffing
                    buffer_write(&(client->socket_data->write_buffer), '\n');
                    buffer_write(&(client->socket_data->write_buffer), '.');
                    buffer_write(&(client->socket_data->write_buffer), '.');
                    mbPtr = buffer_read_adv(&(client_data->mail_buffer), 2);
                    mail_len -= 2;
                } else {
                    break;
                }
            } else {
                // Caso falsa alarma  
                buffer_write(&(client->socket_data->write_buffer), '\n');
                mbPtr = buffer_read_adv(&(client_data->mail_buffer), 1);
                mail_len -= 1;
            }
        } else { 
            // Caso normal
            ssize_t copied = buffer_ncopy(&(client->socket_data->write_buffer), mbPtr, mail_len);
            mbPtr = buffer_read_adv(&(client_data->mail_buffer), copied);
            mail_len -= copied;
        }
    } while(newline != NULL && buffer_can_write(&(client->socket_data->write_buffer)) && buffer_can_read(&(client_data->mail_buffer)));
}

static int mail_file_to_pipe(struct selector_key* key, Client* client) {
    log(LOG_DEBUG, "reading mail to pipe");
    ClientData * client_data = client->client_data;
    uint8_t buff[1024] = {0}; // TODO: Poner una variable estatica con tamaño maximo del pipe?
    ssize_t read_count = read(client_data->mail_fd, buff, 1024);
    logf(LOG_DEBUG, "Read %ld from mail", read_count);
    if (read_count > 0) {
        ssize_t written_count = write(client_data->pop3_to_transf_fd, buff, read_count);
        if (written_count < read_count) {
            if (written_count == -1) {
                if (errno != EAGAIN) {
                    perror("pipe write");
                    return ERROR;
                }
                written_count = 0;
            }
            lseek(client_data->mail_fd, -(read_count - written_count), SEEK_CUR); // TODO: PREGUNTAR SI ESTO ESTA BIEN!
            logf(LOG_DEBUG, "rolling back %ld", -(read_count - written_count));
            selector_set_interest(key->s, client_data->mail_fd, OP_NOOP);
            selector_set_interest(key->s, client_data->pop3_to_transf_fd, OP_WRITE);
        } else {
            selector_set_interest(key->s, client_data->mail_fd, OP_READ);
            selector_set_interest(key->s, client_data->pop3_to_transf_fd, OP_NOOP);
        }
    } else if(read_count == 0){
        client_data->current_mail = EMAIL_READ_DONE; 
        selector_unregister_fd(key->s, client_data->mail_fd);
        close(client_data->mail_fd);
        selector_unregister_fd(key->s, client_data->pop3_to_transf_fd);
        close(client_data->pop3_to_transf_fd);
        selector_set_interest(key->s, client_data->transf_to_pop3_fd, OP_READ);
        return COMMAND_READ;
    } else {
        return ERROR;
    }
    return COMMAND_WRITE;
}

static int processed_mail_to_mail_buffer(struct selector_key* key, Client* client, int pre_byte_stuffing_fd) {
    ClientData * client_data = client->client_data;
    log(LOG_DEBUG, "reading processed mail");

    size_t mail_len;
    uint8_t* mbPtr = buffer_write_ptr(&(client_data->mail_buffer), &mail_len);

    errno = 0;
    ssize_t read_count = read(pre_byte_stuffing_fd, mbPtr, mail_len);
    logf(LOG_DEBUG, "read processed%ld ", read_count);
    if (read_count < 0) {
        if (errno == EAGAIN) {
            // selector_set_interest(key->s, pre_byte_stuffing_fd, OP_NOOP);
            log(LOG_DEBUG, "EAGAIN");
            return COMMAND_WRITE;
        }
        perror("read");
        return ERROR;
    } else if (read_count == 0) {
        if (buffer_can_read(&(client_data->mail_buffer))) {
            log(LOG_DEBUG, "Quedaron cosas por limpiar en el buffer");
            size_t mail_len;
            uint8_t * mrPtr = buffer_read_ptr(&(client_data->mail_buffer), &mail_len);
            ssize_t copied = buffer_ncopy(&(client->socket_data->write_buffer), mrPtr, mail_len);
            buffer_read_adv(&(client_data->mail_buffer), copied);
            if (copied < (ssize_t) mail_len) {
            // TODO: Como podemos manejar esto???? 
                return COMMAND_WRITE;
            }
        }

        log(LOG_DEBUG, "Terminando...");
        selector_unregister_fd(key->s, pre_byte_stuffing_fd);
        client_data->current_mail = EMAIL_FINISHED;
        selector_set_interest(key->s, client->socket_data->fd, OP_WRITE);
    } else {
        if (!buffer_can_read(&(client_data->mail_buffer))) {
            log(LOG_DEBUG, "muting client socket, nothing to send");
            if (selector_set_interest(key->s, client->socket_data->fd, OP_NOOP) != SELECTOR_SUCCESS) {
                return ERROR;
            }
        }
    }
    buffer_write_adv(&(client_data->mail_buffer), read_count);
    return COMMAND_WRITE;
}

unsigned command_write(struct selector_key* key)
{
    Client * client = ATTACHMENT(key);
    ClientData * client_data = client->client_data;
    CommandState * command_state = &client->command_state;
    log(LOG_DEBUG, "\n")
    logf(LOG_DEBUG, "arrived to write r:%d s:%d m:%d rp:%d wp:%d", key->fd, client->socket_data->fd, client_data->mail_fd, client_data->transf_to_pop3_fd, client_data->pop3_to_transf_fd);
    
    int result_state = -1;
    if (command_state->command_index != -1) {
        log(LOG_DEBUG, "handling!!");
        result_state = available_commands[command_state->command_index].handler(client, command_state->arguments, command_state->argLen);
        if (result_state == REGISTER_PENDING) {
            result_state = COMMAND_WRITE;
            register_mail_fds(key, client);
        }
        log(LOG_DEBUG, "ended handling!!");
    }

    if (client_data->byte_stuffing && buffer_can_write(&(client->socket_data->write_buffer)) && buffer_can_read(&(client_data->mail_buffer))) {
        mail_buffer_to_client_buffer(client);
        selector_set_interest(key->s, client->socket_data->fd, OP_WRITE);
    }


    if (buffer_can_read(&(client->socket_data->write_buffer))) {
        size_t len = 0;
        ssize_t sent_count = 0;
        uint8_t* wbPtr = buffer_read_ptr(&(client->socket_data->write_buffer), &len);
        logf(LOG_DEBUG, "sending to client, can send %ld", len);

        //TODO: ver si puede fallar el send si no es su turno
        sent_count = send(client->socket_data->fd, wbPtr, len, MSG_NOSIGNAL);
        logf(LOG_DEBUG, "sent %ld", sent_count);
        if (sent_count == -1) {
            if (errno == EAGAIN) {
                log(LOG_DEBUG, "EAGAIN"); // TODO: Ver si esto esta bien
                return COMMAND_WRITE;
            }
            //  selector_set_interest(key->s, client->socket_data->fd, OP_NOOP);
            perror("send"); 
            return DONE;
        }
        buffer_read_adv(&(client->socket_data->write_buffer), sent_count);

        // Estadistica de bytes transferidos
        global_state.total_bytes_sent += sent_count;
    }

    if (client_data->current_mail != NO_EMAIL && client_data->current_mail != EMAIL_FINISHED) {
        log(LOG_DEBUG, "mail section");
        int pre_byte_stuffing_fd = client_data->pop3_to_transf_fd != -1 ? client_data->transf_to_pop3_fd : client_data->mail_fd;
        if (client_data->pop3_to_transf_fd != -1) {
            mail_file_to_pipe(key, client);
        }

        if (buffer_can_write(&client_data->mail_buffer)) {
            processed_mail_to_mail_buffer(key, client, pre_byte_stuffing_fd);
        }
    }

    //HANDLE INTERESTS

    if (client->command_state.finished && !buffer_can_read(&(client->socket_data->write_buffer))) {
        log(LOG_DEBUG, "command finished, going to command read");
        client->command_state.command_index = -1;
        client->command_state.argLen = 0;
        client->command_state.finished = false;
        // if (selector_set_interest(key->s, key->fd, OP_READ) != SELECTOR_SUCCESS) {
        //     return ERROR;
        // }
        result_state = process_read_buffer(client);
    }
    log(LOG_DEBUG, "exiting command write");
    return result_state != -1? result_state : COMMAND_WRITE; 
}


void done_arrival(const unsigned prev_state, const unsigned state, struct selector_key* key) {
    Client * client = ATTACHMENT(key);
    log(LOG_DEBUG, "I'm done");
    if (selector_unregister_fd(key->s, client->socket_data->fd) != SELECTOR_SUCCESS) {
        abort();
    }
    if (client->client_data->mail_fd != -1) {
        selector_unregister_fd(key->s, client->client_data->mail_fd);
        close(client->client_data->mail_fd);
    }
    if  (client->client_data->pop3_to_transf_fd != -1) {
        selector_unregister_fd(key->s, client->client_data->mail_fd);
        close(client->client_data->mail_fd);
    }
    if  (client->client_data->transf_to_pop3_fd != -1) {
        selector_unregister_fd(key->s, client->client_data->transf_to_pop3_fd);
        close(client->client_data->transf_to_pop3_fd);
    }
    remove_client(client);

    global_state.current_connections--;
}

unsigned error_write(struct selector_key* key) {
    log(LOG_DEBUG, "entered error");
    Client * client = ATTACHMENT(key);
    //ClientData * client_data = client->client_data;
    size_t len = 0;
    uint8_t* ptr = buffer_read_ptr(&(client->socket_data->write_buffer), &len);
    ssize_t sent_count = send(key->fd, ptr, len, MSG_NOSIGNAL);

    if (sent_count == -1) return ERROR;

    buffer_read_adv(&(client->socket_data->write_buffer), sent_count);

    // Estadistica de bytes transferidos
    global_state.total_bytes_sent += sent_count;

    if (!buffer_can_read(&(client->socket_data->write_buffer))) {
        // if (selector_set_interest(key->s, key->fd, OP_READ) != SELECTOR_SUCCESS) {
        //     return ERROR;
        // }
        // if (selector_set_interest(key->s, key->fd, OP_READ) != SELECTOR_SUCCESS) {
        //     return DONE;
        // }
        return COMMAND_READ;
    }
    return ERROR;
}