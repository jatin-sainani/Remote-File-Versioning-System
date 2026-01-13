#ifndef CLIENT_H
#define CLIENT_H

#include <stddef.h>
#include <sys/types.h>

/* Helper function */
ssize_t recv_line(int sock, char *buf, size_t maxlen);

/* Commands */
void handle_ls(int socket_desc, char *remote);
void handle_rm(int socket_desc, const char *remote);
void handle_get(int socket_desc, const char *local, const char *remote, int version);
void handle_write(int socket_desc, const char *local, const char *remote);

#endif
