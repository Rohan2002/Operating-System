#ifndef QUEUE_H
#define QUEUE_H


// Node structure for the queue
typedef struct node
{
    void *data;
    struct node *next;
} node_t;

// Queue structure
typedef struct queue
{
    node_t *front; // Points to the front node in the queue
    node_t *rear;  // Points to the rear node in the queue
} queue_t;

queue_t *create_queue();
void enqueue(queue_t *q, void *value);
void *dequeue(queue_t *q);
int is_empty(queue_t *q);
void *front(queue_t *q);
void destroy_queue(queue_t *q);

#endif