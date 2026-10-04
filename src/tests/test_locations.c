#include "test_harness.h"
#include "locations/locations.h"

#include <string.h>

static void test_init_and_free(void);
static void test_init_rejects_bad_args(void);
static void test_add_basic(void);
static void test_add_past_capacity(void);
static void test_add_growth(void);
static void test_name_pool_growth(void);
static void test_name_length_limit(void);
static void test_reserve(void);
static void test_add_child(void);
static void test_add_child_cycles(void);
static void test_children_order(void);
static void test_connect(void);
static void test_connect_nodes(void);
static void test_connect_growth(void);
static void test_connect_duplicates(void);
static void test_name_and_get(void);
static void test_accessors_invalid_ids(void);
static void test_randomized_model(void);

void test_locations_run_all(void) {
    printf("\nlocations:\n");
    test_init_and_free();
    test_init_rejects_bad_args();
    test_add_basic();
    test_add_past_capacity();
    test_add_growth();
    test_name_pool_growth();
    test_name_length_limit();
    test_reserve();
    test_add_child();
    test_add_child_cycles();
    test_children_order();
    test_connect();
    test_connect_nodes();
    test_connect_growth();
    test_connect_duplicates();
    test_name_and_get();
    test_accessors_invalid_ids();
    test_randomized_model();
}

static bool children_are(const LocationTable *table, u32 id, const u32 *expected, u32 expected_count) {
    u32 count = 0;
    const u32 *children = location_children(table, id, &count);
    if(count != expected_count) { return false; }
    for(u32 i = 0; i < count; ++i) {
        if(children[i] != expected[i]) { return false; }
    }
    return true;
}

static bool edges_are(const LocationTable *table, u32 id, const u32 *expected_ids, 
    const u32 *expected_distances, const u8 *expected_modes, u32 expected_count) {
    LocationEdges edges = location_edges(table, id);
    if(edges.count != expected_count) { return false; }
    for(u32 i = 0; i < edges.count; ++i) {
        if(edges.ids[i]       != expected_ids[i]) { return false; }
        if(edges.distances[i] != expected_distances[i]) { return false; }
        if(edges.modes[i]     != expected_modes[i]) { return false; }
    }
    return true;
}

static u32 prng_next(u32 *state) {
    u32 x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void test_init_and_free(void) {
    LocationTable table;
    test_require(location_table_init(&table, 16));
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

static void test_init_rejects_bad_args(void) {
    LocationTable table;
    test_assert(!location_table_init(NULL, 4));
    
    memset(&table, 0xAB, sizeof(table));
    test_assert(!location_table_init(&table, 0));
    test_assert(table.locations == NULL);
    test_assert_eq_u32(table.count, 0);
    test_assert(location_table_add(&table, "X", LOCATION_NONE) == LOCATION_NONE);
    location_table_free(&table);
    
    memset(&table, 0xAB, sizeof(table));
    test_assert(!location_table_init(&table, UINT32_MAX));
    test_assert(table.locations == NULL);
    location_table_free(&table);
}

static void test_add_basic(void) {
    LocationTable table;
    test_require(location_table_init(&table, 4));
    
    u32 a = location_table_add(&table, "City", LOCATION_NONE);
    test_require(a != LOCATION_NONE);
    test_assert_eq_u32(a, 0);
    test_assert_eq_u32(table.count, 1);
    test_assert_eq_u32(table.locations[0].parent, LOCATION_NONE);
    
    u32 b = location_table_add(&table, "Park", a);
    test_require(b != LOCATION_NONE);
    test_assert_eq_u32(b, 1);
    test_assert_eq_u32(table.count, 2);
    test_assert_eq_u32(table.locations[1].parent, a);
    
    Location *location = &table.locations[a];
    const char *name = table.name_pool + location->name_offset;
    test_assert(location->name_len == 4);
    test_assert(memcmp(name, "City", 5) == 0);
    
    test_assert(location_table_add(&table, "Nope", 999) == LOCATION_NONE);
    test_assert(location_table_add(&table, NULL, LOCATION_NONE) == LOCATION_NONE);
    test_assert(location_table_add(NULL, "Nope", LOCATION_NONE) == LOCATION_NONE);
    test_assert_eq_u32(table.count, 2);
    
    location_table_free(&table);
}

static void test_add_past_capacity(void) {
    LocationTable table;
    test_require(location_table_init(&table, 2));
    test_assert(location_table_add(&table, "A", LOCATION_NONE) != LOCATION_NONE);
    test_assert(location_table_add(&table, "B", LOCATION_NONE) != LOCATION_NONE);
    
    u32 c = location_table_add(&table, "C", LOCATION_NONE);
    test_assert_eq_u32(c, 2);
    test_assert_eq_u32(table.count, 3);
    test_assert(table.capacity >= 3);
    test_assert(strcmp(location_name(&table, c), "C") == 0);
    location_table_free(&table);
}

static void test_add_growth(void) {
    LocationTable table;
    test_require(location_table_init(&table, 2));
    
    u32 num = 100;
    for(u32 i = 0; i < num; ++i) {
        char name[16];
        snprintf(name, sizeof(name), "L%u", i);
        u32 parent = (i == 0) ? LOCATION_NONE : (i - 1) / 2;
        test_assert_eq_u32(location_table_add(&table, name, parent), i);
    }
    test_assert_eq_u32(table.count, num);
    test_assert(table.capacity >= num);
    
    bool intact = true;
    for(u32 i = 0; i < num; ++i) {
        char expected_name[16];
        snprintf(expected_name, sizeof(expected_name), "L%u", i);
        u32 expected_parent = (i = 0) ? LOCATION_NONE : (i - 1) / 2;
        u32 expected_children[2];
        u32 expected_count = 0;
        if((2 * i + 1) < num) { expected_children[expected_count++] = 2 * i + 1; }
        if((2 * i + 2) < num) { expected_children[expected_count++] = 2 * i + 2; }
        
        intact = intact && strcmp(location_name(&table, i), expected_name) == 0;
        intact = intact && location_get(&table, i)->parent == expected_parent;
        intact = intact && location_get(&table, i)->id == i;
        intact = intact && children_are(&table, i, expected_children, expected_count);
    }
    test_assert(intact);
    
    location_table_free(&table);
}

static void test_name_pool_growth(void) {
    LocationTable table;
    test_require(location_table_init(&table, 4));
    u32 initial_pool_capacity = table.name_pool_capacity;
    
    static char filler[65001];
    memset(filler, 'A', 65000);
    filler[65000] = '\0';
    
    u32 a = location_table_add(&table, filler, LOCATION_NONE);
    test_require(a != LOCATION_NONE);
    test_assert_eq_u32(table.count, 1);
    test_assert_eq_u32(table.name_pool_used, 65001);
    
    static char big[1001];
    memset(big, 'B', 1000);
    big[1000] = '\0';
    
    u32 b = location_table_add(&table, big, LOCATION_NONE);
    test_require(b != LOCATION_NONE);
    test_assert_eq_u32(table.count, 2);
    test_assert_eq_u32(table.name_pool_used, 66002);
    test_assert(table.name_pool_capacity > initial_pool_capacity);
    test_assert(table.name_pool_capacity >= table.name_pool_used);
    
    test_assert(memcmp(location_name(&table, a), filler, 65001) == 0);
    test_assert(memcmp(location_name(&table, b), big, 1001) == 0);
    
    location_table_free(&table);
}

static void test_name_length_limit(void) {
    LocationTable table;
    test_require(location_table_init(&table, 4));
    
    static char longest[65536];
    memset(longest, 'L', 65535);
    longest[65535] = '\0';
    
    static char too_long[65537];
    memset(too_long, 'T', 65536);
    too_long[65536] = '\0';
    
    u32 a = location_table_add(&table, longest, LOCATION_NONE);
    test_require(a != LOCATION_NONE);
    test_assert(location_get(&table, a)->name_len == 65535);
    
    u32 count_before = table.count;
    u32 used_before = table.name_pool_used;
    test_assert(location_table_add(&table, too_long, LOCATION_NONE) == LOCATION_NONE);
    test_assert_eq_u32(table.count, count_before);
    test_assert_eq_u32(table.name_pool_used, used_before);
    
    location_table_free(&table);
}

static void test_reserve(void) {
    LocationTable table;
    test_require(location_table_init(&table, 4));
    
    u32 a[102];
    a[0] = location_table_add(&table, "A", LOCATION_NONE);
    a[1] = location_table_add(&table, "B", a[0]);
    test_require(a[0] != LOCATION_NONE && a[1] != LOCATION_NONE);
    test_require(location_connect(&table, a[0], a[1], 5, TRAVEL_MODE_WALK));
    
    char name[16];
    u32 parent;
    for(u32 i = 2; i < 7; ++i) {
        snprintf(name, sizeof(name), "L%u", i);
        parent = (i = 0) ? LOCATION_NONE : (i - 1) / 2;
        a[i] = location_table_add(&table, name, parent);
    }
    test_assert_eq_u32(table.capacity, 8);
    for(u32 i = 7; i < 102; ++i) {
        snprintf(name, sizeof(name), "L%u", i);
        parent = (i = 0) ? LOCATION_NONE : (i - 1) / 2;
        a[i] = location_table_add(&table, name, parent);
    }
    test_assert_eq_u32(table.capacity, 128);
    
    const u32 expected_children[] = { a[1] };
    const u32 expected_ids[] = { a[1] };
    const u32 expected_distances[] = { 5 };
    const u8 expected_modes[] = { TRAVEL_MODE_WALK };
    test_assert(strcmp(location_name(&table, a[1]), "B") == 0);
    test_assert_eq_u32(location_get(&table, a[1])->parent, a[0]);
    test_assert(children_are(&table, a[0], expected_children, 1));
    test_assert(edges_are(&table, a[0], expected_ids, expected_distances, expected_modes, 1));
    
    location_table_free(&table);
}

static void test_add_child(void) {
    LocationTable table;
    test_require(location_table_init(&table, 8));
    
    u32 city = location_table_add(&table, "City", LOCATION_NONE);
    u32 north = location_table_add(&table, "North", city);
    u32 south = location_table_add(&table, "South", city);
    u32 house = location_table_add(&table, "House", north);
    test_require(house != LOCATION_NONE);
    
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

static void test_add_child_cycles(void) {
    LocationTable table;
    test_require(location_table_init(&table, 8));
    
    u32 city = location_table_add(&table, "City", LOCATION_NONE);
    u32 north = location_table_add(&table, "North", city);
    test_require(north != LOCATION_NONE);
    
    test_assert(!location_add_child(&table, north, city));
    test_assert_eq_u32(location_get(&table, city)->parent, LOCATION_NONE);
    
    u32 a = location_table_add(&table, "A", LOCATION_NONE);
    u32 b = location_table_add(&table, "B", LOCATION_NONE);
    u32 c = location_table_add(&table, "C", b);
    test_require(c != LOCATION_NONE);
    test_assert(location_add_child(&table, c, a));
    test_assert_eq_u32(location_get(&table, a)->parent, c);
    
    test_assert(!location_add_child(&table, a, b));
    test_assert(!location_add_child(&table, b, b));
    test_assert_eq_u32(location_get(&table, b)->parent, LOCATION_NONE);
    
    const u32 under_b[] = { c };
    const u32 under_c[] = { a };
    test_assert(children_are(&table, b, under_b, 1));
    test_assert(children_are(&table, c, under_c, 1));
    
    location_table_free(&table);
}

static void test_children_order(void) {
    LocationTable table;
    test_require(location_table_init(&table, 16));
    
    u32 a  = location_table_add(&table, "A",  LOCATION_NONE);
    u32 b  = location_table_add(&table, "B",  LOCATION_NONE);
    u32 a1 = location_table_add(&table, "A1", a);
    u32 b1 = location_table_add(&table, "B1", b);
    u32 a2 = location_table_add(&table, "A2", a);
    u32 b2 = location_table_add(&table, "B2", b);
    u32 a3 = location_table_add(&table, "A3", a);
    test_require(a3 != LOCATION_NONE);
    
    const u32 under_a[] = { a1, a2, a3 };
    const u32 under_b[] = { b1, b2 };
    test_assert(children_are(&table, a, under_a, 3));
    test_assert(children_are(&table, b, under_b, 2));
    test_assert(children_are(&table, a1, NULL, 0));
    
    location_table_free(&table);
}

static void test_connect(void) {
    LocationTable table;
    test_require(location_table_init(&table, 8));
    
    u32 a = location_table_add(&table, "A", LOCATION_NONE);
    u32 b = location_table_add(&table, "B", LOCATION_NONE);
    u32 c = location_table_add(&table, "C", LOCATION_NONE);
    test_require(c != LOCATION_NONE);
    
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
    test_assert(!location_connect(&table, a, c, 0, 0));
    test_assert(!location_connect(&table, a, 999, 0, TRAVEL_MODE_WALK));
    
    location_table_free(&table);
}

static void test_connect_nodes(void) {
    LocationTable table;
    test_require(location_table_init(&table, 8));
    
    u32 a = location_table_add(&table, "A", LOCATION_NONE);
    u32 b = location_table_add(&table, "B", LOCATION_NONE);
    test_require(b != LOCATION_NONE);
    
    u8 walk_only = TRAVEL_MODE_WALK;
    test_assert(location_connect(&table, a, b, 100, walk_only));
    
    u32 ea = table.connection_offsets[a];
    u32 eb = table.connection_offsets[b];
    
    test_assert((table.connection_modes[ea] & TRAVEL_MODE_WALK) != 0);
    test_assert((table.connection_modes[ea] & TRAVEL_MODE_CAR) == 0);
    test_assert(table.connection_modes[ea] == table.connection_modes[eb]);
    
    location_table_free(&table);
}

static void test_connect_growth(void) {
    LocationTable table;
    test_require(location_table_init(&table, 4));
    
    u32 a = location_table_add(&table, "A", LOCATION_NONE);
    u32 b = location_table_add(&table, "B", LOCATION_NONE);
    u32 c = location_table_add(&table, "C", LOCATION_NONE);
    u32 d = location_table_add(&table, "D", LOCATION_NONE);
    test_require(d != LOCATION_NONE);
    
    test_assert(location_connect(&table, a, b, 10, TRAVEL_MODE_WALK));
    test_assert(location_connect(&table, c, d, 20, TRAVEL_MODE_CAR));
    test_assert(location_connect(&table, a, c, 30, TRAVEL_MODE_BUS));
    test_assert(location_connect(&table, b, d, 40, TRAVEL_MODE_TRAIN));
    test_assert(location_connect(&table, a, d, 50, TRAVEL_MODE_WALK | TRAVEL_MODE_CAR));
    test_assert(location_connect(&table, b, c, 60, TRAVEL_MODE_BUS | TRAVEL_MODE_TRAIN));
    
    test_assert(table.connection_capacity >= 12);
    test_assert_eq_u32(table.connection_offsets[table.count], 12);
    
    const u32 a_ids[] = { b, c, d };    const u32 a_dist[] = { 10, 30, 50 };
    const u8  a_mod[] = { TRAVEL_MODE_WALK, TRAVEL_MODE_BUS, TRAVEL_MODE_WALK | TRAVEL_MODE_CAR };
    const u32 b_ids[] = { a, d, c };    const u32 b_dist[] = { 10, 40, 60 };
    const u8  b_mod[] = { TRAVEL_MODE_WALK, TRAVEL_MODE_TRAIN, TRAVEL_MODE_BUS | TRAVEL_MODE_TRAIN };
    const u32 c_ids[] = { d, a, b };    const u32 c_dist[] = { 20, 30, 60 };
    const u8  c_mod[] = { TRAVEL_MODE_CAR, TRAVEL_MODE_BUS, TRAVEL_MODE_BUS | TRAVEL_MODE_TRAIN };
    const u32 d_ids[] = { c, b, a };    const u32 d_dist[] = { 20, 40, 50 };
    const u8  d_mod[] = { TRAVEL_MODE_CAR, TRAVEL_MODE_TRAIN, TRAVEL_MODE_WALK | TRAVEL_MODE_CAR };
    
    test_assert(edges_are(&table, a, a_ids, a_dist, a_mod, 3));
    test_assert(edges_are(&table, b, b_ids, b_dist, b_mod, 3));
    test_assert(edges_are(&table, c, c_ids, c_dist, c_mod, 3));
    test_assert(edges_are(&table, d, d_ids, d_dist, d_mod, 3));
    
    location_table_free(&table);
}

static void test_connect_duplicates(void) {
    LocationTable table;
    test_require(location_table_init(&table, 4));
    
    u32 a = location_table_add(&table, "A", LOCATION_NONE);
    u32 b = location_table_add(&table, "B", LOCATION_NONE);
    test_require(b != LOCATION_NONE);
    
    test_assert(location_connect(&table, a, b, 300, TRAVEL_MODE_WALK));
    
    test_assert(!location_connect(&table, a, b, 999, TRAVEL_MODE_WALK));
    test_assert(!location_connect(&table, b, a, 999, TRAVEL_MODE_WALK));
    test_assert(!location_connect(&table, a, b, 999, TRAVEL_MODE_WALK | TRAVEL_MODE_CAR));
    test_assert_eq_u32(table.connection_offsets[table.count], 2);
    
    test_assert(location_connect(&table, a, b, 1200, TRAVEL_MODE_CAR));
    test_assert_eq_u32(table.connection_offsets[table.count], 4);
    
    const u32 ids[] = { b, b };
    const u32 distances[] = { 300, 1200 };
    const u8 modes[] = { TRAVEL_MODE_WALK, TRAVEL_MODE_CAR };
    test_assert(edges_are(&table, a, ids, distances, modes, 2));
    
    location_table_free(&table);
}

static void test_name_and_get(void) {
    LocationTable table;
    test_require(location_table_init(&table, 4));
    
    u32 a = location_table_add(&table, "City", LOCATION_NONE);
    u32 b = location_table_add(&table, "Park", LOCATION_NONE);
    test_require(b != LOCATION_NONE);
    
    test_assert(strcmp(location_name(&table, a), "City") == 0);
    test_assert(strcmp(location_name(&table, b), "Park") == 0);
    test_assert(location_name(&table, 999) == NULL);
    
    const Location *location = location_get(&table, a);
    
    test_require(location != NULL);
    test_assert_eq_u32(location->id, a);
    test_assert(location_get(&table, 999) == NULL);
    
    location_table_free(&table);
}

static void test_accessors_invalid_ids(void) {
    LocationTable table;
    test_require(location_table_init(&table, 4));
    u32 a = location_table_add(&table, "A", LOCATION_NONE);
    test_require(a != LOCATION_NONE);
    
    u32 count = 123;
    test_assert(location_children(&table, 999, &count) == NULL);
    test_assert_eq_u32(count, 0);
    count = 123;
    test_assert(location_children(NULL, 0, &count) == NULL);
    test_assert_eq_u32(count, 0);
    test_assert(location_children(&table, a, NULL) != NULL);
    
    LocationEdges edges = location_edges(&table, 999);
    test_assert_eq_u32(edges.count, 0);
    test_assert(edges.ids == NULL && edges.distances == NULL && edges.modes == NULL);
    
    edges = location_edges(NULL, 0);
    test_assert_eq_u32(edges.count, 0);
    test_assert(edges.ids == NULL);
    
    location_table_free(&table);
}

#define MODEL_LOCATIONS 32
#define MODEL_ATTEMPTS  400
#define MODEL_MAX_EDGES (MODEL_ATTEMPTS * 2)

static void test_randomized_model(void) {
    static u32 ref_ids[MODEL_LOCATIONS][MODEL_MAX_EDGES];
    static u32 ref_distances[MODEL_LOCATIONS][MODEL_MAX_EDGES];
    static u8  ref_modes[MODEL_LOCATIONS][MODEL_MAX_EDGES];
    u32 ref_degree[MODEL_LOCATIONS] = {0};
    u32 ref_parent[MODEL_LOCATIONS];
    
    LocationTable table;
    test_require(location_table_init(&table, 2));
    u32 state = 0x12345678u;
    
    for(u32 i = 0; i < MODEL_LOCATIONS; ++i) {
        char name[16];
        snprintf(name, sizeof(name), "L%u", i);
        
        u32 roll = prng_next(&state);
        u32 pick = prng_next(&state);
        ref_parent[i] = (i == 0 || roll % 4 == 0) ? LOCATION_NONE : pick % i;
        test_assert_eq_u32(location_table_add(&table, name, ref_parent[i]), i);
    }
    
    u32 accepted = 0;
    bool results_match = true;
    for(u32 attempt = 0; attempt < MODEL_ATTEMPTS; ++attempt) {
        u32 a = prng_next(&state) % MODEL_LOCATIONS;
        u32 b = prng_next(&state) % MODEL_LOCATIONS;
        u32 distance = prng_next(&state) % 1000;
        u8 modes = (u8)(1 + prng_next(&state) % 15);
        
        bool overlaps = false;
        for(u32 k = 0; k < ref_degree[a]; ++k) {
            if(ref_ids[a][k] == b && (ref_modes[a][k] & modes) != 0) { overlaps = true; }
        }
        bool expected = (a != b) && !overlaps;
        
        bool actual = location_connect(&table, a, b, distance, modes);
        results_match = results_match && (actual == expected);
        
        if(expected) {
            ref_ids[a][ref_degree[a]] = b;
            ref_distances[a][ref_degree[a]] = distance;
            ref_modes[a][ref_degree[a]++] = modes;
            ref_ids[b][ref_degree[b]] = a;
            ref_distances[b][ref_degree[b]] = distance;
            ref_modes[b][ref_degree[b]++] = modes;
            accepted++;
        }
    }
    test_assert(results_match);
    test_assert(accepted > 100);
    test_assert_eq_u32(table.connection_offsets[table.count], accepted * 2);
    test_assert(table.connection_capacity >= accepted * 2);
    
    bool structure_matches = true;
    for(u32 i = 0; i < MODEL_LOCATIONS; ++i) {
        char expected_name[16];
        snprintf(expected_name, sizeof(expected_name), "L%u", i);
        structure_matches = structure_matches && strcmp(location_name(&table, i), expected_name) == 0;
        structure_matches = structure_matches && location_get(&table, i)->parent == ref_parent[i];
        structure_matches = structure_matches && edges_are(&table, i, ref_ids[i], ref_distances[i], ref_modes[i], ref_degree[i]);
        
        u32 expected_children[MODEL_LOCATIONS];
        u32 expected_count = 0;
        for(u32 j = 0; j < MODEL_LOCATIONS; ++j) {
            if(ref_parent[j] == i) { expected_children[expected_count++] = j; }
        }
        structure_matches = structure_matches && children_are(&table, i, expected_children, expected_count);
    }
    test_assert(structure_matches);
    
    location_table_free(&table);
}