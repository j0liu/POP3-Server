#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_HOST_LEN 50
#define MAX_USER_LEN 30
#define MAX_PASS_LEN 30
#define MAX_PORT_LEN 6
#define MAX_PATH_LEN 100
#define MAX_URL_LEN MAX_HOST_LEN + MAX_USER_LEN + MAX_PASS_LEN + MAX_PORT_LEN + MAX_PATH_LEN + 7
#define MAX_BODY_LEN 100
#define WRITE_BUF_SIZE 1024
#define READ_BUF_SIZE 1024
#define MAX_COMMAND_LEN 4

typedef struct {
    int use_ipv6;
    char url[MAX_URL_LEN];
    char body[MAX_BODY_LEN];
} Arguments;

char command[WRITE_BUF_SIZE]; // Global variable to store the command
const char* err_msg; // Global variable to store the error message

// Command handler function prototypes
int handle_quit(const char* path);
int handle_stat(const char* path);
int handle_ttra(const char* path);
int handle_tran(const char* path, const char* body);
int handle_buff(const char* path);

int handle_quit(const char* path)
{
    if (strlen(path) > 0 && path[0] != '/') {
        err_msg = "QUIT command does not accept arguments.";
        return 1;
    }
    snprintf(command, sizeof(command), "QUIT\r\n");
    return 0;
}

int handle_stat(const char* path)
{
    if (strcmp(path, "/c") != 0 && strcmp(path, "/t") != 0 && strcmp(path, "/b") != 0) {
        err_msg = "Invalid argument for STAT command. Usage: STAT /c|/t|/b";
        return 1;
    }
    snprintf(command, sizeof(command), "STAT %s\r\n", path + 1);
    return 0;
}

int handle_ttra(const char* path)
{
    int path_len = strlen(path);
    if (strcmp(path, "/0") != 0 && strcmp(path, "/1") != 0 && path_len > 0 && path[0] != '/') {
        err_msg = "Invalid argument for TTRA command. Usage: TTRA /0|/1|";
        return 1;
    }
    if (path_len == 0) {
        snprintf(command, sizeof(command), "TTRA\r\n");
        return 0;
    }
    snprintf(command, sizeof(command), "TTRA %s\r\n", path + 1);
    return 0;
}

int handle_tran(const char* path, const char* body)
{
    int path_len = strlen(path);
    if (path_len > 0 && path[0] != '/') {
        err_msg = "Invalid argument for TRAN command. Usage: TRAN -body \"commands\"";
        return 1;
    }
    if (path_len == 0) {
        snprintf(command, sizeof(command), "TRAN %s\r\n", body);
        return 0;
    }
    return 0;
}

int handle_buff(const char* path)
{
    int path_len = strlen(path);
    if (path_len > 0 && path[0] != '/') {
        err_msg = "Invalid argument for BUFF command. Usage: BUFF /number";
        return 1;
    }
    if (path_len == 0) {
        snprintf(command, sizeof(command), "BUFF\r\n");
        return 0;
    }
    snprintf(command, sizeof(command), "BUFF %s\r\n", path + 1);
    return 0;
}

// Función para parsear la URL
int parse_url(const char* url, char* user, char* pass, char* host, char* port, char* path)
{
    if (strncmp(url, "DAJT://", 7) != 0 && strncmp(url, "dajt://", 7) != 0) {
        printf("Protocolo no soportado.\n");
        return 1;
    }

    const char* url_start = url + 7; // Skip "DAJT://"

    const char* user_pass_end = strchr(url_start, '@');
    if (user_pass_end == NULL) {
        return 1;
    }

    const char* pass_start = strchr(url_start, ':');
    if (pass_start == NULL || pass_start > user_pass_end) {
        return 1;
    }

    const char* host_port_start = user_pass_end + 1;
    const char* port_start = strchr(host_port_start, ':');
    if (port_start == NULL) {
        return 1;
    }

    const char* path_start = strchr(port_start, '/');
    if (path_start == NULL) {
        return 1;
    }

    int user_len = pass_start - url_start;
    int pass_len = user_pass_end - pass_start - 1;
    int host_len = port_start - host_port_start;
    int port_len = path_start - port_start - 1;
    int path_len = strlen(path_start);

    snprintf(user, MAX_USER_LEN, "%.*s", user_len, url_start);
    snprintf(pass, MAX_PASS_LEN, "%.*s", pass_len, pass_start + 1);
    snprintf(host, MAX_HOST_LEN, "%.*s", host_len, host_port_start);
    snprintf(port, MAX_PORT_LEN, "%.*s", port_len, port_start + 1);
    snprintf(path, MAX_PATH_LEN, "%.*s", path_len, path_start + 1);

    return 0;
}

int parse_path(const char* path, const char* body)
{
    if (strncmp(path, "quit", MAX_COMMAND_LEN) == 0) {
        return handle_quit(path + MAX_COMMAND_LEN);
    } else if (strncmp(path, "stat", MAX_COMMAND_LEN) == 0) {
        return handle_stat((path + MAX_COMMAND_LEN));
    } else if (strncmp(path, "ttra", MAX_COMMAND_LEN) == 0) {
        return handle_ttra(path + MAX_COMMAND_LEN);
    } else if (strncmp(path, "tran", MAX_COMMAND_LEN) == 0) {
        return handle_tran(path + MAX_COMMAND_LEN, body);
    } else if (strncmp(path, "buff", MAX_COMMAND_LEN) == 0) {
        return handle_buff(path + MAX_COMMAND_LEN);
    } else {
        err_msg = "Unrecognized command in path.";
        return 1;
    }
}

#define AVAILABLE_COMMANDS "Available paths:\n\
quit\n\
stat/[c|t|b]\n\
ttra/[0|1|]|\n\
tran [-body \"commands\"|]\n\
buff/[number|]\n"

int parse_args(int argc, char* argv[], Arguments* args)
{
    args->use_ipv6 = 0; // Default to IPv4
    args->url[0] = '\0';
    args->body[0] = '\0';

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            puts(AVAILABLE_COMMANDS);
            return 1;
        } else if (strcmp(argv[i], "-v4") == 0) {
            args->use_ipv6 = 0;
        } else if (strcmp(argv[i], "-v6") == 0) {
            args->use_ipv6 = 1;
        } else if (strncmp(argv[i], "-body", 5) == 0) {
            if (i + 1 < argc) {
                strncpy(args->body, argv[i + 1], MAX_BODY_LEN - 1);
                i++;
            } else {
                err_msg = "Error: No body provided after -body option";
                return 1;
            }
        } else if (args->url[0] == '\0') {
            strncpy(args->url, argv[i], MAX_URL_LEN - 1);
        } else {
            err_msg = "Error: Unrecognized argument";
            return 1;
        }
    }

    if (args->url[0] == '\0') {
        err_msg = "Error: URL not provided";
        return 1;
    }

    return 0;
}

int main(int argc, char* argv[])
{
    Arguments args;
    if (parse_args(argc, argv, &args) != 0) {
        goto usage_error;
        return 1;
    }

    if (args.body[0] != '\0') {
        printf("Body: %s\n", args.body);
    }

    char user[MAX_USER_LEN], pass[MAX_PASS_LEN], host[MAX_HOST_LEN], port[MAX_PORT_LEN], path[MAX_PATH_LEN];
    if (parse_url(args.url, user, pass, host, port, path) != 0) {
        err_msg = "Error parsing URL.";
        goto parsing_error;
    }

    if (parse_path(path, args.body) != 0) {
        goto parsing_error;
    }
    // -- End of args parsing --

    // -- Connect to server --
    struct addrinfo hints, *res;
    int sockfd;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = args.use_ipv6 ? AF_INET6 : AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port, &hints, &res) != 0) {
        err_msg = "Error getting address info.";
        goto addr_error;
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        err_msg = "Error creating socket.";
        goto client_error;
    }

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        err_msg = "Error connecting to server.";
        goto client_error;
    }

    char response[READ_BUF_SIZE]; // Buffer for the welcome message
    memset(response, 0, sizeof(response));
    ssize_t welcome_bytes_received = recv(sockfd, response, sizeof(response) - 1, 0);
    if (welcome_bytes_received < 0) {
        err_msg = "Error receiving welcome message from server.";
        goto client_error;
    }

    // -- Send AUTH command --
    char auth[WRITE_BUF_SIZE];
    snprintf(auth, sizeof(auth), "AUTH %s %s\r\n", user, pass);
    if (send(sockfd, auth, strlen(auth), 0) < 0) {
        err_msg = "Error sending auth.";
        goto client_error;
    }

    memset(response, 0, sizeof(response));
    ssize_t bytes_received = recv(sockfd, response, sizeof(response) - 1, 0);
    if (bytes_received < 0) {
        err_msg = "Error receiving response from server.";
        goto client_error;
    } else if (strncmp(response, "<ERR", 4) == 0) {
        err_msg = "Invalid credentials.";
        goto client_error;
    }

    if (send(sockfd, command, strlen(command), 0) < 0) {
        err_msg = "Error sending command.";
        goto client_error;
    }

    memset(response, 0, sizeof(response));
    bytes_received = recv(sockfd, response, sizeof(response) - 1, 0);
    if (bytes_received < 0) {
        err_msg = "Error receiving response from server.";
        goto client_error;
    }

    // Output the response to standard output
    printf("Server Response: %s", response);

    freeaddrinfo(res); // Liberar la información de la dirección
    close(sockfd); // Cerrar el socket

    return 0;

usage_error:
    printf("Uso: %s [-v4|-v6] dajt://user:pass@host:port/path\n", argv[0]);
    return 1;

parsing_error:
    printf("%s\n", err_msg);
    return 1;

addr_error:
    printf("%s\n", err_msg);
    // freeaddrinfo(res);
    return 1;

client_error:
    printf("%s\n", err_msg);
    freeaddrinfo(res);
    close(sockfd);
    return 1;
}