#include "test_harness.h"
#include "locations/locations.h"

#include <string.h>

static void test_init_and_free(void) {
    LocationTable table;
    test_assert(location_table_init(&table, 16) == true);
    test_assert_eq_u32(table.count, 0);
    test_assert_eq_u32(table.capacity, 16);
    test_assert(table.locations != NULL);
    test_assert(table.name_pool != NULL);
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

void test_locations_run_all(void) {
    printf("locations:\n");
    test_init_and_free();
    test_add_basic();
    test_add_overflow();
    test_name_pool_capacity();
}