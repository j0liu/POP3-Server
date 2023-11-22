#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include <stdio.h> //TODO: quitar

/* CDT del parser */
struct parser {
    /** definición de estados */
    const struct parser_definition* def;

    /* estado actual */
    unsigned state;

    parser_event* eventToReturn;
};

void parser_destroy(struct parser* p)
{
    if (p != NULL) {
        if (p->eventToReturn != NULL) free(p->eventToReturn);
        free(p);
    }
}

struct parser* parser_init(const struct parser_definition* def)
{
    struct parser* newParser = malloc(sizeof(*newParser));
    if (newParser != NULL) {
        memset(newParser, 0, sizeof(*newParser));
        newParser->def = def;
        newParser->state = def->start_state;
        newParser->eventToReturn = calloc(1, sizeof(parser_event));
    }
    return newParser;
}

void parser_reset(struct parser* p)
{
    p->state = p->def->start_state;
}

const struct parser_event* parser_feed(struct parser* p, const uint8_t c)
{
    const struct parser_state_transition* state = p->def->states[p->state];
    const size_t n = p->def->states_n[p->state];
    bool matched = false;

    for (unsigned i = 0; i < n; i++) {
        const int when = state[i].when;
        if (state[i].when <= 0xFF) {
            matched = (c == when);
        } else if (state[i].when == (int)ANY) {
            matched = true;
            // } else if(state[i].when > 0xFF) {
            //     matched = (type & when);
        } else {
            matched = false;
        }

        if (matched) {
            if (state[i].act != NULL) {
                state[i].act(p->eventToReturn, c);
            }
            p->state = state[i].dest;
            break;
        }
    }
    return p->eventToReturn;
}

parser_event* parser_pop_event(parser* p)
{
    parser_event* ret = p->eventToReturn;
    p->eventToReturn = calloc(1, sizeof(parser_event));
    return ret;
}