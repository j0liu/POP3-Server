#include "globalstate.h"
#include <string.h>

GlobalState global_state = {
    .current_connections = 0,
    .total_connections = 0,
    .total_bytes_sent = 0,
    .total_errors = 0,
    .transformation_path = NULL, 
    .transformations_enabled = false,
    .current_buffer_size = 1024
};

static void free_transformation_path() {
    if (global_state.transformation_path == NULL) return;

    for (int i = 0; global_state.transformation_path[i] != NULL; i++)
        free(global_state.transformation_path[i]);
    free(global_state.transformation_path);
}

void set_transformation(char *raw_transformation) {
    free_transformation_path();

    char *token = strtok(raw_transformation, " ");

    int initial_size = 1;

    global_state.transformation_path = malloc(initial_size * sizeof(char*));

    int i = 0;

    while (token != NULL) {
        if (i >= initial_size) {
            initial_size *= 2;
            global_state.transformation_path = realloc(global_state.transformation_path, initial_size * sizeof(char*));
        }
        int len = strlen(token) + 1;
        global_state.transformation_path[i] = malloc(len * sizeof(char));
        strcpy(global_state.transformation_path[i], token);
        token = strtok(NULL, " ");
        i++;
    }
    global_state.transformation_path = realloc(global_state.transformation_path, (i + 1) * sizeof(char*));
    global_state.transformation_path[i] = NULL;
}
