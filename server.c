/*
 * server.c -- TCP Socket Server
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
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>
#include <libgen.h>
#include <limits.h>
#include "config.h"
#include "server.h"

pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
int socket_desc;
int server_flag = 1;

/*helper function to append the root directory*/
void normalize_remote_path(char *remote) {
    // Prevent path traversal
    if (strstr(remote, "..")) {
        fprintf(stderr, "Security warning: rejecting path with ..\n");
        remote[0] = '\0';
        return;
    }

    char newpath[PATH_MAX];
    snprintf(newpath, sizeof(newpath), "%s/%s", RFS_ROOT, remote);

    // Create parent directory automatically
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", newpath);
    char *dir = dirname(tmp);

    char cmd[PATH_MAX];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", dir);
    system(cmd);

    snprintf(remote, PATH_MAX, "%s", newpath);
}


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

/* ls function which returns files and all versions */
void handle_ls(int client_sock, char *filename) {
    char cmd[512];
    char response[8192] = "";

    snprintf(cmd, sizeof(cmd), "ls -l -- %s*", filename);


    FILE *fp = popen(cmd, "r");
    if (!fp) {
        perror("popen failed");
        send(client_sock, "Error running LS\n", 17, 0);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        strcat(response, line);
    }

    pclose(fp);
    send(client_sock, response, strlen(response), 0);
    send(client_sock, "ENDLS\n", 6, 0);
}

/* versioning function which saves versions of files being overwritten */
void handle_versioning(char *filename) {
    
struct stat st;
char ver_name[512];
int v = 1;

  snprintf(ver_name, sizeof(ver_name), "%s.v%d", filename, v);
  while(stat(ver_name, &st) == 0){
    v++;
    snprintf(ver_name, sizeof(ver_name), "%s.v%d", filename, v);
  }

  if (rename(filename, ver_name) != 0) {
        perror("rename for versioning");
    } else {
        printf("Versioned %s to %s\n", filename, ver_name);
    }
  return;

}

/* rm function which removes files and all versions */
void handle_rm(int client_sock, const char *remote) {

  char server_message[256]="";

  pthread_mutex_lock(&file_mutex);

    if (remove(remote) == 0) {
      snprintf(server_message, sizeof(server_message), "File deleted successfully\n");
      for (int v = 1;; v++) {
          char ver_name[512];
          snprintf(ver_name, sizeof(ver_name), "%s.v%d", remote, v);
          if (remove(ver_name) != 0) {
              break;
          }
      }
    } else {
      snprintf(server_message, sizeof(server_message), "Failed to delete file\n");
    }

  pthread_mutex_unlock(&file_mutex);

    send(client_sock, server_message, strlen(server_message), 0);

    return;
}


/* get function which reads file and sends the content  */
void handle_get(int client_sock, const char *remote) {
    FILE *file = fopen(remote, "rb");
if (!file) {
    char msg[] = "ERR File not found\n";
    send(client_sock, msg, strlen(msg), 0);
    return;
}

    if(fseek(file, 0, SEEK_END) != 0) {
        perror("fseek failed");
        fclose(file);
        return;
    }
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    //header with filesize
    char header[256];
    snprintf(header, sizeof(header), "%ld\n", file_size);
    send(client_sock, header, strlen(header), 0);


    //contents
    char buffer[1024];
    size_t bytesRead;
     while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (send(client_sock, buffer, bytesRead, 0) < 0) {
            perror("send file data");
            break;
        }
    }


    fclose(file);
    return;
}

/* write function which writes data from client into the server file */
void handle_write(int client_sock, const char *remote, long file_size) {

  struct stat st;
  
  pthread_mutex_lock(&file_mutex);


  if(stat(remote, &st) == 0) {
      handle_versioning((char *)remote);
  }

    FILE *file = fopen(remote, "wb");
    if (!file) {
        perror("Failed to open remote file");
        pthread_mutex_unlock(&file_mutex);
        return;
    }


    char buffer[1024];
    long total_bytes_received = 0;

    while (total_bytes_received < file_size) {

      size_t to_read = (size_t)(file_size - total_bytes_received);
        if (to_read > sizeof(buffer)) {
            to_read = sizeof(buffer);
        }
      
        ssize_t n = recv(client_sock, buffer, to_read, 0);
        if (n <= 0) {
            perror("recv file data");
            break;
        }

        size_t written = fwrite(buffer, 1, (size_t)n, file);
        if (written < (size_t)n) {
            perror("fwrite");
            break;
        }

        total_bytes_received += n;
      }
  fclose(file);
  pthread_mutex_unlock(&file_mutex);
  return;
}

/* thread function to create one thread per client */
void *client_thread(void *arg) {
  
    struct client_args *cargs = (struct client_args *)arg;
    int client_sock = cargs->client_sock;
    struct sockaddr_in client_addr = cargs->client_addr;
    free(cargs);

    printf("Client connected at IP: %s and port: %i\n",
           inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));

    char client_message[8196];
    memset(client_message, '\0', sizeof(client_message));

    if (recv_line(client_sock, client_message, sizeof(client_message)) <= 0) {
        printf("Couldn't receive from client\n");
        close(client_sock);
        pthread_exit(NULL);
    }

    char cmd[16], remote[PATH_MAX];
    long file_size = 0;

    sscanf(client_message, "%s", cmd);

    if (strcmp(cmd, "WRITE") == 0) {
        sscanf(client_message, "%s %s %ld", cmd, remote, &file_size);
        normalize_remote_path(remote);
        handle_write(client_sock, remote, file_size);
    } else if (strcmp(cmd, "GET") == 0) {
        sscanf(client_message, "%s %s", cmd, remote);
        normalize_remote_path(remote);
        handle_get(client_sock, remote);
    } else if (strcmp(cmd, "RM") == 0) {
        sscanf(client_message, "%s %s", cmd, remote);
        normalize_remote_path(remote);
        handle_rm(client_sock, remote);
    } else if (strcmp(cmd, "LS") == 0) {
        sscanf(client_message, "%s %s", cmd, remote);
        normalize_remote_path(remote);
        handle_ls(client_sock, remote);
    } else if (strcmp(cmd, "STOP") == 0) {
      server_flag = 0;
      char stop_msg[] = "Server stopping as per client request.\n";
      send(client_sock, stop_msg, strlen(stop_msg), 0);
      printf("Server shutting down now.\n");
      shutdown(socket_desc, SHUT_RDWR);
      close(socket_desc);
      close(client_sock);
      pthread_exit(NULL);
    }
    else {
        printf("Unknown command received: %s\n", cmd);
    }

    close(client_sock);
    pthread_exit(NULL);
}

int main(void)
{
  //int socket_desc, client_sock;
  int client_sock;
  socklen_t client_size;
  struct sockaddr_in server_addr, client_addr;
  char server_message[8196], client_message[8196];
  
  // Clean buffers:
  memset(server_message, '\0', sizeof(server_message));
  memset(client_message, '\0', sizeof(client_message));
  
  // Create socket:
  socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  
  if(socket_desc < 0){
    printf("Error while creating socket\n");
    return -1;
  }
  printf("Socket created successfully\n");
  
  // Set port and IP:
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(RFS_DEFAULT_PORT);
  server_addr.sin_addr.s_addr = inet_addr(RFS_DEFAULT_IP);
  
  // Bind to the set port and IP:
  if(bind(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr))<0){
    printf("Couldn't bind to the port\n");
    return -1;
  }
  printf("Done with binding\n");
  
  // Listen for clients:
  if(listen(socket_desc, 1) < 0){
    printf("Error while listening\n");
    close(socket_desc);
    return -1;
  }
  printf("\nListening for incoming connections.....\n");
  
  // Accept an incoming connection:
  while (server_flag)
  {

    if (!server_flag) {
      break;
    }

    client_size = sizeof(client_addr);
    client_sock = accept(socket_desc, (struct sockaddr*)&client_addr, &client_size);

    
  
  if (client_sock < 0){
    if (!server_flag) {
      break;
    }
    printf("Can't accept\n");
    continue;
    // close(socket_desc);
    // close(client_sock);
  }

  struct client_args *args = malloc(sizeof(struct client_args));
        if (!args) {
            perror("malloc");
            close(client_sock);
            continue;
        }

        args->client_sock = client_sock;
        args->client_addr = client_addr;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, args) != 0) {
            perror("pthread_create");
            close(client_sock);
            free(args);
            continue;
        }

  pthread_detach(tid);
      }

  close(socket_desc);
  return 0;
}
