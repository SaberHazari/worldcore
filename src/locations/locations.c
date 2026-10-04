#include "locations.h"
#include "reallocation.h"

#include <assert.h>
#include <string.h>

#define LOCATION_DEFAULT_NAME_POOL_KB 64

static bool location_table_reserve(LocationTable *table, u64 capacity);
static bool name_pool_reserve(LocationTable *table, u64 needed);
static void child_insert_raw(LocationTable *table, u32 parent, u32 child);
static bool connection_overlaps(const LocationTable *table, u32 a, u32 b, u8 modes);
static bool is_ancestor(const LocationTable *table, u32 ancestor, u32 node);
static bool subtree_connects_to_chain(const LocationTable *table, u32 node, u32 chain_bottom);
static bool connection_grow(LocationTable *table, u64 needed);
static void connection_insert_raw(LocationTable *table, u32 from, u32 to, u32 distance, u8 modes);

bool location_table_init(LocationTable *table, u32 capacity) {
    if(table == NULL) { return false; }
    
    memset(table, 0, sizeof(*table));
    
    if(capacity == 0) { return false; }
    if(capacity > LOCATION_MAX_COUNT) { return false; }
    
    size_t offsets_count = (size_t)capacity + 1;
    
    table->locations            = calloc(capacity,      sizeof(*table->locations));
    table->child_offsets        = calloc(offsets_count, sizeof(*table->child_offsets));
    table->child_ids            = calloc(capacity,      sizeof(*table->child_ids));
    table->connection_offsets   = calloc(offsets_count, sizeof(*table->connection_offsets));
    table->connection_ids       = calloc(capacity,      sizeof(*table->connection_ids));
    table->connection_distances = calloc(capacity,      sizeof(*table->connection_distances));
    table->connection_modes     = calloc(capacity,      sizeof(*table->connection_modes));
    
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
    
    if(parent != LOCATION_NONE && parent >= table->count) {
        return LOCATION_NONE;
    }
    
    size_t len_raw = strlen(name);
    if(len_raw > UINT16_MAX) { return LOCATION_NONE; }
    
    u32 name_len = (u32)len_raw;
    u32 bytes_to_copy = name_len + 1;
    
    if(!name_pool_reserve(table, (u64)table->name_pool_used + bytes_to_copy)) {
        return LOCATION_NONE;
    }
    if(table->count == table->capacity) {
        if(table->count >= LOCATION_MAX_COUNT) { return LOCATION_NONE; }
        
        u64 doubled = (u64)table->capacity * 2;
        u32 new_capacity = (doubled > LOCATION_MAX_COUNT) ? LOCATION_MAX_COUNT : (u32)doubled;
        if(!location_table_reserve(table, new_capacity)) { return LOCATION_NONE; }
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
        child_insert_raw(table, parent, id);
    }
    return id;
}

bool location_add_child(LocationTable *table, u32 parent, u32 child) {
    if(table == NULL) { return false; }
    if(parent >= table->count) { return false; }
    if(child >= table->count) { return false; }
    if(table->locations[child].parent != LOCATION_NONE) { return false; }
    
    u32 steps = 0;
    for(u32 cur = parent; cur != LOCATION_NONE; cur = table->locations[cur].parent) {
        if(cur == child) { return false; }
        if(++steps > table->count) { return false; }
    }
    if(subtree_connects_to_chain(table, child, parent)) { return false; }
    child_insert_raw(table, parent, child);
    return true;
}

bool location_connect(LocationTable *table, u32 a, u32 b, u32 distance, u8 modes) {
    if(table == NULL) { return false; }
    if(a >= table->count) { return false; }
    if(b >= table->count) { return false; }
    if(a == b) { return false; }
    if(modes == 0) { return false; }
    if(is_ancestor(table, a, b) || is_ancestor(table, b, a)) { return false; }
    if(connection_overlaps(table, a, b, modes)) { return false; }
    
    u64 total = table->connection_offsets[table->count];
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

const Location *location_get(const LocationTable *table, u32 id) {
    if(table == NULL) { return NULL; }
    if(id >= table->count) { return NULL; }
    return &table->locations[id];
}

const u32 *location_children(const LocationTable *table, u32 id, u32 *out_count) {
    if(out_count != NULL) { *out_count = 0; }
    if(table == NULL) { return NULL; }
    if(id >= table->count) { return NULL; }
    
    u32 start = table->child_offsets[id];
    if(out_count != NULL) { *out_count = table->child_offsets[id + 1] - start; }
    return &table->child_ids[start];
}

LocationEdges location_edges(const LocationTable *table, u32 id) {
    LocationEdges edges = {0};
    if(table == NULL) { return edges; }
    if(id >= table->count) { return edges; }
    
    u32 start = table->connection_offsets[id];
    
    edges.count     = table->connection_offsets[id + 1] - start;
    edges.ids       = &table->connection_ids[start];
    edges.distances = &table->connection_distances[start];
    edges.modes     = &table->connection_modes[start];
    return edges;
}

static bool location_table_reserve(LocationTable *table, u64 capacity) {
    if(table == NULL) { return false; }
    if(table->locations == NULL) { return false; }
    if(capacity <= table->capacity) { return false; }
    if(capacity > LOCATION_MAX_COUNT) { return false; }
    
    grow_field(table, locations,          capacity);
    grow_field(table, child_offsets,      capacity + 1);
    grow_field(table, child_ids,          capacity);
    grow_field(table, connection_offsets, capacity + 1);
    
    table->capacity = (u32)capacity;
    return true;
}

static bool name_pool_reserve(LocationTable *table, u64 needed) {
    if(needed <= table->name_pool_capacity) { return true; }
    if(needed > UINT32_MAX) { return false; }
    
    u64 new_capacity = table->name_pool_capacity ? table->name_pool_capacity : kilo_bytes(LOCATION_DEFAULT_NAME_POOL_KB);
    while(new_capacity < needed) { new_capacity *= 2; }
    if(new_capacity > UINT32_MAX) { new_capacity = UINT32_MAX; }
    
    grow_field(table, name_pool, new_capacity);
    table->name_pool_capacity = (u32)new_capacity;
    
    return true;
}

static void child_insert_raw(LocationTable *table, u32 parent, u32 child) {
    u32 total = table->child_offsets[table->count];
    
    assert(total < table->capacity);
    
    u32 insert_pos = table->child_offsets[parent + 1];
    u32 tail = total - insert_pos;
    
    if(tail > 0) {
        memmove(&table->child_ids[insert_pos + 1], 
            &table->child_ids[insert_pos], 
            tail * sizeof(*table->child_ids));
    }
    table->child_ids[insert_pos] = child;
    
    for(u32 i = parent + 1; i <= table->count; ++i) {
        table->child_offsets[i]++;
    }
    
    table->locations[child].parent = parent;
}

static bool connection_overlaps(const LocationTable *table, u32 a, u32 b, u8 modes) {
    u32 start = table->connection_offsets[a];
    u32 end = table->connection_offsets[a + 1];
    
    for(u32 i = start; i < end; ++i) {
        if(table->connection_ids[i] == b && (table->connection_modes[i] & modes) != 0) {
            return true;
        }
    }
    return false;
}

static bool is_ancestor(const LocationTable *table, u32 ancestor, u32 node) {
    u32 steps = 0;
    for(u32 cur = table->locations[node].parent; cur != LOCATION_NONE; cur = table->locations[cur].parent) {
        if(cur == ancestor) { return true; }
        if(++steps > table->count) { return false; }
    }
    return false;
}

static bool subtree_connects_to_chain(const LocationTable *table, u32 root, u32 chain_bottom) {
    u32 *stack = malloc(sizeof(u32) * (table->count ? table->count : 1));
    if (stack == NULL) { return true; }

    u32 top = 0;
    stack[top++] = root;
    bool found = false;

    while (top > 0 && !found) {
        u32 node = stack[--top];

        LocationEdges edges = location_edges(table, node);
        for (u32 i = 0; i < edges.count; ++i) {
            u32 other = edges.ids[i];
            if (other == chain_bottom || is_ancestor(table, other, chain_bottom)) {
                found = true;
                break;
            }
        }
        if (found) { break; }

        u32 child_count = 0;
        const u32 *children = location_children(table, node, &child_count);
        for (u32 i = 0; i < child_count; ++i) {
            stack[top++] = children[i];
        }
    }

    free(stack);
    return found;
}

static bool connection_grow(LocationTable *table, u64 needed) {
    if(table->connection_capacity >= needed) { return true; }
    if(needed > UINT32_MAX) { return false; }
    
    u64 new_capacity = table->connection_capacity ? table->connection_capacity : 4;
    while(new_capacity < needed) { new_capacity *= 2; }
    if(new_capacity > UINT32_MAX) { new_capacity = UINT32_MAX; }
    
    grow_field(table, connection_ids,       new_capacity);
    grow_field(table, connection_distances, new_capacity);
    grow_field(table, connection_modes,     new_capacity);
    
    table->connection_capacity = (u32)new_capacity;
    return true;
}

static void connection_insert_raw(LocationTable *table, u32 from, u32 to, u32 distance, u8 modes) {
    u32 total = table->connection_offsets[table->count];
    assert(total < table->connection_capacity);
    
    u32 insert_pos = table->connection_offsets[from + 1];
    u32 tail = total - insert_pos;
    
    if(tail > 0) {
        memmove(&table->connection_ids[insert_pos + 1], 
            &table->connection_ids[insert_pos], 
            tail * sizeof(*table->connection_ids));
        memmove(&table->connection_distances[insert_pos + 1], 
            &table->connection_distances[insert_pos], 
            tail * sizeof(*table->connection_distances));
        memmove(&table->connection_modes[insert_pos + 1], 
            &table->connection_modes[insert_pos], 
            tail * sizeof(*table->connection_modes));
    }
    
    table->connection_ids[insert_pos] = to;
    table->connection_distances[insert_pos] = distance;
    table->connection_modes[insert_pos] = modes;
    
    for(u32 i = from + 1; i <= table->count; ++i) {
        table->connection_offsets[i]++;
    }
}