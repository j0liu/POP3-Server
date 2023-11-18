#ifndef CLIENT_H
#define CLIENT_H

#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>

#include "args.h"
#include "mail.h"
#include "protocols.h"
#include "socket_data.h"
#include "stm/stm.h"
#include "parser/parser.h"

// TODO: Change ClientData for DAJT protocol

#define ARG_MAX_LENGTH 255

#define NO_EMAIL -1
#define EMAIL_FINISHED 0

typedef struct Client Client;

typedef struct CommandState {
    int command_index;
    char arguments[ARG_MAX_LENGTH];
    int argLen;
    bool finished;
} CommandState;

typedef struct ClientData {
    SocketData* socket_data;
    uint8_t state;
    struct users* user;
    MailInfo* mail_info_list;
    int mail_count;
    int mail_count_not_deleted;
    time_t last_activity_time;
    CommandState command_state;
    int mail_fd; 
    int current_mail;
    buffer mail_buffer;
    bool byte_stuffing;
} ClientData;

typedef struct CommandDescription {
    char* name;
    int (*handler)(Client* client, char* commandParameters, uint8_t parameters_length);
    uint8_t valid_states;
} CommandDescription;

typedef struct Connection {
    int fd;
    socklen_t addrlen;
    struct sockaddr_in6 addr;
} Connection;

struct Client {
    Connection* connection;
    struct state_machine stm;
    ClientData* client_data;
    parser* pop3parser;

    // CommandState* command_st;
};

#define ATTACHMENT(key) ((Client*)(key)->data)

ClientData* initialize_client_data(SocketData* socket_data);
void free_client_data(ClientData* client_data);

Client* new_client(int client_fd, struct sockaddr_in6* client_addr, socklen_t client_addr_len);
void free_client(Client* client);

#endif