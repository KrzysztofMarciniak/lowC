#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

extern uint64_t networking_extensions_test(uint64_t tcp_port, uint64_t udp_port);

struct TestServer {
    int tcp_socket;
    int udp_socket;
    unsigned int tcp_port;
    unsigned int udp_port;
    int tcp_failed;
    int udp_failed;
};

static int send_all(int descriptor, const char *data, size_t length)
{
    size_t sent = 0;

    while (sent < length) {
        ssize_t result = send(descriptor, data + sent, length - sent, 0);
        if (result <= 0) {
            return -1;
        }
        sent += (size_t)result;
    }
    return 0;
}

static void *udp_server(void *context)
{
    struct TestServer *server = context;
    struct sockaddr_storage peer;
    socklen_t peer_length = sizeof(peer);
    char buffer[32];
    ssize_t received = recvfrom(
        server->udp_socket,
        buffer,
        sizeof(buffer),
        0,
        (struct sockaddr *)&peer,
        &peer_length
    );

    if (received != 4 || memcmp(buffer, "ping", 4) != 0 ||
        sendto(
            server->udp_socket,
            "pong",
            4,
            0,
            (struct sockaddr *)&peer,
            peer_length
        ) != 4) {
        server->udp_failed = 1;
    }
    return NULL;
}

static int receive_request(int client, char *request, size_t capacity)
{
    size_t used = 0;
    size_t header_end = 0;
    size_t expected_body = 0;
    char *content_length;

    while (used + 1 < capacity) {
        ssize_t received = recv(client, request + used, capacity - used - 1, 0);
        if (received <= 0) {
            return -1;
        }
        used += (size_t)received;
        request[used] = '\0';
        content_length = strstr(request, "\r\n\r\n");
        if (content_length != NULL) {
            header_end = (size_t)(content_length - request) + 4;
            content_length = strstr(request, "Content-Length: ");
            if (content_length != NULL && (size_t)(content_length - request) < header_end) {
                expected_body = (size_t)strtoul(content_length + 16, NULL, 10);
            }
            if (used >= header_end + expected_body) {
                return (int)used;
            }
        }
    }
    return -1;
}

static void *tcp_server(void *context)
{
    struct TestServer *server = context;
    int request_number;

    for (request_number = 0; request_number < 5; request_number++) {
        int client = accept(server->tcp_socket, NULL, NULL);
        char request[2048] = {0};
        const char *response = NULL;

        if (client < 0 || receive_request(client, request, sizeof(request)) < 0) {
            server->tcp_failed = 1;
            if (client >= 0) {
                close(client);
            }
            return NULL;
        }

        if (strstr(request, "GET /old HTTP/1.0\r\n") != NULL) {
            response =
                "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n"
                "Connection: close\r\n\r\nhello";
        } else if (strstr(request, "POST /chunk HTTP/1.0\r\n") != NULL) {
            if (strstr(request, "X-Test: yes\r\n") == NULL ||
                strstr(request, "Content-Length: 4\r\n") == NULL ||
                strstr(request, "\r\n\r\ndata") == NULL) {
                server->tcp_failed = 1;
            }
            response =
                "HTTP/1.1 201 Created\r\nX-Reply: yes\r\n"
                "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
                "4\r\nWiki\r\n5;part=two\r\npedia\r\n"
                "0\r\nX-Trailer: ok\r\n\r\n";
        } else if (strstr(request, "GET /malformed HTTP/1.0\r\n") != NULL) {
            response =
                "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
                "Z\r\nbad\r\n0\r\n\r\n";
        } else if (strstr(request, "GET /partial HTTP/1.0\r\n") != NULL) {
            response = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nab";
        } else if (strstr(request, "GET /slow HTTP/1.0\r\n") != NULL) {
            response = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n";
        } else {
            server->tcp_failed = 1;
        }

        if (response != NULL &&
            send_all(client, response, strlen(response)) != 0) {
            server->tcp_failed = 1;
        }
        if (request_number == 4) {
            usleep(250000);
        }
        close(client);
    }
    return NULL;
}

static int make_socket(int type, unsigned int *port)
{
    int descriptor = socket(AF_INET, type, 0);
    struct sockaddr_in address = {0};
    socklen_t length = sizeof(address);

    if (descriptor < 0) {
        return -1;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(descriptor, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        getsockname(descriptor, (struct sockaddr *)&address, &length) < 0) {
        close(descriptor);
        return -1;
    }
    *port = ntohs(address.sin_port);
    return descriptor;
}

int main(void)
{
    struct TestServer server = {0};
    pthread_t tcp_thread;
    pthread_t udp_thread;
    uint64_t test_result;
    struct timeval timeout = {.tv_sec = 8, .tv_usec = 0};

    server.tcp_socket = make_socket(SOCK_STREAM, &server.tcp_port);
    server.udp_socket = make_socket(SOCK_DGRAM, &server.udp_port);
    if (server.tcp_socket < 0 || server.udp_socket < 0 ||
        listen(server.tcp_socket, 8) < 0) {
        return 2;
    }
    setsockopt(
        server.tcp_socket,
        SOL_SOCKET,
        SO_RCVTIMEO,
        &timeout,
        sizeof(timeout)
    );

    if (pthread_create(&tcp_thread, NULL, tcp_server, &server) != 0 ||
        pthread_create(&udp_thread, NULL, udp_server, &server) != 0) {
        return 2;
    }
    test_result = networking_extensions_test(server.tcp_port, server.udp_port);
    pthread_join(tcp_thread, NULL);
    pthread_join(udp_thread, NULL);
    close(server.tcp_socket);
    close(server.udp_socket);
    if (test_result != 0) {
        fprintf(stderr, "networking test failed at case %llu\n",
                (unsigned long long)test_result);
        return 1;
    }
    if (server.tcp_failed || server.udp_failed) {
        fprintf(stderr, "local test server rejected client behavior\n");
        return 1;
    }
    return 0;
}
