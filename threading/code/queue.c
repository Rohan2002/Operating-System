#include <stdlib.h>
#include <stdio.h>
#include "queue.h"
// queue_t holds reference to the front and rear node of the queue
queue_t *create_queue()
{
    queue_t *q = malloc(sizeof(queue_t));
    if (q == NULL)
    {
        perror("Unable to allocate memory for new queue");
        exit(EXIT_FAILURE); // Exit the program as we couldn't create the queue
    }
    q->front = q->rear = NULL;
    return q;
}

// adds element to the queue
void enqueue(queue_t *q, void* value)
{
    node_t *new_node = malloc(sizeof(node_t));
    
    if (new_node == NULL)
    {
        perror("Unable to allocate memory for new node");
        exit(EXIT_FAILURE); // Exit the program as we couldn't enqueue new item
    }
    new_node->data = value;
    new_node->next = NULL;

    // If queue is empty, the new node is both the front and the rear
    if (q->rear == NULL)
    {
        q->front = q->rear = new_node;
        return;
    }

    // Add the new node at the end of the queue and change rear
    q->rear->next = new_node;
    q->rear = new_node;
}

void* dequeue(queue_t *q)
{
    // If queue is empty, return a special value or error
    if (q->front == NULL)
    {
        fprintf(stderr, "Queue is empty, unable to dequeue\n");
        return NULL;
    }

    // Store previous front and move front one node ahead
    node_t *temp_node = q->front;
    void* item = temp_node->data;
    q->front = q->front->next;

    // If front becomes NULL, then change rear also to NULL
    if (q->front == NULL)
    {
        q->rear = NULL;
    }

    free(temp_node);
    return item;
}

// Function to check if the queue is empty
int is_empty(queue_t *q)
{
    if(q == NULL){
        printf("Queue is null\n");
    }
    return q->front == NULL;
}


// Function to get the front of the queue
void* front(queue_t *q)
{
    if (q->front == NULL)
    {
        fprintf(stderr, "Queue is empty\n");
        return NULL; // Or another special value to indicate error
    }
    return q->front->data;
}

// Function to free the queue and its nodes
void destroy_queue(queue_t *q)
{
    node_t *current = q->front;
    node_t *next;

    while (current != NULL)
    {
        next = current->next;
        free(current);
        current = next;
    }

    free(q);
}