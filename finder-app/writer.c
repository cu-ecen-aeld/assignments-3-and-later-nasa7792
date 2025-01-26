/*
This file contains the implementation of writer.c as specified in coursera requirements
of assignment-2
authour- Nalin Saxena
sources- Linux System Programming, https://www.gnu.org/software/libc/manual/html_node/openlog.html 
*/
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

/*
the required number of arguments is 3 
0 the argument is by default the file name
1st argument should be path of the file to be wrriten to
2nd argument should be text to be written
*/
#define REQUIRED_ARGS 3 
#define FAIL_EXIT 1
#define NORM_EXIT 0

//a simple function to send exit 1 signal and  close log
void cleanup_error_exit(){
    closelog();
    exit(FAIL_EXIT);
}

int main(int argc, char *argv[])
{
    const char *ident = "WRITER_SCRIPT_SAYS";
    //open a logging connection to syslog
    openlog(ident, LOG_PID | LOG_PERROR, LOG_USER);

// we first check if the required number of arguments are passed
    if (argc != REQUIRED_ARGS)
    {
        syslog(LOG_ERR, "The required number of arguments are 2");
        syslog(LOG_INFO, "Example usage: writer <file path> <content to be written>");
        cleanup_error_exit();
    }

    char *file_path = argv[1]; //get the file path
    char *text_arg = argv[2]; //get text argument to be written

    //open a file descriptor with trunc option since we need to override the contents
    int fd = open(file_path, O_WRONLY | O_TRUNC | O_CREAT, S_IWUSR | S_IRUSR);
    //check for errors in case unable to open or create files
    if (fd == -1)
    {
        syslog(LOG_ERR, "Unable to open/create the file");
        cleanup_error_exit();
    }
    size_t string_len = strlen(text_arg);
    //attempt to do a write operation
    ssize_t bytes_written = write(fd, text_arg, string_len);
    //check for write operation status
    if (bytes_written == -1)
    {
        syslog(LOG_ERR, "The write operation failed !");
        cleanup_error_exit();
    }
    else if (bytes_written < string_len)
    {
        syslog(LOG_ERR, "Only a partial write occured !");
        cleanup_error_exit();;
    }
    syslog(LOG_DEBUG, "Writing %s to %s", text_arg, file_path);
//attempt to close file
    if (close(fd) == -1)
    {
        syslog(LOG_ERR, "Unable to close file!");
        cleanup_error_exit();
    }
    syslog(LOG_INFO, "Closed File Successfuly");
    closelog();
    exit(0);
}
