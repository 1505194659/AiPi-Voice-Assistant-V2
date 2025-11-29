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

#include "whisper_live_client.h"

#define DBG_TAG "WhisperLive"

// WebSocket opcodes
#define WS_OPCODE_TEXT 0x01
#define WS_OPCODE_BINARY 0x02
#define WS_OPCODE_CLOSE 0x08
#define WS_OPCODE_PING 0x09
#define WS_OPCODE_PONG 0x0A

// Parse WebSocket URL
static int parse_ws_url(const char* url, char* host, int* port, char* path) {
    const char* p = url;

    // Check for ws:// or wss://
    if (strncmp(p, "ws://", 5) == 0) {
        p += 5;
        *port = 80;
    } else if (strncmp(p, "wss://", 6) == 0) {
        LOG_E("WSS (secure WebSocket) not supported yet\r\n");
        return -1;
    } else {
        LOG_E("Invalid WebSocket URL (must start with ws://)\r\n");
        return -1;
    }

    // Extract host and port
    const char* path_start = strchr(p, '/');
    const char* port_start = strchr(p, ':');

    if (port_start && (!path_start || port_start < path_start)) {
        // Port is specified
        int host_len = port_start - p;
        if (host_len >= WHISPER_LIVE_MAX_HOST_LEN) {
            LOG_E("Host name too long\r\n");
            return -1;
        }
        memcpy(host, p, host_len);
        host[host_len] = '\0';

        // Parse port
        *port = atoi(port_start + 1);
        p = port_start + 1;
        while (*p && *p != '/') p++;
    } else {
        // No port specified, use default
        int host_len = path_start ? (path_start - p) : strlen(p);
        if (host_len >= WHISPER_LIVE_MAX_HOST_LEN) {
            LOG_E("Host name too long\r\n");
            return -1;
        }
        memcpy(host, p, host_len);
        host[host_len] = '\0';
        p += host_len;
    }

    // Extract path
    if (*p == '/') {
        strncpy(path, p, WHISPER_LIVE_MAX_PATH_LEN - 1);
        path[WHISPER_LIVE_MAX_PATH_LEN - 1] = '\0';
    } else {
        strcpy(path, "/");
    }

    return 0;
}

// Perform WebSocket handshake
static int websocket_handshake(int socket_fd, const char* host, const char* path) {
    char request[512];
    char response[1024];

    // Create WebSocket upgrade request
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
             "Sec-WebSocket-Version: 13\r\n"
             "\r\n",
             path, host);

    LOG_I("Sending WebSocket handshake...\r\n");

    // Send handshake
    int sent = send(socket_fd, request, strlen(request), 0);
    if (sent < 0) {
        LOG_E("Failed to send handshake\r\n");
        return -1;
    }

    // Receive response
    int received = recv(socket_fd, response, sizeof(response) - 1, 0);
    if (received <= 0) {
        LOG_E("Failed to receive handshake response\r\n");
        return -1;
    }
    response[received] = '\0';

    LOG_I("Handshake response: %s\r\n", response);

    // Check for 101 Switching Protocols
    if (strstr(response, "101") == NULL || strstr(response, "Switching Protocols") == NULL) {
        LOG_E("WebSocket handshake failed\r\n");
        return -1;
    }

    LOG_I("WebSocket handshake successful\r\n");
    return 0;
}

// Send WebSocket frame
static int send_ws_frame(int socket_fd, uint8_t opcode, const uint8_t* payload, uint32_t payload_len) {
    uint8_t header[14];
    int header_len = 2;

    // First byte: FIN=1, RSV=0, Opcode
    header[0] = 0x80 | (opcode & 0x0F);

    // Second byte: MASK=1, Payload length
    if (payload_len < 126) {
        header[1] = 0x80 | payload_len;
    } else if (payload_len < 65536) {
        header[1] = 0x80 | 126;
        header[2] = (payload_len >> 8) & 0xFF;
        header[3] = payload_len & 0xFF;
        header_len = 4;
    } else {
        // 64-bit length (Extended payload length = 127)
        header[1] = 0x80 | 127;
        header[2] = 0;
        header[3] = 0;
        header[4] = 0;
        header[5] = 0;
        header[6] = (payload_len >> 24) & 0xFF;
        header[7] = (payload_len >> 16) & 0xFF;
        header[8] = (payload_len >> 8) & 0xFF;
        header[9] = payload_len & 0xFF;
        header_len = 10;
    }

    // Generate masking key (simple random)
    uint8_t mask[4];
    uint32_t random = xTaskGetTickCount();
    mask[0] = (random >> 24) & 0xFF;
    mask[1] = (random >> 16) & 0xFF;
    mask[2] = (random >> 8) & 0xFF;
    mask[3] = random & 0xFF;

    // Add masking key to header
    memcpy(&header[header_len], mask, 4);
    header_len += 4;

    // Send header
    if (send(socket_fd, header, header_len, 0) < 0) {
        LOG_E("Failed to send WebSocket header\r\n");
        return -1;
    }

    // Mask and send payload in chunks to avoid large memory allocation
    #define WS_SEND_CHUNK_SIZE 1024
    uint8_t chunk_buffer[WS_SEND_CHUNK_SIZE];
    uint32_t offset = 0;
    
    while (offset < payload_len) {
        uint32_t chunk_len = payload_len - offset;
        if (chunk_len > WS_SEND_CHUNK_SIZE) {
            chunk_len = WS_SEND_CHUNK_SIZE;
        }
        
        // Copy and mask
        for (uint32_t i = 0; i < chunk_len; i++) {
            chunk_buffer[i] = payload[offset + i] ^ mask[(offset + i) % 4];
        }
        
        // Send chunk
        int sent = send(socket_fd, chunk_buffer, chunk_len, 0);
        if (sent < 0) {
            LOG_E("Failed to send WebSocket payload chunk at offset %d\r\n", offset);
            return -1;
        }
        
        offset += sent;
    }

    return payload_len;
}

// Initialize WhisperLive client
int whisper_live_init(whisper_live_client_t *client, const char *server_url) {
    if (!client || !server_url) {
        return -1;
    }

    memset(client, 0, sizeof(whisper_live_client_t));
    client->socket_fd = -1;

    // Parse URL
    if (parse_ws_url(server_url, client->host, &client->port, client->path) < 0) {
        return -1;
    }

    LOG_I("WhisperLive initialized: host=%s, port=%d, path=%s\r\n",
          client->host, client->port, client->path);

    return 0;
}

// Connect to WhisperLive server
int whisper_live_connect(whisper_live_client_t *client) {
    if (!client) {
        return -1;
    }

    struct sockaddr_in server_addr;
    struct hostent *server;

    LOG_I("Resolving hostname: %s\r\n", client->host);

    // Resolve hostname
    server = gethostbyname(client->host);
    if (server == NULL) {
        LOG_E("Failed to resolve hostname\r\n");
        return -1;
    }

    // Create socket
    client->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->socket_fd < 0) {
        LOG_E("Failed to create socket\r\n");
        return -1;
    }

    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(client->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client->socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(client->port);

    LOG_I("Connecting to %s:%d\r\n", client->host, client->port);

    // Connect
    if (connect(client->socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOG_E("Failed to connect to server\r\n");
        close(client->socket_fd);
        client->socket_fd = -1;
        return -1;
    }

    LOG_I("TCP connection established\r\n");

    // Perform WebSocket handshake
    if (websocket_handshake(client->socket_fd, client->host, client->path) < 0) {
        close(client->socket_fd);
        client->socket_fd = -1;
        return -1;
    }

    client->connected = true;
    client->config_sent = false;
    LOG_I("WhisperLive connected successfully\r\n");

    // Send configuration message (required by WhisperLive protocol)
    // Disable server-side VAD - device already does VAD before starting recording
    const char *config_json = "{"
        "\"uid\":\"aipi-voice-assistant\","
        "\"language\":\"zh\","
        "\"task\":\"transcribe\","
        "\"model\":\"small\","
        "\"use_vad\":false"
    "}";

    LOG_I("Sending WhisperLive config: %s\r\n", config_json);
    if (send_ws_frame(client->socket_fd, WS_OPCODE_TEXT, (const uint8_t*)config_json, strlen(config_json)) < 0) {
        LOG_E("Failed to send config message\r\n");
        close(client->socket_fd);
        client->socket_fd = -1;
        client->connected = false;
        return -1;
    }

    // Wait for server ready response
    char response[512];
    int resp_len = whisper_live_recv_transcription(client, response, sizeof(response), 5000);
    if (resp_len > 0) {
        LOG_I("Server response: %s\r\n", response);
    }

    client->config_sent = true;
    LOG_I("WhisperLive config sent successfully\r\n");

    return 0;
}

// Send audio data to WhisperLive server
int whisper_live_send_audio(whisper_live_client_t *client, const uint8_t *audio_data, uint32_t len) {
    if (!client || !client->connected || !audio_data) {
        return -1;
    }

    // Send audio as binary WebSocket frame
    return send_ws_frame(client->socket_fd, WS_OPCODE_BINARY, audio_data, len);
}

// Receive transcription from WhisperLive server
int whisper_live_recv_transcription(whisper_live_client_t *client, char *buffer, uint32_t buffer_size, uint32_t timeout_ms) {
    struct timeval timeout;
    uint8_t header[2];
    int received;
    uint8_t opcode, masked;
    uint32_t payload_len;
    int total_received;

    if (!client || !client->connected || !buffer) {
        LOG_E("recv_transcription: invalid params or not connected\r\n");
        return -1;
    }

    // Set timeout
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(client->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

retry_recv:
    // Receive WebSocket frame header
    received = recv(client->socket_fd, header, 2, 0);
    if (received <= 0) {
        // Timeout or error - don't log for short timeouts (expected during streaming)
        if (received == -1) {
            // Check if it's just a timeout (EAGAIN/EWOULDBLOCK) vs real error
            // In lwip, timeout returns -1 with errno EAGAIN
            return 0;  // Treat as timeout, not fatal error
        }
        return received;  // 0 = connection closed
    }

    opcode = header[0] & 0x0F;
    masked = (header[1] >> 7) & 0x01;
    payload_len = header[1] & 0x7F;

    LOG_I("WS frame: opcode=%d, masked=%d, len=%d\r\n", opcode, masked, payload_len);

    // Handle extended payload length
    if (payload_len == 126) {
        uint8_t len_bytes[2];
        if (recv(client->socket_fd, len_bytes, 2, 0) != 2) {
            return -1;
        }
        payload_len = (len_bytes[0] << 8) | len_bytes[1];
    } else if (payload_len == 127) {
        uint8_t len_bytes[8];
        if (recv(client->socket_fd, len_bytes, 8, 0) != 8) {
            return -1;
        }
        payload_len = (len_bytes[4] << 24) | (len_bytes[5] << 16) | (len_bytes[6] << 8) | len_bytes[7];
    }

    // Skip masking key if present
    if (masked) {
        uint8_t mask[4];
        recv(client->socket_fd, mask, 4, 0);
    }

    // Handle PING frame - respond with PONG
    if (opcode == WS_OPCODE_PING) {
        uint8_t ping_data[125];
        uint32_t ping_len = (payload_len < 125) ? payload_len : 125;
        if (ping_len > 0) {
            recv(client->socket_fd, ping_data, ping_len, 0);
        }
        send_ws_frame(client->socket_fd, WS_OPCODE_PONG, ping_data, ping_len);
        goto retry_recv;
    }

    // Handle CLOSE frame
    if (opcode == WS_OPCODE_CLOSE) {
        client->connected = false;
        return -1;
    }

    // Skip non-text frames
    if (opcode != WS_OPCODE_TEXT) {
        uint8_t drain[256];
        while (payload_len > 0) {
            uint32_t chunk = (payload_len < 256) ? payload_len : 256;
            int r = recv(client->socket_fd, drain, chunk, 0);
            if (r <= 0) break;
            payload_len -= r;
        }
        goto retry_recv;
    }

    // Receive text payload
    if (payload_len >= buffer_size) {
        payload_len = buffer_size - 1;
    }

    total_received = 0;
    while (total_received < payload_len) {
        received = recv(client->socket_fd, buffer + total_received, payload_len - total_received, 0);
        if (received <= 0) break;
        total_received += received;
    }

    if (total_received > 0) {
        buffer[total_received] = '\0';
    }

    return total_received;
}

// Send END_OF_AUDIO signal to server
int whisper_live_send_end_of_audio(whisper_live_client_t *client) {
    if (!client || !client->connected) {
        return -1;
    }

    const char *end_signal = "END_OF_AUDIO";
    LOG_I("Sending END_OF_AUDIO signal\r\n");

    // Send as binary frame (same as audio data)
    return send_ws_frame(client->socket_fd, WS_OPCODE_BINARY, (const uint8_t*)end_signal, strlen(end_signal));
}

// Disconnect from WhisperLive server
void whisper_live_disconnect(whisper_live_client_t *client) {
    if (!client) {
        return;
    }

    if (client->socket_fd >= 0) {
        // Send close frame
        uint8_t close_payload[2] = {0x03, 0xE8};  // Status code 1000
        send_ws_frame(client->socket_fd, WS_OPCODE_CLOSE, close_payload, 2);

        // Wait for server to close connection or timeout (100ms)
        // This prevents "Connection reset by peer" error on server side
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100 * 1000; // 100ms
        setsockopt(client->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        uint8_t buffer[128];
        int received;
        // Drain any remaining data/close frame
        while ((received = recv(client->socket_fd, buffer, sizeof(buffer), 0)) > 0) {
            // Just drain
        }

        close(client->socket_fd);
        client->socket_fd = -1;
    }

    client->connected = false;
    LOG_I("WhisperLive disconnected\r\n");
}
