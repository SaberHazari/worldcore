#include "locations.h"

#include <stdlib.h>
#include <string.h>

#define LOCATION_DEFAULT_NAME_POOL_KB 64

static bool connection_grow(LocationTable *table, u32 needed);
static void connection_insert_raw(LocationTable *table, u32 from, u32 to, u32 distance, u8 modes);

bool location_table_init(LocationTable *table, u32 capacity) {
    if(table == NULL) { return false; }
    if(capacity == 0) { return false; }
    
    memset(table, 0, sizeof(*table));
    
    table->locations            = calloc(capacity,     sizeof(*table->locations));
    table->child_offsets        = calloc(capacity + 1, sizeof(*table->child_offsets));
    table->child_ids            = calloc(capacity,     sizeof(*table->child_ids));
    table->connection_offsets   = calloc(capacity + 1, sizeof(*table->connection_offsets));
    table->connection_ids       = calloc(capacity,     sizeof(*table->connection_ids));
    table->connection_distances = calloc(capacity,     sizeof(*table->connection_distances));
    table->connection_modes     = calloc(capacity,     sizeof(*table->connection_modes));
    
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

bool location_add_child(LocationTable *table, u32 parent, u32 child) {
    if(table == NULL) { return false; }
    if(parent >= table->count) { return false; }
    if(child >= table->count) { return false; }
    if(parent == child) { return false; }
    if(table->locations[child].parent != LOCATION_NONE) { return false; }
    
    u32 total = table->child_offsets[table->count];
    
    if(total >= table->child_ids_capacity) {
        u32 new_cap = table->child_ids_capacity ? (table->child_ids_capacity * 2) : 4;
        u32 *new_ids = realloc(table->child_ids, new_cap * sizeof(u32));
        if(new_ids == NULL) { return false; }
        table->child_ids = new_ids;
        table->child_ids_capacity = new_cap;
    }
    
    u32 insert_pos = table->child_offsets[parent + 1];
    u32 tail = total - insert_pos;
    
    if(tail > 0) {
        memmove(&table->child_ids[insert_pos + 1], 
            &table->child_ids[insert_pos], 
            tail * sizeof(u32));
    }
    table->child_ids[insert_pos] = child;
    
    for(u32 i = parent + 1; i <= table->count; ++i) {
        table->child_offsets[i]++;
    }
    
    table->locations[child].parent = parent;
    return true;
}

bool location_connect(LocationTable *table, u32 a, u32 b, u32 distance, u8 modes) {
    if(table == NULL) { return false; }
    if(a >= table->count) { return false; }
    if(b >= table->count) { return false; }
    if(a == b) { return false; }
    if(modes == 0) { return false; }
    
    u32 total = table->connection_offsets[table->count];
    if(!connection_grow(table, total + 2)) { return false; }
    
    connection_insert_raw(table, a, b, distance, modes);
    connection_insert_raw(table, b, a, distance, modes);
    return true;
}

const char *location_name(const LocationTable *table, u32 id) {
    if(table == NULL) { return NULL; }
    if(id >= table->count) { return NULL; }
    return (table->name_pool + table->locations[id].name_offset);
}

Location *location_get(LocationTable *table, u32 id) {
    if(table == NULL) { return NULL; }
    if(id >= table->count) { return NULL; }
    return &table->locations[id];
}

static bool connection_grow(LocationTable *table, u32 needed) {
    if(table->connection_capacity >= needed) { return true; }
    if(needed > UINT32_MAX / 2) { return false; }
    
    u32 new_cap = table->connection_capacity ? table->connection_capacity : 4;
    while(new_cap < needed) { new_cap *= 2; }
    
    u32 *new_ids = realloc(table->connection_ids, new_cap * sizeof(u32));
    if(new_ids == NULL) { return false; }
    table->connection_ids = new_ids;
    
    u32 *new_dists = realloc(table->connection_distances, new_cap * sizeof(u32));
    if(new_dists == NULL) { return false; }
    table->connection_distances = new_dists;
    
    u8 *new_modes = realloc(table->connection_modes, new_cap * sizeof(u8));
    if(new_modes == NULL) { return false; }
    table->connection_modes = new_modes;
    
    table->connection_capacity = new_cap;
    return true;
}

static void connection_insert_raw(LocationTable *table, u32 from, u32 to, u32 distance, u8 modes) {
    u32 total = table->connection_offsets[table->count];
    u32 insert_pos = table->connection_offsets[from + 1];
    u32 tail = total - insert_pos;
    
    if(tail > 0) {
        memmove(&table->connection_ids[insert_pos + 1], 
            &table->connection_ids[insert_pos], 
            tail * sizeof(u32));
        memmove(&table->connection_distances[insert_pos + 1], 
            &table->connection_distances[insert_pos], 
            tail * sizeof(u32));
        memmove(&table->connection_modes[insert_pos + 1], 
            &table->connection_modes[insert_pos], 
            tail * sizeof(u8));
    }
    
    table->connection_ids[insert_pos] = to;
    table->connection_distances[insert_pos] = distance;
    table->connection_modes[insert_pos] = modes;
    
    for(u32 i = from + 1; i <= table->count; ++i) {
        table->connection_offsets[i]++;
    }
}