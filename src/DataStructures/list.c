/*
 * Created by paul magos on 07/01/22.
 */
#include "../../headers/list.h"
#include <stdlib.h>
#include <string.h>

List returnNewList(){
    List myList =  malloc (sizeof (list));
    if(!(myList)) {
        /* ERRORE DOVUTO ALL'ALLOCAZIONE NON ANDATA A BUON FINE */
        errno = ENOMEM;
        return NULL;
    }
    return myList;
}


int createList(List* myList){
    *myList =  calloc( 1, sizeof (list));
    if(!(*myList)) {
        /* ERRORE DOVUTO ALL'ALLOCAZIONE NON ANDATA A BUON FINE */
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

void printList(Node head){
    if(head == NULL) return;
    fprintf(stderr, "%s, %s\n", head->index, (char*)head->data);
    printList(head->next);
}

int pushTop(List* myList, char* index, void* data){
    if(!(*myList)){
        /* ERRORE DOVUTO AD ARGOMENTO INVALIDO */
        errno = EINVAL;
        return -1;
    }

    node* temp = createNode(index, data);
    if(!temp) {
        /* ERRORE DOVUTO ALL'ALLOCAZIONE NON ANDATA A BUON FINE */
        errno = ENOMEM;
        return -1;
    }

    temp->next = (*myList)->head;
    temp->prev = NULL;

    if((*myList)->head!=NULL) {
        (*myList)->head->prev = temp;
    }
    (*myList)->head = temp;
    (*myList)->len++;
    return 0;
}

int pushBottom(List* myList, char* index, void* data){
    if(!(*myList)){
        /* ERRORE DOVUTO AD ARGOMENTO INVALIDO */
        errno = EINVAL;
        return -1;
    }

    node* temp = createNode(index, data);
    if(!temp) {
        /* ERRORE DOVUTO ALL'ALLOCAZIONE NON ANDATA A BUON FINE */
        errno = ENOMEM;
        return -1;
    }

    Node last = (*myList)->head;
    temp->next = NULL;

    (*myList)->len++;
    if((*myList)->head==NULL){
        temp->prev=NULL;
        (*myList)->head = temp;
        return 0;
    }

    while(last->next!=NULL) last=last->next;

    last->next=temp;
    temp->prev=last;
    return 0;
}

int pullTop(List* myList, char** index, void** data){
    if(!(*myList) || (*myList)->len < 1){
        /* ERRORE DOVUTO AD ARGOMENTO INVALIDO */
        errno = EINVAL;
        return -1;
    }

    node* tmp = (*myList)->head;

    (*myList)->len--;
    if((*myList)->len == 0){
        (*myList)->head = NULL;
    }else {
        (*myList)->head = (*myList)->head->next;
        (*myList)->head->prev = NULL;
    }

    if(tmp->index!= NULL) {
        *index = malloc(strlen(tmp->index)+1);
        strncpy(*index, tmp->index, strlen(tmp->index)+1);
        /* *data = malloc() */
        if(tmp->data!=NULL){
            *data = malloc(strlen(tmp->data)+1);
            memcpy(*data, tmp->data, strlen(tmp->data)+1);
        }
    }

    freeNode(tmp);
    return 0;
}

int removeByInt(List* myList, void* data){
    if(!(*myList) || (*myList)->len < 1){
        /* ERRORE DOVUTO AD ARGOMENTO INVALIDO */
        errno = EINVAL;
        return -1;
    }

    Node tmp=(*myList)->head;
    Node prev=NULL;

    if(tmp!=NULL&&tmp->dataInt==*(int*)data){
        if(tmp->next) (*myList)->head = tmp->next;
        else (*myList)->head=NULL;
        if(tmp->next) tmp->next->prev=NULL;
        (*myList)->len--;
        freeNode(tmp);
        return 0;
    }

    while(tmp!=NULL && tmp->dataInt==*(int*)data){
        prev = tmp;
        tmp = tmp->next;
    }

    if(tmp==NULL) return -1;
    (*myList)->len--;
    if(prev) prev->next=tmp->next;
    if(tmp->next) tmp->next->prev = prev;

    freeNode(tmp);
    return 0;
}

Node getHead(List* myList){
    if(!(*myList) || (*myList)->len < 1){
        /* ERRORE DOVUTO AD ARGOMENTO INVALIDO */
        errno = EINVAL;
        return NULL;
    }

    node* tmp = (*myList)->head;

    (*myList)->len--;
    if((*myList)->len == 0){
        (*myList)->head = NULL;
    }else {
        (*myList)->head = (*myList)->head->next;
    }


    return tmp;
}
/*
int pullBottom(List* myList, char** index, void** data){
    if(!(*myList) || (*myList)->len < 1){
         ERRORE DOVUTO AD ARGOMENTO INVALIDO
        errno = EINVAL;
        return -1;
    }

    node* tmp = (*myList)->tail;

    (*myList)->len--;
    if((*myList)->len == 0){
        (*myList)->head = NULL;
        (*myList)->tail = NULL;
    }else {
        (*myList)->tail = (*myList)->tail->prev;
        (*myList)->tail->next = NULL;
    }


    if(tmp->index!= NULL) {
        *index = malloc(strlen(tmp->index)+1);
        strncpy(*index, tmp->index, strlen(tmp->index)+1);
        *data = malloc()
        if(tmp->data!=NULL){
            *data = malloc(strlen(tmp->data)+1);
            memcpy(*data, tmp->data, strlen(tmp->data)+1);
        }
    }

    freeNode(tmp);
    return 0;
}*/

void freeNodes(Node head){
    if((head) == NULL) return;
    freeNodes(head->next);
    freeNode(head);
    return;
}

int deleteList(List* myList) {
    if (!(*myList)) {
        /* ERRORE DOVUTO AD ARGOMENTO INVALIDO */
        errno = EINVAL;
        return -1;
    }

    freeNodes((*myList)->head);
    free(*myList);
    return 0;
}

int search(Node head, char* str){
    if(head == NULL) return 0;
    if (*head->index == *str) return 1;
    return search(head->next, str);
}

int searchInt(Node head, int num){
    if(head == NULL) return 0;
    if (head->dataInt == num){
        return 1;
    }
    return searchInt(head->next, num);
}

void getArg(Node head, char* str, char** dir){
    if(head == NULL) return;
    if (*head->index == *str) *dir = head->data;
    getArg(head->next, str, &(*dir));
}