/**
 * main.c - servidor proxy socks concurrente
 *
 * Interpreta los argumentos de línea de comandos, y monta un socket
 * pasivo. Por cada nueva conexión lanza un hilo que procesará de
 * forma bloqueante utilizando el protocolo SOCKS5.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <unistd.h>
#include "buffer.h"
#include "netutils.h"
#include "tests.h"
#include "pop3_cmd.h"
// #include "socks5.h"

/**
 * estructura utilizada para transportar datos entre el hilo
 * que acepta sockets y los hilos que procesa cada conexión
 */
struct connection {
  int fd; 
  socklen_t addrlen;
  struct sockaddr_in6 addr;
};

static bool done = false;

static void sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}


/**
 * maneja cada conexión entrante
 *
 * @param fd   descriptor de la conexión entrante.
 * @param caddr información de la conexiónentrante.
 */

static void pop3_handle_connection(const int fd, const struct sockaddr *caddr) {
    struct buffer serverBuf;
    buffer *b = &serverBuf;
    uint8_t serverDirectBuff[1024];
    buffer_init(&serverBuf, N(serverDirectBuff), serverDirectBuff);

    struct buffer clientBuf;
    buffer *b = &clientBuf;
    uint8_t clientDirectBuffer[1024];
    buffer_init(&clientBuf, N(clientDirectBuffer), clientDirectBuffer);
    
    memcpy(serverDirectBuff, "+OK POP3 server ready\r\n", 23);
    buffer_write_adv(b, 23);
    sock_blocking_write(fd, 0);    

   {
    bool error = false;
    size_t buffsize;
    ssize_t n;
    struct pop3_cmd_parser pop3cmd_parser = {
    };
    pop3cmd_parser_init(&pop3cmd_parser);


    do {
        uint8_t *ptr = buffer_write_ptr(&clientBuf, &buffsize);
        n = recv(fd, ptr, buffsize, 0); 
        if(n > 0) {
            buffer_write_adv(&clientBuf, n); 
            const enum pop3_cmd_state st = pop3_cmd_consume(&clientBuf, &pop3cmd_parser, &error);
            if (pop3_cmd_is_done(st, &error)) {
                break;
            }
        } else {
            break;
        }   
    } while(true);
    // if(!hello_is_done(hello_parser.state, &error)) {
    //     error = true;
    // }
    // hello_parser_close(&hello_parser);
    // return error;
   } 
    close(fd);
}


/** rutina de cada hilo worker */

static void * handle_connection_pthread(void *args) {
  const struct connection *c = args;
  pthread_detach(pthread_self());
  pop3_handle_connection(c->fd, (struct sockaddr *)&c->addr);
  free(args);
  return 0;
}


int serve_pop3_concurrent_blocking(const int server)
{
    for (;!done;)
    {
        struct sockaddr_in6 caddr;
        socklen_t caddrlen = sizeof(caddr);
        // Wait for a client to connect
        const int client = accept(server, (struct sockaddr *)&caddr, &caddrlen);
        if (client < 0)
        {
            perror("unable to accept incoming socket");
        }
        else
        {
            // TODO(juan): limitar la cantidad de hilos concurrentes
            struct connection *c = malloc(sizeof(struct connection));
            if (c == NULL)
            {
                // lo trabajamos iterativamente
                pop3_handle_connection(client, (struct sockaddr *)&caddr);
            }
            else
            {
                pthread_t tid;
                c->fd = client;
                c->addrlen = caddrlen;
                memcpy(&(c->addr), &caddr, caddrlen);

                if (pthread_create(&tid, 0, handle_connection_pthread, c))
                {
                    free(c);
                    // lo trabajamos iterativamente
                    pop3_handle_connection(client, (struct sockaddr *)&caddr);
                }
            }
        }
    }
    return 0;
}


/**

* atiende a los clientes de forma concurrente con I/O bloqueante.

*/

int main(const int argc, const char **argv)
{
    unsigned port = 1110;

    if (argc == 1)
    {
        // utilizamos el default
    }
    else if (argc == 2)
    {
        char *end = 0;
        const long sl = strtol(argv[1], &end, 10);

        if (end == argv[1] || '\0' != *end || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno) || sl < 0 || sl > USHRT_MAX)
        {
            fprintf(stderr, "port should be an integer: %s\n", argv[1]);
            return 1;
        }
        port = sl;
    }
    else
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    const char *err_msg;

    const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server < 0)
    {
        err_msg = "unable to create socket";
        goto finally;
    }

    fprintf(stdout, "Listening on TCP port %d\n", port);

    // man 7 ip. no importa reportar nada si falla.
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        err_msg = "unable to bind socket";
        goto finally;
    }

    if (listen(server, 20) < 0)
    {
        err_msg = "unable to listen";
        goto finally;
    }

    // registrar sigterm es útil para terminar el programa normalmente.
    // esto ayuda mucho en herramientas como valgrind.
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);

    err_msg = 0;
    int ret = serve_pop3_concurrent_blocking(server);


    getchar();

finally:
    if (err_msg)
    {
        perror(err_msg);
        ret = 1;
    }
    if (server >= 0)
    {
        close(server);
    }
    return ret;
}
