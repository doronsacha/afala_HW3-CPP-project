#include "segel.h"
#include "request.h"
#include "list.h"


/**************************************************************************************************************************/
/**                                       static functions helpers                                                       **/
/**************************************************************************************************************************/
static List* allocateList()
{
    return malloc(sizeof(List));
}

static bool allocationListError(List* new_list)
{
    if (new_list==NULL)
    {
        return true;
    }
    return false;
}

static void initializeListValues(List* new_list)
{
    new_list->head = NULL;
    new_list->tail = NULL;
    new_list->num_of_running = 0;
    new_list->num_of_waiting = 0;
}

static Node* allocateNode()
{
    return malloc(sizeof(Node));
}

static bool allocationNodeError(Node* node)
{
    if (node==NULL)
    {
        return true;
    }
    return false;
}

static void initializeNodeValues(Node* new_node,int connfd, struct timeval arrival_time)
{
    new_node->arrivalTm = arrival_time;
    new_node->next = NULL;
    new_node->connfd = connfd;
    new_node->prev = NULL;
}

static void pushFirst(List* list,Node* new_node)
{
    list->tail = new_node;
    list->head = new_node;
}

static void push(Node* new_node,List* list)
{
    list->tail->next = new_node;
    new_node->prev = list->tail;
    list->tail = new_node;
}

static void increaseNumOfWaiting(List* list)
{
    list->num_of_waiting++;
}

static bool onlyOneElementIn(List* list)
{
    if(list->num_of_waiting==1)
    {
        return true;
    }
    return false;
}

static void removeTheLastOf(List* list)
{
    list->head=NULL;
    list->tail=NULL;
}

static void changeToNewTail(List* list)
{
    list->tail = list->tail->prev;
    list->tail->next = NULL;
}

static void changeToNewHead(List* list)
{
    list->head = list->head->next;
    list->head->prev = NULL;
}

static void initializeNodeBeforeReturn(Node* node)
{
    node->next = NULL;
    node->prev = NULL;
    struct timeval now;
    gettimeofday(&now, NULL);
    timersub(&now, &node->arrivalTm, &node->dispatchTm);
}

static int getNumOfWaitingIn(List* list)
{
    return list->num_of_waiting;
}

static Node* getTheNodeToRemove(List* list, int n)
{
    Node* node = list->head;
    for (int i = 1; i < n; i++)
    {
        node = node->next;
    }
    return node;
}

static void changePointersOfMiddleNode(Node* node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = NULL;
    node->prev = NULL;
}

static bool isRemovable(List* list, int n)
{
    if(list==NULL || n > list->num_of_waiting || list->head==NULL)
    {
        return false;
    }
    return true;
}
/**************************************************************************************************************************/
/**                                       list Functions                                                                 **/
/**************************************************************************************************************************/

List* createList()
{
    List* new_list = allocateList();
    if (allocationListError(new_list))
    {
        return NULL;
    }
    initializeListValues(new_list);
    return new_list;
}

bool isFull(List* list,int list_size)
{
    if((list->num_of_waiting + list->num_of_running) ==list_size)
    {
        return true;
    }
    return false;
}

int listAdd(List* list, int connfd, struct timeval arrival_time)
{
    if (list==NULL)
    {
        return -1;
    }
    Node* new_node = allocateNode();
    if(allocationNodeError(new_node))
    {
        return -1;
    }
    initializeNodeValues(new_node,connfd, arrival_time);
    if (!RequestWaitIn(list))
    {
        pushFirst(list,new_node);
    }
    else
    {
        push(new_node,list);
    }
    increaseNumOfWaiting(list);
    return 0;
}

Node* listRemoveTail(List* list)
{
    if (list==NULL || !RequestWaitIn(list))
    {
        return NULL;
    }
    Node* tail_removed = list->tail;
    if (onlyOneElementIn(list)) // empty completly list
    {
        removeTheLastOf(list);
    }
    else
    {
        changeToNewTail(list);
    }
    list->num_of_waiting--;
    initializeNodeBeforeReturn(tail_removed);
    return tail_removed;
}


int listGetTotalRequests(List* list)
{
    if (list) return list->num_of_waiting + list->num_of_running;
    else return 0;
}

Node* listRemoveHead(List* list)
{
    if (list==NULL || !RequestWaitIn(list))
    {
        return NULL;
    }
    Node* head_removed = list->head;
    if (onlyOneElementIn(list)) // empty completly list
    {
        removeTheLastOf(list);
    }
    else
    {
        changeToNewHead(list);
    }
    list->num_of_waiting--;
    initializeNodeBeforeReturn(head_removed);
    return head_removed;
}

bool RequestWaitIn(List* list)
{
    if(list->num_of_waiting>0)
    {
        return true;
    }
    return false;
}

Node* listRemoveNth(List* list, int n)
{
    if(isRemovable(list,n)==false)
    {
        return NULL;
    }
    if (n == 1)
    {
        return listRemoveHead(list);
    }
    if (n == getNumOfWaitingIn(list))
    {
        return listRemoveTail(list);
    }
    Node* tmp = getTheNodeToRemove(list, n);
    changePointersOfMiddleNode(tmp);
    list->num_of_waiting--;
    return tmp;
}