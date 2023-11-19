#ifndef POP3_H
#define POP3_H

#include "selector.h"

#define CRLF "\r\n"
#define OK "+OK"
#define OKCRLF (OK CRLF)
#define ERR "-ERR"
#define SERVER_READY OK " POP3 Party Started" CRLF
#define CAPA_MSG_AUTHORIZATION (OK CRLF "CAPA" CRLF "USER" CRLF "PIPELINING" CRLF "." CRLF)
#define CAPA_MSG_TRANSACTION (OK CRLF "CAPA" CRLF "PIPELINING" CRLF "." CRLF)
#define NO_USERNAME_GIVEN (ERR " No username given." CRLF)
#define NO_MSG_NUMBER_GIVEN (ERR " Message number required." CRLF)
#define UNKNOWN_COMMAND (ERR " Unknown command." CRLF)
#define LOGGING_IN (OK " Logged in." CRLF)
#define LOGGING_OUT (OK " Logging out." CRLF)
#define AUTH_FAILED (ERR " [AUTH] Authentication failed." CRLF)
#define DISCONNECTED_FOR_INACTIVITY (ERR " Disconnected for inactivity." CRLF)
#define ERR_NOISE (ERR " Noise after message number: %s" CRLF)
#define ERR_INVALID_NUMBER (ERR " Invalid message number: %s" CRLF)
#define ERR_INACTIVITY_TIMEOUT (ERR " Disconnected for inactivity." CRLF)
#define ERR_LOCKED_MAILDROP (ERR " Unable to lock maildrop." CRLF)
#define ERR_COMMAND_TOO_LONG (ERR " Command too long." CRLF)
#define OK_OCTETS (OK " %ld octets" CRLF)
#define TERMINATION ("." CRLF)
// LIST
#define OK_LIST (OK " %d messages:" CRLF)
#define NO_MESSAGE_LIST (ERR " There's no message %d." CRLF)
#define INVALID_NUMBER_LIST (ERR " Invalid message number: %d" CRLF)
// RETR
#define NO_SUCH_MESSAGE (ERR " No such message" CRLF)
#define RETR_TERMINATION (CRLF "." CRLF)
// DELE
#define OK_DELE (OK " Marked to be deleted." CRLF)
#define MESSAGE_IS_DELETED (ERR " Message is deleted." CRLF)
// QUIT
#define NOT_REMOVED (ERR " Some messages may not have been deleted." CRLF)
#define OK_QUIT_NO_AUTH (OK " POP3 Party over" CRLF)
#define OK_QUIT (OK " POP3 Party over (%d messages left)" CRLF)
#define OK_QUIT_EMPTY (OK " POP3 Party over (maildrop empty)" CRLF)

void welcome_init(const unsigned prev_state, const unsigned state, struct selector_key* key);
void welcome_close(const unsigned state, struct selector_key* key);
unsigned welcome_write(struct selector_key* key);
unsigned command_read(struct selector_key* key);
void command_read_arrival(const unsigned prev_state, const unsigned state, struct selector_key* key);
unsigned command_write(struct selector_key* key);
void command_write_arrival(const unsigned prev_state, const unsigned state, struct selector_key* key);
void done_arrival(const unsigned prev_state, const unsigned state, struct selector_key* key);
unsigned error_write(struct selector_key* key);
#endif