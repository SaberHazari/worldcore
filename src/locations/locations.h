#ifndef LOCATIONS_H
#define LOCATIONS_H

#include "utils.h"

#define LOCATION_NONE UINT32_MAX
#define LOCATION_MAX_COUNT (UINT32_MAX - 1u)

#define TRAVEL_MODE_WALK  (1u << 0)
#define TRAVEL_MODE_CAR   (1u << 1)
#define TRAVEL_MODE_BUS   (1u << 2)
#define TRAVEL_MODE_TRAIN (1u << 3)

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
    u32 *connection_distances;
    u8 *connection_modes;
    u32 connection_capacity;
} LocationTable;

typedef struct LocationEdges {
    u32 count;
    const u32 *ids;
    const u32 *distances;
    const u8 *modes;
} LocationEdges;

NODISCARD bool location_table_init(LocationTable *table, u32 capacity);
void location_table_free(LocationTable *table);

NODISCARD u32 location_table_add(LocationTable *table, const char *name, u32 parent);
NODISCARD bool location_add_child(LocationTable *table, u32 parent, u32 child);
NODISCARD bool location_connect(LocationTable *table, u32 a, u32 b, u32 distance, u8 modes);

const char *location_name(const LocationTable *table, u32 id);
const Location *location_get(const LocationTable *table, u32 id);
const u32 *location_children(const LocationTable *table, u32 id, u32 *out_count);
LocationEdges location_edges(const LocationTable *table, u32 id);

#endif // LOCATIONS_H