#ifndef LADLE_COLLECT_H
#define LADLE_COLLECT_H
#include <errno.h>
#include <stdbool.h>
#include <threads.h>

#include <ladle/common/header.h>

// ---- Implementation-Exclusive ----

export extern thread_local void *_coll_cur;

BEGIN

export void _coll_blank(void *);
export bool _coll_ctor(void);
export void _coll_dtor(void);

END

// ---- End Implementation-Exclusive ----

/* Creates a new, function-local garbage collector
 * __VA_ARGS__ is the list of parameter variables
 * Sets errno accordingly on internal error */
#define coll_init(type, function, ...)              \
    if (function != _coll_cur && _coll_ctor()) {    \
        _coll_cur = function;                       \
        type __retval = function(__VA_ARGS__);      \
        _coll_dtor();                               \
        return __retval;                            \
    }
#define coll_nrinit(function, ...)                  \
    if (function != _coll_cur && _coll_ctor()) {    \
        _coll_cur = function;                       \
        function(__VA_ARGS__);                      \
        _coll_dtor();                               \
        return;                                     \
    }

// Returns specified value on internal error
#define coll_einit(type, function, error, ...)  \
    coll_init(type, function, __VA_ARGS__);     \
    if (errno)  return (error);

// Returns in internal error
#define coll_enrinit(function, ...)     \
    coll_nrinit(function, __VA_ARGS__)  \
    if (errno)  return;

/* Queues freeing of memory block once the calling function returns
 * Returns pointer queued memory
 * If NULL is passed, no action is taken
 * Returns NULL on internal error
 * Terminates program if coll_init() has not been called beforehand */
#define coll_queue(block)   coll_dqueue(block, _coll_blank)

BEGIN

/* Queues freeing of memory block once the calling function returns
 * Returns pointer queued memory
 * Calls destructor function with memory block as argument before freeing
 * If block is NULL, destructor must be able to handle such a case
 * Returns NULL on internal error
 * Terminates program if coll_init() has not been called beforehand */
export void *coll_dqueue(void *block, void (*destructor)(void *)) noexcept pure;

/* Prevents queued memory block from being freed once the calling function returns
 * If NULL is passed or if block has not been queued, no action is taken */
export void *coll_unqueue(void *block) noexcept;

END

#include <ladle/common/end_header.h>
#endif  // #ifndef LADLE_COLLECT_H
