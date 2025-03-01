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
#include <pthread.h>
#include <sys/queue.h>
#include <stdbool.h>
#define FAILURE -1
#define BACKLOG 10 // how many pending connections queue will hold
#define PORT_NUMBER "9000"
#define FILE_LOG_PATH "/var/tmp/aesdsocketdata" // path to output file

int socket_fd;
bool sig_recv = false;
pthread_mutex_t mutex_socket;
pthread_t timestamp_thread;
// signal handler
void signal_handler(int sig_num)
{
    syslog(LOG_USER, "Caught Signal : %s\n", strsignal(sig_num));
    close(socket_fd);
    remove(FILE_LOG_PATH);
    sig_recv = true; // indicate signal caught
    closelog();
    exit(0);
}

typedef struct thread_node
{
    pthread_t id;
    int thread_fd;
    bool is_thread_complete;
    char client_ip[INET6_ADDRSTRLEN];
    SLIST_ENTRY(thread_node)
    next_ptr;
} thread_node;

void *append_timestamp_to_file(void *arg)
{
    // written with the help of chatgpt for formatting in RFC 2822
    while (1)
    {
        // sleep for 10 seconds
        sleep(10);

        // Get current system time
        time_t rawtime;
        struct tm *timeinfo;
        char timestamp_buffer[100];

        time(&rawtime);
        timeinfo = localtime(&rawtime);

        // Format time in RFC 2822 format
        strftime(timestamp_buffer, sizeof(timestamp_buffer), "timestamp:%a, %d %b %Y %H:%M:%S %z\n", timeinfo);

        // Open file for appending with file locking
        int aux_fd_write = open(FILE_LOG_PATH, O_CREAT | O_WRONLY | O_APPEND, 0666);
        if (aux_fd_write == -1)
        {
            syslog(LOG_ERR, "Failed to open file for timestamp");
            continue;
        }
        syslog(LOG_INFO, "10 seconds passed");
        // Lock the file for atomic writing
        pthread_mutex_lock(&mutex_socket);

        // Write the timestamp to the file
        if (write(aux_fd_write, timestamp_buffer, strlen(timestamp_buffer)) != strlen(timestamp_buffer))
        {
            syslog(LOG_ERR, "Failed to write timestamp to file");
        }

        // Unlock the file after writing
        pthread_mutex_unlock(&mutex_socket);
        close(aux_fd_write);
    }

    return NULL;
}

int thread_task(void *thread_args)
{
    thread_node *thread_info = (thread_node *)thread_args;
    int new_fd = thread_info->thread_fd;
    // open a new file
    int error_status = 0;
    int aux_fd_write = open(FILE_LOG_PATH, O_CREAT | O_WRONLY | O_APPEND, 0666);
    if (aux_fd_write == -1)
    {
        syslog(LOG_ERR, "Failed to open a new file");
        thread_info->is_thread_complete = true;
        error_status = 1;
        return -1;
    }
//below logic remains same as assignment 5
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
            thread_info->is_thread_complete = true;
            error_status = 1;
            return error_status;
        }
        temp_buffer = new_buffer;
    }
    // new line happened
    //use mutex for reads and wites
    pthread_mutex_lock(&mutex_socket);
    if (write(aux_fd_write, temp_buffer, total_bytes) != total_bytes)
    {
        syslog(LOG_ERR, "Failed to  write data to socket file");
        free(temp_buffer);
        close(aux_fd_write);
        close(new_fd);
        pthread_mutex_unlock(&mutex_socket);
        thread_info->is_thread_complete = true;
        error_status = 1;
        return -1;
    }
    pthread_mutex_unlock(&mutex_socket);
    close(aux_fd_write);

    // once a new line is appear we need to loop back to client
    int aux_fd_read = open(FILE_LOG_PATH, O_RDONLY);
    if (aux_fd_read == -1)
    {
        syslog(LOG_ERR, "Failed to open file to read");
        free(temp_buffer);
        close(new_fd);
        thread_info->is_thread_complete = true;
        error_status = 1;
        return -1;
    }
    lseek(aux_fd_read, 0, SEEK_SET);
    int bytes_read;
    pthread_mutex_lock(&mutex_socket); // lock before reading
    do
    {
        bytes_read = read(aux_fd_read, temp_buffer, buffer_size);
        if (bytes_read > 0)
            if (send(new_fd, temp_buffer, bytes_read, 0) == -1)
            {
                syslog(LOG_ERR, "Error sending data to client");
                error_status = 1;
                break;
            }
    } while (bytes_read > 0);
    pthread_mutex_unlock(&mutex_socket);
    syslog(LOG_ERR, "closing file after send");
    free(temp_buffer);
    close(aux_fd_read);
    close(new_fd);
    syslog(LOG_INFO, "Closeded connection from %s", thread_info->client_ip);
    thread_info->is_thread_complete = true; //indicate thread executed to completion 
    return error_status == 1 ? -1 : 0;
}

int main(int argc, char *argv[])
{
    thread_node *curr_ptr = NULL;
    thread_node *itr_ptr = NULL;
    pthread_mutex_init(&mutex_socket, NULL);
    SLIST_HEAD(thread_list, thread_node)
    head;
    SLIST_INIT(&head);
    bool terminate_main_thread = false;
    const char *ident = "SOCKET_SCRIPT_SAYS";
    // open a logging connection to syslog
    openlog(ident, LOG_PID | LOG_PERROR, LOG_USER);
    int status;
    struct addrinfo hints;
    struct addrinfo *res;
    struct sockaddr_storage client_addr;
    socklen_t addr_size;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int run_as_daemon = 0;
    // check if application should be run as daemon
    if (argc > 1 && (strcmp(argv[1], "-d") == 0))
    {
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
    // concept taken with help of chat gpt- since full test immidately calls for another socket connection SO_REUSEADDR makes it available
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
    // check daemon mode
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
        // value is 0 meaning chuld is successfully created
        else
        {
            if (setsid() == -1)
            {
                syslog(LOG_ERR, "Failed to set ssid for daemon !");
                exit(FAILURE);
                closelog();
            }
            // change to root
            if(chdir("/")!=0){
                 syslog(LOG_ERR, "chdir root failed");
                   closelog();
                exit(FAILURE);
              
            }
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
//start timestamp thread
    if (pthread_create(&timestamp_thread, NULL, append_timestamp_to_file, NULL) != 0)
    {
        syslog(LOG_ERR, "Failed to create timestamp thread");
        return FAILURE;
    }
//run till signal is recieved or error happens
    while (!terminate_main_thread && !sig_recv)
    {
        addr_size = sizeof client_addr;
        int new_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &addr_size);
        if (new_fd == -1)
        {
            syslog(LOG_ERR, "Failed to accept to socket : %d\n", new_fd);
            continue; // skip iteration
        }
        thread_node *current_thread_data = (thread_node *)malloc(sizeof(thread_node));
        if (current_thread_data == NULL)
        {
            syslog(LOG_ERR, "malloc operation failed exiting");
            terminate_main_thread = true;
            break;
        }
        struct sockaddr_in *client_info = (struct sockaddr_in *)&client_addr;
        inet_ntop(client_info->sin_family, &client_info->sin_addr, current_thread_data->client_ip, sizeof(current_thread_data->client_ip));
        // Log accepted connection
        syslog(LOG_INFO, "Accepted connection from %s", current_thread_data->client_ip);
        current_thread_data->is_thread_complete = false;
        current_thread_data->thread_fd = new_fd;
        pthread_t thread_id;
        int thread_stat = pthread_create(&thread_id, NULL, (void *)thread_task, current_thread_data);
        if (thread_stat == -1)
        {
            syslog(LOG_ERR, "Unable to create thread");
            terminate_main_thread = true;
            break;
        }
        current_thread_data->id = thread_id;
        SLIST_INSERT_HEAD(&head, current_thread_data, next_ptr);
        curr_ptr = SLIST_FIRST(&head);
        while (curr_ptr != NULL)
        {
            itr_ptr = SLIST_NEXT(curr_ptr, next_ptr); // Store the next item before potential removal
            if (curr_ptr->is_thread_complete)
            {
                // Join the completed thread
                if (pthread_join(curr_ptr->id, NULL) != 0)
                {
                    syslog(LOG_ERR, "Unable to join thread: %s", strerror(errno));
                }
                else
                {
                    syslog(LOG_INFO, "Thread joined successfully");
                }

                // Remove from the list
                SLIST_REMOVE(&head, curr_ptr, thread_node, next_ptr);
                syslog(LOG_INFO, "Removing thread resources");

                // Free memory and prevent dangling pointer
                free(curr_ptr);
            }

            // Move to the next item
            curr_ptr = itr_ptr;
        }
    }
    pthread_join(timestamp_thread, NULL);
    remove(FILE_LOG_PATH);
    closelog();
    close(socket_fd);
}
