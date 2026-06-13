#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>

#define SERVER_PORT 9000
#define READ_BUF_SIZE 1024
#define ACCUM_BUF_SIZE 8192
static volatile sig_atomic_t exit_requested = 0;

static void handle_int_term_signal(int sig_no, siginfo_t *info, void *context)
{
    (void)sig_no;
    (void)info;
    (void)context;

    exit_requested = 1;
}

int main(void)
{
    int ret = EXIT_FAILURE;
    struct sigaction sig_action;
    memset(&sig_action, 0, sizeof(sig_action));
    sig_action.sa_sigaction = handle_int_term_signal;
    sig_action.sa_flags = SA_SIGINFO;
    sigemptyset(&sig_action.sa_mask);
    sigaction(SIGINT, &sig_action, 0);
    openlog("aesdsocket", LOG_PID, LOG_USER);
    FILE *fp = NULL;
    fp = fopen("/var/tmp/aesdsocketdata", "ab+");

    if (fp == NULL)
    {
        perror("fopen");
        goto cleanup;
    }

    int server_fd = -1;
    int new_socket = -1;
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
        if (exit_requested)
        {
            syslog(LOG_DEBUG, "Caught signal, exiting");
            break;
        }
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

    // sending data
    fflush(fp);

    if (fseek(fp, 0, SEEK_SET) != 0)
    {
        perror("fseek");
        goto cleanup;
    }

    char send_buf[1024];
    size_t bytes_read;

    while ((bytes_read = fread(send_buf,
                               1,
                               sizeof(send_buf),
                               fp)) > 0)
    {
        if (exit_requested)
        {
            syslog(LOG_DEBUG, "Caught signal, exiting");
            break;
        }

        size_t total_sent = 0;

        while (total_sent < bytes_read)
        {
            ssize_t sent = send(new_socket,
                                send_buf + total_sent,
                                bytes_read - total_sent,
                                0);

            if (sent < 0)
            {
                perror("send");
                goto cleanup;
            }

            total_sent += sent;
        }
    }
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
    syslog(LOG_DEBUG,
           "Closed connection from \"%s\"",
           client_ip);

    if (fp != NULL)
    {
        fclose(fp);
    }
    unlink("/var/tmp/aesdsocketdata");
    closelog();

    return ret;
}