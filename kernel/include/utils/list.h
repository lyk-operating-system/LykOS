#pragma once

#include <stddef.h>

typedef struct list_node
{
    struct list_node *prev;
    struct list_node *next;
}
list_node_t;

typedef struct
{
    list_node_t *head;
    list_node_t *tail;
    size_t length;
}
list_t;

/// @param NODE List node.
/// @param TYPE The type of the container.
/// @param MEMBER The list_node_t member inside of the container.
/// @return Pointer to the container.
#define LIST_GET_CONTAINER(NODE, TYPE, MEMBER) ((TYPE *)((uintptr_t)(NODE) - __builtin_offsetof(TYPE, MEMBER)))

#define LIST_NODE_INIT (list_node_t) { .prev = NULL, .next = NULL }

#define LIST_INIT (list_t) { .head = NULL, .tail = NULL }

#define LIST_FIRST(T) ((T)->head)

#define LIST_LAST(T) ((T)->tail)

#define FOREACH(NODE, LIST) for (list_node_t *NODE = LIST.head; NODE != NULL; NODE = NODE->next)

bool list_is_empty(list_t *list);

void list_insert_after(list_t *list, list_node_t *pos, list_node_t *new);
void list_insert_before(list_t *list, list_node_t *pos, list_node_t *new);

void list_append(list_t *list, list_node_t *node);
void list_prepend(list_t *list, list_node_t *node);

void list_remove(list_t *list, list_node_t *node);

list_node_t *list_pop_head(list_t *list);
list_node_t *list_pop_tail(list_t *list);
