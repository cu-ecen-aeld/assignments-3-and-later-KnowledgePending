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

    FILE *fp = fopen("/var/tmp/aesdsocketdata", "ab+");
    if (!fp)
    {
        perror("fopen");
        return EXIT_FAILURE;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        goto cleanup;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        goto cleanup;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind");
        goto cleanup;
    }

    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        goto cleanup;
    }

    char client_ip[INET_ADDRSTRLEN];

    while (!exit_requested)
    {
        socklen_t addrlen = sizeof(address);

        int new_socket = accept(server_fd,
                                 (struct sockaddr *)&address,
                                 &addrlen);

        if (new_socket < 0)
        {
            if (exit_requested)
                break;
            perror("accept");
            continue;
        }

        inet_ntop(AF_INET, &address.sin_addr,
                  client_ip, sizeof(client_ip));

        syslog(LOG_DEBUG, "Accepted connection from \"%s\"", client_ip);

        char read_buf[READ_BUF_SIZE];
        char accum[ACCUM_BUF_SIZE];
        size_t accum_len = 0;

        int done = 0;

        while (!done && !exit_requested)
        {
            ssize_t n = read(new_socket, read_buf, sizeof(read_buf));

            if (n <= 0)
                break;

            if (accum_len + n > sizeof(accum))
            {
                fprintf(stderr, "message too large\n");
                break;
            }

            memcpy(accum + accum_len, read_buf, n);
            accum_len += n;

            char *start = accum;
            char *newline;

            while ((newline = memchr(start, '\n',
                   accum + accum_len - start)) != NULL)
            {
                size_t len = newline - start + 1;

                // append packet
                fwrite(start, 1, len, fp);
                fflush(fp);

                // rewind and send full file
                fflush(fp);
                fseek(fp, 0, SEEK_SET);

                char send_buf[1024];
                size_t bytes;

                while ((bytes = fread(send_buf, 1, sizeof(send_buf), fp)) > 0)
                {
                    size_t sent_total = 0;

                    while (sent_total < bytes)
                    {
                        ssize_t sent = send(new_socket,
                                            send_buf + sent_total,
                                            bytes - sent_total,
                                            0);

                        if (sent < 0)
                        {
                            perror("send");
                            close(new_socket);
                            goto cleanup;
                        }

                        sent_total += sent;
                    }
                }

                done = 1;

                // consume processed data
                start = newline + 1;
                break;
            }

            // shift leftover
            size_t remaining = accum + accum_len - start;
            memmove(accum, start, remaining);
            accum_len = remaining;
        }

        close(new_socket);
        syslog(LOG_DEBUG, "Closed connection from \"%s\"", client_ip);
    }

cleanup:
    if (server_fd >= 0)
        close(server_fd);

    if (fp)
        fclose(fp);

    unlink("/var/tmp/aesdsocketdata");
    closelog();

    return ret;
}