#ifndef LIBDS_H
#define LIBDS_H

#include <stdint.h>
#include <stddef.h>
#include "nano_libc.h"

// ==========================================
// 1. Vector Dinamic (Dynamic Array)
// ==========================================
typedef struct {
    void** data;
    int size;
    int capacity;
} Vector;

Vector* vector_create(int initial_capacity);
void vector_free(Vector* vec);
void vector_push(Vector* vec, void* item);
void* vector_get(Vector* vec, int index);
void vector_remove(Vector* vec, int index);

// ==========================================
// 2. Listă Dublu Înlănțuită (Doubly Linked List)
// ==========================================
typedef struct Node {
    void* data;
    struct Node* prev;
    struct Node* next;
} Node;

typedef struct {
    Node* head;
    Node* tail;
    int size;
} LinkedList;

LinkedList* list_create(void);
void list_free(LinkedList* list);
Node* list_push_back(LinkedList* list, void* data);   // Adaugă la final
Node* list_push_front(LinkedList* list, void* data);  // Adaugă la început
void list_remove_node(LinkedList* list, Node* node);  // Șterge un nod specific
void list_to_tail(LinkedList* list, Node* node);     // Pune nodul la sfârșit (Z-order focus)

// ==========================================
// 3. Coadă Circulară (Queue / Ring Buffer)
// ==========================================
#define QUEUE_DEFAULT_CAPACITY 128

typedef struct {
    void** buffer;
    int capacity;
    int head;
    int tail;
    int count;
} Queue;

Queue* queue_create(int capacity);
void queue_free(Queue* q);
int queue_enqueue(Queue* q, void* item);
void* queue_dequeue(Queue* q);
int queue_is_empty(Queue* q);

#endif