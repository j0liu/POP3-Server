#include "globalstate.h"
#include <string.h>

GlobalState global_state = {
    .current_connections = 0,
    .total_connections = 0,
    .total_bytes_sent = 0,
    .total_errors = 0,
    .transformation_path = NULL, 
    .transformation_path_len = 0,
    .transformations_enabled = false,
    .current_buffer_size = 2048 
};

void set_transformation(char *raw_transformation) {
    size_t len = strlen(raw_transformation) + 1;
    global_state.transformation_path_len = len;

    if (global_state.transformation_path == NULL) {
        global_state.transformation_path = malloc(len * sizeof(char));
    } else {
        global_state.transformation_path = realloc(global_state.transformation_path, len * sizeof(char));
    }
    strcpy(global_state.transformation_path, raw_transformation);
}
