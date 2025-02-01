#include "systemcalls.h"
/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
 */

//a simple function to invoke a syslog instance
void initiate_syslog()
{
    const char *ident = "SYSTEMCALLS_SAYS";
    // open a logging connection to syslog
    openlog(ident, LOG_PID|LOG_PERROR ,LOG_USER); 
}

bool do_system(const char *cmd)
{

    /*
     * TODO  add your code here
     *  Call the system() function with the command set in the cmd
     *   and return a boolean true if the system() call completed with success
     *   or false() if it returned a failure
     */
    initiate_syslog();
    int status_code = system(cmd);
    //if cmd is NULL we just return with fasle
    if (cmd == NULL)
    {
        syslog(LOG_ERR, "Given command is NULL!");
        closelog();
        return false;
    }
    //handle errors
    else if (status_code == -1)
    {
        syslog(LOG_ERR, "The child process could not be created");
        closelog();
        return false;
    }
// here  WIFEXITED checks if we exited normally and  WEXITSTATUS to check if status code was 0
    else if (WIFEXITED(status_code) && WEXITSTATUS(status_code) == 0)
    {
        syslog(LOG_DEBUG, "child process exited successfully");
        closelog();
        return true;
    }
    syslog(LOG_ERR, "child process exited with error code %d", status_code);
    closelog();
    return false;
}

/**
 * @param count -The numbers of variables passed to the function. The variables are command to execute.
 *   followed by arguments to pass to the command
 *   Since exec() does not perform path expansion, the command to execute needs
 *   to be an absolute path.
 * @param ... - A list of 1 or more arguments after the @param count argument.
 *   The first is always the full path to the command to execute with execv()
 *   The remaining arguments are a list of arguments to pass to the command in execv()
 * @return true if the command @param ... with arguments @param arguments were executed successfully
 *   using the execv() call, false if an error occurred, either in invocation of the
 *   fork, waitpid, or execv() command, or if a non-zero return value was returned
 *   by the command issued in @param arguments with the specified arguments.
 */

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char *command[count + 1];
    int i;
    for (i = 0; i < count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];
    initiate_syslog();
    //attempt to create a child process
    pid_t process_id = fork();
    int status;
    //error handling for fork
    if (process_id == -1)
    {
        syslog(LOG_ERR, "The child process could not be created!");
        closelog();
        va_end(args);
        return false;
    }
    else if (process_id == 0)
    {
        //replace current process with command[0]
        if (execv(command[0], command) == -1)
        {
            syslog(LOG_ERR, "execv was not successfull");
            closelog();
            va_end(args);
            exit(1); //indicate failure status
        } 
    }

    if (waitpid(process_id, &status, 0) == -1)
    {
        syslog(LOG_ERR, "waitpid was not successfull");
        closelog();
        va_end(args);
        return false;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
    {
        syslog(LOG_DEBUG, "child process exited succesfully");
        closelog();
        va_end(args);
        return true;
    }

    else{
        syslog(LOG_ERR, "child process exit failed with err code %d",status);
        closelog();
        va_end(args);
        return false;
    }
}

/**
 * @param outputfile - The full path to the file to write with command output.
 *   This file will be closed at completion of the function call.
 * All other parameters, see do_exec above
 */
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char *command[count + 1];
    int i;
    for (i = 0; i < count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];
    initiate_syslog();
    int fd = open(outputfile, O_WRONLY | O_TRUNC | O_CREAT, S_IWUSR | S_IRUSR);
    if (fd == -1)
    {
        syslog(LOG_ERR, "Error opening file");
        closelog();
        va_end(args);
        close(fd);
        return false;
    }
    pid_t process_id = fork();
    int status;
    if (process_id == -1)
    {
        syslog(LOG_ERR, "The child process could not be created!");
        closelog();
        close(fd);
        va_end(args);
        return false;
    }
    else if (process_id == 0)
    {
//calling dup2 after fork is done , else parent stdout messages are redirect to testfile as well
    if (dup2(fd, 1) == -1)
    {
        syslog(LOG_ERR, "Failed to redirect stdout to file");
        closelog();
        close(fd);
        va_end(args);
        return false;
    }
        if (execv(command[0], command) == -1)
        {
            syslog(LOG_ERR, "execv was not successfull");
            closelog();
            va_end(args);
            exit(1);
        }
    }
     close(fd);
    if (waitpid(process_id, &status, 0) == -1)
    {
        syslog(LOG_ERR, "waitpid was not successfull");
        closelog();
        va_end(args);
        return false;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
    {
        syslog(LOG_DEBUG, "child process exited succesfully");
        closelog();
        return true;
    }

   else{
        syslog(LOG_ERR, "child process exit failed with err code %d",status);
        closelog();
        va_end(args);
        return false;
    }
}
