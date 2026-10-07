# worldcore

A small in-memory graph store. Nodes, parent/child links (one parent per node),
and connections between nodes that carry a distance and a set of travel modes
(walk, car, bus, train). Right now it only handles locations, but underneath
it's just a graph.

Nodes live in flat arrays with offset-based adjacency, so walking a node's
children or edges is just a slice. A node's ID is its index and never changes.
Names are copied into one shared pool. Connections are stored from both ends,
so they work both ways. Children and edges come back in the order they were
added.

Pointers are not stable. Whatever `location_get`, `location_name`,
`location_children` or `location_edges` returns is only valid until the next
`location_table_add`, `location_add_child` or `location_connect`, because
those can realloc or shift the arrays. Keep IDs and look things up again.

## What gets rejected

- a second parent for a node, or a parent link that would make a cycle
- a connection from a node to itself, or to one of its own ancestors or
  descendants
- a parent link that would make two connected nodes ancestor and descendant
- a connection with no modes, or one that repeats a mode the pair already has
  (walk and car as two separate connections is fine, walk twice isn't)
- IDs that don't exist

## Example

```c
LocationTable table;
if(!location_table_init(&table, 64)) { return 1; }

u32 city  = location_table_add(&table, "City",  LOCATION_NONE);
u32 north = location_table_add(&table, "North", city);
u32 south = location_table_add(&table, "South", city);

// 500 units apart, on foot or by bus
if(!location_connect(&table, north, south, 500, TRAVEL_MODE_WALK | TRAVEL_MODE_BUS)) {
    // rejected
}

LocationEdges edges = location_edges(&table, north);
for(u32 i = 0; i < edges.count; ++i) {
    printf("%s: %u\n", location_name(&table, edges.ids[i]), edges.distances[i]);
}

location_table_free(&table);
```

## Notes

- IDs are `u32`. `LOCATION_NONE` (`UINT32_MAX`) means "no parent" or "failed".
- Names can be up to 65535 bytes.
- `location_table_init` takes a starting capacity, which can't be zero. The
  table doubles when it fills up.
- A failed call returns `false`, or `LOCATION_NONE` from `location_table_add`,
  and doesn't say why. Accessors return `NULL` (or an empty edge list) for a
  bad ID.
- Linking a node to a parent or connecting two nodes shifts everything after it
  in the arrays, so building a big table is roughly quadratic. Reads are plain
  slices.
- There's no remove, rename or lookup by name, and no locking.