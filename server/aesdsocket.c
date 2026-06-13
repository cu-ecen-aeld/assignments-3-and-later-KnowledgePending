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
#define BUFFER_LEN 1024

int main(void)
{
    openlog("writer", LOG_PID, LOG_USER);
    int server_fd, new_socket;
    ssize_t valread;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int opt = 1;
    char buffer[BUFFER_LEN];

    memset(&address, 0, sizeof(address));

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET,
                   SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(SERVER_PORT);

    if (bind(server_fd,
             (struct sockaddr *)&address,
             sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0)
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    new_socket = accept(
        server_fd,
        (struct sockaddr *)&address,
        &addrlen
    );

    if (new_socket < 0)
    {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }

    valread = read(new_socket, buffer, BUFFER_LEN - 1);

    if (valread < 0)
    {
        perror("read failed");
        exit(EXIT_FAILURE);
    }

    buffer[valread] = '\0';

    printf("message received: %s\n", buffer);
    char client_ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET,
            &address.sin_addr,
            client_ip,
            sizeof(client_ip));

    syslog(LOG_DEBUG,
        "Accepted connection from \"%s\"",
        client_ip);

    close(new_socket);
    close(server_fd);
    closelog();

    return 0;
}