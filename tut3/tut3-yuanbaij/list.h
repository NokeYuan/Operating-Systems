#include <pthread.h>

struct node {
    int value;
	pthread_mutex_t lock; //ONLY USED IN HANDOFF CODE
    struct node *next;
};

struct list {
	struct node *head;
	pthread_mutex_t lock; 
};

struct node *create_node(int value);
void insert(struct list *L, int value);
void print_list(struct list *L);
int length(struct list *L);
