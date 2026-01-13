/*
 * client.c -- TCP Socket Client
 * 
 * adapted from: 
 *   https://www.educative.io/answers/how-to-implement-tcp-sockets-in-c
 */

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include "config.h"
#include "client.h"


/* helper function to read only 1 line */
ssize_t recv_line(int sock, char *buf, size_t maxlen) {
    size_t pos = 0;
    while (pos < maxlen - 1) {
        char c;
        ssize_t n = recv(sock, &c, 1, 0);
        if (n <= 0) {
            return n;
        }
        buf[pos++] = c;
        if (c == '\n') {
            break;
        }
    }
    buf[pos] = '\0';
    return (ssize_t)pos;
}

void handle_ls(int socket_desc, char *remote) {
    char command[256];
    snprintf(command, sizeof(command), "LS %s\n", remote);
    send(socket_desc, command, strlen(command), 0);

    char response[2000];
    ssize_t n;
    while ((n = recv(socket_desc, response, sizeof(response) - 1, 0)) > 0) {
        response[n] = '\0';
        printf("%s", response);
    }

    return;
}

void handle_rm(int socket_desc, const char *remote) {
    char command[256];
    snprintf(command, sizeof(command), "RM %s\n", remote);
    send(socket_desc, command, strlen(command), 0);

    char response[256];
    if (recv_line(socket_desc, response, sizeof(response)) <= 0) {
        perror("recv response");
        return;
    }
    printf("Server response: %s", response);

    return;
}

void handle_get(int socket_desc, const char *local, const char *remote, int version) {
    FILE *file = fopen(local, "wb");
    if (!file) {
        perror("Failed to open local file");
        return;
    }

    // Request file from server
    char command[256];

    //handling versioning
    if (version > 0) {
        snprintf(command, sizeof(command), "GET %s.v%d\n", remote, version);
    } else {
        snprintf(command, sizeof(command), "GET %s\n", remote);
    }
    send(socket_desc, command, strlen(command), 0);

    // Receive header with filesize
    char header[256];
    if (recv_line(socket_desc, header, sizeof(header)) <= 0) {
        perror("recv header");
        fclose(file);
        return;
    }

    if (strncmp(header, "ERR", 3) == 0) {
    printf("Server Error: %s\n", header);
    fclose(file);
    return;
}

    long file_size = 0;
    sscanf(header, "%ld", &file_size);

    // Receive file contents from server
    char buffer[1024];
    ssize_t bytesReceived=0;
     while (bytesReceived < file_size) {
        ssize_t chunk = recv(socket_desc, buffer, sizeof(buffer), 0);
        if (chunk <= 0) {
            perror("recv file data");
            break;
        }
        fwrite(buffer, 1, (size_t)chunk, file);
        bytesReceived += chunk;
    }

    fclose(file);
    return;
}

void handle_write(int socket_desc, const char *local, const char *remote) {
    FILE *file = fopen(local, "rb");
    if (!file) {
        perror("Failed to open local file");
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);


    //Header
    char command[256];
    snprintf(command, sizeof(command), "WRITE %s %ld\n", remote, file_size);
    send(socket_desc, command, strlen(command), 0);

    //Contents
    char buffer[1024];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(socket_desc, buffer, bytesRead, 0);
    }

    fclose(file);
    return;
}

int main(int argc , char *argv[])
{

  if (argc < 2) {
    fprintf(stderr, "Invalid arguments");
    exit(1); 
  }



  int socket_desc;
  struct sockaddr_in server_addr;
  char server_message[2000], client_message[2000];
  
  // Clean buffers:
  memset(server_message,'\0',sizeof(server_message));
  memset(client_message,'\0',sizeof(client_message));
  
  // Create socket:
  socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  
  if(socket_desc < 0){
    printf("Unable to create socket\n");
    close(socket_desc);
    return -1;
  }
  
  printf("Socket created successfully\n");
  
  // Set port and IP the same as server-side:
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(RFS_DEFAULT_PORT);
  server_addr.sin_addr.s_addr = inet_addr(RFS_DEFAULT_IP);
  
  // Send connection request to server:
  if(connect(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
    printf("Unable to connect\n");
    close(socket_desc);
    return -1;
  }
  printf("Connected with server successfully\n");


  char *cmd = argv[1];
  char *local = NULL;
  char *remote = NULL;


 if (strcmp(cmd, "WRITE") == 0) {
        local = argv[2];
        remote = (argc == 4 ? argv[3] : argv[2]);
        handle_write(socket_desc, local, remote);
    } else if (strcmp(cmd, "GET") == 0) {
      if (argc <= 4) {
        remote = argv[2];
        local = (argc == 4 ? argv[3] : argv[2]);
        handle_get(socket_desc, local, remote, 0);
    } else if (argc == 5) {
        remote = argv[2];
        int version = atoi(argv[4]);
        local  = argv[3];
        handle_get(socket_desc, local, remote, version);
      }
    } else if(strcmp(cmd, "RM") == 0) {
        remote = argv[2];
        handle_rm(socket_desc, remote);
    } else if(strcmp(cmd, "LS") == 0) {
        remote = argv[2];
        handle_ls(socket_desc, remote);
    } else if (strcmp(cmd, "STOP") == 0) {
        char command[256];
        snprintf(command, sizeof(command), "STOP\n");
        send(socket_desc, command, strlen(command), 0);

        char response[256];
        ssize_t n = recv_line(socket_desc, response, sizeof(response));
        if (n > 0) {
          printf("%s", response);
        }
    }
    
    else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        exit(1);
    }

  close(socket_desc);
  
  return 0;
}
