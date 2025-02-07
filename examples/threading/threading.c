#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    thread_func_args->thread_complete_success=false;
    usleep(thread_func_args->wait_to_obtain_ms*1000);
    int mutex_lock_ret_status=pthread_mutex_lock(thread_func_args->mutex);
    if(mutex_lock_ret_status!=0){
        ERROR_LOG("mutex lock failed ! %d",mutex_lock_ret_status);
    }
    else{
       usleep(thread_func_args->wait_to_release_ms*1000); 
       int mutex_release_stat=pthread_mutex_unlock(thread_func_args->mutex);
       if(mutex_release_stat!=0){
        ERROR_LOG("mutex release failed ! %d",mutex_release_stat);
       }
       else{
        DEBUG_LOG("mutex lock released successfully!");
        thread_func_args->thread_complete_success=true;
       }
    }
    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */ 
    struct thread_data *ptr_thread_data=(struct thread_data*)malloc(sizeof(struct thread_data));
    ptr_thread_data->mutex=mutex;
    ptr_thread_data->thread_complete_success=false;
    ptr_thread_data->wait_to_obtain_ms=wait_to_obtain_ms;
    ptr_thread_data->wait_to_release_ms=wait_to_release_ms;

    bool thread_creation_stat=pthread_create(thread,NULL,threadfunc,ptr_thread_data);
    if(thread_creation_stat!=0){
        ERROR_LOG("failed to create thread");
        return false;
    }
    DEBUG_LOG("thread created successfully");
    return true;
}

