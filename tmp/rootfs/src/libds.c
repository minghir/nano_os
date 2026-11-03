#include "libds.h"

// --- VECTOR DINAMIC ---
Vector* vector_create(int initial_capacity) {
    Vector* vec = (Vector*)nano_malloc(sizeof(Vector));
    if (!vec) return 0;
    vec->capacity = initial_capacity > 0 ? initial_capacity : 8;
    vec->size = 0;
    vec->data = (void**)nano_malloc(vec->capacity * sizeof(void*));
    if (!vec->data) {
        nano_free(vec);
        return 0;
    }
    return vec;
}

void vector_free(Vector* vec) {
    if (vec) {
        if (vec->data) nano_free(vec->data);
        nano_free(vec);
    }
}

void vector_push(Vector* vec, void* item) {
    if (vec->size >= vec->capacity) {
        vec->capacity *= 2;
        void** new_data = (void**)nano_malloc(vec->capacity * sizeof(void*));
        if (!new_data) return;
        for (int i = 0; i < vec->size; i++) {
            new_data[i] = vec->data[i];
        }
        nano_free(vec->data);
        vec->data = new_data;
    }
    vec->data[vec->size++] = item;
}

void* vector_get(Vector* vec, int index) {
    if (index < 0 || index >= vec->size) return 0;
    return vec->data[index];
}

// --- LISTĂ DUBLU ÎNLĂNȚUITĂ ---
LinkedList* list_create(void) {
    LinkedList* list = (LinkedList*)nano_malloc(sizeof(LinkedList));
    if (!list) return 0;
    list->head = 0;
    list->tail = 0;
    list->size = 0;
    return list;
}

void list_free(LinkedList* list) {
    if (!list) return;
    Node* current = list->head;
    while (current) {
        Node* next = current->next;
        nano_free(current);
        current = next;
    }
    nano_free(list);
}

Node* list_push_back(LinkedList* list, void* data) {
    Node* node = (Node*)nano_malloc(sizeof(Node));
    if (!node) return 0;
    node->data = data;
    node->next = 0;
    node->prev = list->tail;

    if (list->tail) {
        list->tail->next = node;
    } else {
        list->head = node;
    }
    list->tail = node;
    list->size++;
    return node;
}

void list_remove_node(LinkedList* list, Node* node) {
    if (!node) return;
    if (node->prev) node->prev->next = node->next;
    else list->head = node->next;

    if (node->next) node->next->prev = node->prev;
    else list->tail = node->prev;

    nano_free(node);
    list->size--;
}

void list_to_tail(LinkedList* list, Node* node) {
    if (!node || list->tail == node) return;

    // Scoate din poziția curentă
    if (node->prev) node->prev->next = node->next;
    else list->head = node->next;

    if (node->next) node->next->prev = node->prev;
    else list->tail = node->prev;

    // Pune la sfârșit
    node->next = 0;
    node->prev = list->tail;
    if (list->tail) list->tail->next = node;
    list->tail = node;
    if (!list->head) list->head = node;
}