#include "mail.h"
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

#define INITIAL_SIZE 20

MailInfo* get_mail_info_list(const char* directory_path, int* size)
{
    DIR* dir = opendir(directory_path);

    if (dir == NULL)
        return NULL; // TODO: Agregar mas validaciones
    int capacity = INITIAL_SIZE;
    *size = 0;
    MailInfo* mail_info_list = malloc(sizeof(MailInfo) * capacity);

    struct dirent* entry;

    const int directory_path_len = strlen(directory_path);

    while ((entry = readdir(dir)) != NULL) {

        // TODO: Ver si esto sirve
        if (entry->d_name[0] == '.')
            continue;
        char* file_name = entry->d_name;
        int name_len = strlen(file_name);
        char* file_path = malloc(directory_path_len + name_len + 2);
        strcpy(file_path, directory_path);
        strcat(file_path, "/");
        strcat(file_path, file_name);
        // file_path[directory_path_len + name_len + 1] = '\0';
        struct stat file_info;
        stat(file_path, &file_info);

        if (S_ISREG(file_info.st_mode)) {
            if (*size == capacity) {
                capacity *= 2;
                mail_info_list = realloc(mail_info_list, sizeof(MailInfo) * capacity);
            }
            mail_info_list[*size].filename = file_path;
            mail_info_list[*size].size = file_info.st_size;
            (*size)++;
        }
    }

    closedir(dir);

    mail_info_list = realloc(mail_info_list, sizeof(MailInfo) * (*size));

    return mail_info_list;
}