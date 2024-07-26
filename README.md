# shadow

shadow is a simple HTTP server for serving files, especially large files. The
video below shows two clients making the requests to server to fetch a 2GB file.


https://github.com/user-attachments/assets/3aae2468-63c8-4da4-91a3-b503f007e564

Another video below shows fetching a 11.5 GB file. The time taken to fetch that file
in chunks of 200 MB is around 2 minutes (its very fast, beacause it was tested locally :P).
The video also shows that the baseline memory consumption of the process is around 400 MB \o/



https://github.com/user-attachments/assets/153e620c-cc55-44c2-a0ed-e7469409282e





## Supported HTTP Methods

It supports the following HTTP Methods
1. HEAD
2. GET
    * Simple GET requests loads the entire file into memory and sends that back
      in HTTP response
    * [HTTP Range
      Request](https://developer.mozilla.org/en-US/docs/Web/HTTP/Range_requests),
      This fetches the file partially. Useful for serving large files without
      worrying about memory 

## Installation

To install and run Shadow, follow these steps:

1. **Clone the repository**:
    ```sh
    git clone https://github.com/yourusername/shadow.git
    cd shadow
    ```

2. **Compile the server**:
    ```sh
    gcc -o shadow server.c -pthread
    ```

3. **Run the server**:
    ```sh
    ./shadow
    ```

    The server spins up on the port `8082`

## Usage

Once the server is running, you can use clients like `curl`, web browser or the toy `client.py` to fetch files. For eg:

- To fetch the headers of a file:
    ```sh
    curl -I http://localhost:8082/yourfile
    ```

Note, `yourfile` needs to be the full path to the file you are requesting. In
case you aren't sure if a file exists - use the HEAD request to verify that.

### Server Information

To get the server information, use `HEAD` request. 

- **Request**
    ```sh
    curl -I http://localhost:8082
    ```
- **Response**
    ```HTTP
    HTTP/1.1 200 OK
    Content-Length: 4096
    Accept-Ranges: bytes
    Content-Type: application/octet-stream
    ```

The
[Accept-Ranges](https://developer.mozilla.org/en-US/docs/Web/HTTP/Range_requests)
response header signifies that fetching a partial files is allowed by the
server.

### Get the entire file

To get the complete file, issue a GET request with the full path of the file as URI

- **Request**
    ```sh
    curl http://localhost:8082/var/www/test_file.png  -H "Authorization: secret" >> file.png
    ```
- **Response**
    ```HTTP
    HTTP/1.1 200 OK
    Content-Length: 2048
    Content-Type: application/octet-stream

    <PAYLOAD>
    ```

The above request fetches the file from server and stores it in `file.png` file on the client.

> This method is not recommened for fetching large files. 

In this method, the server loads the entire file into it's memory and then add
the binary data to the payload. If the file is too large and if the server does
not have enough memory, it'll die.

### Get a part of the file (Recommened for large files)

To get a part of the file, issue a GET request with the `Range` Header specifying what data you want from the file. The full path of the file needs to be given as URI. This is possible due to the [HTTP Range Requests](https://developer.mozilla.org/en-US/docs/Web/HTTP/Range_requests).

- **Request**
    ```sh
    curl http://localhost:8082/var/www/test_file.png  -H "Authorization: secret" -H "Range: bytes=0-50000" >> file.png
    ```
- **Response**
    ```HTTP
    HTTP/1.1 206 Partial Content
    Content-Length: 50001
    Content-Type: application/octet-stream
    Content-Range: bytes 0-50000/125246

    <PAYLOAD>
    ```

This request fetches the partial file content. Clients can use this method to
fetch file in chunks so that server does not run out of memory. It is client's
responsibility to stitch the chunks to form the whole file.

A sample client is provided to do the same.

- **Install Sample Client**
    ```sh
    pip -r requirements.txt
    ```
- **Run the client**
    ```sh
    python client.py http://localhost:8082/tmp/upload_test /tmp/upload_test /tmp/upload_test_2 102400
    ```
    This requests the `upload_test` file from server in chunks of 100MB and
    stores it in this location `/tmp/upload_test_2` on client computer.
