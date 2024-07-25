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

/* Note about HTTTP Request and Response

  A sample GET request:
    GET /test HTTP/1.1\r\n
    Host: localhost:8082\r\n
    User-Agent: curl/7.81.0\r\n
    Authorization: as \r\n
    \r\n

  A sample POST request:
    POST /path HTTP/1.0\r\n
    Content-Type: text/plain\r\n
    Content-Length: 12\r\n
    \r\n
    query_string

  A sample response:
    HTTP/1.1 200 OK\r\n
    Access-Control-Allow-Origin: *\r\n
    Server: abc\r\n
    Content-Length: 200\r\n
    \r\n
    body

  According to the HTTP RFC, the first line of the request is the request line
  and the rest of the lines are headers followed by an optional body. Each of
  the lines are separated by \r\n. The headers are separated from the body by
  the \r\n.

  If the payload is present, the Content-Length header is used to determine the
  size.
*/
struct HTTPRequest {
  // Method details
  char method[4];    // "GET" or "POST"
  char uri[25];      // "/index.html" things before '?' (used for GET request)
  char protocol[10]; // "HTTP/1.1"

  // Headers
  char host_hdr[20];          // "localhost:8082"
  char authorization_hdr[20]; // <secret>
  char content_type_hdr[20];  // "application/json"
  int content_length_hdr;     // 38 (used for POST request)

  // Payload
  char paylod[BUFFER_SIZE]; // { "title":"foo","body":"bar", "id": 1}
};

struct HTTPResponse {
  char protocol[10];  // "HTTP/1.1"
  int status_code;    // 200
  int status_message; // "OK"

  // Headers
  char content_type_hdr[20]; // "application/json"
  int content_length_hdr;    // 38 (sent back in GET response)

  // Payload
  char paylod[BUFFER_SIZE]; // { "title":"foo","body":"bar", "id": 1}
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

// Parse the raw HTTP request and populate the HTTPRequest struct
void parse_http_request(char *buffer, struct HTTPRequest *http_request) {
  char *method, *uri, *prot, *payload;
  int payload_size;

  // Parse the request line
  // GET /test HTTP/1.1\r\n
  method = strtok(buffer, " ");
  uri = strtok(NULL, " ");
  uri = uri + 1; // remove the leading '/'
  prot = strtok(NULL, " \r\n");

  strcpy(http_request->method, method);
  strcpy(http_request->uri, uri);
  strcpy(http_request->protocol, prot);

  bool headers_done = false;
  char *payload_body;

  // Parse headers
  // Note: Currently we only support the headers mentioned in the Header
  // constants
  while (!headers_done) {
    char *key, *value, *next;

    // Extract the key and value from the header
    key = strtok(NULL, "\r\n: \t");
    value = strtok(NULL, "\r\n");
    while (*value && *value == ' ')
      value++;

    if (strcasecmp(key, HOST_HEADER) == 0) {
      strcpy(http_request->host_hdr, value);
    } else if (strcasecmp(key, AUTHORIZATION_HEADER) == 0) {
      strcpy(http_request->authorization_hdr, value);
    } else if (strcasecmp(key, CONTENT_TYPE_HEADER) == 0) {
      strcpy(http_request->content_type_hdr, value);
    } else if (strcasecmp(key, CONTENT_LENGTH_HEADER) == 0) {
      http_request->content_length_hdr = atoi(value);
    }

    // we want to keep checking if the next line after header is \r\n which
    // indicates the end of headers and start of payload body
    next = value + strlen(value) + 2;
    if (next[0] == '\r' && next[1] == '\n') {
      payload_body = next;
      headers_done = true;
    }
  }

  // The pointer is now at the start of the payload body
  payload_body = strtok(NULL, "\0");
  // The headers and payload body is seperated by "\r\n\r\n", hence move the
  // pointer that much forward
  payload_body = payload_body + 3;
  strcpy(http_request->paylod, payload_body);
}

void handle_client_request(int client_fd) {
  char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
  int bytes_recvd;

  char *method, *uri, *prot, *payload;
  int payload_size;
  struct HTTPRequest *http_request = malloc(sizeof(struct HTTPRequest));

  // Get the data from client and store it in buffer
  // FIXME: Current assumption is that, we get the entire request in a single
  // packet. But this is not true. We need to keep reading from the socket until
  // we get the entire request or the `/r/n/r/n` which indicates the end of the
  // request.
  // REF:
  // https://stackoverflow.com/questions/49821687/how-to-determine-if-i-received-entire-message-from-recv-calls
  bytes_recvd = recv(client_fd, buffer, BUFFER_SIZE, 0);

  // Validate the received bytes
  if (bytes_recvd < 0) {
    perror("error reading data from socket");
    exit(EXIT_FAILURE);
  }

  if (bytes_recvd == 0) {
    perror("client disconnected upexpectedly");
    exit(EXIT_FAILURE);
  }

  // Add a null terminator to know that we have reached the end of the HTTP
  // request This is useful for getting the payload body below.
  buffer[bytes_recvd] = '\0';
  parse_http_request(buffer, http_request);

  printHTTPRequest(http_request);

  return;
}

int main(int argc, char *argv[]) {

  int server_fd, client_fd;
  char buffer[256];

  struct sockaddr_in server_addr;

  // 1. Create server socket
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
  server_addr.sin_addr.s_addr = INADDR_ANY; // the host address
  server_addr.sin_port = htons(PORT); // convert to network byte order

  // 3. bind socket to port
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  // 4. listen for connections
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

    if (client_fd < 0) {
      perror("bind failed");
      exit(EXIT_FAILURE);
    }

    // process the request
    handle_client_request(client_fd);
  }

  close(client_fd);
  close(server_fd);

  return 0;
}