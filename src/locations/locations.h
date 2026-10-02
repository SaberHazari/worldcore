#ifndef LOCATIONS_H
#define LOCATIONS_H

#include "utils.h"

#define LOCATION_NONE UINT32_MAX

#define LOCATION_MODE_WALK  (1u << 0)
#define LOCATION_MODE_CAR   (1u << 1)
#define LOCATION_MODE_BUS   (1u << 2)
#define LOCATION_MODE_TRAIN (1u << 3)

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
    u32 child_ids_capacity;
    
    u32 *connection_offsets;
    u32 *connection_ids;
    u32 *connection_distances;
    u32 *connection_modes;
    u32 connection_capacity;
} LocationTable;

bool location_table_init(LocationTable *table, u32 capacity);
void location_table_free(LocationTable *table);
u32 location_table_add(LocationTable *table, const char *name, u32 parent);

#endif // LOCATIONS_H