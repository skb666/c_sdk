#include "linked_list.h"

#include <stdlib.h>

void LinkedListInit(LinkedList *linked_list) {
    linked_list->root = NULL;
    linked_list->length = 0;
    linked_list->last = NULL;
}

void LinkedListClean(LinkedList *linked_list) {
    LinkedListNode *p = NULL;
    while (linked_list->root) {
        p = linked_list->root;
        linked_list->root = linked_list->root->next;
        free(p);
    }
    linked_list->last = NULL;
    linked_list->length = 0;
}

uint8_t LinkedListEmpty(LinkedList *linked_list) {
    return (linked_list->length == 0);
}

int LinkedListLength(LinkedList *linked_list) {
    return linked_list->length;
}

int LinkedListLocate(LinkedList *linked_list, linkedlist_datatype data, int8_t (*equal)(void *, void *)) {
    LinkedListNode *p = linked_list->root;
    int index = -1;
    while (p) {
        index++;
        if (equal(&(p->data), &data)) break;
        p = p->next;
    }
    return index;
}

uint8_t LinkedListGet(LinkedList *linked_list, int index, linkedlist_datatype *data) {
    LinkedListNode *p = linked_list->root;
    int count = 0;
    if (!linked_list->root || index < 0 || index >= linked_list->length) return 0;
    while (count < index) {
        p = p->next;
        count++;
    }
    *data = p->data;
    return 1;  // isok
}

uint8_t LinkedListModify(LinkedList *linked_list, int index, linkedlist_datatype data) {
    LinkedListNode *p = linked_list->root;
    int count = 0;
    if (!linked_list->root || index < 0 || index >= linked_list->length) return 0;
    while (count < index) {
        p = p->next;
        count++;
    }
    p->data = data;
    return 1;  // isok
}

int LinkedListRemove(LinkedList *linked_list, linkedlist_datatype data, int8_t (*equal)(void *, void *)) {
    LinkedListNode *p = linked_list->root, *pt = NULL;
    int index = 0;
    if (!linked_list->root) return -1;  // fail
    // 删头
    if (equal(&(linked_list->root->data), &data)) {
        pt = linked_list->root;
        linked_list->root = linked_list->root->next;
        free(pt);
        linked_list->length -= 1;
        return 0;
    }
    // 中间
    while (p->next) {
        index++;
        if (equal(&(p->next->data), &data)) {
            if (linked_list->last == p->next) {
                linked_list->last = p;
            }
            pt = p->next;
            p->next = p->next->next;
            free(pt);
            linked_list->length -= 1;
            return index;
        }
        p = p->next;
    }
    return -1;  // fail
}

uint8_t LinkedListAppend(LinkedList *linked_list, linkedlist_datatype data) {
    if (!linked_list->root) {
        linked_list->root = (LinkedListNode *)malloc(sizeof(LinkedListNode));
        if (!linked_list->root) return 0;
        linked_list->root->data = data;
        linked_list->root->next = NULL;
        linked_list->last = linked_list->root;
        linked_list->length += 1;
    } else {
        linked_list->last->next = (LinkedListNode *)malloc(sizeof(LinkedListNode));
        if (!linked_list->last->next) return 0;
        linked_list->last->next->data = data;
        linked_list->last->next->next = NULL;
        linked_list->last = linked_list->last->next;
        linked_list->length += 1;
    }
    return 1;  // isok
}

uint8_t LinkedListInsert(LinkedList *linked_list, linkedlist_datatype data, int index) {
    LinkedListNode *p = linked_list->root, *q = NULL;
    int count = 0;
    // 头插
    if (index == 0) {
        q = (LinkedListNode *)malloc(sizeof(LinkedListNode));
        if (!q) return 0;
        q->data = data;
        q->next = linked_list->root;
        linked_list->root = q;
        if (!linked_list->last) {
            linked_list->last = linked_list->root;
        }
        linked_list->length += 1;
        return 1;  // isok
    }
    if (!linked_list->root || index < 0 || index > linked_list->length) return 0;
    // 尾插
    if (index == linked_list->length) {
        return LinkedListAppend(linked_list, data);
    }
    // 中间
    while (count < index - 1) {
        p = p->next;
        count++;
    }
    q = (LinkedListNode *)malloc(sizeof(LinkedListNode));
    if (!q) return 0;
    q->data = data;
    q->next = p->next;
    p->next = q;
    linked_list->length += 1;
    return 1;  // isok
}

uint8_t LinkedListExtend(LinkedList *linked_list1, LinkedList *linked_list2) {
    LinkedListNode *p_l2 = linked_list2->root;
    uint8_t isok;
    while (p_l2) {
        isok = LinkedListAppend(linked_list1, p_l2->data);
        if (!isok) return 0;
        p_l2 = p_l2->next;
    }
    return 1;  // isok
}

int LinkedListUnique(LinkedList *linked_list, int8_t (*equal)(void *, void *)) {
    LinkedListNode *p = NULL, *pt = NULL, *pl = NULL, *pf = NULL;
    int count = 0;
    if (!linked_list->root) return 0;
    pl = linked_list->root;
    p = linked_list->root->next;
    while (p) {
        pt = linked_list->root;
        while (pt != p) {
            if (equal(&(p->data), &(pt->data))) {
                if (p == linked_list->last) {
                    linked_list->last = pl;
                }
                pf = p;
                pl->next = p->next;
                free(pf);
                linked_list->length -= 1;
                count++;
                p = pl;
                break;
            }
            pt = pt->next;
        }
        pl = p;
        p = p->next;
    }
    return count;
}

uint8_t LinkedListReverse(LinkedList *linked_list) {
    uint8_t isok;
    LinkedList linklist_temp;
    LinkedListNode *p = linked_list->root;
    if (!linked_list->root || linked_list->length <= 1) return 0;
    LinkedListInit(&linklist_temp);
    while (p) {
        isok = LinkedListInsert(&linklist_temp, p->data, 0);
        if (!isok) {
            LinkedListClean(&linklist_temp);
            return 0;
        }
        p = p->next;
    }
    LinkedListClean(linked_list);
    *linked_list = linklist_temp;
    return 1;  // isok
}

static LinkedListNode *_merge(LinkedListNode *first, LinkedListNode *second, int8_t (*compare)(void *, void *)) {
    LinkedListNode *head = NULL, *current = NULL, *temp = NULL;
    if (!first) return second;
    if (!second) return first;
    if (compare(&(first->data), &(second->data)) > 0) {
        current = first;
        first = second;
        second = current;
    }
    head = first;
    current = first;
    first = first->next;
    while (first && second) {
        if (compare(&(first->data), &(second->data)) > 0) {
            temp = second->next;
            current->next = second;
            second->next = first;
            current = current->next;
            second = temp;
        } else {
            current = first;
            first = first->next;
        }
    }
    if (!first) current->next = second;
    return head;
}

static LinkedListNode *_mergeSort(LinkedListNode *list, int length, int8_t (*compare)(void *, void *)) {
    LinkedListNode *temp = list, *mid = NULL, *left = NULL, *right = NULL;
    int i;
    if (length <= 1) return list;
    for (i = 1; i < length / 2; ++i) temp = temp->next;
    mid = temp->next;
    temp->next = NULL;
    left = _mergeSort(list, i, compare);
    right = _mergeSort(mid, length - i, compare);
    return _merge(left, right, compare);
}

void LinkedListSort(LinkedList *linked_list, int8_t (*compare)(void *, void *)) {
    if (!linked_list->root || linked_list->length <= 1) return;
    linked_list->root = _mergeSort(linked_list->root, linked_list->length, compare);
}
