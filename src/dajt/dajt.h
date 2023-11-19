#ifndef DAJT_H
#define DAJT_H

#include "../selector.h"

void welcome_dajt_init(const unsigned prev_state, const unsigned state, struct selector_key* key);
// void welcome_dajt_close(const unsigned state, struct selector_key* key);
unsigned welcome_dajt_write(struct selector_key* key);
unsigned command_dajt_read(struct selector_key* key);
void command_dajt_read_arrival(const unsigned prev_state, const unsigned state, struct selector_key* key);
unsigned command_dajt_write(struct selector_key* key);
void command_dajt_write_arrival(const unsigned prev_state, const unsigned state, struct selector_key* key);
void done_dajt_arrival(const unsigned prev_state, const unsigned state, struct selector_key* key);
unsigned error_dajt_write(struct selector_key* key);

#endif