
// 1. Opens a stream socket bound to port 9000, (there are two types of sockets udp based tcp based we are using tcp based)

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#define FAILURE -1
#define BACKLOG 10 // how many pending connections queue will hold
#define PORT_NUMBER "9000"
#define FILE_LOG_PATH "/var/tmp/aesdsocketdata"

int socket_fd;
void signal_handler(int sig_num)
{
    syslog(LOG_USER, "Caught Signal : %s\n", strsignal(sig_num));
    close(socket_fd);
    remove(FILE_LOG_PATH);
    closelog();
    exit(0);
}
int main(int argc, char *argv[])
{
    const char *ident = "SOCKET_SCRIPT_SAYS";
    // open a logging connection to syslog
    openlog(ident, LOG_PID | LOG_PERROR, LOG_USER);
    int status;
    struct addrinfo hints;
    struct addrinfo *res;
    struct sockaddr_storage client_addr;
    socklen_t addr_size;
    // char ipstr[INET6_ADDRSTRLEN];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    char client_ip[INET6_ADDRSTRLEN];
    int run_as_daemon = 0;
    if (argc > 1 && (strcmp(argv[1], "-d") == 0))
    {
        printf("Daemon?\n");
        syslog(LOG_INFO, "application will run as daemon \n");
        run_as_daemon = 1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // call get addrinfo
    if ((status = getaddrinfo(NULL, PORT_NUMBER, &hints, &res)) != 0)
    {
        syslog(LOG_ERR, "getaddrinfo error: %d\n", status);
        freeaddrinfo(res);
        closelog();
        return FAILURE;
    }
    // create socket
    socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (socket_fd == -1)
    {
        syslog(LOG_ERR, "Failed to open socket error: %d\n", socket_fd);
        freeaddrinfo(res);
        closelog();
        return FAILURE;
    }
    // bind socket
    int bind_stat;
    //concept taken with help of chat gpt- since full test immidately calls for another socket connection SO_REUSEADDR makes it available
    int optval = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    bind_stat = bind(socket_fd, res->ai_addr, res->ai_addrlen);
    if (bind_stat == -1)
    {
        syslog(LOG_ERR, "Failed to bind to socket error: %d\n", bind_stat);
        freeaddrinfo(res);
        closelog();
        return FAILURE;
    }
    freeaddrinfo(res);
    if (run_as_daemon)
    {
        pid_t daemon_pid = fork();
        if (daemon_pid == -1)
        {
            syslog(LOG_ERR, "Failed to create child process: Daemon cannot be created !");
            closelog();
            exit(FAILURE);
        }
        else if (daemon_pid > 0)
        {
            syslog(LOG_INFO, "Exiting parent process!");
            exit(EXIT_SUCCESS);
        }
        // value is 0
        else
        {
            if (setsid() == -1)
            {
                syslog(LOG_ERR, "Failed to set ssid for daemon !");
                exit(FAILURE);
                closelog();
            }
            // change to root
            chdir("/");
            // redirect stdout 0,1,2 to dev null
            int dev_null = open("/dev/null", O_RDWR);
            dup2(dev_null, STDIN_FILENO);
            dup2(dev_null, STDOUT_FILENO);
            dup2(dev_null, STDERR_FILENO);
        }
    }

    if (listen(socket_fd, BACKLOG) == -1)
    {
        syslog(LOG_ERR, "Failed to bind to socket, error: %s", strerror(errno));
        return FAILURE;
    }

    while (1)
    {
        addr_size = sizeof client_addr;
        int new_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &addr_size);
        if (new_fd == -1)
        {
            syslog(LOG_ERR, "Failed to accept to socket : %d\n", new_fd);
            continue; // skip iteration
        }
        struct sockaddr_in *client_info = (struct sockaddr_in *)&client_addr;
        inet_ntop(client_info->sin_family, &client_info->sin_addr, client_ip, sizeof(client_ip));

        // Log accepted connection
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        // open a new file
        int aux_fd_write = open(FILE_LOG_PATH, O_CREAT | O_WRONLY | O_APPEND, 0666);
        if (aux_fd_write == -1)
        {
            syslog(LOG_ERR, "Failed to open a new file");
            continue; // skip iteration
        }
        int buffer_size = 1024;
        int total_bytes = 0;
        char *temp_buffer = (char *)malloc(buffer_size);
        int bytes_recv;
        while ((bytes_recv = recv(new_fd, temp_buffer + total_bytes, buffer_size - total_bytes - 1, 0)) > 0)
        {
            total_bytes += bytes_recv;
            // make a valid string
            temp_buffer[total_bytes] = '\0';
            // check for a new line
            if (strchr(temp_buffer, '\n') != NULL)
                break;
            // if not then we need to increase buffer_size
            buffer_size += buffer_size;
            char *new_buffer = realloc(temp_buffer, buffer_size);
            if (new_buffer == NULL)
            {
                syslog(LOG_ERR, "Failed to extend buffer size!");
                free(temp_buffer);
                close(aux_fd_write);
                close(new_fd);
                return FAILURE;
            }
            temp_buffer = new_buffer;
        }
        // new line happened
        write(aux_fd_write, temp_buffer, total_bytes);
        close(aux_fd_write);

        // once a new line is appear we need to loop back to client
        int aux_fd_read = open(FILE_LOG_PATH, O_RDONLY);
        if (aux_fd_read == -1)
        {
            syslog(LOG_ERR, "Failed to open file to read");
            free(temp_buffer);
            close(new_fd);
            continue;
        }
        lseek(aux_fd_read, 0, SEEK_SET);
        int bytes_read;
        do
        {
            bytes_read = read(aux_fd_read, temp_buffer, buffer_size);
            if (bytes_read > 0)
                if (send(new_fd, temp_buffer, bytes_read, 0) == -1)
                {
                    syslog(LOG_ERR, "Error sending data to client");
                    break;
                }
        } while (bytes_read > 0);
        syslog(LOG_ERR, "closing file after send");
        free(temp_buffer);
        close(aux_fd_read);
        close(new_fd);
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }
    closelog();
}
