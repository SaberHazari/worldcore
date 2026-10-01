#ifndef LOCATIONS_H
#define LOCATIONS_H

#include "utils.h"

#define LOCATION_NONE UINT32_MAX

typedef struct Location {
    u32 id;
    u32 parent;
    u32 name_offset;
    u16 name_len;
} Location;

typedef struct LocationTable {
    u32 count;
    u32 capacity;
    
    Location *locations;
    
    char *name_pool;
    u32 name_pool_used;
    u32 name_pool_capacity;
    
    u32 *child_offsets;
    u32 *child_ids;
    u32 *connection_offsets;
    u32 *connection_ids;
    u32 *connection_times;
} LocationTable;

bool location_table_init(LocationTable *table, u32 capacity);
void location_table_free(LocationTable *table);
u32 location_table_add(LocationTable *table, const char *name, u32 parent);

#endif // LOCATIONS_H