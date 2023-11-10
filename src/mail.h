#ifndef MAIL_DATA_H
#define MAIL_DATA_H

#include <stdint.h>
#include <dirent.h>
#include <stdlib.h>

typedef struct MailInfo {
    char * filename;
    size_t size;
} MailInfo;

MailInfo * get_mail_info_list(const char * directory_path, int * size);

#endif