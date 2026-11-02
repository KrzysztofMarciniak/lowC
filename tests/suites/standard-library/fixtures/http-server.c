#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void)
{
    int server = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address = {0};
    socklen_t address_length = sizeof(address);
    int request_number;

    if (server < 0) {
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(server, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        listen(server, 2) < 0 ||
        getsockname(server, (struct sockaddr *)&address, &address_length) < 0) {
        close(server);
        return 1;
    }

    printf("%u\n", (unsigned int)ntohs(address.sin_port));
    fflush(stdout);

    for (request_number = 0; request_number < 2; request_number++) {
        int client = accept(server, NULL, NULL);
        char request[1024] = {0};
        size_t request_length = 0;

        if (client < 0) {
            close(server);
            return 1;
        }

        while (request_number == 1 && request_length < sizeof(request) - 1) {
            ssize_t received = recv(
                client,
                request + request_length,
                sizeof(request) - 1 - request_length,
                0
            );
            if (received <= 0) {
                break;
            }

            request_length += (size_t)received;
            if (strstr(request, "\r\n\r\n") != NULL) {
                break;
            }
        }

        if (request_number == 1) {
            static const char response[] =
                "HTTP/1.0 200 OK\r\n"
                "Content-Length: 12\r\n"
                "Connection: close\r\n"
                "\r\n"
                "hello world!";
            size_t sent = 0;

            if (strstr(request, "GET /test HTTP/1.0\r\n") == NULL) {
                close(client);
                close(server);
                return 1;
            }

            while (sent < sizeof(response) - 1) {
                ssize_t written = send(
                    client,
                    response + sent,
                    sizeof(response) - 1 - sent,
                    0
                );
                if (written <= 0) {
                    close(client);
                    close(server);
                    return 1;
                }
                sent += (size_t)written;
            }
        }

        close(client);
    }

    close(server);
    return 0;
}
