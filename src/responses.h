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
#define GET_STATS_DAJT (OK_DAJT " Sure! Here you have" CRLF "Total bytes sent: %ld" CRLF "Current connections: %ld" CRLF "Total connections: %ld" CRLF "Total errors: %ld" CRLF)
#define GET_BUFFER_DAJT (OK_DAJT " Here is the buffer size: %d" CRLF) 
#define SET_BUFFER_DAJT (OK_DAJT " Buffer size changed" CRLF) 
#define SERVER_READY_DAJT (OK_DAJT " DAJT is ready to PUSH IT!" CRLF)
#define ERR_INVALID_AUTH_DAJT (ERR_DAJT " Invalid authentication!" CRLF) 
#define UNKNOWN_COMMAND_DAJT (ERR_DAJT " What are you talking about?! Get out of my sight" CRLF)
#define NO_TRANSFORMATIONS_SET_DAJT (ERR_DAJT " No transformations set" CRLF)

#endif