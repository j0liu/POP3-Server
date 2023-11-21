#ifndef LOGGER_H
#define LOGGER_H
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#define ERROR_LOG_LENGTH 64

#define LOG_AND_RETURN(error_level, error_message, return_value)           \
	do                                                                     \
	{                                                                      \
		log((error_level), "%s - %s\n", (error_message), strerror(errno)); \
		return (return_value);                                             \
	} while (0)

typedef enum
{
	LOG_DEBUG = 0,
	LOG_INFO,
	LOG_ERROR,
	LOG_FATAL
} LOG_LEVEL;

extern LOG_LEVEL current_level;

/**
 *  Minimo nivel de log a registrar. Cualquier llamada a log con un nivel mayor a newLevel sera ignorada
 **/
void set_log_level(LOG_LEVEL newLevel);

const char *level_description(LOG_LEVEL level);

#define logf(level, fmt, ...)                                                                           \
	{                                                                                                  \
		if (level >= current_level)                                                                    \
		{                                                                                              \
			time_t now = time(NULL);                                                                   \
			char *time_str = ctime(&now);                                                              \
			time_str[strlen(time_str) - 1] = '\0';                                                     \
			fprintf(stderr, "%s: %s:%d,%s, ", level_description(level), __FILE__, __LINE__, time_str); \
			fprintf(stderr, fmt, __VA_ARGS__);                                                         \
			fprintf(stderr, "\n");                                                                     \
		}                                                                                              \
		if (level == LOG_FATAL)                                                                            \
			exit(1);                                                                                   \
	}

#define log(level, s) logf(level, "%s", s)

#endif
