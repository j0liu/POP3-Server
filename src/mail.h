#ifndef MAIL_DATA_H
#define MAIL_DATA_H

#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define MAX_PATH_LENGTH 1024
typedef struct MailInfo {
    char* filename;
    size_t size;
    bool deleted;
    int file_descriptor;
} MailInfo;

MailInfo* get_mail_info_list(const char* directory_path, int* size, const char* username);
void free_mail_info_list(MailInfo* mail_info_list, int size);

#endif