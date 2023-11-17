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
typedef struct ClientData {
    SocketData* socket_data;
    uint8_t state;
    struct users* user;
    MailInfo* mail_info_list;
    int mail_count;
    int mail_count_not_deleted;
    time_t last_activity_time;
} ClientData;

typedef struct CommandDescription {
    char* name;
    bool (*handler)(ClientData* client_data, char* commandParameters, uint8_t parameters_length);
    uint8_t valid_states;
} CommandDescription;

typedef struct Connection {
    int fd;
    socklen_t addrlen;
    struct sockaddr_in6 addr;
} Connection;

typedef struct Client {
    Connection* connection;
    struct state_machine stm;
    ClientData* client_data;
    parser* pop3parser;
    // CommandState* command_st;
} Client;

#define ATTACHMENT(key) ((Client*)(key)->data)

ClientData* initialize_client_data(SocketData* socket_data);
void free_client_data(ClientData* client_data);

Client* new_client(int client_fd, struct sockaddr_in6* client_addr, socklen_t client_addr_len);
void free_client(Client* client);

#endif