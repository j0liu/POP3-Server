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

#define EMAIL_READ_DONE -2
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
    MailInfo* mail_info_list;
    int list_current_mail;
    int mail_count;
    int mail_count_not_deleted;
    int mail_fd; 
    int current_mail;
    buffer mail_buffer;
    bool byte_stuffing;
    int pop3_to_transf_fd;
    int transf_to_pop3_fd;
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
    parser* command_parser;
    
    SocketData* socket_data;
    uint8_t state;
    struct User* user;
    time_t last_activity_time;

    // TODO: Ver si es mejor un CommandState * 
    CommandState command_state;
};

#define ATTACHMENT(key) ((Client*)(key)->data)

void free_client_data(ClientData* client_data);

Client* new_client(int client_fd, struct sockaddr_in6* client_addr, socklen_t client_addr_len, bool pop3);
void free_client(Client* client);

#endif