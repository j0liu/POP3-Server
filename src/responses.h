#ifndef RESPONSES_H
#define RESPONSES_H

#define CRLF "\r\n"
// POP 3

#define OK "+OK"
#define ERR "-ERR"
#define OKCRLF (OK CRLF)

#define SERVER_READY OK " POP3 Party Started" CRLF


// DAJT
#define OK_DAJT ">OKEY"
#define ERR_DAJT "<ERR"
#define OKCRLF_DAJT (OK_DAJT CRLF)
#define SERVER_READY_DAJT OK_DAJT " DAJT is ready to PUSH IT!" CRLF
#define ERR_INVALID_AUTH_DAJT (ERR_DAJT " Invalid authentication!" CRLF) 
#define UNKNOWN_COMMAND_DAJT (ERR_DAJT " What are you talking about?!" CRLF)


#endif