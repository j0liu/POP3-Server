#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h> /* LONG_MIN et al */
#include <stdio.h>
#include <stdlib.h> /* for exit */
#include <string.h> /* memset */
#include <sys/stat.h>
#include "logger/logger.h"

#include "global_state/globalstate.h"
#include "args.h"

Args args;
extern GlobalState global_state;


static unsigned short port(const char* s)
{
    char* end = 0;
    const long sl = strtol(s, &end, 10);

    if (end == s || '\0' != *end || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno) || sl < 0 || sl > USHRT_MAX) {
        logf(LOG_ERROR, "Port should in in the range of 1-65536: %s", s);
        exit(1);
        return 1;
    }
    return (unsigned short)sl;
}

static void user(char* s, struct User* user)
{
    char* p = strchr(s, ':');
    if (p == NULL) {
        log(LOG_ERROR, "Password not found");
        exit(1);
    } else {
        *p = 0;
        p++;
        user->name = s;
        user->pass = p;
    }
}

static void version(void)
{
    puts("POP3DAJT version Alpha\n"
                  "ITBA Protocolos de Comunicación 2023/2 -- Grupo 7\n"
                  "Todos los derechos reservados bajo pena de muerte");
}

static void usage(const char* progName)
{
    printf("Usage: %s [options]\n", progName);
    puts("Options:");
    puts("  -h          Show this help message");
    puts("  -v          Display version information");
    puts("  -p <port>   Set the POP3 port (default: 1110)");
    puts("  -P <port>   Set the DAJT port (default: 9090)");
    puts("  -u <user>   Add a pop3 user in the format user:pass");
    puts("  -U <user>   Add a dajt user in the format user:pass");
    puts("  -d <path>   Set the mail directory path (default: ./Maildir)");
    puts("  -t <path> <args...>  Set the transformation path and arguments (default: ./bin/usr/tr 'a-z' 'A-Z')");
}

void parse_args(const int argc, char** argv, Args* args)
{
    memset(args, 0, sizeof(*args));

    args->maildir_path = "./Maildir";
    args->pop3_port = 1110; // Default POP3 port
    args->dajt_port = 9090; // Default DAJT port

    int c;
    args->quantity_users = 0;

    while ((c = getopt(argc, argv, "hp:d:u:U:vP:t:")) != -1) {
        switch (c) {
        case 'h':
            usage(argv[0]);
            exit(0);
            break;
        case 'p':
            args->pop3_port = port(optarg);
            break;
        case 'P':
            args->dajt_port = port(optarg);
            break;
        case 'u':
            if (args->quantity_users >= MAX_USERS) {
                logf(LOG_ERROR, "Maximum number of command line users reached: %d.", MAX_USERS);
                exit(1);
            }
            user(optarg, &args->users[args->quantity_users]);
            args->quantity_users++;
            break;
        case 'U':
            if (args->quantity_users >= MAX_USERS) {
                logf(LOG_ERROR, "Maximum number of command line users reached: %d.", MAX_USERS);
                exit(1);
            }
            user(optarg, &args->admins[args->quantity_admins]);
            args->quantity_admins++;
            break;
        case 'v':
            version();
            exit(0);
            break;
        case 'd':
            args->maildir_path = optarg;
            struct stat statbuf;
            if (stat(args->maildir_path, &statbuf) != 0 || !S_ISDIR(statbuf.st_mode)) {
                logf(LOG_ERROR, "Invalid maildir path: %s", args->maildir_path);
                exit(1);
            }
            break;
        case 't':
            set_transformation(optarg);
            global_state.transformations_enabled = true;
            break;
        default:
            logf(LOG_ERROR, "Unknown argument %c.", c);
            exit(1);
        }
    }

    if (args->quantity_users == 0) {
        log(LOG_ERROR, "At least one user must be specified.");
        exit(1);
    }

    if (optind < argc) {
        log(LOG_ERROR, "Non-option argument(s) not accepted: ");
        while (optind < argc) {
            logf(LOG_ERROR, "%s ", argv[optind++]);
        }
        exit(1);
    }
}
