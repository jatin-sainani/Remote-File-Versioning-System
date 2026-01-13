#ifndef SERVER_H
#define SERVER_H

#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

/* helper function */
ssize_t recv_line(int sock, char *buf, size_t maxlen);

/* commands */
void handle_ls(int client_sock, char *filename);
void handle_versioning(char *filename);
void handle_rm(int client_sock, const char *remote);
void handle_get(int client_sock, const char *remote);
void handle_write(int client_sock, const char *remote, long file_size);

/* client thread structure */
struct client_args {
    int client_sock;
    struct sockaddr_in client_addr;
};

/* client thread */
void *client_thread(void *arg);

#endif
