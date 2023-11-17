#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h> /* LONG_MIN et al */
#include <stdio.h> /* for printf */
#include <stdlib.h> /* for exit */
#include <string.h> /* memset */
#include <sys/stat.h>

#include "args.h"

Args args;

static unsigned short port(const char* s)
{
    char* end = 0;
    const long sl = strtol(s, &end, 10);

    if (end == s || '\0' != *end || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno) || sl < 0 || sl > USHRT_MAX) {
        fprintf(stderr, "Port should in in the range of 1-65536: %s\n", s);
        exit(1);
        return 1;
    }
    return (unsigned short)sl;
}

static void user(char* s, struct users* user)
{
    char* p = strchr(s, ':');
    if (p == NULL) {
        fprintf(stderr, "Password not found\n");
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
    fprintf(stderr, "POP3 version Alpha\n"
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
    printf("  -P <port>   Set the DAJT port (default: 6969)\n");
    printf("  -u <user>   Add a user in the format user:pass\n");
    printf("  -d <path>   Set the mail directory path (default: ./Maildir)\n");
}

void parse_args(const int argc, char** argv, Args* args)
{
    memset(args, 0, sizeof(*args));

    args->maildir_path = "./Maildir";
    args->pop3_port = 110; // Default POP3 port
    args->dajt_port = 6969; // Default DAJT port

    int c;
    args->quantity_users = 0;

    while ((c = getopt(argc, argv, "hp:d:u:vP:")) != -1) {
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
            args->maildir_path = optarg;
            struct stat statbuf;
            if (stat(args->maildir_path, &statbuf) != 0 || !S_ISDIR(statbuf.st_mode)) {
                fprintf(stderr, "Invalid maildir path: %s\n", args->maildir_path);
                exit(1);
            }
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
