#ifndef MAIL_DATA_H
#define MAIL_DATA_H

#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct MailInfo {
    char* filename;
    size_t size;
    bool deleted;
} MailInfo;

MailInfo* get_mail_info_list(const char* directory_path, int* size, const char* username);
void free_mail_info_list(MailInfo* mail_info_list, int size);

#endif