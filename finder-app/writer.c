#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include<string.h>
#define REQUIRED_ARGS 3

int main(int argc, char *argv[])
{

    const char *ident = "WRITER_SCRIPT_SAYS";
    openlog(ident, LOG_PID | LOG_PERROR, LOG_USER);

    if (argc != REQUIRED_ARGS)
    {
        syslog(LOG_ERR, "The required number of arguments are 2");
        syslog(LOG_INFO, "Example usage: writer <file path> <content to be written>");
        exit(1);
    }

    char *file_path = argv[1];
    char *text_arg = argv[2];

    int fd = open(file_path, O_WRONLY | O_TRUNC | O_CREAT, S_IWUSR | S_IRUSR);
    if (fd == -1)
    {
        syslog(LOG_ERR, "Unable to open/create the file");
        exit(1);
    }
    size_t string_len=strlen(text_arg);
    ssize_t bytes_written=write(fd,text_arg,string_len);
   
    if(bytes_written==-1){
        syslog(LOG_ERR, "The write operation failed !");
        exit(1);
    }
    else if(bytes_written<string_len){
         syslog(LOG_ERR, "Only a partial write occured !");
         exit(1);
    }
     syslog(LOG_DEBUG,"Writing %s to %s",text_arg,file_path);

     if(close(fd)==-1){
        syslog(LOG_ERR, "Unable to close file!");
        exit(1);
     }
     syslog(LOG_INFO, "Closed File Successfuly");
     exit(0);
}
