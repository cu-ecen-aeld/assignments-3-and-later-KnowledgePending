#include <syslog.h>

// View syslog output using `journalctl -t writer`
int main(int argc, char *argv[]){
    openlog("writer", LOG_PID, LOG_USER);
    if (argc != 3) {
        syslog(LOG_ERR,
               "Invalid number of arguments: expected 2, got %d",
               argc - 1);
        closelog();
        return 1;
    }

    const char *write_file = argv[1];
    const char *write_str = argv[2];

    syslog(LOG_DEBUG,
           "Writing \"%s\" to \"%s\"",
           write_file,
           write_str);
    closelog();

    return 0;
}