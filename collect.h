#ifndef LADLE_COLLECT_H
#define LADLE_COLLECT_H
#include <errno.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include <threads.h>

#include <ladle/common/defs.h>

// ---- Implementation-Exclusive ----

typedef void (*dtor_t)(void *);

typedef struct gc_t {
    void **queue;
    dtor_t *dtors;
    size_t capacity, count;
    bool init;
} gc_t;

extern thread_local gc_t *local;

noreturn void __coll_abort(void);
bool __coll_ctor(void);
void __coll_dtor(void);

// ---- End Implementation-Exclusive ----

/* Creates a new, function-local garbage collector
 * Sets errno accordingly on internal error */
#define coll_init(type, function, ...)                  \
    if ((!local || !local->init) && __coll_ctor()) {    \
        type __retval = function(__VA_ARGS__);          \
        __coll_dtor();                                  \
        return __retval;                                \
    }

#define coll_init_nr(function, ...)                     \
    if ((!local || !local->init) && __coll_ctor()) {    \
        function(__VA_ARGS__);                          \
        __coll_dtor();                                  \
        return;                                         \
    }

/* Queues freeing of memory block once the calling function returns
 * Returns pointer queued memory
 * If NULL is passed, no action is taken
 * Returns NULL on internal error
 * Terminates program if coll_init() has not been called beforehand */
void *coll_queue(void *block)
attribute(nothrow, pure);

/* Queues freeing of memory block once the calling function returns
 * Returns pointer queued memory
 * Calls destructor function with memory block as argument before freeing
 * If block is NULL, destructor must be able to handle such a case
 * Returns NULL on internal error
 * Terminates program if coll_init() has not been called beforehand */
void *coll_dqueue(void *block, dtor_t destructor)
attribute(nothrow, pure);

/* Prevents queued memory block from being freed once the calling function returns
 * If NULL is passed or if block has not been queued, no action is taken */
void coll_unqueue(void *block)
attribute(nothrow);

#include <ladle/common/undefs.h>
#endif  // #ifndef LADLE_COLLECT_H
