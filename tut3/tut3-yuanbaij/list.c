#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "list.h"

struct node *head = NULL;

struct node *create_node(int value) {
    struct node *newnode = malloc(sizeof(struct node));
    newnode->value = value;
    newnode->next = NULL;
    return newnode;
}


void insert(struct list *L, int value){
    struct node *newnode = create_node(value);
    
    struct node *cur = L->head;
    
    if(L->head == NULL) {
        L->head = newnode;
        return; 
    } else if(L->head->value >value) {
        newnode->next = L->head;
        L->head = newnode;
        return;
    }
    
    while(cur->next != NULL && cur->next->value <= value) {
        cur = cur->next;
    }
    
    newnode->next = cur->next;
    cur->next = newnode;
    // head doesn't change
    return;
}

int length(struct list *L) {
    struct node *cur = L->head;
    int count = 0;
    while(cur != NULL) {
        count++;
        cur = cur->next;
    }
    return count;
}

void print_list(struct list *L) {
    struct node *cur = L->head;
    while(cur != NULL) {
        printf("%d -> ", cur->value);
        cur = cur->next;
    }
    printf("\n");
}
