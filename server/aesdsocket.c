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
#include <errno.h>

#define PORT       9000
#define READ_CHUNK 1024
#define DATA_FILE  "/var/tmp/aesdsocketdata"

static volatile sig_atomic_t stop = 0;

static void handle_signal(int sig)
{
    (void)sig;
    stop = 1;
}

static int send_all(int fd, const char *data, size_t len)
{
    size_t sent = 0;
    while (sent < len)
    {
        ssize_t s = send(fd, data + sent, len - sent, 0);
        if (s < 0) { if (errno == EINTR) continue; return -1; }
        if (s == 0) return -1;
        sent += (size_t)s;
    }
    return 0;
}

static int append_and_dump(FILE *fp, int client_fd, const char *data, size_t len)
{
    if (fwrite(data, 1, len, fp) != len) return -1;
    if (fflush(fp) != 0) return -1;

    if (fseek(fp, 0, SEEK_SET) != 0) return -1;
    clearerr(fp);

    char out[READ_CHUNK];
    size_t r;
    while ((r = fread(out, 1, sizeof(out), fp)) > 0)
        if (send_all(client_fd, out, r) != 0) return -1;
    return 0;
}

int main(void)
{
    openlog("aesdsocket", LOG_PID, LOG_USER);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    FILE *fp = fopen(DATA_FILE, "a+");
    if (!fp) { perror("fopen"); return EXIT_FAILURE; }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); fclose(fp); return EXIT_FAILURE; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    { perror("bind"); close(server_fd); fclose(fp); return EXIT_FAILURE; }

    if (listen(server_fd, 5) < 0)
    { perror("listen"); close(server_fd); fclose(fp); return EXIT_FAILURE; }

    while (!stop)
    {
        struct sockaddr_in caddr;
        socklen_t len = sizeof(caddr);

        int client_fd = accept(server_fd, (struct sockaddr *)&caddr, &len);
        if (client_fd < 0)
        {
            if (stop || errno == EINTR) break;
            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &caddr.sin_addr, client_ip, sizeof(client_ip));
        syslog(LOG_DEBUG, "Accepted connection from %s", client_ip);

        size_t cap  = READ_CHUNK;
        size_t used = 0;
        char  *buf  = malloc(cap);
        if (!buf) { close(client_fd); continue; }

        while (!stop)
        {
            if (used == cap)
            {
                size_t ncap = cap * 2;
                char *nb = realloc(buf, ncap);
                if (!nb) break;
                buf = nb;
                cap = ncap;
            }

            ssize_t n = read(client_fd, buf + used, cap - used);
            if (n < 0) { if (errno == EINTR) continue; break; }
            if (n == 0) break;
            used += (size_t)n;

            for (;;)
            {
                char *nl = memchr(buf, '\n', used);
                if (!nl) break;

                size_t pkt = (size_t)(nl - buf) + 1;
                if (append_and_dump(fp, client_fd, buf, pkt) != 0)
                    goto drop_client;

                memmove(buf, buf + pkt, used - pkt);
                used -= pkt;
            }
        }

    drop_client:
        free(buf);
        close(client_fd);
        syslog(LOG_DEBUG, "Closed connection from %s", client_ip);
    }

    close(server_fd);
    fclose(fp);
    unlink(DATA_FILE);
    closelog();
    return 0;
}
