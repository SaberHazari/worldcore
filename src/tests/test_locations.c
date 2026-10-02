#include "test_harness.h"
#include "locations/locations.h"

#include <string.h>

static void test_init_and_free(void);
static void test_add_basic(void);
static void test_add_overflow(void);
static void test_name_pool_capacity(void);
static void test_add_child(void);
static void test_connect(void);
static void test_connect_nodes(void);
static void test_name_and_get(void);

void test_locations_run_all(void) {
    printf("\nlocations:\n");
    test_init_and_free();
    test_add_basic();
    test_add_overflow();
    test_name_pool_capacity();
    test_add_child();
    test_connect();
    test_connect_nodes();
    test_name_and_get();
}

static void test_init_and_free(void) {
    LocationTable table;
    test_assert(location_table_init(&table, 16));
    test_assert_eq_u32(table.count, 0);
    test_assert_eq_u32(table.capacity, 16);
    test_assert(table.locations != NULL);
    test_assert(table.name_pool != NULL);
    test_assert(table.child_ids != NULL);
    test_assert(table.connection_ids != NULL);
    test_assert(table.connection_distances != NULL);
    test_assert(table.connection_modes != NULL);
    location_table_free(&table);
}

static void test_add_basic(void) {
    LocationTable table;
    location_table_init(&table, 4);
    
    u32 a = location_table_add(&table, "City", LOCATION_NONE);
    test_assert_eq_u32(a, 0);
    test_assert_eq_u32(table.count, 1);
    test_assert_eq_u32(table.locations[0].parent, LOCATION_NONE);
    
    u32 b = location_table_add(&table, "Park", a);
    test_assert_eq_u32(b, 1);
    test_assert_eq_u32(table.count, 2);
    test_assert_eq_u32(table.locations[1].parent, a);
    
    Location *location = &table.locations[a];
    const char *name = table.name_pool + location->name_offset;
    test_assert(location->name_len == 4);
    test_assert(memcmp(name, "City", 5) == 0);
    
    location_table_free(&table);
}

static void test_add_overflow(void) {
    LocationTable table;
    location_table_init(&table, 2);
    test_assert(location_table_add(&table, "A", LOCATION_NONE) != LOCATION_NONE);
    test_assert(location_table_add(&table, "B", LOCATION_NONE) != LOCATION_NONE);
    test_assert(location_table_add(&table, "C", LOCATION_NONE) == LOCATION_NONE);
    test_assert_eq_u32(table.count, 2);
    location_table_free(&table);
}

static void test_name_pool_capacity(void) {
    LocationTable table;
    location_table_init(&table, 4);
    
    static char filler[65001];
    memset(filler, 'A', 65000);
    filler[65000] = '\0';
    
    test_assert(location_table_add(&table, filler, LOCATION_NONE) != LOCATION_NONE);
    test_assert_eq_u32(table.count, 1);
    test_assert_eq_u32(table.name_pool_used, 65001);
    
    static char big[1001];
    memset(big, 'B', 1000);
    big[1000] = '\0';
    
    test_assert(location_table_add(&table, big, LOCATION_NONE) == LOCATION_NONE);
    test_assert_eq_u32(table.count, 1);
    test_assert_eq_u32(table.name_pool_used, 65001);
    
    location_table_free(&table);
}

static void test_add_child(void) {
    LocationTable table;
    location_table_init(&table, 8);
    
    u32 city = location_table_add(&table, "City", LOCATION_NONE);
    u32 north = location_table_add(&table, "North", city);
    u32 south = location_table_add(&table, "South", city);
    u32 house = location_table_add(&table, "House", north);
    
    test_assert_eq_u32(table.child_offsets[city + 1] - table.child_offsets[city], 2);
    test_assert_eq_u32(table.child_offsets[north + 1] - table.child_offsets[north], 1);
    test_assert_eq_u32(table.child_offsets[south + 1] - table.child_offsets[south], 0);
    test_assert_eq_u32(table.child_offsets[house + 1] - table.child_offsets[house], 0);
    
    test_assert_eq_u32(table.locations[north].parent, city);
    test_assert_eq_u32(table.locations[south].parent, city);
    test_assert_eq_u32(table.locations[house].parent, north);
    
    test_assert(!location_add_child(&table, south, north));
    test_assert(!location_add_child(&table, 999, north));
    test_assert(!location_add_child(&table, north, north));
    
    location_table_free(&table);
}

static void test_connect(void) {
    LocationTable table;
    location_table_init(&table, 8);
    
    u32 a = location_table_add(&table, "A", LOCATION_NONE);
    u32 b = location_table_add(&table, "B", LOCATION_NONE);
    u32 c = location_table_add(&table, "C", LOCATION_NONE);
    
    test_assert(location_connect(&table, a, b, 100, TRAVEL_MODE_WALK));
    test_assert(location_connect(&table, b, c, 200, TRAVEL_MODE_WALK | TRAVEL_MODE_CAR));
    
    test_assert_eq_u32(table.connection_offsets[a + 1] - table.connection_offsets[a], 1);
    test_assert_eq_u32(table.connection_offsets[b + 1] - table.connection_offsets[b], 2);
    test_assert_eq_u32(table.connection_offsets[c + 1] - table.connection_offsets[c], 1);
    
    u32 ea = table.connection_offsets[a];
    test_assert_eq_u32(table.connection_ids[ea], b);
    test_assert_eq_u32(table.connection_distances[ea], 100);
    test_assert(table.connection_modes[ea] == TRAVEL_MODE_WALK);
    
    test_assert(!location_connect(&table, a, a, 0, TRAVEL_MODE_WALK));
    test_assert(!location_connect(&table, a, b, 0, 0));
    test_assert(!location_connect(&table, a, 999, 0, TRAVEL_MODE_WALK));
    
    location_table_free(&table);
}

static void test_connect_nodes(void) {
    LocationTable table;
    location_table_init(&table, 8);
    
    u32 a = location_table_add(&table, "A", LOCATION_NONE);
    u32 b = location_table_add(&table, "B", LOCATION_NONE);
    
    u8 walk_only = TRAVEL_MODE_WALK;
    test_assert(location_connect(&table, a, b, 100, walk_only));
    
    u32 ea = table.connection_offsets[a];
    u32 eb = table.connection_offsets[b];
    
    test_assert((table.connection_modes[ea] & TRAVEL_MODE_WALK) != 0);
    test_assert((table.connection_modes[ea] & TRAVEL_MODE_CAR) == 0);
    test_assert(table.connection_modes[ea] == table.connection_modes[eb]);
    
    location_table_free(&table);
}

static void test_name_and_get(void) {
    LocationTable table;
    location_table_init(&table, 4);
    
    u32 a = location_table_add(&table, "City", LOCATION_NONE);
    u32 b = location_table_add(&table, "Park", LOCATION_NONE);
    
    test_assert(strcmp(location_name(&table, a), "City") == 0);
    test_assert(strcmp(location_name(&table, b), "Park") == 0);
    test_assert(location_name(&table, 999) == NULL);
    
    Location *location = location_get(&table, a);
    
    test_assert(location != NULL);
    test_assert_eq_u32(location->id, a);
    test_assert(location_get(&table, 999) == NULL);
    
    location_table_free(&table);
}