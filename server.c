#include <asm-generic/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// todo: take this as an argument
#define PORT 8082
#define BUFFER_SIZE 104857600 // 100 MiB

// Header constansts
#define HOST_HEADER "HOST"
#define AUTHORIZATION_HEADER "Authorization"
#define CONTENT_TYPE_HEADER "Content-Type"
#define CONTENT_LENGTH_HEADER "Content-Length"

struct HTTPRequest {
  // Method details
  char method[4];
  char uri[25];
  char protocol[10];

  // Headers
  char host_hdr[20];
  char authorization_hdr[20];
  char content_type_hdr[20];
  int content_length_hdr;

  // Payload
  char paylod[BUFFER_SIZE];
};

struct HTTPResponse {
  char protocol[10];
  int status_code;
  int status_message;

  // Headers
  char content_type_hdr[20];

  // Payload
  char paylod[BUFFER_SIZE];
};

void printHTTPRequest(struct HTTPRequest *request) {
  printf("-------------- HTTP Request -----------\n");
  printf("Method: %s\n", request->method);
  printf("URI: %s\n", request->uri);
  printf("Protocol: %s\n", request->protocol);
  printf("Host Header: %s\n", request->host_hdr);
  printf("Authorization Header: %s\n", request->authorization_hdr);
  printf("Content-Type Header: %s\n", request->content_type_hdr);
  printf("Content-Length Header: %d\n", request->content_length_hdr);
  printf("Payload: %s\n", request->paylod);
  printf("-------------------------------\n");
}

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
    client_fd =
        accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    printf("Client socket %d\n", client_fd);

    if (client_fd < 0) {
      perror("bind failed");
      exit(EXIT_FAILURE);
    }

    // Read from the socket
    //
    //  FIXME: In our current POC - we assume that the request and response
    //  would at max be 100MB. So we just maintain a buffer of that size and
    //  assume that we receive the entire request/response in a single recv and
    //  we do not get any additional data. Assumption: whenever recv returns
    //  something, it is encapsulates the entire request/response
    //
    //  See
    //  https://stackoverflow.com/questions/49821687/how-to-determine-if-i-received-entire-message-from-recv-calls
    //  for properly reading from recv.

    int bytes_recvd;
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
    // FIXME:
    bytes_recvd = recv(client_fd, buffer, BUFFER_SIZE, 0);

    if (bytes_recvd < 0) {
      perror("error reading data from socket");
      exit(EXIT_FAILURE);
    }

    if (bytes_recvd == 0) {
      perror("client disconnected upexpectedly");
      exit(EXIT_FAILURE);
    }

    // Add a null terminator to know that we have reached the end of the HTTP
    // request
    buffer[bytes_recvd] = '\0';

    printf("Bytes received: %d\n", bytes_recvd);
    printf("\n-------- Request fetched --------\n");
    printf("%s\n", buffer);

    /*
      GET /test HTTP/1.1\r\n
      Host: localhost:8082\r\n
      User-Agent: curl/7.81.0\r\n
      Accept: *\/*\r\n
      Authorization: as \r\n
      \r\n


      // Use Content-Length to figure out how big the payload is
      POST /path HTTP/1.0\r\n
      Content-Type: text/plain\r\n
      Content-Length: 12\r\n
      \r\n
      query_string

      // Response
      HTTP/1.1 200 OK\r\n
      Access-Control-Allow-Origin: *\r\n
      Server: abc\r\n
      Content-Length: 200\r\n
      \r\n
      body
    */

    // Parse the bytes received
    // 1. Split the line at \r\n
    // 2. If it turns out to be a GET request, no need for body
    // 3. If response contains Content-length then you need to read the body

    // TODO: Put all of this in struct
    char *method, // "GET" or "POST"
        *uri,     // "/index.html" things before '?'
        *prot;    // "HTTP/1.1"

    char *payload; // for POST
    int payload_size;

    printf("--- test ---\n");

    struct HTTPRequest *http_request = malloc(sizeof(struct HTTPRequest));

    method = strtok(buffer, " ");
    puts(method);
    uri = strtok(NULL, " ");
    uri = uri + 1; // remove the leading '/'
    puts(uri);
    prot = strtok(NULL, " \r\n");
    puts(prot);

    strcpy(http_request->method, method);
    strcpy(http_request->uri, uri);
    strcpy(http_request->protocol, prot);

    

    bool headers_done = false;
    char *payload_body;

    printf("---------- HEADERS ----------\n");
    while (!headers_done) {
      char *key, *value, *next;
      key = strtok(NULL, "\r\n: \t");
      value = strtok(NULL, "\r\n");
      while (*value && *value == ' ')
        value++;
      printf("Key: %s, Value: %s\n", key, value);

      if (strcasecmp(key, HOST_HEADER) == 0) {
        strcpy(http_request->host_hdr, value);
      } else if (strcasecmp(key, AUTHORIZATION_HEADER) == 0) {
        strcpy(http_request->authorization_hdr, value);
      } else if (strcasecmp(key, CONTENT_TYPE_HEADER) == 0) {
        strcpy(http_request->content_type_hdr, value);
      } else if (strcasecmp(key, CONTENT_LENGTH_HEADER) == 0) {
        http_request->content_length_hdr = atoi(value);
      }

      // we want to keep checking if the next line after header is \r\n. Which
      // indicated the end of headers and start of body
      next = value + strlen(value) + 2;
      if (next[0] == '\r' && next[1] == '\n') {
        printf("payload beginning %c\n", next[5]);
        payload_body = next;
        headers_done = true;
      }
    }

    payload_body = strtok(NULL, "\0");
    // The headers and payload body is seperated by "\r\n\r\n", hence move the
    // pointer that much forward
    payload_body = payload_body + 3;
    printf("---------- payload body ----------\n");
    printf("%s\n", payload_body);
    printf("---- done ----\n");

    strcpy(http_request->paylod, payload_body);

    printHTTPRequest(http_request);
  }

  close(client_fd);
  close(server_fd);

  // printf() displays the string inside quotation
  printf("Hello, World!");
  return 0;
}
