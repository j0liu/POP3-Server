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

#define MIN_BUFFER_SIZE 1 << 7 
#define MAX_BUFFER_SIZE 1 << 20 

#define REGISTER_PENDING -2
#define STAT_TOTAL_BYTES 'b'
#define STAT_CURRENT_CONNECTIONS 'c'
#define STAT_TOTAL_CONNECTIONS 't'

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
    // client->command_state.finished = true; 
    socket_buffer_write(client->socket_data, ERR_INVALID_AUTH_DAJT, sizeof ERR_INVALID_AUTH_DAJT - 1);
    return ERROR;
}

static int first_argument_to_int(Client* client, char* commandParameters)
{
    char* endptr;
    int num = -1, len = 0;
    if (commandParameters != NULL) {
        num = strtol(commandParameters, &endptr, 10);
        if (num >= MIN_BUFFER_SIZE && num <= MAX_BUFFER_SIZE && *endptr == '\0' && endptr != commandParameters) {
            return num;
        }
    }
    char buff[64] = { 0 }; // TODO: Improve
    if (num <= 0 || endptr == commandParameters) {
        memcpy(buff, ERR_INVALID_NUMBER_DAJT, sizeof ERR_INVALID_NUMBER_DAJT - 1);
        len = sizeof ERR_INVALID_NUMBER_DAJT - 1;
    } else if (*endptr != '\0') {
        memcpy(buff, ERR_NOISE_DAJT, sizeof ERR_NOISE_DAJT - 1);
        len = sizeof ERR_NOISE_DAJT - 1;
    } else {
        memcpy(buff, ERR_INVALID_BUFFER_DAJT, sizeof ERR_INVALID_BUFFER_DAJT - 1);
        len = sizeof ERR_INVALID_BUFFER_DAJT - 1;
    }
    socket_buffer_write(client->socket_data, buff, len);
    return -1;
}


static int buff_handler(Client * client, char* commandParameters, uint8_t parameters_length) {
    if (parameters_length == 0) {
        size_t len = buffer_get_write_len(&client->socket_data->write_buffer);
        char buffer[190] = {0};
        int buffLen = sprintf(buffer, OK_NUMBER_DAJT, global_state.current_buffer_size);
        if (buffLen <= (int) len) {
            socket_buffer_write(client->socket_data, buffer, buffLen);
            client->command_state.finished = true; 
        }
    } else {
        int arg = first_argument_to_int(client, commandParameters);
        if (arg < 0) {
            log(LOG_ERROR, "Error");
            return ERROR;
            // TODO: Manejar error?
        }
        if (socket_buffer_write(client->socket_data, OKCRLF_DAJT, sizeof OKCRLF_DAJT - 1)) {
            global_state.current_buffer_size = arg;
            client->command_state.finished = true; 
        }
    }
    return COMMAND_WRITE;
}

static int stat_handler(Client * client, char* commandParameters, uint8_t parameters_length) {
    if (parameters_length == 0 || parameters_length > 1) {
        socket_buffer_write(client->socket_data, ERR_INVALID_STAT_DAJT, sizeof ERR_INVALID_STAT_DAJT - 1);
        return ERROR;
    }
    size_t len = buffer_get_write_len(&client->socket_data->write_buffer);
    // TODO: Manejar tamaño del buffer
    int result;
    switch (commandParameters[0])
    {
    case STAT_TOTAL_BYTES:
        result = global_state.total_bytes_sent;
        break;
    case STAT_CURRENT_CONNECTIONS:
        result = global_state.current_connections;
        break;
    case STAT_TOTAL_CONNECTIONS:
        result = global_state.total_connections;
        break;
    default:
        socket_buffer_write(client->socket_data, ERR_INVALID_STAT_DAJT, sizeof ERR_INVALID_STAT_DAJT - 1);
        return ERROR;
    }
    char buffer[190] = {0};
    int buffLen = sprintf(buffer, OK_NUMBER_DAJT, result);
    if (buffLen <= (int) len) {
        client->command_state.finished = true;  
        socket_buffer_write(client->socket_data, buffer, buffLen);
    }
    return COMMAND_WRITE;
}

static int quit_handler(Client * client, char* commandParameters, uint8_t parameters_length) {
    socket_buffer_write(client->socket_data, OKCRLF_DAJT, sizeof OKCRLF_DAJT - 1);
    client->exiting = true;
    client->command_state.finished = true;
    return COMMAND_WRITE; 
}

static int ttra_handler(Client * client, char* commandParameters, uint8_t parameters_length) {
    if (parameters_length == 0) {
        char buffer[190] = {0};
        int buffLen = sprintf(buffer, OK_NUMBER_DAJT, global_state.transformations_enabled);
        if (buffLen <= (int) buffer_get_write_len(&client->socket_data->write_buffer)) {
            socket_buffer_write(client->socket_data, buffer, buffLen);
            client->command_state.finished = true; 
        }
        return COMMAND_WRITE;
    }
    if (parameters_length > 1 || (commandParameters[0] != '0' && commandParameters[0] != '1')) {
        socket_buffer_write(client->socket_data, ERR_INVALID_TTRA_DAJT, sizeof ERR_INVALID_TTRA_DAJT - 1);
        return ERROR;
    }
    if (global_state.transformation_path == NULL) {
        log(LOG_ERROR, "no transformations");
        socket_buffer_write(client->socket_data, ERR_NO_TRANSFORMATIONS_SET_DAJT, sizeof ERR_NO_TRANSFORMATIONS_SET_DAJT - 1);
        return ERROR;
    }

    global_state.transformations_enabled = commandParameters[0] == '1'; 
    socket_buffer_write(client->socket_data, OKCRLF_DAJT, sizeof OKCRLF_DAJT - 1);
    client->command_state.finished = true; 
    return COMMAND_WRITE; 
}

static int tran_handler(Client * client, char* commandParameters, uint8_t parameters_length) {
    socket_buffer_write(client->socket_data, OKCRLF_DAJT, sizeof OKCRLF_DAJT - 1);
    if (parameters_length != 0) {
        set_transformation(commandParameters);
    } else if (global_state.transformation_path != NULL) {
        size_t len = 0; 
        uint8_t * wbuffer = buffer_write_ptr(&client->socket_data->write_buffer, &len);
        size_t written_count = sprintf((char *)wbuffer, "%d: %s ", global_state.transformations_enabled, global_state.transformation_path);
        buffer_write_adv(&client->socket_data->write_buffer, written_count);
        // TODO: Garantizar tamaño minimo del buffer
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
    { .name = "QUIT", .handler = quit_handler, .valid_states = AUTHORIZATION | TRANSACTION },
};


static int consume_dajt_buffer(parser* dajtParser, SocketData* socket_data)
{
    for (; buffer_can_read(&socket_data->read_buffer);) {
        const uint8_t c = socket_data_read(socket_data);
        const parser_event* event = parser_feed(dajtParser, c);
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
    log(LOG_INFO, NEW_DAJT_CONNECTION);
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

static int process_read_buffer(Client* client) {
    int result = COMMAND_READ;
    buffer* rb = &client->socket_data->read_buffer;
    while (buffer_can_read(rb)) {
        logf(LOG_DEBUG, "buffer_can_read: %d", buffer_can_read(&client->socket_data->read_buffer));

        if (consume_dajt_buffer(client->command_parser, client->socket_data) == 0) {

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

    int result = process_read_buffer(client);

    return result;
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
    if (command_state->command_index != -1 && !command_state->finished) {
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
        if (client->exiting)
            return DONE;
        client->command_state.command_index = -1;
        client->command_state.argLen = 0;
        client->command_state.finished = false;
        // if (selector_set_interest(key->s, key->fd, OP_READ) != SELECTOR_SUCCESS) {
        //     return ERROR;
        // }
        result_state = process_read_buffer(client);
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
    Client * client = ATTACHMENT(key);
    log(LOG_DEBUG, "I'm d(ajt)one");
    if (selector_unregister_fd(key->s, client->socket_data->fd) != SELECTOR_SUCCESS) {
        abort();
    }
    remove_client(ATTACHMENT(key));
}

unsigned error_dajt_write(struct selector_key* key){
    Client * client = ATTACHMENT(key);
    //ClientData * client_data = client->client_data;
    size_t len = 0;
    uint8_t* ptr = buffer_read_ptr(&(client->socket_data->write_buffer), &len);
    ssize_t sent_count = send(key->fd, ptr, len, MSG_NOSIGNAL);

    if (sent_count < (ssize_t) len) return ERROR;

    buffer_read_adv(&(client->socket_data->write_buffer), sent_count);
    if (!buffer_can_read(&(client->socket_data->write_buffer))) {
        return DONE;
    }
    return ERROR;
}