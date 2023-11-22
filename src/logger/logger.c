#include "logger.h"

LOG_LEVEL current_level = LOG_DEBUG;
static const char *description[] = {"DEBUG", "INFO", "ERROR", "FATAL"};

void set_log_level(LOG_LEVEL newLevel)
{
    if (newLevel >= LOG_DEBUG && newLevel <= LOG_FATAL)
        current_level = newLevel;
}

const char *level_description(LOG_LEVEL level)
{
    if (level < LOG_DEBUG || level > LOG_FATAL)
        return "";
    return description[level];
}
