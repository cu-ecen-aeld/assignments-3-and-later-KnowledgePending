#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>

#define SERVER_PORT 9000
#define READ_BUF_SIZE 1024
#define ACCUM_BUF_SIZE 8192

int main(void)
{
    int ret = EXIT_FAILURE;
    openlog("aesdsocket", LOG_PID, LOG_USER);
    FILE *fp = fopen("/var/tmp/aesdsocketdata", "ab+");

    if (fp == NULL)
    {
        perror("fopen");
        goto cleanup;
    }

    int server_fd, new_socket = -1;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int opt = 1;

    memset(&address, 0, sizeof(address));

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket failed");
        goto cleanup;
    }

    if (setsockopt(server_fd, SOL_SOCKET,
                   SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt failed");
        goto cleanup;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(SERVER_PORT);

    if (bind(server_fd,
             (struct sockaddr *)&address,
             sizeof(address)) < 0)
    {
        perror("bind failed");
        goto cleanup;
    }

    if (listen(server_fd, 3) < 0)
    {
        perror("listen failed");
        goto cleanup;
    }

    new_socket = accept(
        server_fd,
        (struct sockaddr *)&address,
        &addrlen);

    if (new_socket < 0)
    {
        perror("accept failed");
        goto cleanup;
    }

    char client_ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET,
              &address.sin_addr,
              client_ip,
              sizeof(client_ip));

    syslog(LOG_DEBUG,
           "Accepted connection from \"%s\"",
           client_ip);

    // recieving data
    char read_buf[READ_BUF_SIZE];
    char accum[ACCUM_BUF_SIZE];
    size_t accum_len = 0;

    for (;;)
    {
        ssize_t n = read(new_socket,
                         read_buf,
                         sizeof(read_buf));

        if (n < 0)
        {
            perror("read");
            break;
        }

        if (n == 0)
        {
            /* client disconnected */
            break;
        }

        if (accum_len + n > sizeof(accum))
        {
            fprintf(stderr, "message too large\n");
            break;
        }

        memcpy(accum + accum_len, read_buf, n);
        accum_len += n;

        char *line_start = accum;
        char *newline;

        while ((newline = memchr(line_start,
                                 '\n',
                                 accum + accum_len - line_start)) != NULL)
        {
            size_t line_len = newline - line_start + 1;

            fwrite(line_start, 1, line_len, fp);
            fflush(fp);

            line_start = newline + 1;
        }

        size_t remaining = accum + accum_len - line_start;

        memmove(accum, line_start, remaining);
        accum_len = remaining;
    }

    if (accum_len > 0)
    {
        fwrite(accum, 1, accum_len, fp);
    }

    close(new_socket);
    close(server_fd);
    syslog(LOG_DEBUG,
           "Closed connection from \"%s\"",
           client_ip);

    fclose(fp);
    closelog();

    ret = EXIT_SUCCESS;

    cleanup:

    if (new_socket >= 0)
    {
        close(new_socket);
    }

    if (server_fd >= 0)
    {
        close(server_fd);
    }

    if (fp != NULL)
    {
        fclose(fp);
    }

    closelog();

    return ret;
}