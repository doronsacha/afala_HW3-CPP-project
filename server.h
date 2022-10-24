#ifndef __SERVER_H__
#define __SERVER_H__

typedef struct thread_stat
{
    int id_;
    int total_;
    int static_;
    int dynamic_;
} thread_stat;


/// this is the job that all threads need to do, wait for a new request, handle it, and continue
void* workerThreadRoutine(void *stat);

#endif
