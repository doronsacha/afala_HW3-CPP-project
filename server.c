#include "segel.h"
#include "request.h"
#include "list.h"
#include "server.h"
#include <math.h>

List* requests_list;
pthread_mutex_t list_padlock;
pthread_cond_t list_empty_cond;
pthread_cond_t list_full_cond;

//
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//
///************************************************************************************************************************************///
///****************************************    static functions headers     ************************************************************///
///************************************************************************************************************************************///
///initialize the mutex on the request_list
static void initializePadlock();
///initialize the conditions on the mutex ( if the list is full or empty)
static void initializeConditions();
/// initialize the both conditions and the mutex
static void initializeConditionsAndPadlock();
/// just allocate the array of threads,
/// * @param num_of_threads : the num of threads that we want to keep in the array
///  * @return : a pointer to the first element of the allocated array
static pthread_t* workingThreadsAllocation(int num_of_threads);
/// simply allocate a new thread_stat object and return it
static thread_stat* allocateNewStat();
/// initialize a stat previously allocated, the id is initialized to i and the rest to 0
static void initializeStat(int i,thread_stat* stat);
/// using the mutex to lock the request_list for protect it from other threads
static void lockList();
/// unlock the mutex, after that all the threads can access to the request_list
static void unlockList();
///wait in this function "kol od" the request_list is full
static void waitUntilNotFull();
/// wait in this function "kol od" the request_list is empty
static void waitUntilNotEmpty();
/// send a signal to all the threads that the list in not empty( there is at least one element)
static void nowTheListIsNOTEmpty();
///send a signal to all the threads that the request list is not full( there is at least one place for a new request)
static void nowTheListIsNOTFull();
///get the number of waiting request in the list and return the number of request that we need to drop randomly
static int numToDelete(int num_of_waiting);
///delete the "num to delete" element from the request_list
static void deleteThem(int num_to_delete);
/// wait until the list is non empty
static void waitForTheListNonEmpty();
/// used after overloading the sched_alg, we chose a request to handle and return it
static Node* getRequestToHandle();
/// do the request and updat ethe stat, then close the request(connfd)
static void doTheRequest(Node* request,thread_stat* stat);
/// do the block algorithm as request in the schedalg argument of the server
static int blockOverload(int lst_size);
/// do the random algorithm as request in the schedalg argument of the server
static int randomOverload();
/// do the drop tail algorithm as request in the schedalg argument of the server
static int droptailOverload(int connfd);
/// do the drop head algorithm as request in the schedalg argument of the server
static int dropheadOverload();
/// handle the full request_list problem using the requested algorithm
/// the function return 1 if the sched_alg was dropTail and the request_list had some waiting request, else return 0
static int overloading(int lst_size, char* sched_arg, int connfd);
/// compute the arrival time and return it
static struct timeval computeArrivalTime();
///insert to the request_list the new request
static void insertRequestToSystem(int connfd, struct timeval arrival_time);
/// try to handle request, if the request_list is full then jump to the overloading function
///return 1 if the sched_alg was dropTail and the request_list was full and has some waiting request, else return 0
static int tryToHandleRequest(int queue_size, char* sched_alg, int connfd);
/// create threads with his work routine and his arguments
static int createThreadWithStat(thread_stat* stat,pthread_t* working_thread_arr, int index);
/// create the threads pool as suggested in the homework
static int createThreadsPool(int num_of_threads,pthread_t* working_thread_arr);
/// allocate all the threads create them and place them in the pool of threads
/// if the operation success return 0 else retrun -1 if OUT_OF_MEMORY, return the error code of the pthreads create else
static int create_all_threads(int num_of_threads);


///************************************************************************************************************************************///
///********************************************      static functions      ************************************************************///
///************************************************************************************************************************************///
static void initializePadlock()
{
    pthread_mutex_init(&list_padlock, NULL);
}
static void initializeConditions()
{
    pthread_cond_init(&list_empty_cond, NULL);
    pthread_cond_init(&list_full_cond, NULL);
}
static void initializeConditionsAndPadlock()
{
    initializeConditions();
    initializePadlock();
}

static pthread_t* workingThreadsAllocation(int num_of_threads)
{
    return (pthread_t *)calloc(num_of_threads, sizeof(pthread_t));
}
thread_stat* allocateNewStat()
{
    return (thread_stat *)malloc(sizeof(thread_stat));
}

static void initializeStat(int i,thread_stat* stat)
{
    stat->id_ = i;
    stat->dynamic_ = 0;
    stat->static_ = 0;
    stat->total_ = 0;
}

static void lockList()
{
    pthread_mutex_lock(&list_padlock);
}

static void unlockList()
{
    pthread_mutex_unlock(&list_padlock);
}

static void waitUntilNotFull()
{
    pthread_cond_wait(&list_full_cond, &list_padlock);
}

static void waitUntilNotEmpty()
{
    pthread_cond_wait(&list_empty_cond, &list_padlock);
}

static void nowTheListIsNOTEmpty()
{
    pthread_cond_signal(&list_empty_cond);
}

static void nowTheListIsNOTFull()
{
    pthread_cond_signal(&list_full_cond);
}
static int numToDelete(int num_of_waiting)
{
    double res = num_of_waiting;
    res++;
    res=res*0.3;
    return (int) ceil(res);
}

static void deleteThem(int num_to_delete)
{
    for(int i = 0; i < num_to_delete; i++)
    {
        int n = rand() % requests_list->num_of_waiting;
        Close(listRemoveNth(requests_list, n +1)->connfd);
    }
}
static void waitForTheListNonEmpty()
{
    while(requests_list->num_of_waiting == 0)
    {
        waitUntilNotEmpty();
    }
}

static Node* getRequestToHandle()
{
    return listRemoveHead(requests_list);
}

static void doTheRequest(Node* request,thread_stat* stat)
{
    requestHandle(request->connfd, request->arrivalTm, request->dispatchTm, stat);
    Close(request->connfd);
}


static int blockOverload(int lst_size)
{
    while(isFull(requests_list,lst_size))
    {
        waitUntilNotFull();
    }
    return 0;
}

static int randomOverload()
{
    int num_to_delete = numToDelete(requests_list->num_of_waiting);
    deleteThem(num_to_delete);
    return 0;
}

static int droptailOverload(int connfd)
{
    Close(connfd);
    unlockList();
    return 1;
}

static int dropheadOverload()
{
    Close(listRemoveHead(requests_list)->connfd);
    return 0;
}

static int overloading(int lst_size, char* sched_arg, int connfd)
{
    if(!strcmp(sched_arg, "block"))
    {
        return blockOverload(lst_size);
    }
    else if((strcmp(sched_arg, "dt")) && (strcmp(sched_arg, "dh")) && RequestWaitIn(requests_list))
    {
        return randomOverload();
    }
    else if(!strcmp(sched_arg, "dt") && (RequestWaitIn(requests_list)))
    {
        return droptailOverload(connfd);
    }
    else
    {
        return dropheadOverload();
    }

}

static struct timeval computeArrivalTime()
{
    struct timeval arrival_time;
    gettimeofday(&arrival_time, NULL);
    return arrival_time;
}

static void insertRequestToSystem(int connfd, struct timeval arrival_time)
{
    listAdd(requests_list, connfd, arrival_time);
    nowTheListIsNOTEmpty();
}


static int tryToHandleRequest(int queue_size, char* sched_alg, int connfd)
{
    if(isFull(requests_list,queue_size))
    {
        if(overloading(queue_size,sched_alg, connfd)==1)
        {
            return 1;
        }
    }
    return 0;
}

static int createThreadWithStat(thread_stat* stat,pthread_t* working_thread_arr, int index)
{
    return pthread_create(&working_thread_arr[index], NULL, &workerThreadRoutine, (void *)stat);
}
static int createThreadsPool(int num_of_threads,pthread_t* working_thread_arr)
{
    int i=0;
    for( i = 0 ; i < num_of_threads ; i++)
    {
        thread_stat *stat = allocateNewStat();
        if(stat==NULL)
        {
            return -1;
        }
        initializeStat(i, stat);
        int error_of_pthreads_create= createThreadWithStat(stat,working_thread_arr,i);
        if(error_of_pthreads_create!=0)
        {
            unix_error("pthread_create error");
            return error_of_pthreads_create;
        }
    }
    return 0;
}

static int create_all_threads(int num_of_threads)
{
    pthread_t *working_threads_arr = workingThreadsAllocation(num_of_threads);
    if(working_threads_arr == NULL)
    {
        return -1;
    }
    int thread_pool_error= createThreadsPool(num_of_threads,working_threads_arr);
    if(thread_pool_error!=0)
    {
        return thread_pool_error;
    }
    return 0;
}

///************************************************************************************************************************************///
///********************************************      main functions      ************************************************************///
///************************************************************************************************************************************///
void getargs(int *port, int *threads, int *queue_size,char** schedalg, int argc, char *argv[])
{
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <port> <threads> <queue_size> <schedalg>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *threads = atoi(argv[2]);
    *queue_size = atoi(argv[3]);
    *schedalg=argv[4];
}


void* workerThreadRoutine(void *stat)
{
    while(1)
    {
        lockList();
        waitForTheListNonEmpty();
        Node* request = getRequestToHandle();
        requests_list->num_of_running++;
        unlockList();
        doTheRequest(request,(thread_stat *)stat);
        lockList();
        requests_list->num_of_running--;
        nowTheListIsNOTFull();
        unlockList();
    }
}

int main(int argc, char *argv[])
{
    char* schedalg;
    int listenfd, connfd, port, clientlen, num_of_threads, queue_size;
    struct sockaddr_in clientaddr;

    getargs(&port, &num_of_threads, &queue_size,&schedalg, argc, argv);

    initializeConditionsAndPadlock();
    requests_list = createList();
    //
    // HW3: Create some threads...
    //
    int error_code = create_all_threads(num_of_threads);
    if(error_code!=0)
    {
        return error_code;
    }

    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

        //
        // HW3: In general, don't handle the request in the main thread.
        // Save the relevant info in a buffer and have one of the worker threads
        // do the work.
        //
        struct timeval arrival_time= computeArrivalTime();
        lockList();
        if(tryToHandleRequest(queue_size,schedalg,connfd)==1)
        {
            continue;
        }
        insertRequestToSystem(connfd, arrival_time);
        unlockList();
    }
}