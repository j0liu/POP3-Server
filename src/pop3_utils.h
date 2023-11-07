typedef enum command_id {
    CAPA,
    USER,
    PASS,
    STAT,
    LIST,
    RETR,
    DELE,
    NOOP,
    RSET,
    QUIT
    // APOP,
    // TOP,
    // UIDL
} command_id;

typedef struct command_description {
    command_id id;
    char * name;
    void (*handler)(buffer * serverBuffer, const int fd);
} command_description;