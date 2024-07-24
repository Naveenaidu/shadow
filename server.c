#include <asm-generic/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// todo: take this as an argument
#define PORT 8082
#define BUFFER_SIZE 104857600 // 100 MiB

int main(int argc, char *argv[]) {

  // server_fd -> Value returned by socker call
  // client_fd -> value returned when accepting a connection on socket
  int server_fd, client_fd;

  // reads the data from the socker connection into this buffer
  char buffer[256];

  struct sockaddr_in server_addr;

  // 1. Create server socket
  // AF_INET -> Internet address family
  // SOCK_STREAM -> Read the characters in continuous stream (other option is
  // to read them as chunks)
  // protocol 0 -> Let OS choose the appropriate protocol
  // Note to self: in unix everything is a file. Each file is identified by it's
  // descriptor server_fd tell us how to access this socket
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  int option = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
  if (server_fd < 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }
  printf("server_fd %d\n", server_fd);

  // 2. Configure the socket
  server_addr.sin_family = AF_INET;
  // the host address
  server_addr.sin_addr.s_addr = INADDR_ANY;
  // convert to network byte order. The network byte order is defined to be big
  // endian. But in some cases, host byte order might not be that
  server_addr.sin_port = htons(PORT);

  // bind socket to port
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  // listen for connections
  if (listen(server_fd, 5) < 0) {
    perror("listen failed");
    exit(EXIT_FAILURE);
  }
  printf("Server is listening on port %d\n", PORT);

  // infinite loop to listen on connection
  while (1) {
    // client info
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // accept client connection
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
  
    if (client_fd < 0) {
      perror("bind failed");
      exit(EXIT_FAILURE);
    }

    //Read from the socket
    //
    // FIXME: In our current POC - we assume that the request and response would
    // at max be 100MB. So we just maintain a buffer of that size and assume
    // that we receive the entire request/response in a single recv and we do
    // not get any additional data. 
    // Assumption: whenever recv returns something, it is encapsulates the
    // entire request/response 
    //
    // See
    // https://stackoverflow.com/questions/49821687/how-to-determine-if-i-received-entire-message-from-recv-calls
    // for properly reading from recv.
    
    int bytes_recvd;
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
    bytes_recvd = recv(client_fd, buffer, BUFFER_SIZE, 0);
    
    if (bytes_recvd < 0){
      perror("error reading data from socket");
      exit(EXIT_FAILURE);
    }

    if (bytes_recvd == 0){
      perror("client disconnected upexpectedly");
      exit(EXIT_FAILURE);
    }

    printf("Bytes received: %d\n", bytes_recvd);
    printf("Request fetched: %s\n", buffer);


  }

  close(client_fd);
  close(server_fd);

  // printf() displays the string inside quotation
  printf("Hello, World!");
  return 0;
}
