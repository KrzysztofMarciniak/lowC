#define _POSIX_C_SOURCE 200112L

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define HOST "192.168.1.224"
#define SERVICE "8000"
#define HEADER_CAPACITY 4096
#define BODY_CAPACITY 65536
#define RESPONSE_CAPACITY (HEADER_CAPACITY + BODY_CAPACITY + 1)

static int
contains_chunked(const unsigned char *header, size_t length)
{
    size_t index;

    for (index = 0; index + 7 <= length; index++) {
        static const char word[] = "chunked";
        size_t offset;

        for (offset = 0; offset < sizeof(word) - 1; offset++) {
            if (tolower(header[index + offset]) != word[offset]) {
                break;
            }
        }

        if (offset == sizeof(word) - 1) {
            return 1;
        }
    }

    return 0;
}

static long
download_to_file(void)
{
    struct addrinfo hints = {0};
    struct addrinfo *addresses = NULL;
    struct addrinfo *address;
    unsigned char response[RESPONSE_CAPACITY];
    const char request[] =
        "GET / HTTP/1.0\r\n"
        "Host: " HOST "\r\n"
        "Connection: close\r\n"
        "\r\n";
    size_t request_length = sizeof(request) - 1;
    size_t request_sent = 0;
    size_t response_length = 0;
    size_t header_length = 0;
    size_t body_offset;
    size_t status_offset;
    size_t index;
    size_t digit_index;
    int descriptor = -1;
    int resolver_result;
    int status_code = 0;
    long result = -111;
    ssize_t count;
    FILE *output;

    hints.ai_socktype = SOCK_STREAM;
    resolver_result = getaddrinfo(HOST, SERVICE, &hints, &addresses);
    if (resolver_result != 0) {
        int error = resolver_result < 0
            ? -resolver_result
            : resolver_result;
        return -1000 - error;
    }

    for (address = addresses; address != NULL; address = address->ai_next) {
        descriptor = socket(
            address->ai_family,
            address->ai_socktype,
            address->ai_protocol
        );
        if (descriptor < 0) {
            result = -errno;
            continue;
        }

        if (connect(descriptor, address->ai_addr, address->ai_addrlen) == 0) {
            break;
        }

        result = -errno;
        close(descriptor);
        descriptor = -1;
    }
    freeaddrinfo(addresses);

    if (descriptor < 0) {
        return result;
    }

    while (request_sent < request_length) {
        count = send(
            descriptor,
            request + request_sent,
            request_length - request_sent,
            MSG_NOSIGNAL
        );
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            result = count < 0 ? -errno : -20004;
            close(descriptor);
            return result;
        }
        request_sent += (size_t)count;
    }

    for (;;) {
        count = recv(
            descriptor,
            response + response_length,
            sizeof(response) - response_length,
            0
        );
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count < 0) {
            result = -errno;
            close(descriptor);
            return result;
        }
        if (count == 0) {
            break;
        }

        response_length += (size_t)count;
        if (response_length == sizeof(response)) {
            close(descriptor);
            return -20002;
        }
    }
    close(descriptor);

    for (index = 0; index + 3 < response_length; index++) {
        if (
            response[index] == '\r' &&
            response[index + 1] == '\n' &&
            response[index + 2] == '\r' &&
            response[index + 3] == '\n'
        ) {
            header_length = index + 4;
            break;
        }
    }
    if (header_length == 0 || header_length > HEADER_CAPACITY) {
        return -20001;
    }
    if (contains_chunked(response, header_length)) {
        return -20005;
    }

    for (index = 0; index < header_length && response[index] != ' '; index++) {
    }
    if (index == header_length || index + 3 >= header_length) {
        return -20001;
    }

    status_offset = index + 1;
    for (digit_index = 0; digit_index < 3; digit_index++) {
        unsigned char digit = response[status_offset + digit_index];

        if (digit < '0' || digit > '9') {
            return -20001;
        }
        status_code = status_code * 10 + digit - '0';
    }

    body_offset = header_length;
    if (response_length - body_offset > BODY_CAPACITY) {
        return -20002;
    }

    output = fopen("response.html", "wb");
    if (output == NULL) {
        return -errno;
    }

    index = 0;
    while (index < response_length - body_offset) {
        size_t written = fwrite(
            response + body_offset + index,
            1,
            response_length - body_offset - index,
            output
        );
        if (written == 0) {
            result = errno != 0 ? -errno : -20006;
            fclose(output);
            return result;
        }
        index += written;
    }

    if (fclose(output) != 0) {
        return -errno;
    }

    return status_code;
}

int
main(void)
{
    long result = download_to_file();

    if (result == 200) {
        puts("Saved response.html");
    } else if (result > 0) {
        printf("Saved response; HTTP status %ld\n", result);
    } else {
        printf("GET or file error: %ld\n", result);
    }

    return 0;
}
