#include <stdbool.h>
#include "parser.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

static void cmd_action(parser_event *ret, const uint8_t c);
static void arg_action(parser_event *ret, const uint8_t c);
static void finish_action(parser_event *ret, const uint8_t c);


enum pop3_states {
    START,
    FIRST_LETTER_CMD,
    SECOND_LETTER_CMD,
    THIRD_LETTER_CMD,
    FOURTH_LETTER_CMD,
    ARGS,
    ENDING,
    ERROR
};


static const struct parser_state_transition ST_START [] = {
    { .when = ' ',      .dest = ERROR,                 .act = NULL},
    { .when = '\r',     .dest = ERROR,                 .act = NULL}, 
    { .when = ANY,      .dest = FIRST_LETTER_CMD,      .act = cmd_action}
};

static const struct parser_state_transition ST_FIRST_L [] = {
    { .when = ' ',      .dest = ERROR,                  .act = NULL  },
    { .when = '\r',     .dest = ERROR,                  .act = NULL  },
    { .when = ANY,      .dest = SECOND_LETTER_CMD,      .act = cmd_action  }
};

static const struct parser_state_transition ST_SECOND_L [] = {
    { .when = ' ',      .dest = ERROR,                 .act = NULL },
    { .when = '\r',     .dest = ERROR,                 .act = NULL },
    { .when = ANY,      .dest = THIRD_LETTER_CMD,      .act = cmd_action }
};

static const struct parser_state_transition ST_THIRD_L [] = {
    { .when = ' ',      .dest = ARGS,       .act = arg_action },
    { .when = '\r',     .dest = ENDING,     .act = NULL       },
    { .when = ANY,      .dest = ERROR,      .act = cmd_action }
};

static const struct parser_state_transition ST_FOURTH_L [] = {
    { .when = ' ',      .dest = ARGS,       .act = arg_action },
    { .when = '\r',     .dest = ENDING,     .act = NULL       },
    { .when = ANY,      .dest = ERROR,      .act = NULL }
};

static const struct parser_state_transition ST_ARGS [] = {
    { .when = '\r',     .dest = ENDING,     .act = NULL       },
    { .when = ANY,      .dest = ARGS,       .act = arg_action }
};

static const struct parser_state_transition ST_ENDING [] = {
    { .when = '\n',     .dest = START,      .act = finish_action },
    { .when = '\r',     .dest = ENDING,     .act = NULL          }, // TODO: Probar
    { .when = ANY,      .dest = ERROR,      .act = NULL    }
};

static const struct parser_state_transition ST_ERROR [] = {
    { .when = '\r',     .dest = ENDING,      .act = NULL },
    { .when = ANY,      .dest = ERROR,       .act = NULL }
};

static const struct parser_state_transition *states [] = {
    ST_START,
    ST_FIRST_L,
    ST_SECOND_L,
    ST_THIRD_L,
    ST_FOURTH_L,
    ST_ARGS,
    ST_ENDING,
    ST_ERROR
};

static const size_t states_n [] = {
    N(ST_START),
    N(ST_FIRST_L),
    N(ST_SECOND_L),
    N(ST_THIRD_L),
    N(ST_FOURTH_L),
    N(ST_ARGS),
    N(ST_ENDING),
    N(ST_ERROR)
};

const parser_definition pop3_parser_definition = {
    .states_count = N(states),
    .states = states,
    .states_n = states_n,
    .start_state = START    
};


static void cmd_action(parser_event *event, const uint8_t c) {
    if (event->command_length < COMMAND_LENGTH) {
        event->command[event->command_length++] = c;
    }
    // TODO: Handle errors
}

static void arg_action(parser_event *event, const uint8_t c) {
    if (event->args_length < ARGS_LENGTH) {
        event->args[event->args_length++] = c;
    }
    // TODO: Handle errors
}

static void finish_action(parser_event *ret, const uint8_t c) {
    ret->finished = true;
}



