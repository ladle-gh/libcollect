#include <limits.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include <ladle/common/lib.h>
#include <ladle/common/ptrcmp.h>

#include "collect.h"

// ---- Constants ----

#define QUEUE_DEFCAP    4
#define STACK_DEFCAP    8

typedef void (*dtor_t)(void *);
typedef struct entry_t {
    void *block;
    dtor_t dtor;
} entry_t;
typedef struct gc_t {
    entry_t *queue;
    size_t capacity, count, sorted;
} gc_t;

// ---- Variables ----

export thread_local void *_coll_cur;
static thread_local gc_t *stack, *local;
static thread_local size_t capacity, depth;

// ---- Private Functions ----

// Function passed to qsort() when a block is unqueued
static int cmpfunc(const void *a, const void *b) {
    return ptr_gt(*(void **) a, *(void **) b) - ptr_lt(*(void **) a, *(void **) b);
}

/* Modified interpolation search
 * Returns pointer to closest match of entry */
static entry_t *qsearch(entry_t *min, entry_t *max, void *block) {
    entry_t *cur;
    void *minb, *maxb;

    for (;;) {
        minb = min->block;
        maxb = max->block;
        if (ptr_lt(block, minb))
            return min;
        if (ptr_gt(block, maxb))
            return max;
        cur = min + (uintptr_t) (((double)
          (block - minb) / (maxb - minb) + !(maxb - minb)) * (max - min));
        if (ptr_eq(cur->block, block))
            return cur;
        if (ptr_gt(cur->block, block)) {
            max = cur - 1;
            ++min;
        } else {
            min = cur + 1;
            --max;
        }
    }
}

// Destructor that does nothing
export void _coll_blank(void *restrict block) {}

/* Prepares local collector for use
 * Allocates memory for contents of local collector, and initially, the local buffer
 * Returns true on success and false on failure */
export bool _coll_ctor(void) {
    // If collector stack not initialized, initialize
    if (!depth) {
        stack = malloc((capacity = STACK_DEFCAP) * sizeof(gc_t));
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
    local->queue = malloc((local->capacity = QUEUE_DEFCAP) * sizeof(entry_t));
    if (!local->queue) {    // malloc() fails
        if (!depth)
            free(stack);
        return false;
    }

    local->count = local->sorted = 0;
    ++depth;
    return true;
}

/* Frees memory queued to be freed
 * Frees contents of local collector, and intially, the local buffer */
export void _coll_dtor(void) {
    entry_t cur;

    for (size_t i = 0, lim = local->count; i < lim; ++i) {
        cur = local->queue[i];
        cur.dtor(cur.block);
        free(cur.block);
    }
    free((local--)->queue);
    if (!(--depth))
        free(stack);
}

// ---- Public Functions ----

export void *coll_dqueue(void *block, dtor_t destructor) {
    if (!depth) {
        puts("collect.h: Fatal error: No local collector initialized");
        exit(EXIT_FAILURE);
    }
    if (local->count) {
        // Ensure that block is not already queued
        if (local->sorted == local->count &&
          qsearch(local->queue, local->queue + local->sorted - 1,
          block)->block == block)
            return block;
        for (size_t i = local->sorted, lim = local->count; i < lim; ++i) {
            if (block == local->queue[i].block)
                return block;
        }

        // If maximum capacity reached, expand queue
        if (local->count == local->capacity) {
            local->queue =
              realloc(local->queue, (local->capacity *= 2) * sizeof(entry_t));
            if (!local->queue)  // realloc() fails (handles memory overflow)
                return NULL;
        }
    } else
        local->sorted = 1;  // Queue of size 1 is always sorted
    local->queue[local->count++] = (entry_t) {block, destructor};
    return block;
}
export void *coll_unqueue(void *block) {
    if (depth && local->count) {
        entry_t *queue = local->queue;

        if (local->sorted != local->count) {
            qsort(queue, local->count, sizeof(entry_t), cmpfunc);
            local->sorted = local->count;
        }
        if ((queue = qsearch(queue, queue + local->count - 1, block))->block == block)
            queue->block = NULL;    // Causes free() to take no action
    }
    return block;
}
