#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "list.h"

struct node *head = NULL;

struct node *create_node(int value) {
    struct node *newnode = malloc(sizeof(struct node));
	pthread_mutex_init(&newnode->lock, NULL);
    newnode->value = value;
    newnode->next = NULL;
    return newnode;
}


void insert(struct list *L, int value){
    struct node *newnode = create_node(value);

	// Need the list lock until we are sure we aren't going to change 
	// the value of L->head
	pthread_mutex_lock(&L->lock);

    struct node *cur = L->head;
	//printf("inserting %d\n", value);
    
    if(L->head == NULL || L->head->value > value) {
        newnode->next = L->head;
		L->head = newnode;
		pthread_mutex_unlock(&L->lock);
        return; 
    } 
	// we are going to start iterating, so we need the first of the
	// handoff locks (cur->lock)
	pthread_mutex_lock(&L->head->lock);
	//fprintf(stderr, "holding %p %d\n", &L->head->lock, L->head->value);
    // won't be changing head after this
	pthread_mutex_unlock(&L->lock);

    while(cur->next != NULL && cur->next->value <= value) {
		// holding cur->lock
		struct node *temp = cur;
		pthread_mutex_lock(&cur->next->lock);
		//fprintf(stderr, "holding %p %d\n", &cur->next->lock, cur->next->value);
        cur = cur->next;
		//fprintf(stderr, "dropping%p %d\n", &temp->lock, temp->value);
		pthread_mutex_unlock(&temp->lock);
    }
    // holding cur->lock
	if(cur->next != NULL) {
		pthread_mutex_lock(&cur->next->lock);
		//fprintf(stderr, "holding %p %d\n", &cur->next->lock, cur->next->value);
	}
    newnode->next = cur->next;
    cur->next = newnode;
	//fprintf(stderr, "dropping%p %d\n", &cur->lock, cur->value);
	pthread_mutex_unlock(&cur->lock);
	if(cur->next->next != NULL) {
		//fprintf(stderr, "dropping%p %d\n", &cur->next->next->lock, cur->next->next->value);
		pthread_mutex_unlock(&cur->next->next->lock);
	}
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
