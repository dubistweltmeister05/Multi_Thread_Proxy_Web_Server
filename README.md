# 🌐 HTTP Proxy Server with LRU Cache 🚀

A multi-threaded HTTP proxy server implementation in C that supports caching using an LRU (Least Recently Used) cache mechanism.

## 🔥 Features

- ✅ **HTTP/1.0 & HTTP/1.1 support**
- ⚡ **Multi-threaded request handling**
- 🗂️ **LRU caching mechanism**
- 🔐 **Thread-safe cache operations**
- 📥 **Support for GET requests**
- 🎛️ **Configurable cache size & connection limits**
- ⚠️ **Proper error handling with HTTP status codes**

## 🛠 Technical Specifications

- 🏷 **Max Cache Size:** 200MB
- 📌 **Max Cache Element Size:** 10MB
- 👥 **Max Concurrent Clients:** 20
- 🔌 **Default Port:** 8080 (configurable via CLI)
- 📏 **Buffer Size:** 4KB

## 📋 Prerequisites

- 🖥 **GCC Compiler**
- 🏗 **POSIX-compliant OS (Linux/Unix)**
- 🔄 **pthread library**
- 📚 **Standard C libraries**

## ⚙️ Building the Project

```bash
# Compile the proxy server
gcc -o proxy_server proxy_server_with_cache.c proxy_parse.c -pthread

# Run the proxy server
./proxy_server <port_number>
```

## 🎯 Usage

1. Start the proxy server with a specific port:
   ```bash
   ./proxy_server 8080
   ```
2. Configure your web browser or HTTP client to use the proxy:
   - 🌎 **Proxy Address:** localhost (or server's IP)
   - 🔢 **Proxy Port:** 8080 (or specified port)

## 🏗 Architecture

### 🏠 Components

1. **🖥 Main Server**
   - Handles incoming connections
   - Creates threads for client requests
   - Manages server socket operations

2. **⚡ Thread Handler**
   - Processes individual client requests
   - Parses HTTP requests
   - Manages cache lookups & updates

3. **📂 Cache System**
   - Implements LRU mechanism
   - Thread-safe operations
   - Auto cleanup when limit reached

4. **❌ Error Handler**
   - Supports HTTP error codes (400, 403, 404, 500, etc.)
   - Generates appropriate error responses

### 📜 Data Structures

```c
struct cache_element {
    char *data;             // HTTP response data
    int len;               // Data length
    char *url;             // Request URL (cache key)
    time_t lru_time_track; // Last access timestamp
    cache_element *next;   // Next element in cache
};
```

## 🚨 Error Codes

- ❌ **400**: Bad Request
- 🚫 **403**: Forbidden
- 🔍 **404**: Not Found
- ⚙️ **500**: Internal Server Error
- 🛠 **501**: Not Implemented
- 📜 **505**: HTTP Version Not Supported

## 🔐 Thread Safety

- 🔄 **Mutex locks for cache operations**
- 🛑 **Semaphores for connection control**
- ✅ **Thread-safe data structures**

## ⚠️ Limitations

- 📌 Supports only **GET requests**
- 📏 Fixed max cache size
- ❌ No persistent connections
- 🔒 No HTTPS support

## 🤝 Contributing

1. 🍴 **Fork the repository**
2. 🛠 **Create a feature branch**
3. ✍ **Commit your changes**
4. 🚀 **Push to the branch**
5. 📨 **Create a Pull Request**

## 🙌 Acknowledgments

- 📖 Based on HTTP/1.1 specifications
- 🔗 Uses POSIX thread library
- 🏗 Implements standard proxy server patterns

## 🛠 Troubleshooting

### ❗ Common Issues

1. **Port Already in Use**
   ```bash
   Error: Port is not free
   ```
   ✅ **Solution:** Choose another port or check if another service is using it.

2. **Connection Refused**
   ```bash
   Error in Accepting connection
   ```
   ✅ **Solution:** Ensure the server is running and the port is correct.

3. **Max Clients Reached**
   ```bash
   Semaphore value: 0
   ```
   ✅ **Solution:** Wait for clients to disconnect or increase MAX_CLIENTS.

## 🚀 Future Improvements

- 🔒 Add **HTTPS support**
- 📡 Implement **POST, PUT, etc.**
- ⚙️ Add **config file support**
- 🔄 Implement **persistent connections**
- 📝 Add **logging functionality**
- 📦 Improve **cache management algorithms**
