#ifndef REALLOCATION_H
#define REALLOCATION_H

#include "utils.h"

#include <stdlib.h>

#define grow_field(table, field, count) do {                                        \
    void *grown_ = realloc_array((table)->field, sizeof(*(table)->field), (count)); \
    if(grown_ == NULL) { return false; }                                            \
    (table)->field = grown_;                                                        \
} while(0)

static inline void *realloc_array(void *old, size_t element_size, u64 count) {
    if(count > (SIZE_MAX / element_size)) { return NULL; }
    return realloc(old, (size_t)(count * element_size));
}

#endif // REALLOCATION_H