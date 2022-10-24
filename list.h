#ifndef LIST_H__
#define LIST_H__

#include <stdbool.h>

struct node
{
    struct node *next;
    struct node *prev;
    int connfd;
    struct timeval arrivalTm;
    struct timeval dispatchTm;
};
typedef struct node Node;

struct list
{
    int num_of_waiting;
    int num_of_running;
    Node* head;
    Node* tail;
};
typedef struct list List;

List* createList();

bool isFull(List* list,int list_size);

int listAdd(List* list, int connfd, struct timeval arrival_time);

Node* listRemoveTail(List* list);

int listGetTotalRequests(List* list);

Node* listRemoveHead(List* list);

bool RequestWaitIn(List* list);

Node* listRemoveNth(List* list, int n);

#endif