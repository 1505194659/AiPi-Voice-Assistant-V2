#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "log.h"

#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/tcp.h>
#include <lwip/err.h>

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"

#include "https_client.h"

#define DBG_TAG "HTTPS"
#define RECV_BUF_SIZE 2048                        // Reduced from 4096
#define MAX_RESPONSE_SIZE_DEFAULT (8 * 1024)      // 8KB for API responses (reduced from 16KB)
#define MAX_RESPONSE_SIZE_TTS (32 * 1024)         // 32KB for TTS audio (reduced from 48KB)
#define SEND_CHUNK_SIZE 1024                      // Send 1KB at a time (reduced from 2KB)
#define PAUSE_INTERVAL (16 * 1024)                // Pause every 16KB

// Parse URL into components
static int parse_url(const char* url, char* protocol, char* host, int* port, char* path) {
    // Format: http://host:port/path or https://host:port/path
    const char* p = url;
    
    // Extract protocol
    const char* proto_end = strstr(p, "://");
    if (!proto_end) {
        return -1;
    }
    
    int proto_len = proto_end - p;
    memcpy(protocol, p, proto_len);
    protocol[proto_len] = '\0';
    
    p = proto_end + 3;  // Skip "://"
    
    // Extract host and port
    const char* path_start = strchr(p, '/');
    const char* port_start = strchr(p, ':');
    
    if (port_start && (!path_start || port_start < path_start)) {
        // Port is specified
        int host_len = port_start - p;
        memcpy(host, p, host_len);
        host[host_len] = '\0';
        
        *port = atoi(port_start + 1);
        p = path_start ? path_start : (p + strlen(p));
    } else {
        // No port, use default
        int host_len = path_start ? (path_start - p) : strlen(p);
        memcpy(host, p, host_len);
        host[host_len] = '\0';
        
        if (strcmp(protocol, "https") == 0) {
            *port = 443;
        } else {
            *port = 80;
        }
        p = path_start ? path_start : (p + strlen(p));
    }
    
    // Extract path
    if (*p == '\0') {
        strcpy(path, "/");
    } else {
        strcpy(path, p);
    }
    
    return 0;
}

// Perform HTTP request (non-SSL)
static char* http_request_plain(const char* host, int port, const char* path,
                                const char* method, const char* headers,
                                const char* body, int body_len) {
    int sockfd = -1;
    struct sockaddr_in server_addr;
    struct hostent* server;
    char* response = NULL;
    char* recv_buf = NULL;
    int total_received = 0;

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOG_E("Failed to create socket");
        return NULL;
    }

    // Set socket timeout to 5 minutes for TTS
    struct timeval tv_timeout;
    tv_timeout.tv_sec = 300;  // 5 minutes
    tv_timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof(tv_timeout));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv_timeout, sizeof(tv_timeout));

    // Get host by name
    server = gethostbyname(host);
    if (server == NULL) {
        LOG_E("Failed to resolve host: %s", host);
        close(sockfd);
        return NULL;
    }
    
    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(port);
    
    // Connect
    LOG_I("Connecting to %s:%d...", host, port);
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_E("Failed to connect");
        close(sockfd);
        return NULL;
    }
    
    LOG_I("Connected!");
    
    // Build HTTP request header
    char request_header[512];
    int header_len = sprintf(request_header,
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "%s"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        method, path, host, headers ? headers : "", body_len);
    
    // Send header
    int sent = send(sockfd, request_header, header_len, 0);
    if (sent < 0) {
        LOG_E("Failed to send request header");
        close(sockfd);
        return NULL;
    }
    
    int total_sent = sent;
    
    // Send body in chunks if present
    if (body && body_len > 0) {
        int remaining = body_len;
        int offset = 0;
        int pause_counter = 0;
        
        LOG_I("Sending body in chunks (%d bytes total)...", body_len);
        
        while (remaining > 0) {
            int chunk_size = (remaining > SEND_CHUNK_SIZE) ? SEND_CHUNK_SIZE : remaining;
            
            sent = send(sockfd, body + offset, chunk_size, 0);
            if (sent < 0) {
                LOG_E("Failed to send body chunk at offset %d", offset);
                close(sockfd);
                return NULL;
            }
            
            offset += sent;
            remaining -= sent;
            total_sent += sent;
            pause_counter += sent;
            
            // Pause every 16KB to let network stack process
            if (pause_counter >= PAUSE_INTERVAL && remaining > 0) {
                vTaskDelay(pdMS_TO_TICKS(10));
                pause_counter = 0;
                LOG_I("Sent %d/%d bytes (pausing)...", offset, body_len);
            }
        }
        
        LOG_I("Body sent successfully (%d bytes)", body_len);
    }
    
    LOG_I("Request sent (%d bytes total)", total_sent);
    
    // Allocate receive buffers
    recv_buf = pvPortMalloc(RECV_BUF_SIZE);
    if (!recv_buf) {
        LOG_E("Failed to allocate recv_buf (%d bytes)", RECV_BUF_SIZE);
        close(sockfd);
        return NULL;
    }
    
    response = pvPortMalloc(MAX_RESPONSE_SIZE_DEFAULT);
    if (!response) {
        LOG_E("Failed to allocate response buffer (%d bytes)", MAX_RESPONSE_SIZE_DEFAULT);
        vPortFree(recv_buf);
        close(sockfd);
        return NULL;
    }

    while (1) {
        int received = recv(sockfd, recv_buf, RECV_BUF_SIZE - 1, 0);
        if (received <= 0) {
            break;
        }

        if (total_received + received >= MAX_RESPONSE_SIZE_DEFAULT) {
            LOG_W("Response too large, truncating");
            received = MAX_RESPONSE_SIZE_DEFAULT - total_received - 1;
            if (received <= 0) break;
        }
        
        memcpy(response + total_received, recv_buf, received);
        total_received += received;
    }
    
    response[total_received] = '\0';
    vPortFree(recv_buf);
    close(sockfd);
    
    // Find body (after "\r\n\r\n")
    char* body_start = strstr(response, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        int body_length = total_received - (body_start - response);
        char* body_only = pvPortMalloc(body_length + 1);
        if (body_only) {
            memcpy(body_only, body_start, body_length);
            body_only[body_length] = '\0';
            vPortFree(response);
            return body_only;
        }
    }
    
    return response;
}

// Perform HTTPS request (with SSL)
static char* https_request_secure(const char* host, int port, const char* path,
                                  const char* method, const char* headers,
                                  const char* body, int body_len) {
    mbedtls_net_context server_fd;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    char* response = NULL;
    char* recv_buf = NULL;
    int total_received = 0;
    int ret;
    
    // Initialize structures
    mbedtls_net_init(&server_fd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    
    // Seed random number generator
    const char* pers = "https_client";
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                (const unsigned char*)pers, strlen(pers));
    if (ret != 0) {
        LOG_E("mbedtls_ctr_drbg_seed failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    // Connect to server
    char port_str[8];
    sprintf(port_str, "%d", port);
    
    LOG_I("Connecting to %s:%d...", host, port);
    ret = mbedtls_net_connect(&server_fd, host, port_str, MBEDTLS_NET_PROTO_TCP);
    if (ret != 0) {
        LOG_E("mbedtls_net_connect failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    // Setup SSL/TLS
    ret = mbedtls_ssl_config_defaults(&conf,
                                      MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        LOG_E("mbedtls_ssl_config_defaults failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);  // Skip certificate verification for simplicity
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    
    ret = mbedtls_ssl_setup(&ssl, &conf);
    if (ret != 0) {
        LOG_E("mbedtls_ssl_setup failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    ret = mbedtls_ssl_set_hostname(&ssl, host);
    if (ret != 0) {
        LOG_E("mbedtls_ssl_set_hostname failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
    
    // Perform SSL handshake
    LOG_I("Performing SSL handshake...");
    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            LOG_E("mbedtls_ssl_handshake failed: -0x%04x", -ret);
            goto cleanup;
        }
    }
    
    LOG_I("SSL handshake complete!");
    
    // Build HTTP request header
    char request_header[512];
    int header_len = sprintf(request_header,
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "%s"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        method, path, host, headers ? headers : "", body_len);
    
    // Send header
    LOG_I("Sending HTTPS request...");
    ret = mbedtls_ssl_write(&ssl, (unsigned char*)request_header, header_len);
    if (ret < 0) {
        LOG_E("mbedtls_ssl_write (header) failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    int total_sent = ret;
    
    // Send body in chunks if present
    if (body && body_len > 0) {
        int remaining = body_len;
        int offset = 0;
        int pause_counter = 0;
        
        LOG_I("Sending body in chunks (%d bytes total)...", body_len);
        
        while (remaining > 0) {
            int chunk_size = (remaining > SEND_CHUNK_SIZE) ? SEND_CHUNK_SIZE : remaining;
            
            ret = mbedtls_ssl_write(&ssl, (unsigned char*)(body + offset), chunk_size);
            if (ret < 0) {
                LOG_E("mbedtls_ssl_write (body chunk) failed at offset %d: -0x%04x", offset, -ret);
                goto cleanup;
            }
            
            offset += ret;
            remaining -= ret;
            total_sent += ret;
            pause_counter += ret;
            
            // Pause every 16KB to let network stack process
            if (pause_counter >= PAUSE_INTERVAL && remaining > 0) {
                vTaskDelay(pdMS_TO_TICKS(10));
                pause_counter = 0;
                LOG_I("Sent %d/%d bytes (pausing)...", offset, body_len);
            }
        }
        
        LOG_I("Body sent successfully (%d bytes)", body_len);
    }
    
    LOG_I("Request sent (%d bytes total)", total_sent);
    
    // Allocate receive buffers
    recv_buf = pvPortMalloc(RECV_BUF_SIZE);
    if (!recv_buf) {
        LOG_E("Failed to allocate recv_buf (%d bytes)", RECV_BUF_SIZE);
        goto cleanup;
    }

    response = pvPortMalloc(MAX_RESPONSE_SIZE_DEFAULT);
    if (!response) {
        LOG_E("Failed to allocate response buffer (%d bytes)", MAX_RESPONSE_SIZE_DEFAULT);
        vPortFree(recv_buf);
        recv_buf = NULL;
        goto cleanup;
    }

    while (1) {
        ret = mbedtls_ssl_read(&ssl, (unsigned char*)recv_buf, RECV_BUF_SIZE - 1);

        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            continue;
        }

        if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || ret == 0) {
            break;
        }

        if (ret < 0) {
            LOG_E("mbedtls_ssl_read failed: -0x%04x", -ret);
            break;
        }

        if (total_received + ret >= MAX_RESPONSE_SIZE_DEFAULT) {
            LOG_W("Response too large, truncating");
            ret = MAX_RESPONSE_SIZE_DEFAULT - total_received - 1;
            if (ret <= 0) break;
        }

        memcpy(response + total_received, recv_buf, ret);
        total_received += ret;
    }
    
    if (total_received > 0) {
        response[total_received] = '\0';
        
        // Find body (after "\r\n\r\n")
        char* body_start = strstr(response, "\r\n\r\n");
        if (body_start) {
            body_start += 4;
            int body_length = total_received - (body_start - response);
            char* body_only = pvPortMalloc(body_length + 1);
            if (body_only) {
                memcpy(body_only, body_start, body_length);
                body_only[body_length] = '\0';
                vPortFree(response);
                vPortFree(recv_buf);
                response = body_only;
                recv_buf = NULL;
            }
        }
    } else {
        if (response) {
            vPortFree(response);
            response = NULL;
        }
    }
    
cleanup:
    if (recv_buf) vPortFree(recv_buf);
    mbedtls_ssl_close_notify(&ssl);
    mbedtls_net_free(&server_fd);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    
    return response;
}

// Main HTTPS request function
char* https_request(const char* url, const char* method, const char* headers, 
                   const char* body, int body_len) {
    char protocol[16];
    char host[256];
    char path[512];
    int port;
    
    if (parse_url(url, protocol, host, &port, path) < 0) {
        LOG_E("Failed to parse URL: %s", url);
        return NULL;
    }
    
    LOG_I("Protocol: %s, Host: %s, Port: %d, Path: %s", protocol, host, port, path);
    
    if (strcmp(protocol, "https") == 0) {
        return https_request_secure(host, port, path, method, headers, body, body_len);
    } else if (strcmp(protocol, "http") == 0) {
        return http_request_plain(host, port, path, method, headers, body, body_len);
    } else {
        LOG_E("Unsupported protocol: %s", protocol);
        return NULL;
    }
}

// HTTPS request for large responses (TTS audio, etc.) - uses MAX_RESPONSE_SIZE_TTS
char* https_request_large(const char* url, const char* method, const char* headers,
                          const char* body, int body_len) {
    char protocol[16];
    char host[256];
    char path[512];
    int port;

    if (parse_url(url, protocol, host, &port, path) < 0) {
        LOG_E("Failed to parse URL: %s", url);
        return NULL;
    }

    LOG_I("Protocol: %s, Host: %s, Port: %d, Path: %s (LARGE buffer)", protocol, host, port, path);

    // For large responses, we only support HTTP (TTS is local)
    if (strcmp(protocol, "http") == 0) {
        // Inline implementation with large buffer
        int sockfd = -1;
        struct sockaddr_in server_addr;
        struct hostent* server;
        char* response = NULL;
        char* recv_buf = NULL;
        int total_received = 0;

        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            LOG_E("Failed to create socket");
            return NULL;
        }

        struct timeval tv_timeout;
        tv_timeout.tv_sec = 300;
        tv_timeout.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof(tv_timeout));
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv_timeout, sizeof(tv_timeout));

        server = gethostbyname(host);
        if (server == NULL) {
            LOG_E("Failed to resolve host: %s", host);
            close(sockfd);
            return NULL;
        }

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
        server_addr.sin_port = htons(port);

        LOG_I("Connecting to %s:%d...", host, port);
        if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            LOG_E("Failed to connect");
            close(sockfd);
            return NULL;
        }

        LOG_I("Connected!");

        char request_header[512];
        int header_len = sprintf(request_header,
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "%s"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n",
            method, path, host, headers ? headers : "", body_len);

        int sent = send(sockfd, request_header, header_len, 0);
        if (sent < 0) {
            LOG_E("Failed to send request header");
            close(sockfd);
            return NULL;
        }

        int total_sent = sent;

        if (body && body_len > 0) {
            int remaining = body_len;
            int offset = 0;
            int pause_counter = 0;

            LOG_I("Sending body in chunks (%d bytes total)...", body_len);

            while (remaining > 0) {
                int chunk_size = (remaining > SEND_CHUNK_SIZE) ? SEND_CHUNK_SIZE : remaining;

                sent = send(sockfd, body + offset, chunk_size, 0);
                if (sent < 0) {
                    LOG_E("Failed to send body chunk at offset %d", offset);
                    close(sockfd);
                    return NULL;
                }

                offset += sent;
                remaining -= sent;
                total_sent += sent;
                pause_counter += sent;

                if (pause_counter >= PAUSE_INTERVAL && remaining > 0) {
                    vTaskDelay(pdMS_TO_TICKS(10));
                    pause_counter = 0;
                }
            }

            LOG_I("Body sent successfully (%d bytes)", body_len);
        }

        LOG_I("Request sent (%d bytes total)", total_sent);

        recv_buf = pvPortMalloc(RECV_BUF_SIZE);
        if (!recv_buf) {
            LOG_E("Failed to allocate recv_buf");
            close(sockfd);
            return NULL;
        }

        response = pvPortMalloc(MAX_RESPONSE_SIZE_TTS);
        if (!response) {
            LOG_E("Failed to allocate response buffer (%d bytes)", MAX_RESPONSE_SIZE_TTS);
            vPortFree(recv_buf);
            close(sockfd);
            return NULL;
        }

        while (1) {
            int received = recv(sockfd, recv_buf, RECV_BUF_SIZE - 1, 0);
            if (received <= 0) break;

            if (total_received + received >= MAX_RESPONSE_SIZE_TTS) {
                LOG_W("Response too large, truncating");
                received = MAX_RESPONSE_SIZE_TTS - total_received - 1;
                if (received <= 0) break;
            }

            memcpy(response + total_received, recv_buf, received);
            total_received += received;
        }

        response[total_received] = '\0';
        vPortFree(recv_buf);
        close(sockfd);

        LOG_I("Received %d bytes total", total_received);

        char* body_start = strstr(response, "\r\n\r\n");
        if (body_start) {
            body_start += 4;
            int body_length = total_received - (body_start - response);
            char* body_only = pvPortMalloc(body_length + 1);
            if (body_only) {
                memcpy(body_only, body_start, body_length);
                body_only[body_length] = '\0';
                vPortFree(response);
                return body_only;
            }
        }

        return response;
    } else {
        LOG_E("https_request_large only supports HTTP (not %s)", protocol);
        return NULL;
    }
}

// Helper to send data in chunks
static int send_chunked(int sockfd, const char* data, int len) {
    int sent = 0;
    int total_sent = 0;
    int remaining = len;
    int offset = 0;
    int pause_counter = 0;

    while (remaining > 0) {
        int chunk_size = (remaining > SEND_CHUNK_SIZE) ? SEND_CHUNK_SIZE : remaining;
        
        sent = send(sockfd, data + offset, chunk_size, 0);
        if (sent < 0) {
            return -1;
        }
        
        offset += sent;
        remaining -= sent;
        total_sent += sent;
        pause_counter += sent;
        
        if (pause_counter >= PAUSE_INTERVAL && remaining > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            pause_counter = 0;
        }
    }
    return total_sent;
}

// Specialized function to stream audio data without allocating a huge buffer
char* https_post_audio(const char* url, const char* headers, 
                      const char* part1, int part1_len,
                      const char* part2, int part2_len,
                      const char* part3, int part3_len,
                      const char* part4, int part4_len) {
    
    char protocol[16];
    char host[256];
    char path[512];
    int port;
    
    if (parse_url(url, protocol, host, &port, path) < 0) {
        LOG_E("Failed to parse URL: %s", url);
        return NULL;
    }
    
    if (strcmp(protocol, "http") != 0) {
        LOG_E("Only HTTP supported for audio streaming currently");
        return NULL;
    }

    int sockfd = -1;
    struct sockaddr_in server_addr;
    struct hostent* server;
    char* response = NULL;
    char* recv_buf = NULL;
    int total_received = 0;
    
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOG_E("Failed to create socket");
        return NULL;
    }
    
    // Get host by name
    server = gethostbyname(host);
    if (server == NULL) {
        LOG_E("Failed to resolve host: %s", host);
        close(sockfd);
        return NULL;
    }
    
    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(port);
    
    // Connect
    LOG_I("Connecting to %s:%d...", host, port);
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_E("Failed to connect");
        close(sockfd);
        return NULL;
    }
    
    LOG_I("Connected!");
    
    int total_len = part1_len + part2_len + part3_len + part4_len;
    
    // Build HTTP request header
    char request_header[512];
    int header_len = sprintf(request_header,
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "%s"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host, headers ? headers : "", total_len);
    
    // Send header
    if (send(sockfd, request_header, header_len, 0) < 0) {
        LOG_E("Failed to send request header");
        close(sockfd);
        return NULL;
    }
    
    LOG_I("Streaming audio parts (%d bytes total)...", total_len);
    
    // Send parts sequentially
    if (send_chunked(sockfd, part1, part1_len) < 0) goto send_error;
    if (send_chunked(sockfd, part2, part2_len) < 0) goto send_error;
    if (send_chunked(sockfd, part3, part3_len) < 0) goto send_error;
    if (send_chunked(sockfd, part4, part4_len) < 0) goto send_error;
    
    LOG_I("Audio sent successfully!");

    // Receive response (same as plain http)
    recv_buf = pvPortMalloc(RECV_BUF_SIZE);
    if (!recv_buf) goto send_error;

    response = pvPortMalloc(MAX_RESPONSE_SIZE_TTS);
    if (!response) {
        vPortFree(recv_buf);
        goto send_error;
    }

    while (1) {
        int received = recv(sockfd, recv_buf, RECV_BUF_SIZE - 1, 0);
        if (received <= 0) break;

        if (total_received + received >= MAX_RESPONSE_SIZE_TTS) {
            received = MAX_RESPONSE_SIZE_TTS - total_received - 1;
            if (received <= 0) break;
        }

        memcpy(response + total_received, recv_buf, received);
        total_received += received;
    }
    
    response[total_received] = '\0';
    vPortFree(recv_buf);
    close(sockfd);
    
    // Find body
    char* body_start = strstr(response, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        int body_length = total_received - (body_start - response);
        char* body_only = pvPortMalloc(body_length + 1);
        if (body_only) {
            memcpy(body_only, body_start, body_length);
            body_only[body_length] = '\0';
            vPortFree(response);
            return body_only;
        }
    }
    return response;

send_error:
    LOG_E("Failed to send audio data");
    if (recv_buf) vPortFree(recv_buf);
    if (response) vPortFree(response);
    close(sockfd);
    return NULL;
}

