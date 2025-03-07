# HTTP Proxy Server with LRU Cache

A multi-threaded HTTP proxy server implementation in C that supports caching using an LRU (Least Recently Used) cache mechanism.

## Features

- HTTP/1.0 and HTTP/1.1 support
- Multi-threaded request handling
- LRU caching mechanism
- Thread-safe cache operations
- Support for GET requests
- Configurable cache size and connection limits
- Error handling with proper HTTP status codes

## Technical Specifications

- Maximum cache size: 200MB
- Maximum cache element size: 10MB
- Maximum concurrent clients: 20
- Default port: 8080 (configurable via command line)
- Buffer size: 4KB

## Prerequisites

- GCC compiler
- POSIX-compliant operating system (Linux/Unix)
- pthread library
- Standard C libraries

## Building the Project

```bash
# Compile the proxy server
gcc -o proxy_server proxy_server_with_cache.c proxy_parse.c -pthread

# Run the proxy server
./proxy_server <port_number>
```

## Usage

1. Start the proxy server by specifying a port number:

   ```bash
   ./proxy_server 8080
   ```

2. Configure your web browser or HTTP client to use the proxy:
   - Proxy Address: localhost (or your server's IP)
   - Proxy Port: 8080 (or your specified port)

## Architecture

### Components

1. **Main Server**

   - Handles incoming connections
   - Creates threads for client requests
   - Manages server socket operations

2. **Thread Handler**

   - Processes individual client requests
   - Parses HTTP requests
   - Manages cache lookups and updates

3. **Cache System**

   - LRU (Least Recently Used) implementation
   - Thread-safe operations
   - Automatic cache cleanup when size limit is reached

4. **Error Handler**
   - Supports various HTTP error codes (400, 403, 404, 500, 501, 505)
   - Generates appropriate error responses

### Data Structures

```c
struct cache_element {
    char *data;             // HTTP response data
    int len;               // Length of data
    char *url;             // Request URL (cache key)
    time_t lru_time_track; // Last access timestamp
    cache_element *next;   // Next element in cache
};
```

## Error Codes

The proxy server handles the following HTTP error codes:

- 400: Bad Request
- 403: Forbidden
- 404: Not Found
- 500: Internal Server Error
- 501: Not Implemented
- 505: HTTP Version Not Supported

## Thread Safety

The implementation ensures thread safety through:

- Mutex locks for cache operations
- Semaphores for controlling concurrent connections
- Thread-safe data structure operations

## Limitations

- Only supports GET requests
- Fixed maximum cache size
- No persistent connections
- No HTTPS support

## Contributing

1. Fork the repository
2. Create your feature branch
3. Commit your changes
4. Push to the branch
5. Create a new Pull Request

## Acknowledgments

- Based on HTTP/1.1 specifications
- Uses POSIX thread library
- Implements standard proxy server patterns

## Troubleshooting

### Common Issues

1. **Port Already in Use**

   ```bash
   Error: Port is not free
   ```

   Solution: Choose a different port or ensure no other service is using the specified port.

2. **Connection Refused**

   ```bash
   Error in Accepting connection
   ```

   Solution: Check if the server is running and the port is correctly specified.

3. **Maximum Clients Reached**
   ```bash
   Semaphore value: 0
   ```
   Solution: Wait for some clients to disconnect or increase MAX_CLIENTS value.

## Future Improvements

- Add HTTPS support
- Implement additional HTTP methods (POST, PUT, etc.)
- Add configuration file support
- Implement persistent connections
- Add logging functionality
- Improve cache management algorithms

## Technical Details

### Network Architecture

#### Socket Implementation

The proxy server uses TCP/IP sockets (SOCK_STREAM) with the following configuration:

```c
proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);
```

- `AF_INET`: IPv4 Internet protocols
- `SOCK_STREAM`: TCP socket type for reliable, ordered data transmission
- Socket reuse is enabled to prevent "Address already in use" errors:

```c
setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
```

#### Connection Flow

1. **Server Initialization**

   ```c
   bind(proxy_socketId, (struct sockaddr *)&server_addr, sizeof(server_addr));
   listen(proxy_socketId, MAX_CLIENTS);
   ```

   - Binds to specified port
   - Maintains connection queue of MAX_CLIENTS (20) pending connections

2. **Client Connection Handling**
   ```c
   client_socketId = accept(proxy_socketId, (struct sockaddr *)&client_addr, &client_len);
   ```
   - New socket created for each client
   - Client IP and port stored for logging

### Threading Model

#### Thread Management

- Uses POSIX threads (pthreads) for concurrent client handling
- Thread pool size: MAX_CLIENTS (20)
- Thread creation:

```c
pthread_create(&tid[i], NULL, thread_fn, (void *)&Connected_socketId[i]);
```

#### Synchronization Mechanisms

1. **Semaphores**

   ```c
   sem_init(&seamaphore, 0, MAX_CLIENTS);
   ```

   - Controls concurrent client connections
   - Process-private semaphore (0)
   - Initial value: MAX_CLIENTS

2. **Mutex Locks**
   ```c
   pthread_mutex_t lock;
   pthread_mutex_init(&lock, NULL);
   ```
   - Ensures thread-safe cache operations
   - Protects shared cache data structure

### Cache Implementation

#### Cache Structure

```c
struct cache_element {
    char *data;             // Response data
    int len;               // Data length
    char *url;             // Cache key
    time_t lru_time_track; // LRU timestamp
    cache_element *next;   // Next element
};
```

#### Memory Management

- **Maximum Cache Size**: 200MB (`MAX_SIZE = 200 * (1 << 20)`)
- **Maximum Element Size**: 10MB (`MAX_ELEMENT_SIZE = 10 * (1 << 20)`)
- Dynamic memory allocation for cache elements:
  ```c
  element->data = (char *)malloc(size + 1);
  element->url = (char *)malloc(1 + strlen(url));
  ```

#### LRU Algorithm Implementation

1. **Cache Lookup**

   ```c
   cache_element *find(char *url) {
       // Thread-safe search
       pthread_mutex_lock(&lock);
       // Search implementation
       pthread_mutex_unlock(&lock);
   }
   ```

   - O(n) time complexity for lookups
   - Updates access timestamp on hits

2. **Cache Insertion**

   ```c
   int add_cache_element(char *data, int size, char *url) {
       // Calculate total element size
       int element_size = size + 1 + strlen(url) + sizeof(cache_element);

       // Evict elements if necessary
       while (cache_size + element_size > MAX_SIZE) {
           remove_cache_element();
       }

       // Insert new element
       element->lru_time_track = time(NULL);
   }
   ```

   - Checks size constraints
   - Performs LRU eviction if needed
   - Updates cache size tracking

3. **Cache Eviction**
   ```c
   void remove_cache_element() {
       // Find least recently used element
       // Update cache size
       // Free memory
   }
   ```
   - O(n) time complexity for finding LRU element
   - Maintains linked list integrity
   - Properly frees allocated memory

### HTTP Request Processing

#### Request Parsing

1. **Buffer Management**

   - 4KB buffer size (`MAX_BYTES = 4096`)
   - Handles incomplete requests:

   ```c
   while (bytes_send_client > 0) {
       if (strstr(buffer, "\r\n\r\n") == NULL) {
           // Continue receiving
       }
   }
   ```

2. **HTTP Version Validation**
   ```c
   int checkHTTPversion(char *msg) {
       // Supports HTTP/1.0 and HTTP/1.1
   }
   ```

#### Request Handling Flow

1. **Initial Processing**

   - Parse incoming request
   - Validate HTTP method (GET only)
   - Check cache for existing response

2. **Cache Miss Handling**

   - Connect to remote server
   - Forward request
   - Receive response
   - Cache response
   - Send to client

3. **Cache Hit Handling**
   - Retrieve cached response
   - Update LRU timestamp
   - Send to client

### Error Handling

#### HTTP Error Responses

Detailed error handling for various scenarios:

```c
int sendErrorMessage(int socket, int status_code) {
    // Generates appropriate HTTP error response
    // Includes timestamp and HTML body
}
```

Common error scenarios:

- 400: Malformed request
- 403: Permission denied
- 404: Resource not found
- 500: Internal server error
- 501: Method not implemented
- 505: HTTP version not supported

#### Network Error Handling

1. **Socket Errors**

   - Connection failures
   - Send/Receive timeouts
   - Resource exhaustion

2. **Memory Errors**
   - Allocation failures
   - Buffer overflow prevention
   - Memory leak prevention

### Performance Considerations

#### Memory Usage

- Dynamic buffer allocation
- Careful memory deallocation
- Cache size monitoring

#### Thread Management

- Limited thread pool
- Resource cleanup
- Connection timeout handling

#### Network Optimization

- Buffer size tuning
- Connection handling
- Error recovery

### Security Considerations

1. **Input Validation**

   - URL sanitization
   - Request size limits
   - Method validation

2. **Resource Protection**

   - Memory limits
   - Connection limits
   - Thread safety

3. **Error Handling**
   - Graceful degradation
   - Proper client notification
   - Resource cleanup
