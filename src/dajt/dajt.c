#include <string.h>
#include <strings.h>
#include <stdio.h>

#include "dajt.h"
#include "../buffer.h"
#include "../client.h"
#include "../responses.h"
#include "../global_state/globalstate.h"
#include "../logger/logger.h"

extern Args args;
extern GlobalState global_state;
// extern unsigned long total_bytes_sent;
// extern unsigned long current_connections;
// extern unsigned long total_connections;
// extern unsigned long total_errors;
// extern unsigned current_buffer_size;
// extern bool transformations_enabled;

#define MIN_BUFFER_SIZE 1 
#define MAX_BUFFER_SIZE 1 << 20 

// TODO: Mover
#define FINISH_CONNECTION true
#define CONTINUE_CONNECTION false

#define REGISTER_PENDING -2

static int auth_handler(Client * client, char* commandParameters, uint8_t parameters_length)
{
    //ClientData* client_data = client->client_data;
    for (int i = 0; i < (int)args.quantity_admins; i++) {
        // get user from the string commandParameters (the format is "user password")
        int pos = 0;
        while (pos < parameters_length && args.admins[i].name[pos] == commandParameters[pos] && args.admins[i].name[pos] != '\0') {
            pos++;
        }

        if (pos < parameters_length && args.admins[i].name[pos] == '\0' && commandParameters[pos] == ' ') {
            pos++;
            int passPos = 0; 
            while (pos + passPos < parameters_length && args.admins[i].pass[passPos] == commandParameters[pos + passPos] && args.admins[i].pass[passPos] != '\0') {
                passPos++;
            }
            if (pos + passPos == parameters_length && args.admins[i].pass[passPos] == '\0') {
                socket_buffer_write(client->socket_data, OKCRLF_DAJT, sizeof OKCRLF_DAJT - 1);
                client->user = &args.admins[i];
                client->command_state.finished = true; 
                client->state = TRANSACTION;
                return COMMAND_READ; 
            }
        }
    }
    client->command_state.finished = true; 
    socket_buffer_write(client->socket_data, ERR_INVALID_AUTH_DAJT, sizeof ERR_INVALID_AUTH_DAJT - 1);
    return COMMAND_WRITE; 
}

static int first_argument_to_int(Client* client, char* commandParameters)
{
    char* endptr;
    int num = -1, len;
    if (commandParameters != NULL) {
        num = strtol(commandParameters, &endptr, 10);
        if (num >= MIN_BUFFER_SIZE && num <= MAX_BUFFER_SIZE && *endptr == '\0' && endptr != commandParameters) {
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


// user token
static int buff_handler(Client * client, char* commandParameters, uint8_t parameters_length) {
    if (parameters_length == 0) {
        size_t len = buffer_get_write_len(&client->socket_data->write_buffer);
        char buffer[190] = {0};
        int buffLen = sprintf(buffer, GET_BUFFER_DAJT, global_state.current_buffer_size);
        if (buffLen <= (int) len) {
            client->command_state.finished = true;  
            socket_buffer_write(client->socket_data, buffer, buffLen);
            client->command_state.finished = true; 
        }
    } else {
        int arg = first_argument_to_int(client, commandParameters);
        if (arg < 0) {
            log(LOG_ERROR, "Error");
            return ERROR;
            // TODO: Manejar error 
        }
        if (socket_buffer_write(client->socket_data, SET_BUFFER_DAJT, sizeof SET_BUFFER_DAJT - 1)) {
            global_state.current_buffer_size = arg;
            client->command_state.finished = true; 
        }
    }
    return COMMAND_WRITE;
}

static int stat_handler(Client * client, char* commandParameters, uint8_t parameters_length) {
    size_t len = buffer_get_write_len(&client->socket_data->write_buffer);
    // TODO: Manejar tamaño del buffer
    char buffer[190] = {0};
    int buffLen = sprintf(buffer, GET_STATS_DAJT, global_state.total_bytes_sent, global_state.current_connections, global_state.total_connections, global_state.total_errors);
    if (buffLen <= (int) len) {
        client->command_state.finished = true;  
        socket_buffer_write(client->socket_data, buffer, buffLen);
    }
    return COMMAND_WRITE;
}

static int quit_handler(Client * client, char* commandParameters, uint8_t parameters_length) {
    socket_buffer_write(client->socket_data, OKCRLF_DAJT, sizeof OKCRLF_DAJT - 1);
    client->command_state.finished = true; 
    return DONE; 
}

static int ttra_handler(Client * client, char* commandParameters, uint8_t parameters_length) {
    global_state.transformations_enabled = !global_state.transformations_enabled;
    socket_buffer_write(client->socket_data, OKCRLF_DAJT, sizeof OKCRLF_DAJT - 1);
    client->command_state.finished = true; 
    return COMMAND_WRITE; 
}

static int tran_handler(Client * client, char* commandParameters, uint8_t parameters_length) {
    socket_buffer_write(client->socket_data, OKCRLF_DAJT, sizeof OKCRLF_DAJT - 1);
    if (parameters_length != 0) {
        set_transformation(commandParameters);
    } else {
        size_t len = 0; 
        uint8_t * wbuffer = buffer_write_ptr(&client->socket_data->write_buffer, &len);
        size_t initial_len = len; 
        char * arg;
        for (int i = 0; (arg = global_state.transformation_path[i]) != NULL; i++) {
            size_t arg_len = strlen(arg) + 1;
            if (len < arg_len + 1) {
                // TODO: Manejar error
                return ERROR;
            }
            len -= sprintf((char *)wbuffer, "%s ", arg);;
            wbuffer += arg_len;
        }
        if (len >= 2) {
            log(LOG_DEBUG, "Imprimiendo crlf");
            memcpy(wbuffer, CRLF, sizeof CRLF -1);
            buffer_write_adv(&client->socket_data->write_buffer, initial_len + 2 - len);
        } else {
            return ERROR;
        }
    }
    client->command_state.finished = true; 
    return COMMAND_WRITE; 
}

// AUTH user token

CommandDescription dajt_available_commands[] = {
    { .name = "AUTH", .handler = auth_handler, .valid_states = AUTHORIZATION },
    { .name = "TTRA", .handler = ttra_handler, .valid_states = TRANSACTION },
    { .name = "TRAN", .handler = tran_handler, .valid_states = TRANSACTION },
    { .name = "BUFF", .handler = buff_handler, .valid_states = TRANSACTION },
    { .name = "STAT", .handler = stat_handler, .valid_states = TRANSACTION },
    { .name = "IDLE", .handler = NULL, .valid_states = TRANSACTION },
    { .name = "QUIT", .handler = quit_handler, .valid_states = AUTHORIZATION | TRANSACTION },
};


static int consume_pop3_buffer(parser* pop3parser, SocketData* socket_data)
{
    for (; buffer_can_read(&socket_data->read_buffer);) {
        const uint8_t c = socket_data_read(socket_data);
        const parser_event* event = parser_feed(pop3parser, c);
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

// TODO: Es igual a pop3, modularizar?
static bool process_event(parser_event* event, Client* client)
{
    // if (event->command_length + event->args_length > MAX_COMMAND_LENGTH) {
    //     socket_buffer_write(client_data->socket_data, ERR_COMMAND_TOO_LONG, sizeof ERR_COMMAND_TOO_LONG - 1);
    //     return FINISH_CONNECTION;
    // }

    client->last_activity_time = time(NULL);
    for (int i = 0; i < (int)N(dajt_available_commands); i++) {
        if (event->command_length == 4 && strncasecmp(event->command, dajt_available_commands[i].name, event->command_length) == 0) {
            if ((client->state & dajt_available_commands[i].valid_states) == 0) {
                break;
            }
            client->command_state.command_index = i;
            client->command_state.argLen = event->args_length;
            strcpy(client->command_state.arguments, event->args);
            client->command_state.finished = false;
            return CONTINUE_CONNECTION;
        }
    }

    socket_buffer_write(client->socket_data, UNKNOWN_COMMAND_DAJT, sizeof UNKNOWN_COMMAND_DAJT - 1);
    client->command_state.command_index = -1;
    client->command_state.argLen = 0;
    client->command_state.finished = false;
    return FINISH_CONNECTION; 
}

void welcome_dajt_init(const unsigned prev_state, const unsigned state, struct selector_key* key){
    buffer* wb = &(ATTACHMENT(key)->socket_data->write_buffer);

    size_t len = 0;
    uint8_t* ptr = buffer_write_ptr(wb, &len);
    strncpy((char*)ptr, SERVER_READY_DAJT, len);
    buffer_write_adv(wb, sizeof SERVER_READY_DAJT - 1);

}
unsigned welcome_dajt_write(struct selector_key* key){
    buffer* wb = &(ATTACHMENT(key)->socket_data->write_buffer);
    size_t len = 0;
    uint8_t* wbPtr = buffer_read_ptr(wb, &len);
    ssize_t sent_count = send(key->fd, wbPtr, len, MSG_NOSIGNAL);

    if (sent_count == -1) {
        return ERROR;
    }

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

// void welcome_dajt_close(const unsigned state, struct selector_key* key){

// }

unsigned command_dajt_read(struct selector_key* key){
    Client * client = ATTACHMENT(key);

    buffer* rb = &client->socket_data->read_buffer;
    size_t len;
    uint8_t *rbPtr = buffer_write_ptr(rb, &len);
    ssize_t read_count = recv(key->fd, rbPtr, len, 0); // TODO: Ver flags?

    if (read_count <= 0) {
        return DONE;
    }

    buffer_write_adv(rb, read_count);
    rbPtr = buffer_read_ptr(rb, &len);

    int result;

    while (buffer_can_read(rb)) {
        // if (difftime(time(NULL), client_data->last_activity_time) > INACTIVITY_TIMEOUT) {
        //     socket_buffer_write(client_data->socket_data, ERR_INACTIVITY_TIMEOUT, sizeof ERR_INACTIVITY_TIMEOUT - 1);
        //     break;
        // }
        
        logf(LOG_DEBUG, "buffer_can_read: %d", buffer_can_read(&client->socket_data->read_buffer));

        if (consume_pop3_buffer(client->pop3parser, client->socket_data) == 0) {

            parser_event* event = parser_pop_event(client->pop3parser);
            if (event != NULL) {
                result = process_event(event, client);
                free(event);
                parser_reset(client->pop3parser);
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
    return COMMAND_READ;
}
void command_dajt_read_arrival(const unsigned prev_state, const unsigned state, struct selector_key* key){
    if(prev_state != state) {
        selector_set_interest(key->s, key->fd, OP_READ);
    }
}

unsigned command_dajt_write(struct selector_key* key){
    log(LOG_DEBUG, "Arrived to write DAJT");
    Client * client = ATTACHMENT(key);
    // ClientData * client_data = client->client_data;
    CommandState * command_state = &client->command_state;
    
    int result_state = -1;
    if (command_state->command_index != -1) {
        result_state = dajt_available_commands[command_state->command_index].handler(client, command_state->arguments, command_state->argLen);
    }

    size_t len = 0;
    ssize_t sent_count = 0;
    uint8_t* wbPtr = buffer_read_ptr(&(client->socket_data->write_buffer), &len);
    if (len > 0) {
        log(LOG_DEBUG, "Sending to client");
        sent_count = send(client->socket_data->fd, wbPtr, len, MSG_NOSIGNAL);
        if (sent_count == -1) return ERROR;
        buffer_read_adv(&(client->socket_data->write_buffer), sent_count);
    }

    if (client->command_state.finished && !buffer_can_read(&(client->socket_data->write_buffer))) {
        client->command_state.command_index = -1;
        client->command_state.argLen = 0;
        client->command_state.finished = false;
        // if (selector_set_interest(key->s, key->fd, OP_READ) != SELECTOR_SUCCESS) {
        //     return ERROR;
        // }
        return COMMAND_READ;
    }
    log(LOG_DEBUG, "Exiting command write");
    return result_state != -1? result_state : COMMAND_WRITE; 
}

void command_dajt_write_arrival(const unsigned prev_state, const unsigned state, struct selector_key* key){
    if(prev_state != state) {
        selector_set_interest(key->s, key->fd, OP_WRITE);
    }
}

void done_dajt_arrival(const unsigned prev_state, const unsigned state, struct selector_key* key){
    log(LOG_DEBUG, "I'm d(ajt)one");
    free_client(ATTACHMENT(key));
    if (selector_unregister_fd(key->s, key->fd) != SELECTOR_SUCCESS) {
        // TODO: Ver si esto esta ok
        abort();
    }
}

unsigned error_dajt_write(struct selector_key* key){
    Client * client = ATTACHMENT(key);
    //ClientData * client_data = client->client_data;
    size_t len = 0;
    uint8_t* ptr = buffer_read_ptr(&(client->socket_data->write_buffer), &len);
    ssize_t sent_count = send(key->fd, ptr, len, MSG_NOSIGNAL);

    if (sent_count == -1) return ERROR;

    buffer_read_adv(&(client->socket_data->write_buffer), sent_count);
    if (!buffer_can_read(&(client->socket_data->write_buffer))) {
        return DONE;
    }
    return ERROR;
}