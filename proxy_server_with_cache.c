#include "proxy_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define MAX_BYTES 4096                  // max allowed size of request/response
#define MAX_CLIENTS 20                  // max number of client requests
#define MAX_SIZE 200 * (1 << 20)        // cache size
#define MAX_ELEMENT_SIZE 10 * (1 << 20) // max size of an element in cache
typedef struct cache_element cache_element;
/**
 * @brief Represents a cache entry storing HTTP response data
 */
struct cache_element
{
    char *data;            // data stores response
    int len;               // length of data i.e.. sizeof(data)...
    char *url;             // url stores the request
    time_t lru_time_track; // lru_time_track stores the latest time the element is  accesed
    cache_element *next;   // pointer to next element
};

// Function declarations with documentation
/**
 * @brief Searches for a URL in the cache
 * @param url The URL to search for
 * @return Pointer to cache element if found, NULL otherwise
 */
cache_element *find(char *url);

/**
 * @brief Adds a new element to the cache
 * @param data Response data to cache
 * @param size Size of the response data
 * @param url Request URL to use as cache key
 * @return 1 if successful, 0 if element too large
 */
int add_cache_element(char *data, int size, char *url);

/**
 * @brief Removes the least recently used element from cache
 */
void remove_cache_element();

int port_number = 8080;     // Default Port
int proxy_socketId;         // socket descriptor of proxy server
pthread_t tid[MAX_CLIENTS]; // array to store the thread ids of clients
sem_t seamaphore;           // controls access to the threads

// sem_t cache_lock;
pthread_mutex_t lock; // lock is used for locking the cache

cache_element *head; // pointer to the head of the cache LL
int cache_size;      // current size of the cache

/**
 * @brief Sends an HTTP error response to the client
 * @param socket Client socket descriptor
 * @param status_code HTTP status code to send
 * @return 1 if successful, -1 on error
 */
int sendErrorMessage(int socket, int status_code)
{
    char str[1024];
    char currentTime[50];
    time_t now = time(0);

    struct tm data = *gmtime(&now);
    strftime(currentTime, sizeof(currentTime), "%a, %d %b %Y %H:%M:%S %Z", &data);

    switch (status_code)
    {
    // Sending the respective error message to the client
    case 400:
        snprintf(str, sizeof(str), "HTTP/1.1 400 Bad Request\r\nContent-Length: 95\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: RONIN/14785\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Rqeuest</H1>\n</BODY></HTML>", currentTime);
        printf("400 Bad Request\n");
        send(socket, str, strlen(str), 0);
        break;

    case 403:
        snprintf(str, sizeof(str), "HTTP/1.1 403 Forbidden\r\nContent-Length: 112\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: RONIN/14785\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H1>403 Forbidden</H1><br>Permission Denied\n</BODY></HTML>", currentTime);
        printf("403 Forbidden\n");
        send(socket, str, strlen(str), 0);
        break;

    case 404:
        snprintf(str, sizeof(str), "HTTP/1.1 404 Not Found\r\nContent-Length: 91\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: RONIN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>404 Not Found</H1>\n</BODY></HTML>", currentTime);
        printf("404 Not Found\n");
        send(socket, str, strlen(str), 0);
        break;

    case 500:
        snprintf(str, sizeof(str), "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 115\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: RONIN/14785\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H1>500 Internal Server Error</H1>\n</BODY></HTML>", currentTime);
        // printf("500 Internal Server Error\n");
        send(socket, str, strlen(str), 0);
        break;

    case 501:
        snprintf(str, sizeof(str), "HTTP/1.1 501 Not Implemented\r\nContent-Length: 103\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: RONIN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Implemented</TITLE></HEAD>\n<BODY><H1>501 Not Implemented</H1>\n</BODY></HTML>", currentTime);
        printf("501 Not Implemented\n");
        send(socket, str, strlen(str), 0);
        break;

    case 505:
        snprintf(str, sizeof(str), "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 125\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: RONIN/14785\r\n\r\n<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n<BODY><H1>505 HTTP Version Not Supported</H1>\n</BODY></HTML>", currentTime);
        printf("505 HTTP Version Not Supported\n");
        send(socket, str, strlen(str), 0);
        break;

    default:
        return -1;
    }
    return 1;
}

/**
 * @brief Establishes connection with remote server
 * @param host_addr Remote server hostname/IP
 * @param port_num Remote server port
 * @return Socket descriptor if successful, -1 on error
 */
int connectRemoteServer(char *host_addr, int port_num)
{

    int remoteSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (remoteSocket < 0)
    {
        printf("Bravo-6 to Echo 3-1. The socket couldn't be created\n");
        return -1;
    }

    struct hostent *host = gethostbyname(host_addr);
    if (host == NULL)
    {
        fprintf(stderr, "Echo 3-1 to Bravo-6. The host doesn't exist\n");
        return -1;
    }

    // inserts ip address and port number of host in struct `server_addr`
    struct sockaddr_in server_addr;

    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_num);

    bcopy((char *)host->h_addr_list, (char *)&server_addr.sin_addr.s_addr, host->h_length);

    // Try and connect to Remote server
    if (connect(remoteSocket, (struct sockaddr *)&server_addr, (socklen_t)sizeof(server_addr)) < 0)
    {
        fprintf(stderr, "Bravo-6 to Echo 3-1.The connection has not been established!\n");
        return -1;
    }
    // free(host_addr);
    return remoteSocket;
}

/**
 * @brief Processes an HTTP request and forwards it to remote server
 * @param clientSocket Client socket descriptor
 * @param request Parsed HTTP request
 * @param tempReq Original raw request string
 * @return 0 if successful, -1 on error
 */
int handle_request(int clientSocket, struct ParsedRequest *request, char *tempReq)
{
    char *buf = (char *)malloc(sizeof(char) * MAX_BYTES);
    strcpy(buf, "GET ");
    strcat(buf, request->path);
    strcat(buf, " ");
    strcat(buf, request->version);
    strcat(buf, "\r\n");

    size_t len = strlen(buf);

    if (ParsedHeader_set(request, "Connection", "close") < 0)
    {
        printf("Bravo-6 to Gold Eagle Actual. The set is offline\n");
    }

    if (ParsedHeader_get(request, "Host") == NULL)
    {
        if (ParsedHeader_set(request, "Host", request->host) < 0)
        {
            printf("Set \"Host\" header key not working\n");
        }
    }

    if (ParsedRequest_unparse_headers(request, buf + len, (size_t)MAX_BYTES - len) < 0)
    {
        printf("Gold Eagle Actual to Bravo-6. The unparse is fucked John.\n");
        // return -1;				// If this happens Still try to send request without header
    }

    int server_port = 80; // Default Remote Server Port
    if (request->port != NULL)
        server_port = atoi(request->port);

    int remoteSocketID = connectRemoteServer(request->host, server_port);

    if (remoteSocketID < 0)
        return -1;

    int bytes_send = send(remoteSocketID, buf, strlen(buf), 0);

    bzero(buf, MAX_BYTES);

    bytes_send = recv(remoteSocketID, buf, MAX_BYTES - 1, 0);
    char *temp_buffer = (char *)malloc(sizeof(char) * MAX_BYTES); // temp buffer
    int temp_buffer_size = MAX_BYTES;
    int temp_buffer_index = 0;

    while (bytes_send > 0)
    {
        bytes_send = send(clientSocket, buf, bytes_send, 0);

        for (int i = 0; i < bytes_send / sizeof(char); i++)
        {
            temp_buffer[temp_buffer_index] = buf[i];
            // printf("%c",buf[i]); // Response Printing
            temp_buffer_index++;
        }
        temp_buffer_size += MAX_BYTES;
        temp_buffer = (char *)realloc(temp_buffer, temp_buffer_size);

        if (bytes_send < 0)
        {
            perror("Bravo-6 to Gold Eagle Actual. Couldn't send to client socket.\n");
            break;
        }
        bzero(buf, MAX_BYTES);

        bytes_send = recv(remoteSocketID, buf, MAX_BYTES - 1, 0);
    }
    temp_buffer[temp_buffer_index] = '\0';
    free(buf);
    add_cache_element(temp_buffer, strlen(temp_buffer), tempReq);
    printf("Done\n");
    free(temp_buffer);

    close(remoteSocketID);
    return 0;
}

/**
 * @brief Validates HTTP version in request
 * @param msg HTTP version string
 * @return 1 if valid version (1.0/1.1), -1 otherwise
 */
int checkHTTPversion(char *msg)
{
    int version = -1;

    if (strncmp(msg, "HTTP/1.1", 8) == 0)
    {
        version = 1;
    }
    else if (strncmp(msg, "HTTP/1.0", 8) == 0)
    {
        version = 1; // Handling this similar to version 1.1
    }
    else
        version = -1;

    return version;
}

/**
 * @brief Thread handler function for processing client requests
 * @param socketNew Pointer to client socket descriptor
 * @return NULL
 */
void *thread_fn(void *socketNew)
{
    sem_wait(&seamaphore);
    int p;
    sem_getvalue(&seamaphore, &p);
    printf("semaphore value:%d\n", p);
    int *t = (int *)(socketNew);
    int socket = *t;            // Socket is socket descriptor of the connected Client
    int bytes_send_client, len; // Number of bytes to be transferred

    char *buffer = (char *)calloc(MAX_BYTES, sizeof(char)); // Creating buffer of 4kb for a client

    bzero(buffer, MAX_BYTES);                               // Setting the buffer to 0
    bytes_send_client = recv(socket, buffer, MAX_BYTES, 0); // Receiving the request of client by proxy server

    while (bytes_send_client > 0)
    {
        len = strlen(buffer);
        // loop until u find "\r\n\r\n" in the buffer
        if (strstr(buffer, "\r\n\r\n") == NULL)
        {
            bytes_send_client = recv(socket, buffer + len, MAX_BYTES - len, 0);
        }
        else
        {
            break;
        }
    }

    char *tempReq = (char *)malloc(strlen(buffer) * sizeof(char) + 1);
    // tempReq, buffer both store the http request sent by client
    for (int i = 0; i < strlen(buffer); i++)
    {
        tempReq[i] = buffer[i];
    }

    // checking for the request in cache
    struct cache_element *temp = find(tempReq);

    if (temp != NULL)
    {
        // send respose as request has been found in the cache
        int size = temp->len / sizeof(char);
        int pos = 0;
        char response[MAX_BYTES];
        while (pos < size)
        {
            bzero(response, MAX_BYTES);
            for (int i = 0; i < MAX_BYTES; i++)
            {
                response[i] = temp->data[pos];
                pos++;
            }
            send(socket, response, MAX_BYTES, 0);
        }
        printf("Data has been received from the Cache\n\n");
        printf("%s\n\n", response);
    }

    else if (bytes_send_client > 0)
    {
        len = strlen(buffer);
        // Parsing the request
        struct ParsedRequest *request = ParsedRequest_create();
        if (ParsedRequest_parse(request, buffer, len) < 0)
        {
            printf("Parsing failed\n");
        }
        else
        {
            bzero(buffer, MAX_BYTES);
            if (!strcmp(request->method, "GET"))
            {
                if (request->host && request->path && (checkHTTPversion(request->version) == 1))
                {
                    bytes_send_client = handle_request(socket, request, tempReq); // Handle GET request
                    if (bytes_send_client == -1)
                    {
                        sendErrorMessage(socket, 500);
                    }
                }
                else
                    sendErrorMessage(socket, 500); // 500 Internal Error
            }
            else
            {
                printf("This code doesn't support any method other than GET\n");
            }
        }
        ParsedRequest_destroy(request);
    }

    else if (bytes_send_client < 0)
    {
        perror("Error in receiving from client.\n");
    }
    else if (bytes_send_client == 0)
    {
        printf("Client disconnected!\n");
    }

    shutdown(socket, SHUT_RDWR);
    close(socket);
    free(buffer);
    sem_post(&seamaphore);

    sem_getvalue(&seamaphore, &p);
    printf("Semaphore post value:%d\n", p);
    free(tempReq);
    return NULL;
}

/**
 * @brief Main proxy server function
 * @param argc Argument count
 * @param argv Argument vector (expects port number)
 * @return 0 on successful execution
 */
int main(int argc, char *argv[])
{

    int client_socketId, client_len;             // client_socketId == to store the client socket id
    struct sockaddr_in server_addr, client_addr; // Address of client and server to be assigned

    sem_init(&seamaphore, 0, MAX_CLIENTS);
    pthread_mutex_init(&lock, NULL);

    if (argc == 2) // Checking if the port number is provided as an argument
    {
        port_number = atoi(argv[1]);
    }
    else
    {
        printf("Too few arguments\n");
        exit(1);
    }

    printf("Setting Proxy Server Port : %d\n", port_number);

    // Creating a socket for the proxy server
    proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);

    if (proxy_socketId < 0)
    {
        perror("Failed to create socket.\n");
        exit(1);
    }

    int reuse = 1;
    if (setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed\n");

    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number); // Port number assigned to the server
    server_addr.sin_addr.s_addr = INADDR_ANY;  // IP address of the server

    // Binding the socket to the port
    if (bind(proxy_socketId, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Port is not free\n");
        exit(1);
    }
    printf("Binding on port: %d\n", port_number);

    // Listening to the clients
    int listen_status = listen(proxy_socketId, MAX_CLIENTS);

    if (listen_status < 0)
    {
        perror("Error while Listening !\n");
        exit(1);
    }

    int i = 0;                           // Index for the thread array
    int Connected_socketId[MAX_CLIENTS]; // Array to store the connected clients

    // Infinite loop to accept the clients
    while (1)
    {

        bzero((char *)&client_addr, sizeof(client_addr)); // Setting the client address to 0
        client_len = sizeof(client_addr);

        // Accepting the connection from the client
        client_socketId = accept(proxy_socketId, (struct sockaddr *)&client_addr, (socklen_t *)&client_len); // Accepting the connection from the client
        if (client_socketId < 0)
        {
            fprintf(stderr, "Error in Accepting connection !\n");
            exit(1);
        }
        else
        {
            Connected_socketId[i] = client_socketId; // Storing the connected client socket id
        }

        // Printing the client details
        struct sockaddr_in *client_pt = (struct sockaddr_in *)&client_addr;
        struct in_addr ip_addr = client_pt->sin_addr;
        char str[INET_ADDRSTRLEN]; // String to store the IP address of the client
        inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN);
        printf("Client is connected with port number: %d and ip address: %s \n", ntohs(client_addr.sin_port), str);

        pthread_create(&tid[i], NULL, thread_fn, (void *)&Connected_socketId[i]); // Creating a thread for the client
        i++;
    }
    close(proxy_socketId); // Close socket
    return 0;
}

/**
 * @brief Searches for URL in cache with thread safety
 * @param url URL to search for
 * @return Pointer to cache element if found, NULL otherwise
 */
cache_element *find(char *url)
{

    // If cache is not empty searches for the node which has the least lru_time_track and deletes it
    cache_element *site = NULL;

    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Remove Cache Lock Acquired %d\n", temp_lock_val);
    if (head != NULL)
    {
        site = head;
        while (site != NULL)
        {
            if (!strcmp(site->url, url))
            {
                printf("LRU Time Track Before : %ld", site->lru_time_track);
                printf("\nurl found\n");
                // Updating the time track of the accessed element
                site->lru_time_track = time(NULL);
                printf("LRU Time Track After : %ld", site->lru_time_track);
                break;
            }
            site = site->next;
        }
    }
    else
    {
        printf("\nurl not found\n");
    }

    temp_lock_val = pthread_mutex_unlock(&lock);
    printf("Remove Cache Lock Unlocked %d\n", temp_lock_val);
    return site;
}

/**
 * @brief Removes least recently used element from cache with thread safety
 */
void remove_cache_element()
{
    // If cache is not empty searches for the node which has the least lru_time_track and deletes it
    cache_element *p;
    cache_element *q;
    cache_element *temp;
    // sem_wait(&cache_lock);
    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Remove Cache Lock Acquired %d\n", temp_lock_val);
    if (head != NULL)
    { // Cache != empty
        for (q = head, p = head, temp = head; q->next != NULL;
             q = q->next)
        { // Finding the element with the least lru_time_track
            if (((q->next)->lru_time_track) < (temp->lru_time_track))
            {
                temp = q->next;
                p = q;
            }
        }
        if (temp == head)
        {
            head = head->next; /*Handle the base case*/
        }
        else
        {
            p->next = temp->next;
        }
        cache_size = cache_size - (temp->len) - sizeof(cache_element) -
                     strlen(temp->url) - 1; // Updating the cache size
        free(temp->data);
        free(temp->url); // Freeing the memory of the element
        free(temp);
    }
    // sem_post(&cache_lock);
    temp_lock_val = pthread_mutex_unlock(&lock);
    printf("Remove Cache Lock Unlocked %d\n", temp_lock_val);
}

/**
 * @brief Adds new element to cache with thread safety
 * @param data Response data to cache
 * @param size Size of response data
 * @param url Request URL as cache key
 * @return 1 if successful, 0 if element too large
 */
int add_cache_element(char *data, int size, char *url)
{
    // Adds element to the cache
    // sem_wait(&cache_lock);
    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Add Cache Lock Acquired %d\n", temp_lock_val);
    int element_size = size + 1 + strlen(url) + sizeof(cache_element); // Calculating the size of the element to be added
    if (element_size > MAX_ELEMENT_SIZE)
    {
        // sem_post(&cache_lock);
        //  free(data);
        //  printf("--\n");
        temp_lock_val = pthread_mutex_unlock(&lock);
        printf("Add Cache Lock Unlocked %d\n", temp_lock_val);
        // free(data);
        // printf("--\n");
        // free(url);
        return 0;
    }
    else
    {
        while (cache_size + element_size > MAX_SIZE)
        {
            // If the cache is full, remove the least recently used element
            remove_cache_element();
        }
        cache_element *element = (cache_element *)malloc(sizeof(cache_element)); // Allocating memory for the cache element
        element->data = (char *)malloc(size + 1);                                // Allocating memory for the response to be stored in the cache element
        strcpy(element->data, data);
        element->url = (char *)malloc(1 + (strlen(url) * sizeof(char))); // Allocating memory for the request to be stored in the cache element (as a key)
        strcpy(element->url, url);
        element->lru_time_track = time(NULL); // Updating the time_track
        element->next = head;
        element->len = size;
        head = element;
        cache_size += element_size;
        temp_lock_val = pthread_mutex_unlock(&lock);
        printf("Add Cache Lock Unlocked %d\n", temp_lock_val);
        // sem_post(&cache_lock);
        //  free(data);
        //  printf("--\n");
        //  free(url);
        return 1;
    }
    return 0;
}
