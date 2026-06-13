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

#define PORT 9000
#define BUF_SIZE 1024

static volatile sig_atomic_t stop = 0;

static void handle_signal(int sig)
{
    (void)sig;
    stop = 1;
}

int main(void)
{
    openlog("aesdsocket", LOG_PID, LOG_USER);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* IMPORTANT: use a+ not ab+ */
    FILE *fp = fopen("/var/tmp/aesdsocketdata", "a+");
    if (!fp)
    {
        perror("fopen");
        return EXIT_FAILURE;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
               &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 5) < 0)
    {
        perror("listen");
        return EXIT_FAILURE;
    }

    char client_ip[INET_ADDRSTRLEN];

    while (!stop)
    {
        socklen_t len = sizeof(addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&addr, &len);
        if (client_fd < 0)
        {
            if (stop) break;
            perror("accept");
            continue;
        }

        inet_ntop(AF_INET, &addr.sin_addr, client_ip, sizeof(client_ip));
        syslog(LOG_DEBUG, "Accepted connection from %s", client_ip);

        char buf[BUF_SIZE];
        size_t used = 0;
        int done = 0;

        while (!done && !stop)
        {
            ssize_t n = read(client_fd, buf + used, sizeof(buf) - used);

            if (n <= 0)
                break;

            used += n;

            char *nl = memchr(buf, '\n', used);
            if (!nl)
                continue;

            size_t packet_len = nl - buf + 1;

            /* append full packet */
            fwrite(buf, 1, packet_len, fp);
            fflush(fp);

            /* IMPORTANT FIX: reset stream state before reading */
            fflush(fp);
            fseek(fp, 0, SEEK_SET);
            clearerr(fp);

            char out[BUF_SIZE];
            size_t r;

            while ((r = fread(out, 1, sizeof(out), fp)) > 0)
            {
                size_t sent = 0;

                while (sent < r)
                {
                    ssize_t s = send(client_fd, out + sent, r - sent, 0);
                    if (s <= 0)
                    {
                        close(client_fd);
                        goto cleanup;
                    }
                    sent += s;
                }
            }

            done = 1;
        }

        close(client_fd);
        syslog(LOG_DEBUG, "Closed connection from %s", client_ip);
    }

cleanup:
    close(server_fd);
    fclose(fp);
    unlink("/var/tmp/aesdsocketdata");
    closelog();

    return 0;
}