#ifndef GLOBALSTATE_H
#define GLOBALSTATE_H

#include <stdlib.h>
#include <stdbool.h>

typedef struct GlobalState {
    unsigned long total_connections;
    unsigned long current_connections;
    unsigned long total_bytes_sent;
    unsigned long total_errors;
    bool transformations_enabled;
    char * transformation_path;
    size_t transformation_path_len;
    unsigned current_buffer_size;
} GlobalState;

void set_transformation(char *raw_transformation);
#endif