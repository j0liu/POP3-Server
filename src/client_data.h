#ifndef CLIENT_DATA_H
#define CLIENT_DATA_H
#include "args.h"
#include "mail.h"
#include "socket_data.h"
#include <time.h>

#define AUTHORIZATION ((uint8_t)1)
#define TRANSACTION ((uint8_t)2)
#define UPDATE ((uint8_t)4)
#define INACTIVITY_TIMEOUT 600

typedef struct ClientData {
    SocketData* socket_data;
    uint8_t state;
    struct users* user;
    MailInfo* mail_info_list;
    int mail_count;
    int mail_count_not_deleted;
    time_t last_activity_time;
} ClientData;

ClientData* initialize_client_data(SocketData* socket_data);
void free_client_data(ClientData* client_data);

#endif