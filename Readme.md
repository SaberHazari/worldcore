# worldcore

A small in-memory graph store. Nodes, parent/child links, typed connections
between them with distances and travel modes. Right now it only handles
locations, but underneath it's just a graph.

Nodes live in flat arrays with offset-based adjacency, so walking a node's
children or edges is just a slice. IDs are indices and don't move, so pointers
from accessors stay valid as the table grows. Insert rejects cycles, edges to
ancestors, and duplicate connections with overlapping modes.