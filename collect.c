#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <threads.h>

#include "collect.h"

// TODO implement BST

// ---- Macros ----

/* Ensures that if maximum count reached, expand block and destructor buffer
 * If block to be queued is a duplicate, returns block
 * Causes calling function to return NULL on internal error */
#define validate_local(block)                                                   \
    if (local->count == local->capacity) {                                      \
        local->queue =                                                          \
          realloc(local->queue, (local->capacity *= 2) * sizeof(void *));       \
        if (!local->queue) {    /* realloc() fails (handles memory overflow) */ \
            free(local->dtors);                                                 \
            return NULL;                                                        \
        }                                                                       \
        local->dtors = realloc(local->dtors, local->capacity * sizeof(dtor_t)); \
        if (!local->dtors) {    /* realloc() fails (handles memory overflow) */ \
            free(local->queue);                                                 \
            return NULL;                                                        \
        }                                                                       \
    }                                                                           \
    for (size_t i = 0; i < local->count; ++i) {                                 \
        if ((block) == local->queue[i])                                         \
            return (block);                                                     \
    }

// ---- Constants ----

#define LOCAL_DEF_CAP   4
#define GLOBL_DEF_CAP   8

// ---- Globals ----

thread_local gc_t *stack, *local;
thread_local size_t capacity, depth;

// ---- Private Functions ----

// Destructor that does nothing
static void blank_dtor(void *block) {}

/* Prepares local collector for use
 * Allocates memory for contents of local collector, and initially, the local buffer
 * Returns true on success and false on failure */
bool __coll_ctor(void) {
    // If global collector not initialized, initialize
    if (!depth) {
        stack = malloc((capacity = GLOBL_DEF_CAP) * sizeof(gc_t));
        if (!stack) // malloc() fails
            return false;
        local = stack;    

    // If maximum depth reached, expand local buffer
    } else if (++local && depth == capacity) {
        stack =
          realloc(stack, (capacity *= 2) * sizeof(gc_t));
        if (!stack) // realloc() fails (handles memory overflow)
            return false;
    }

    // Initialize local queue
    local->queue = malloc((local->capacity = LOCAL_DEF_CAP) * sizeof(void *));
    if (!local->queue) {    // malloc() fails
        --depth;
        return false;
    }

    // Initialize local destructor buffer
    local->dtors = malloc(local->capacity * sizeof(dtor_t));
    if (!local->dtors) {
        --depth;
        return false;
    }

    local->init = true;
    local->count = 0;
    ++depth;
    return true;
}

/* Frees memory queued to be freed
 * Frees contents of local collector, and intially, the local buffer */
void __coll_dtor(void) {
    for (size_t i = 0, lim = local->count; i < lim; ++i) {
        local->dtors[i](local->queue[i]);
        free(local->queue[i]);
    }
    free(local->queue);
    free(local->dtors);
    if (!(--depth))
        free(stack);
}

// ---- Public Functions ----

void *coll_queue(void *block) {
    if (!depth || !local->init) {   // Local collector not initialized
        puts("collect.h: Fatal error: Local collector not initialized");
        exit(EXIT_FAILURE);
    }
    validate_local(block);
    local->dtors[local->count] = blank_dtor;
    return local->queue[local->count++] = block;
}
void *coll_dqueue(void *block, dtor_t destructor) {
    if (!depth || !local->init) {   // Local collector not initialized
        puts("collect.h: Fatal error: Local collector not initialized");
        exit(EXIT_FAILURE);
    }
    validate_local(block);
    local->dtors[local->count] = destructor;
    return local->queue[local->count++] = block;
}
void coll_unqueue(void *block) {
    if (depth && local->init) {
        for (size_t i = 0, lim = local->count; i < lim; ++i) {
            if (block == local->queue[i])
                local->queue[i] = NULL; // Causes free() to take no action
        }
    }
}
