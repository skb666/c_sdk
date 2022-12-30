#ifndef LINKED_LIST_H_
#define LINKED_LIST_H_

#include <stdint.h>

#include "linked_list_datatype.h"

#ifndef LINKED_LIST_TYPE
typedef long long linkedlist_datatype;
#else
typedef LINKED_LIST_TYPE linkedlist_datatype;
#endif

typedef struct linked_list_node {
    linkedlist_datatype data;
    struct linked_list_node *next;
} LinkedListNode;

typedef struct {
    LinkedListNode *root;
    int length;
    LinkedListNode *last;
} LinkedList;

void LinkedListInit(LinkedList *linked_list);
void LinkedListClean(LinkedList *linked_list, void *(*callback)(LinkedListNode *));
uint8_t LinkedListEmpty(LinkedList *linked_list);
int LinkedListLength(LinkedList *linked_list);
int LinkedListLocate(LinkedList *linked_list, linkedlist_datatype data, int8_t (*equal)(void *, void *));
uint8_t LinkedListGet(LinkedList *linked_list, int index, linkedlist_datatype *data);
uint8_t LinkedListModify(LinkedList *linked_list, int index, linkedlist_datatype data);
int LinkedListRemove(LinkedList *linked_list, linkedlist_datatype data, int8_t (*equal)(void *, void *), void *(*callback)(LinkedListNode *));
uint8_t LinkedListAppend(LinkedList *linked_list, linkedlist_datatype data);
uint8_t LinkedListInsert(LinkedList *linked_list, linkedlist_datatype data, int index);
uint8_t LinkedListExtend(LinkedList *linked_list1, LinkedList *linked_list2);
int LinkedListUnique(LinkedList *linked_list, int8_t (*equal)(void *, void *), void *(*callback)(LinkedListNode *));
uint8_t LinkedListReverse(LinkedList *linked_list);
void LinkedListSort(LinkedList *linked_list, int8_t (*compare)(void *, void *));

#endif
