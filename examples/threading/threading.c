#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <error.h>

// Optional: use these functions to add debug or error prints to your application
//#define DEBUG_LOG(msg,...)
#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    DEBUG_LOG("inside threadfunc");
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    usleep(thread_func_args->wait_to_obtain_ms * 1000);
    DEBUG_LOG("waited %ds, obtaining lock", thread_func_args->wait_to_obtain_ms);
    pthread_mutex_lock(thread_func_args->mutex);
    usleep(thread_func_args->wait_to_release_ms * 1000);
    DEBUG_LOG("waited %ds, releasing lock", thread_func_args->wait_to_release_ms);
    pthread_mutex_unlock(thread_func_args->mutex);

    DEBUG_LOG("thread complete success set to true");
    thread_func_args->thread_complete_success=true;

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    
     struct thread_data *thread_param = (struct thread_data *)calloc(1, sizeof(struct thread_data));
     if(thread_param == NULL)
     {
	     ERROR_LOG("memory allocation failed for thread_param");
	     return false;
     }
     DEBUG_LOG("memory allocation success for thread_param");
     thread_param->wait_to_obtain_ms=wait_to_obtain_ms;
     thread_param->wait_to_release_ms=wait_to_release_ms;

     thread_param->mutex=mutex;

     if(pthread_create(thread, NULL, threadfunc, thread_param) == -1)
     {
	     ERROR_LOG("Thread creation failed");
             return false;
     }
     DEBUG_LOG("thread created sucsessfully");
   /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    return true;
}

