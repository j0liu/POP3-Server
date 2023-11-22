#ifndef RESPONSES_H
#define RESPONSES_H

#define CRLF "\r\n"
// POP 3

#define OK "+OK"
#define ERR "-ERR"
#define OKCRLF (OK CRLF)

#define SERVER_READY OK " POP3 Party Started" CRLF


// DAJT
#define OK_DAJT ">OKE"
#define ERR_DAJT "<ERR"
#define OKCRLF_DAJT (OK_DAJT CRLF)
#define OK_NUMBER_DAJT (OK_DAJT CRLF "%d" CRLF)
#define ERR_INVALID_NUMBER_DAJT (ERR_DAJT " Invalid number" CRLF)
#define ERR_NOISE_DAJT (ERR_DAJT " Noise detected" CRLF)

#define ERR_INVALID_STAT_DAJT (ERR_DAJT " stat is invalid" CRLF)
#define ERR_INVALID_TTRA_DAJT (ERR_DAJT " Invalid use of TTRA" CRLF)
#define GET_BUFFER_DAJT (OK_DAJT CRLF "%d" CRLF) 
#define SERVER_READY_DAJT (OK_DAJT " DAJT is ready to PUSH IT!" CRLF)
#define ERR_INVALID_AUTH_DAJT (ERR_DAJT " Invalid authentication!" CRLF) 
#define UNKNOWN_COMMAND_DAJT (ERR_DAJT " What are you talking about?! Get out of my sight" CRLF)
#define ERR_NO_TRANSFORMATIONS_SET_DAJT (ERR_DAJT " No transformations set" CRLF)

#define NEW_DAJT_CONNECTION "New DAJT connection"
#endif
