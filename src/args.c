#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */
#include <limits.h>    /* LONG_MIN et al */
#include <string.h>    /* memset */
#include <errno.h>
#include <getopt.h>

#include "args.h"

static unsigned short
port(const char *s) {
     char *end     = 0;
     const long sl = strtol(s, &end, 10);

     if (end == s|| '\0' != *end
        || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
        || sl < 0 || sl > USHRT_MAX) {
         fprintf(stderr, "port should in in the range of 1-65536: %s\n", s);
         exit(1);
         return 1;
     }
     return (unsigned short)sl;
}

static void
user(char *s, struct users *user) {
    char *p = strchr(s, ':');
    if(p == NULL) {
        fprintf(stderr, "password not found\n");
        exit(1);
    } else {
        *p = 0;
        p++;
        user->name = s;
        user->pass = p;
    }

}

static void
version(void) {
    fprintf(stderr, "POP3 version alpha\n"
                    "ITBA Protocolos de Comunicación 2023/2 -- Grupo 7\n"
                    "Todos los derechos reservados bajo pena de muerte\n");
}

static void
usage(const char *progname) {
    fprintf(stderr,
        "Usage: %s [OPTION]...\n"
        "\n"
        "   -h               Imprime la ayuda y termina.\n"
        "   -p <POP3 port>   Puerto entrante conexiones POP3.\n"
        "   -u <name>:<pass> Usuario y contraseña de usuario. Hasta 10.\n"
        "   -v               Imprime información sobre la versión versión y termina.\n"
        "   -d <path>        Directorio donde se encuentran los mails.\n"
        "\n",
        progname);
    exit(1);
}

void 
parse_args(const int argc, char **argv, pop3Args *args) {
    memset(args, 0, sizeof(*args)); // sobre todo para setear en null los punteros de users

    args->maildir_path = "./Maildir";
    args->pop3_port = 110;
    // TODO: Default users?
    
    int c;
    int nusers = 0;

    while (true) {
        c = getopt(argc, argv, "hp:u:vd:");
        if (c == -1)
            break;

        switch (c) {
            case 'h':
                usage(argv[0]);
                exit(0);
                break;
            case 'p':
                args->pop3_port = port(optarg);
                break;
            case 'u':
                if(nusers >= MAX_USERS) {
                    fprintf(stderr, "maximun number of command line users reached: %d.\n", MAX_USERS);
                    exit(1);
                } else {
                    user(optarg, args->users + nusers);
                    nusers++;
                }
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
                fprintf(stderr, "unknown argument %d.\n", c);
                exit(1);
        }

    }
    if (optind < argc) {
        fprintf(stderr, "argument not accepted: ");
        while (optind < argc) {
            fprintf(stderr, "%s ", argv[optind++]);
        }
        fprintf(stderr, "\n");
        exit(1);
    }
}
