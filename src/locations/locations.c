#include "locations.h"

#include <stdlib.h>
#include <string.h>

#define LOCATION_DEFAULT_NAME_POOL_KB 64

bool location_table_init(LocationTable *table, u32 capacity) {
    if(table == NULL) { return false; }
    if(capacity == 0) { return false; }
    
    memset(table, 0, sizeof(*table));
    
    table->locations = calloc(capacity,     sizeof(*table->locations));
    table->child_offsets = calloc(capacity + 1, sizeof(*table->child_offsets));
    table->child_ids = calloc(capacity,     sizeof(*table->child_ids));
    table->connection_offsets = calloc(capacity + 1, sizeof(*table->connection_offsets));
    table->connection_ids = calloc(capacity,     sizeof(*table->connection_ids));
    table->connection_distances = calloc(capacity,     sizeof(*table->connection_distances));
    table->connection_modes = calloc(capacity,     sizeof(*table->connection_modes));
    
    u32 pool_bytes = kilo_bytes(LOCATION_DEFAULT_NAME_POOL_KB);
    table->name_pool = malloc(pool_bytes);
    
    if(table->locations            == NULL || 
        table->child_offsets        == NULL || 
        table->child_ids            == NULL || 
        table->connection_offsets   == NULL || 
        table->connection_ids       == NULL || 
        table->connection_distances == NULL || 
        table->connection_modes     == NULL || 
        table->name_pool            == NULL
    ) {
        location_table_free(table);
        return false;
    }
    
    table->count = 0;
    table->capacity = capacity;
    table->child_ids_capacity = capacity;
    table->connection_capacity = capacity;
    table->name_pool_used = 0;
    table->name_pool_capacity = pool_bytes;
    
    return true;
}

void location_table_free(LocationTable *table) {
    if(table == NULL) { return; }
    
    free(table->locations);
    free(table->name_pool);
    free(table->child_offsets);
    free(table->child_ids);
    free(table->connection_offsets);
    free(table->connection_ids);
    free(table->connection_distances);
    free(table->connection_modes);
    
    memset(table, 0, sizeof(*table));
}

u32 location_table_add(LocationTable *table, const char *name, u32 parent) {
    if(table == NULL || name == NULL) { return LOCATION_NONE; }
    if(table->locations == NULL) { return LOCATION_NONE; }
    
    if(table->count >= table->capacity) {
        return LOCATION_NONE;
    }
    
    if(parent != LOCATION_NONE && parent >= table->count) {
        return LOCATION_NONE;
    }
    
    u64 len_raw = strlen(name);
    if(len_raw > UINT16_MAX) { return LOCATION_NONE; }
    
    u32 name_len = (u32)len_raw;
    u32 bytes_to_copy = name_len + 1;
    if(bytes_to_copy > (table->name_pool_capacity - table->name_pool_used)) {
        return LOCATION_NONE;
    }
    
    u32 id = table->count;
    
    u32 name_offset = table->name_pool_used;
    memcpy(table->name_pool + name_offset, name, bytes_to_copy);
    table->name_pool_used += bytes_to_copy;
    
    Location *location = &table->locations[id];
    location->id = id;
    location->parent = LOCATION_NONE;
    location->name_offset = name_offset;
    location->name_len = (u16)name_len;
    
    table->child_offsets[id + 1] = table->child_offsets[id];
    table->connection_offsets[id + 1] = table->connection_offsets[id];
    
    table->count++;
    
    if(parent != LOCATION_NONE) {
        location_add_child(table, parent, id);
    }
    
    return id;
}