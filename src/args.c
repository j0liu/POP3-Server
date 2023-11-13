#include <errno.h>
#include <getopt.h>
#include <limits.h> /* LONG_MIN et al */
#include <stdio.h> /* for printf */
#include <stdlib.h> /* for exit */
#include <string.h> /* memset */

#include "args.h"

static unsigned short port(const char* s)
{
    char* end = 0;
    const long sl = strtol(s, &end, 10);

    if (end == s || '\0' != *end || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno) || sl < 0 || sl > USHRT_MAX) {
        fprintf(stderr, "port should in in the range of 1-65536: %s\n", s);
        exit(1);
        return 1;
    }
    return (unsigned short)sl;
}

static void user(char* s, struct users* user)
{
    char* p = strchr(s, ':');
    if (p == NULL) {
        fprintf(stderr, "password not found\n");
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
    fprintf(stderr, "POP3 version alpha\n"
                    "ITBA Protocolos de Comunicación 2023/2 -- Grupo 7\n"
                    "Todos los derechos reservados bajo pena de muerte\n");
}

static void usage(const char* progName)
{
    printf("Usage: %s [options]\n", progName);
    printf("Options:\n");
    printf("  -h          Show this help message\n");
    printf("  -v          Display version information\n");
    printf("  -p <port>   Set the POP3 port (default: 110)\n");
    printf("  -u <user>   Add a user in the format user:pass\n");
    printf("  -d <path>   Set the mail directory path (default: ./Maildir)\n");
}

void parse_args(const int argc, char** argv, Pop3Args* args)
{
    memset(args, 0, sizeof(*args)); // sobre todo para setear en null los punteros de users

    args->maildir_path = "./Maildir";
    args->pop3_port = 110;
    // TODO: Default users?

    int c;
    args->quantity_users = 0;

    while ((c = getopt(argc, argv, "hp:u:vd:")) != -1) {
        switch (c) {
        case 'h':
            usage(argv[0]);
            exit(0);
            break;
        case 'p':
            args->pop3_port = port(optarg);
            break;
        case 'u':
            if (args->quantity_users >= MAX_USERS) {
                fprintf(stderr, "Maximum number of command line users reached: %d.\n", MAX_USERS);
                exit(1);
            }
            user(optarg, &args->users[args->quantity_users]);
            args->quantity_users++;
            break;
        case 'v':
            version();
            exit(0);
            break;
        case 'd':
            // TODO: Properly validate the path
            args->maildir_path = optarg;
            break;
        default:
            fprintf(stderr, "Unknown argument %c.\n", c);
            exit(1);
        }
    }

    if (args->quantity_users == 0) {
        fprintf(stderr, "At least one user must be specified.\n");
        exit(1);
    }

    if (optind < argc) {
        fprintf(stderr, "Non-option argument(s) not accepted: ");
        while (optind < argc) {
            fprintf(stderr, "%s ", argv[optind++]);
        }
        fprintf(stderr, "\n");
        exit(1);
    }
}
