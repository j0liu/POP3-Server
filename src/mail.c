#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE
#endif

#include "mail.h"
#include <dirent.h>
#include <libgen.h> // for dirnam
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include "logger/logger.h"

int is_regular_file(const char* path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

int count_mails(const char* directory_path)
{
    DIR* dir = opendir(directory_path);
    if (dir == NULL)
        return 0;

    int count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            count++;
        }
    }

    closedir(dir);
    return count;
}

void free_mail_info_list(MailInfo* mail_info_list, int size)
{
    for (int i = 0; i < size; i++) {
        free(mail_info_list[i].filename); // Free the filename string
    }
    free(mail_info_list); // Free the array itself
}

MailInfo* get_mail_info_list(const char* directory_path, int* size, const char* username)
{
    char user_mail_path[MAX_PATH_LENGTH];

    if (directory_path[0] == '/') {
        snprintf(user_mail_path, MAX_PATH_LENGTH, "%s/%s", directory_path, username);
    } else {
        char *file = strdup(__FILE__);
        char* base_path = dirname(dirname(file)); // Two levels up from this file
        snprintf(user_mail_path, MAX_PATH_LENGTH, "%s/%s/%s", base_path, directory_path, username);
        free(file);
    }

    char cur_path[PATH_MAX], new_path[PATH_MAX];
    snprintf(cur_path, PATH_MAX, "%s/cur", user_mail_path);
    snprintf(new_path, PATH_MAX, "%s/new", user_mail_path);
    int counts[2] = { count_mails(cur_path), count_mails(new_path) };

    int total_count = counts[0] + counts[1];
    *size = total_count;

    if (!total_count) {
        return NULL;
    }

    MailInfo* mail_info_list = malloc(sizeof(MailInfo) * total_count);

    DIR* dir;
    struct dirent* entry;
    struct stat file_stat;
    int index = 0;

    // Process 'cur' and 'new' directories
    const char* directories[] = { cur_path, new_path };
    for (int i = 0; i < 2; ++i) {
        if (counts[i] == 0)
            continue;
        dir = opendir(directories[i]);
        if (dir == NULL) {
            logf(LOG_ERROR, "Error opening directory %s", directories[i])
            total_count -= counts[i];
            continue; // Si hay un error, no se cargaran archivos de ese directorio
        }
        while ((entry = readdir(dir)) != NULL) {
            char filepath[MAX_PATH_LENGTH];
            snprintf(filepath, MAX_PATH_LENGTH, "%s/%s", directories[i], entry->d_name);

            if (stat(filepath, &file_stat) == 0 && is_regular_file(filepath)) {

                // Allocate and copy filename
                mail_info_list[index].filename = malloc(strlen(filepath) + 1);
                strcpy(mail_info_list[index].filename, filepath);
                mail_info_list[index].size = file_stat.st_size;
                mail_info_list[index].deleted = false;
                index++;
            }
        }
        closedir(dir);
    }

    *size = index;

    return mail_info_list;
}
