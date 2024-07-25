#include <asm-generic/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// errors
#define FILE_NOT_FOUND -2

// HTTP Status code
#define HTTP_OK 200
#define HTTP_NOT_FOUND 404
#define HTTP_PARTIAL_CONTENT 206
#define HTTP_INTERNAL_ERROR 500

// todo: take this as an argument
#define PORT 8082
#define BUFFER_SIZE 309715200 // 300 MiB

// NOTE: choose appropriate buffer size, If the amount of data you want to
// receive/send in HTTP request-response cycle is greater than 200 MiB, then increase.
// #define BUFFER_SIZE 104857600 // 100 MiB 
// #define BUFFER_SIZE 16106127360 // 15 GiB

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

// Ref: https://developer.mozilla.org/en-US/docs/Web/HTTP/Range_requests
struct HTTPRangeRequest {
  long start;
  long end;
  char range_type[30];
};

struct HTTPRangeResponse {
  long start;
  long end;
  long total_length;
};

struct HTTPRequest {
  // Method details
  char method[30];   // "GET" or "POST"
  char uri[200];     // "/index.html" things before '?' (used for GET request)
  char protocol[30]; // "HTTP/1.1"

  // Headers
  char host_hdr[30];                 // "localhost:8082"
  char authorization_hdr[30];        // <secret>
  char content_type_hdr[30];         // "application/json"
  long content_length_hdr;           // 38 (used for POST request)
  char range_hdr_raw[30];            // "0-100" (used for range request)
  struct HTTPRangeRequest range_hdr; // 0-100 (used for range request)

  // Payload
  char payload[BUFFER_SIZE]; // { "title":"foo","body":"bar", "id": 1}
};

struct HTTPResponse {
  char protocol[30];       // "HTTP/1.1"
  int status_code;         // 200
  char status_message[30]; // "OK"

  // Headers
  char content_type_hdr[30];          // "application/json"
  char accept_ranges_hdr[30];         // "bytes"
  long content_length_hdr;            // 38 (sent back in GET response)
  struct HTTPRangeResponse range_hdr; // 0-100 (used for range request)

  // Payload
  char payload[BUFFER_SIZE]; // { "title":"foo","body":"bar", "id": 1}
};

struct HTTPResponseMetadata {
  char *response_string;
  long response_length;
};

/*  ---------------- util function ------------------------------*/

void printHTTPRequest(struct HTTPRequest *request) {
  printf("-------------- HTTP Request -----------\n");
  printf("Method: %s\n", request->method);
  printf("URI: %s\n", request->uri);
  printf("Protocol: %s\n", request->protocol);
  printf("Host Header: %s\n", request->host_hdr);
  printf("Authorization Header: %s\n", request->authorization_hdr);
  printf("Content-Type Header: %s\n", request->content_type_hdr);
  printf("Content-Length Header: %ld\n", request->content_length_hdr);
  printf("Range: %ld - %ld\n", request->range_hdr.start,
         request->range_hdr.end);
  printf("Payload: %s\n", request->payload);
}

void printHTTPResponse(struct HTTPResponse *http_response) {
  printf("-------------- HTTP Response -----------\n");
  printf("%s %d %s\n", http_response->protocol, http_response->status_code,
         http_response->status_message);
  printf("Content-Length: %ld\n", http_response->content_length_hdr);
  printf("Accept-Ranges: %s\n", http_response->accept_ranges_hdr);
  printf("Content-Type: %s\n", http_response->content_type_hdr);
  printf("Content-Range: %ld-%ld/%ld\n", http_response->range_hdr.start,
         http_response->range_hdr.end, http_response->range_hdr.total_length);
  printf("Payload: %s\n", http_response->payload);
}

long get_file_length(char *file_name) {
  FILE *fp = fopen(file_name, "r");
  if (fp == NULL) {
    return FILE_NOT_FOUND;
  }
  int file_descriptor = fileno(fp);
  if (file_descriptor < 0) {
    perror("error getting file descriptor");
    exit(EXIT_FAILURE);
  }
  // Get the file attributes
  // This allows us to get the size of file without opening it
  struct stat file_stat;
  fstat(file_descriptor, &file_stat);

  fclose(fp);
  return file_stat.st_size;
}

size_t get_file_content(char *file_name, unsigned char *buffer,
                        long file_size) {
  FILE *fp = fopen(file_name, "rb");
  if (fp == NULL) {
    return FILE_NOT_FOUND;
  }
  size_t bytes_read = fread(buffer, 1, file_size, fp);
  // The total bytes read should be same as the file size
  if (bytes_read != file_size) {
    perror("error reading file");
    exit(EXIT_FAILURE);
  }

  fclose(fp);
  return bytes_read;
}

size_t get_file_content_range(char *file_name, unsigned char *buffer,
                              long start, long range_length) {
  FILE *fp = fopen(file_name, "rb");
  if (fp == NULL) {
    return FILE_NOT_FOUND;
  }
  fseek(fp, start, SEEK_SET);
  size_t bytes_read = fread(buffer, 1, range_length, fp);

  // The total bytes read should be same as the file size
  fclose(fp);
  return bytes_read;
}

void generate_http_response(int code, struct HTTPResponse *http_response) {
  switch (code) {
  case HTTP_OK:
    http_response->status_code = 200;
    strcpy(http_response->status_message, "OK");
    strcpy(http_response->content_type_hdr, "application/octet-stream");
    break;
  case HTTP_PARTIAL_CONTENT:
    http_response->status_code = 206;
    strcpy(http_response->status_message, "Partial Content");
    strcpy(http_response->content_type_hdr, "application/octet-stream");
    break;
  case HTTP_NOT_FOUND:
    http_response->status_code = 404;
    strcpy(http_response->status_message, "Not Found");
    break;
  default:
    http_response->status_code = 500;
    strcpy(http_response->status_message, "Internal Server Error");
    break;
  }
}

/*  ---------------- util function ------------------------------*/

long construct_http_response_string(struct HTTPResponse *http_response, struct HTTPRequest *http_request,
                                    char *response_string) {

  if (response_string == NULL) {
    perror("Failed to allocate memory");
    exit(EXIT_FAILURE);
  }

  // Initialize the response string
  response_string[0] = '\0';

  // Add the status line
  sprintf(response_string, "%s %d %s\r\n", http_response->protocol,
          http_response->status_code, http_response->status_message);

  // Add headers if they are not empty

  char content_length[64];
  sprintf(content_length, "Content-Length: %ld\r\n",
          http_response->content_length_hdr);
  strcat(response_string, content_length);

  if (strlen(http_response->accept_ranges_hdr) > 0) {
    strcat(response_string, "Accept-Ranges: ");
    strcat(response_string, http_response->accept_ranges_hdr);
    strcat(response_string, "\r\n");
  }
  if (strlen(http_response->content_type_hdr) > 0) {
    strcat(response_string, "Content-Type: ");
    strcat(response_string, http_response->content_type_hdr);
    strcat(response_string, "\r\n");
  }

  if (http_response->status_code == HTTP_PARTIAL_CONTENT) {
    char content_range[64];
    sprintf(content_range, "Content-Range: bytes %ld-%ld/%ld\r\n",
            http_response->range_hdr.start, http_response->range_hdr.end,
            http_response->range_hdr.total_length);
    strcat(response_string, content_range);
  }

  // End headers section
  strcat(response_string, "\r\n");

  long response_length = strlen(response_string);

  // Add the payload only when the method is GET
  if (strcasecmp(http_request->method, "GET") == 0)
    for (long i = 0; i < http_response->content_length_hdr; i++) {
    response_string[response_length] = (unsigned char)http_response->payload[i];
    response_length++;
  }
  

  return response_length;
}

struct HTTPRangeRequest *parse_http_range_request_hdr(char *range_hdr) {
  struct HTTPRangeRequest *range_request =
      malloc(sizeof(struct HTTPRangeRequest));
  char *range = malloc(strlen(range_hdr) * sizeof(char));
  strcpy(range, range_hdr);
  char *range_type = strtok(range, "=");
  char *range_start = strtok(NULL, "-");
  char *range_end = strtok(NULL, "");

  range_request->start = atoi(range_start);
  range_request->end = atoi(range_end);
  strcpy(range_request->range_type, range_type);

  return range_request;
}

// Parse the raw HTTP request and populate the HTTPRequest struct
void parse_http_request(char *buffer, struct HTTPRequest *http_request) {
  char *method, *uri, *prot, *payload;
  int payload_size;

  // Parse the request line
  // GET /test HTTP/1.1\r\n
  method = strtok(buffer, " ");
  uri = strtok(NULL, " ");
  // uri = uri + 1; // remove the leading '/'
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
    } else if (strcasecmp(key, "Range") == 0) {
      strcpy(http_request->range_hdr_raw, value);
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
  strcpy(http_request->payload, payload_body);

  // TODO: Maybe strtok_r can be used to make this function reentrant?
  // Since strtok is not a reentrant function, we weren't able to parse the
  // range header in the same loop. Hence, we parse it here
  if (strlen(http_request->range_hdr_raw) > 0) {
    http_request->range_hdr =
        *parse_http_range_request_hdr(http_request->range_hdr_raw);
    printf("Range: %ld - %ld\n", http_request->range_hdr.start,
           http_request->range_hdr.end);
  }
}

int process_head_http_request(struct HTTPRequest *http_request,
                              struct HTTPResponse *http_response) {
  // Read the file length. Store it in content length. If file not found return
  // 404
  long file_length = get_file_length(http_request->uri);
  if (file_length == FILE_NOT_FOUND) {
    return HTTP_NOT_FOUND;
  }
  http_response->content_length_hdr = file_length;
  strcpy(http_response->protocol, http_request->protocol);
  strcpy(http_response->accept_ranges_hdr, "bytes");
  strcpy(http_response->content_type_hdr, "application/octet-stream");

  return HTTP_OK;
}

int proccess_get_http_request(struct HTTPRequest *http_request,
                              struct HTTPResponse *http_response) {

  strcpy(http_response->protocol, http_request->protocol);

  // Read the file length. Store it in content length. If file not found return
  // 404
  long file_length = get_file_length(http_request->uri);
  if (file_length == FILE_NOT_FOUND) {
    return HTTP_NOT_FOUND;
  }

  http_response->content_length_hdr = file_length;

  // Read the file content and store it in the payload
  unsigned char *file_content = malloc(file_length * sizeof(char));
  size_t bytes_read =
      get_file_content(http_request->uri, file_content, file_length);
  // Add a null terminator to the end of the file content
  file_content[bytes_read] = '\0';
  // The contents of the file should be treated as binary and as such should be
  // copied as is. Hence, we use memcpy instead of strcpy
  memcpy(http_response->payload, file_content, bytes_read);
  return HTTP_OK;
}

int process_get_http_range_request(struct HTTPRequest *http_request,
                                   struct HTTPResponse *http_response) {

  strcpy(http_response->protocol, http_request->protocol);

  // Validate the range requests
  if (http_request->range_hdr.start < 0 || http_request->range_hdr.end < 0) {
    return HTTP_INTERNAL_ERROR;
  }
  if (http_request->range_hdr.start > http_request->range_hdr.end) {
    return HTTP_INTERNAL_ERROR;
  }

  long file_length = get_file_length(http_request->uri);
  if (file_length == FILE_NOT_FOUND) {
    return HTTP_NOT_FOUND;
  }

  http_response->range_hdr.total_length = file_length;

  long request_bytes_length =
      http_request->range_hdr.end - http_request->range_hdr.start + 1;

  // TODO: What do we do, if the range request is beyond the BUFFER_LENGTH
  // allowed?
  unsigned char *partial_file_content =
      malloc(request_bytes_length * sizeof(char));

  size_t bytes_read = get_file_content_range(
      http_request->uri, partial_file_content, http_request->range_hdr.start,
      request_bytes_length);

  // Add a null terminator to the end of the file content
  partial_file_content[bytes_read] = '\0';
  // The contents of the file should be treated as binary and as such should be
  // copied as is. Hence, we use memcpy instead of strcpy
  memcpy(http_response->payload, partial_file_content, bytes_read);

  // Set the "Content-Range" and "Content-Length" header
  http_response->range_hdr.start = http_request->range_hdr.start;
  http_response->range_hdr.end = http_request->range_hdr.end;
  http_response->content_length_hdr = bytes_read;

  return HTTP_PARTIAL_CONTENT;
}

void process_http_request(struct HTTPRequest *http_request,
                          struct HTTPResponse *http_response) {
  int response_code;
  // 1. Check the http method and process accordingly
  // 2. If GET, then fetch the file and send it back
  if (strcasecmp(http_request->method, "HEAD") == 0) {
    response_code = process_head_http_request(http_request, http_response);
  } else if (strcasecmp(http_request->method, "GET") == 0) {
    if (strlen(http_request->range_hdr_raw) > 0) {
      response_code =
          process_get_http_range_request(http_request, http_response);
    } else {
      response_code = proccess_get_http_request(http_request, http_response);
    }

  } else {
    response_code = HTTP_NOT_FOUND;
  }

  generate_http_response(response_code, http_response);

  return;
}

void send_http_request(int client_fd, struct HTTPResponse *http_response, struct HTTPRequest *http_request) {
  // Allocate a buffer for the response string
  char *response_string = malloc(BUFFER_SIZE); // Adjust size as needed
  long total_response_bytes =
      construct_http_response_string(http_response, http_request, response_string);
  long bytes_sent = send(client_fd, response_string, total_response_bytes, 0);

  if (bytes_sent < 0) {
    perror("error sending data to client");
    exit(EXIT_FAILURE);
  }
}

// Handle the client request
void handle_client_request(int client_fd) {
  char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
  int bytes_recvd;

  char *method, *uri, *prot, *payload;
  int payload_size;
  struct HTTPRequest *http_request = malloc(sizeof(struct HTTPRequest));
  struct HTTPResponse *http_response = malloc(sizeof(struct HTTPResponse));

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
  process_http_request(http_request, http_response);
  printHTTPResponse(http_response);
  send_http_request(client_fd, http_response, http_request);
  printf("\nResponse sent\n");
  printf("-------------------------------\n\n");

  return;
}

int main(int argc, char *argv[]) {

  int server_fd, client_fd;

  struct sockaddr_in server_addr;

  // 1. Create server socket
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  int option = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
  if (server_fd < 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  // 2. Configure the socket
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY; // the host address
  server_addr.sin_port = htons(PORT);       // convert to network byte order

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