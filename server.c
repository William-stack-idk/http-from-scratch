#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

//------------------------------------------------------------------
/**
 * @brief Print a string with escaped newline and carriage return characters.
 *
 * This function prints the given string, replacing newline ('\n') characters with "\\n"
 * and carriage return ('\r') characters with "\\r". It is useful for debugging and
 * displaying strings with non-printable characters.
 *
 * @param str The input string to be printed.
 */
void printStringWithEscapeChars(const char *str){
  for(int i = 0; str[i]!='\0'; i++){
    if(str[i] == '\n') printf("\\n");
    if(str[i] == '\r') printf("\\r");
    else putchar(str[i]);
  }
  printf("\n");
}

/**
 * @brief Read the contents of a file into a dynamically allocated buffer.
 *
 * This function opens the specified file, reads its contents into a dynamically allocated
 * buffer, and returns a pointer to the buffer. It also sets the size of the file in the
 * `size` parameter.
 *
 * @param filename The name of the file to be read.
 * @param size A pointer to a long variable where the size of the file will be stored.
 * @return A pointer to the dynamically allocated buffer containing the file contents,
 *         or NULL if an error occurs.
 *
 * @note The caller is responsible for freeing the memory allocated for the file contents
 *       when it is no longer needed.
 * @warning Ensure that the returned buffer is properly freed to prevent memory leaks.
 */
char *ReadFile(const char *filename, long *size) {
    FILE *file = fopen(filename, "rb"); // Open in binary mode
    if (file == NULL) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        return NULL;
    }

    // Get the size of the file
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate memory for the file contents
    char *file_contents = (char *)malloc(file_size + 1);
    if (file_contents == NULL) {
        fprintf(stderr, "Memory allocation error.\n");
        fclose(file);
        return NULL;
    }

    // Read the entire file into the buffer
    size_t bytes_read = fread(file_contents, 1, file_size, file);
    if (bytes_read != (size_t)file_size) {
        fprintf(stderr, "Error reading file.\n");
        fclose(file);
        free(file_contents);
        return NULL;
    }

    // Null-terminate the string
    file_contents[file_size] = '\0';

    // Clean up and close the file
    fclose(file);

    // Set the size parameter
    *size = file_size;

    return file_contents;
}

//------------------------------------------------------------------
#define MAX_METHOD_SIZE 10
#define MAX_PATH_SIZE 100
#define MAX_BODY_SIZE 4096
#define MAX_STATUS_MESSAGE_SIZE 50

/**
 * @brief Structure representing an HTTP request.
 *
 * This structure stores information about an HTTP request, including the HTTP method,
 * the request path, and, if applicable, the request body.
 */
typedef struct {
    char method[MAX_METHOD_SIZE]; ///< The HTTP method (e.g., "GET").
    char path[MAX_PATH_SIZE];     ///< The request path (e.g., "/index.html").
    char *body;                   ///< Pointer to the request body (may be NULL if not present).
    size_t body_size;             ///< Size of the request body (0 if not present).
} HttpRequest;

/**
 * @brief Structure representing an HTTP response.
 *
 * This structure stores information about an HTTP response, including the HTTP status code,
 * status message, content length, and the response content.
 */
typedef struct {
    int status_code;            ///< The HTTP status code (e.g., 200, 404).
    char status_message[MAX_STATUS_MESSAGE_SIZE];    ///< The HTTP status message (e.g., "OK", "Not Found").
    size_t content_length;      ///< The length of the response content.
    char *content;              ///< Pointer to the response content.
} HttpResponse;

/**
 * @brief Structure representing a route mapping for an HTTP server.
 *
 * This structure is used to map URL paths to corresponding resources or files.
 * It also allows for associating optional callback functions to handle requests
 * for specific routes.
 */
typedef struct {
    char path[MAX_PATH_SIZE];       ///< The URL path to match.
    char link[MAX_PATH_SIZE];       ///< The corresponding file or resource path.
    void (*callback)(HttpRequest); ///< Optional callback function to handle requests for this route.
} RouteMapping;


/**
 * @brief Global array of GET method route mappings for the HTTP server.
 *
 * This array defines the routes and associated resources for handling HTTP GET requests.
 * Each element in the array represents a route mapping, consisting of a URL path,
 * a corresponding file path, and an optional callback function.
 */
RouteMapping getRouteMappings[] = {
    {"/", "./public_html/index.html", NULL},
    {"/test", "./public_html/test.html", NULL},
    // Add more route mappings as needed
};

/**
 * @brief Parse an HTTP request string and create an HttpRequest structure.
 *
 * This function takes an HTTP request string and extracts information such as the HTTP method,
 * request path, and request body size and content. It creates and initializes an HttpRequest structure
 * with the parsed data.
 *
 * @param request The HTTP request string to parse.
 * @return A pointer to the created and populated HttpRequest structure. The caller is responsible for freeing
 * the memory allocated for the structure when it's no longer needed. Returns NULL if parsing fails
 * or if the input is invalid.
 */
HttpRequest* parseHttpRequest(const char* request) {
    if (request == NULL) {
        // Handle invalid input
        return NULL;
    }

    HttpRequest *http = malloc(sizeof(HttpRequest));
    if (http == NULL) {
        fprintf(stderr, "Allocation in parseHttpRequest: %s\n", strerror(errno));
        return NULL;
    }

    char* method;
    char* path;
    char* start;

    // Initialize the HTTP struct
    memset(http, 0, sizeof(HttpRequest));

    // Create a copy of the request string
    char *request_copy = strdup(request);
    if (request_copy == NULL) {
        fprintf(stderr, "Allocation in parseHttpRequest: %s\n", strerror(errno));
        free(http);
        return NULL;
    }

    method = strtok(request_copy, " ");
    path = strtok(NULL, " ");
    if (method && path) {
        strncpy(http->method, method, sizeof(http->method) - 1);
        strncpy(http->path, path, sizeof(http->path) - 1);

        // Parse Content-Length header
        start = strstr(request, "Content-Length:");
        if (start) {
            sscanf(start, "Content-Length: %zu", &http->body_size);

            start = strstr(request, "\r\n\r\n");
            if (start) {
                start += 4;
                size_t available_space = sizeof(http->body) - 1; // Leave space for null terminator
                size_t copy_size = http->body_size < available_space ? http->body_size : available_space;
                strncpy(http->body, start, copy_size);
                http->body[copy_size] = '\0'; // Null-terminate the body
            }
        }
    }

    free(request_copy);
    return http;
}


/**
 * @brief Convert an HttpResponse struct to an HTTP response string.
 *
 * This function takes an HttpResponse struct and converts it into a well-formed
 * HTTP response string. The resulting string includes the HTTP status code,
 * status message, content length, content type, and the response content.
 *
 * @param response Pointer to the HttpResponse struct to be converted.
 * @param size Pointer to a long variable to store the size of the generated string (optional).
 * @return A dynamically allocated string representing the HTTP response. The caller is responsible
 * for freeing the memory when it's no longer needed. Returns NULL on allocation failure.
 */
char* HttpResponseToString(const HttpResponse *response, long *size) {
    int total_length = snprintf(NULL, 0, "HTTP/1.1 %d %s\r\nContent-Type: text/html;charset=UTF-8\r\nContent-Length: %zu\r\n\r\n%s",
        response->status_code, response->status_message, response->content_length, response->content);

    if (total_length < 0) {
        // Handle snprintf error
        fprintf(stderr, "Error in snprintf\n");
        free(response->content);
        return NULL;
    }

    char *http_response = malloc(total_length + 1);
    if (http_response == NULL) {
        // Handle memory allocation failure
        fprintf(stderr, "Memory allocation error in HttpResponseToString\n");
        free(response->content);
        return NULL;
    }

    snprintf(http_response, total_length + 1, "HTTP/1.1 %d %s\r\nContent-Type: text/html;charset=UTF-8\r\nContent-Length: %zu\r\n\r\n%s",
        response->status_code, response->status_message, response->content_length, response->content);

    if (size != NULL) {
        *size = total_length;
    }

    return http_response;
}

/**
 * @brief Handle an HTTP GET request and send an appropriate response.
 *
 * This function processes an HTTP GET request, attempts to match the request path to configured routes,
 * and sends a corresponding HTTP response. If a matching route is found, it reads the content from
 * the specified file and sends a "200 OK" response with the content. If no matching route is found,
 * it sends a "404 Not Found" response.
 *
 * @param client_socket The socket connected to the client.
 * @param request Pointer to the HttpRequest structure representing the request.
 */
void handleGetRequest(int client_socket, const HttpRequest *request) {
    HttpResponse response;
    long size = 0;
    response.status_code = 404;
    strcpy(response.status_message, "Not Found");
    response.content_length = size;
    response.content = malloc(1);
    strcpy(response.content, "");

    for (size_t i = 0; i < (sizeof(getRouteMappings) / sizeof(RouteMapping)); i++) {
        if (strncmp(request->path, getRouteMappings[i].path, MAX_PATH_SIZE) == 0) {
            response.status_code = 200;
            strcpy(response.status_message, "OK");
            char *content = ReadFile(getRouteMappings[i].link, &size);
            if (content != NULL) {
                response.content_length = size;
                free(response.content);
                response.content = content;
            } else {
                // Handle file read error, e.g., by sending a 500 Internal Server Error response
                response.status_code = 500;
                strcpy(response.status_message, "Internal Server Error");
            }
            break; // Stop searching for routes once a match is found
        }
    }

    char *response_message = HttpResponseToString(&response, &size); // Don't need the size here
    ssize_t bytes_sent = send(client_socket, response_message, size, 0);

    free(response.content); // Free content memory

    if (bytes_sent == -1) {
        fprintf(stderr, "Error sending response: %s\n", strerror(errno));
    }
    printf("Response Sent: \n");
    printStringWithEscapeChars(response_message);
    free(response_message);
}


/**
 * @brief Handle an HTTP request and route it based on the request method.
 *
 * This function dispatches the HTTP request to specific handling functions based on the request method.
 *
 * @param client_socket The socket connected to the client.
 * @param request Pointer to the HttpRequest structure representing the request.
 */
void handleHttpRequest(int client_socket, const HttpRequest* request) {
    if (strncmp(request->method, "GET", 3) == 0) {
        handleGetRequest(client_socket, request);
    } else {
        // Handle unsupported methods or other errors here
        fprintf(stderr, "Unsupported HTTP method: %s\n", request->method);
    }
}

/**
 * @brief Start an HTTP server that listens on a specified IP address and port.
 *
 * This function creates a TCP socket, binds it to the specified IP address and port, and listens
 * for incoming connections. When a client connects, it accepts the connection, receives HTTP
 * requests, and dispatches them to a handler function for processing.
 *
 * @param addr The IP address to bind the server to.
 * @param port The port number to listen on.
 * @return 0 on success, -1 on failure.
 */
int startHttpServer(in_addr_t addr, uint16_t port) {
    // Create a TCP socket and set up server configuration
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        fprintf(stderr, "Failed to create server socket\n");
        return -1;
    }

    // Bind the socket to the specified IP address and port
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = port;
    server_address.sin_addr.s_addr = addr;

    if (bind(server_socket, (struct sockaddr *)&server_address,
             sizeof(server_address)) == -1) {
        fprintf(stderr, "Failed to bind server socket\n");
        close(server_socket);
        return -1;
    }

    // Listen for incoming connections
    if (listen(server_socket, 10) == -1) {
        fprintf(stderr, "Failed to listen on server socket\n");
        close(server_socket);
        return -1;
    }
    printf("\nServer Listening\n");
    struct sockaddr_in client_address;
    socklen_t client_address_length = sizeof(client_address);

    while (1) {
        // Accept incoming connections
        printf("\n---------Waiting for new connection---------\n\n");
        int client_socket =
            accept(server_socket, (struct sockaddr *)&client_address,
                   &client_address_length);
        if (client_socket == -1) {
            fprintf(stderr, "Error accepting client connection: %s\n",
                    strerror(errno));
            continue;
        }
        printf("Connection Established\n");
        // Receive and process HTTP requests
        char buffer[30000] = {0};
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);

        if (bytes_received == -1) {
            fprintf(stderr, "Error receiving data: %s\n", strerror(errno));
            close(client_socket);
            continue;
        }
        printf("Received Data:\n");
        printStringWithEscapeChars(buffer);
        HttpRequest *http = parseHttpRequest(buffer);
        handleHttpRequest(client_socket, http);

        // Clean up resources
        close(client_socket);
        free(http);
    }

    // Clean up the server socket
    close(server_socket);

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP address> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* ipAddressStr = argv[1];
    const char* portStr = argv[2];

    struct in_addr addr;
    if (inet_pton(AF_INET, ipAddressStr, &addr) != 1) {
        fprintf(stderr, "Invalid IP address: %s\n", ipAddressStr);
        return EXIT_FAILURE;
    }

    unsigned short port = atoi(portStr);
    if (port <= 0 || port > 65534) {
        fprintf(stderr, "Invalid port number: %s\n", portStr);
        return EXIT_FAILURE;
    }

    int result = startHttpServer(addr.s_addr, htons(port));
    if (result == -1) {
        fprintf(stderr, "Failed to start HTTP server\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}