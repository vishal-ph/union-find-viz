# Union-Find 3D Decoder Visualizer — Documentation

## Table of Contents

1. [What This Program Does](#1-what-this-program-does)
2. [Background: Quantum Error Correction in One Page](#2-background-quantum-error-correction-in-one-page)
3. [Build System and Project Layout](#3-build-system-and-project-layout)
4. [C++ Conventions Used in This Codebase](#4-c-conventions-used-in-this-codebase)
5. [Input Format: DEM Files and Detection Events](#5-input-format-dem-files-and-detection-events)
6. [Module Reference](#6-module-reference)
   - [dem_parser — reading the graph from disk](#61-dem_parser--reading-the-graph-from-disk)
   - [surface_code — building the lattice from scratch](#62-surface_code--building-the-lattice-from-scratch)
   - [union_find — the three phases of decoding](#63-union_find--the-three-phases-of-decoding)
   - [decoder_stepper — animating the decoder one step at a time](#64-decoder_stepper--animating-the-decoder-one-step-at-a-time)
   - [ui — keyboard input and application state](#65-ui--keyboard-input-and-application-state)
   - [renderer — drawing everything with Raylib](#66-renderer--drawing-everything-with-raylib)
   - [main — the game loop](#67-main--the-game-loop)
7. [Data Flow Through the Program](#7-data-flow-through-the-program)
8. [State Encoding Conventions](#8-state-encoding-conventions)
9. [Visual Design Choices](#9-visual-design-choices)
10. [Glossary](#10-glossary)

---

## 1. What This Program Does

This is an **interactive 3D visualizer** for the Union-Find quantum error decoder. Given a quantum circuit description (a `.dem` file) and a set of measurement errors (a detection events file), it:

1. Parses the **detector error model (DEM)** to construct the decoding graph.
2. Loads the **syndrome** — a binary vector of which detectors fired.
3. Runs the **Union-Find decoder** algorithm phase by phase, letting you step through it one action at a time.
4. Renders everything in **3D** using Raylib: the lattice faces, detector nodes, growing clusters, spanning trees, and finally the correction edges.

The decoder solves the question: *given these measurement results, which physical qubit errors most likely caused them?*

---

## 2. Background: Quantum Error Correction in One Page

### Qubits and errors

A **qubit** is like a bit, but it can be in a superposition of 0 and 1. Errors come in two flavours:
- **X error** — flips the qubit (like a classical bit flip).
- **Z error** — flips the phase (invisible classically, but matters for quantum computation).

### Stabilizers: the error detectors

The **rotated surface code** protects a logical qubit by surrounding it with a lattice of **ancilla** measurements called **stabilizers**. Two kinds exist:
- **Z-stabilizers** — detect X errors on neighbouring data qubits.
- **X-stabilizers** — detect Z errors on neighbouring data qubits.

A stabilizer that *changed* its value between rounds is called a **defect** (or **syndrome node**). The collection of all such changes is the **syndrome**.

### The decoding graph

The syndrome is mapped to a graph where:
- Each **node** is a stabilizer that could fire.
- Each **edge** is a data qubit shared by two stabilizers. An error on that data qubit causes both endpoints to fire.
- **Boundary nodes** are virtual nodes representing the code boundary. An error on a data qubit at the boundary only fires one stabilizer; the boundary node is its other "partner."

The decoder's job is to find a set of edges (a **correction**) whose endpoints exactly cancel the syndrome.

### Union-Find decoding in three phases

The Union-Find algorithm (Delfosse & Nickerson 2021) solves this in three phases:

**Phase 1 — Syndrome Validation:** Grow a "cluster" outward from every defect, half-an-edge at a time. Merge clusters that touch. Stop a cluster when it touches the boundary. Keep growing until every cluster has been stopped.

**Phase 2 — Spanning Forest:** Build a spanning tree inside each cluster using BFS.

**Phase 3 — Tree Peeling:** Process each tree from its leaves inward. At each leaf, if it is a defect, mark the edge to its parent as a correction and flip the parent's defect status. This "peels" defects up the tree until they either cancel or reach the root.

---

## 3. Build System and Project Layout

```
union-find-viz/
├── cpp/
│   ├── CMakeLists.txt       # Build configuration
│   └── src/
│       ├── main.cpp         # Entry point and game loop
│       ├── dem_parser.h/.cpp    # Parse .dem files from disk
│       ├── surface_code.h/.cpp  # Build surface code from scratch (unused at runtime)
│       ├── union_find.h/.cpp    # Core decoding algorithm
│       ├── decoder_stepper.h/.cpp  # Step-by-step wrapper around the decoder
│       ├── renderer.h/.cpp  # All Raylib 3D/2D drawing
│       └── ui.h/.cpp        # Keyboard input and app mode
├── data/
│   ├── test.dem             # Example detector error model
│   └── test_events.txt      # Example detection events
└── py/                      # Python tools (separate project)
```

### CMake basics for beginners

`CMakeLists.txt` is a recipe that tells the compiler what to do. Key lines:

```cmake
set(CMAKE_CXX_STANDARD 17)   # Use C++17 features
```

`FetchContent` automatically downloads **Raylib** (a graphics library) from GitHub the first time you build. You never need to install it manually.

```cmake
add_executable(union_find_viz src/main.cpp ...)  # Compile all .cpp files into one program
target_link_libraries(union_find_viz PRIVATE raylib)  # Link against Raylib
```

On macOS, three extra Apple frameworks are linked: `IOKit`, `Cocoa`, and `OpenGL`.

### Building and running

```bash
mkdir -p build && cd build && cmake .. && cmake --build .
./union_find_viz ../data/test.dem ../data/test_events.txt
```

---

## 4. C++ Conventions Used in This Codebase

### Header guards (`#ifndef` / `#define` / `#endif`)

Every `.h` file starts with:

```cpp
#ifndef SURFACE_CODE_H
#define SURFACE_CODE_H
// ... declarations ...
#endif
```

This prevents the file from being included twice during compilation, which would cause "already defined" errors. Think of it as a "if you haven't seen me before, read me."

### `#include` — importing other files

```cpp
#include "union_find.h"   // Our own file: use quotes
#include <vector>          // Standard library: use angle brackets
```

### `std::vector<T>` — a resizable array

The most common container. `std::vector<int>` is an array of integers that can grow or shrink. Access elements with `v[i]`, get size with `v.size()`.

```cpp
std::vector<int> v = {1, 2, 3};
v.push_back(4);   // append
v.size();         // 4
```

### `std::array<T, N>` — a fixed-size array

`std::array<int, 2>` always holds exactly 2 integers. Faster than vector for small, known sizes. Used for edges: each edge is always `{node0, node1}`.

### `struct` — grouping related data

```cpp
struct DataQubit {
    int index;
    int row, col;
    ErrorType error = ErrorType::NONE;  // default value
};
```

A `struct` is a named bundle of variables. You access members with `.` (e.g., `qubit.row`).

### `class` — a struct with private members and methods

```cpp
class DecoderStepper {
public:          // accessible from outside
    bool step();
private:         // only accessible inside this class
    int n_nodes_;
};
```

The underscore suffix (`n_nodes_`) is a naming convention in this codebase for **private member variables**, distinguishing them from local variables.

### `enum class` — a named set of constants

```cpp
enum class DecoderPhase {
    IDLE,
    SYNDROME_VALIDATION,
    SPANNING_FOREST,
    FOREST_PEELING,
    DONE
};
```

Use `DecoderPhase::IDLE` to refer to a value. The `class` keyword makes the enum *scoped* — you must always write the prefix, preventing accidental collisions with other names.

### References (`&`) — aliases without copying

```cpp
void grow_clusters(std::vector<std::vector<int>>& clusters_nodes, ...)
```

The `&` means "give me a reference to the real thing, don't copy it." Changes made inside the function affect the original. `const &` means "I can read it but not change it" — efficient and safe.

### `const` — read-only promise

```cpp
const std::vector<int>& clusters_activity
```

Declaring something `const` makes a compile-time promise that you won't modify it. The compiler will refuse to compile if you accidentally try.

### Lambdas — inline anonymous functions

```cpp
auto merge_pair = [&](int c0, int c1) {
    // ...
};
```

`[&]` means "capture all local variables by reference." This creates a small function that can be called like `merge_pair(0, 1)`. Used here to avoid repeating merge logic in a loop.

### `std::unique_ptr<T>` — owning pointer

```cpp
std::unique_ptr<DecoderStepper> stepper_;
```

A smart pointer that automatically deletes the object when it goes out of scope. `stepper_.reset()` destroys the object. `std::make_unique<T>(...)` creates one. You never need to call `delete`.

### Range-based for loop

```cpp
for (int b : boundary_nodes) { ... }
for (auto& [neighbor, edge_idx] : adj_list[node]) { ... }
```

The second form uses **structured bindings** (C++17): `adj_list[node]` holds pairs, and this unpacks each pair into `neighbor` and `edge_idx` automatically.

### `auto` — let the compiler deduce the type

```cpp
auto t0 = std::chrono::high_resolution_clock::now();
```

The type of `now()` is very long; `auto` saves typing. Only use it when the type is obvious from context.

### `static` inside a `.cpp` file — file-private

```cpp
static int dq_index(int row, int col, int d) { ... }
```

`static` on a free function means it is invisible outside this `.cpp` file — like a "private" function at file scope. Avoids name collisions between files.

### Macros for profiling

```cpp
#define PROFILE_BEGIN auto _prof_t0 = std::chrono::high_resolution_clock::now()
#define PROFILE_END(accum) do { ... } while(0)
```

`#define` does text substitution before compilation. `PROFILE_BEGIN` expands to a line that records the current time. `PROFILE_END` records how many milliseconds have passed and adds it to an accumulator. The `do { } while(0)` pattern makes the macro safe to use in `if` statements without braces.

### `goto` — a deliberate jump

```cpp
goto peel;
// ...
peel:
    int edge_idx = tree[leaf];
```

`goto` is generally avoided in modern C++, but here it is used deliberately in `peel_leaf` to jump past stale-entry detection when a valid leaf is found mid-loop. The comment in the code explains why.

---

## 5. Input Format: DEM Files and Detection Events

### `.dem` files (Detector Error Model)

Generated by Stim (a quantum circuit simulator). Each line describes either a detector or an error mechanism:

```
detector(2, 4, 0) D0        # Detector 0 is at spatial coords (2,4), time 0
error(0.056) D0 D2           # P=5.6% error connecting detectors 0 and 2
error(0.056) D1              # P=5.6% error connecting detector 1 to the boundary
error(0.04) D2 D5 ^ D3 D7   # Two independent error segments, separated by ^
repeat 10 {                  # Repeat this block 10 times (for multiple rounds)
    ...
    shift_detectors(0,0,1) 50   # Each repetition increments detector IDs by 50
}
```

**Key syntax rules:**
- `D#` — detector index (local within a repeat block; made absolute using `detector_offset`)
- `L#` — logical observable index (which logical qubit is affected)
- `^` — separates independent error segments within one `error` line
- `shift_detectors(dx,dy,dt) count` — shifts coordinate offsets and detector numbering

The parser (`dem_parser.cpp`) expands all `repeat` blocks and resolves all offsets, producing a flat list of detectors and edges.

### Detection events file

A plain text file listing the indices of detectors that fired (one per line):

```
0
2
5
17
```

This becomes the **syndrome vector** — a `std::vector<int>` of length `num_detectors`, where `syndrome[i] = 1` if detector `i` fired.

---

## 6. Module Reference

### 6.1 `dem_parser` — reading the graph from disk

**Header:** [cpp/src/dem_parser.h](cpp/src/dem_parser.h)
**Implementation:** [cpp/src/dem_parser.cpp](cpp/src/dem_parser.cpp)

#### Key data structures

```cpp
struct Vec3 { float x, y, z; };

struct DetectorNode {
    int id;
    Vec3 coords;         // (col, row, time) in Stim coordinates
    bool is_x_type;      // true if X-stabilizer (cube in renderer)
};

struct GraphEdge {
    int node0, node1;    // indices into node_positions
    double probability;  // error probability (not used by UF decoder)
    bool is_boundary;    // true if node1 is a boundary node
    std::vector<int> observables;  // which logical qubits this error affects
};

struct DecodingGraph3D {
    int num_detectors;
    std::vector<DetectorNode> detectors;
    std::vector<GraphEdge> graph_edges;
    std::vector<int> boundary_node_indices;

    // Flat format for the UF decoder
    std::vector<std::vector<int>> uf_edges;   // [[n0,n1], ...]
    std::vector<int> uf_boundary_nodes;

    std::vector<Vec3> node_positions;  // detectors first, then boundary nodes
    int num_rounds;
    LatticeInfo lattice;               // inferred geometry for drawing
};
```

#### How coordinate parity determines X vs Z type

Stim places detectors at even integer coordinates. The rule:

```cpp
int ix = (int)(det.coords.x / 2.0f);
int iy = (int)(det.coords.y / 2.0f);
det.is_x_type = ((ix + iy) % 2 == 1);
```

This mirrors the checkerboard pattern of the surface code lattice.

#### `parse_dem_file(filepath)` — top-level parser

1. Reads all lines from the file, stripping blank lines.
2. Calls `process_lines` recursively, which handles `repeat` blocks.
3. Collects `DetectorNode` and `RawEdge` objects.
4. Deduplicates detectors (same `id`) and edges (same node pair).
5. Creates one **boundary node per boundary-connected detector** to avoid parallel edges in the adjacency matrix.
6. Infers `LatticeInfo` (data qubit positions + stabilizer face rectangles) from the spatial coordinates.
7. Returns the fully populated `DecodingGraph3D`.

#### Why one boundary node per detector?

The UF decoder internally uses a **dense adjacency matrix** (`adjacency_[n0][n1] = edge_index`). A matrix can only store one edge between any two nodes. If two boundary detectors both connected to the same shared boundary node, they'd overwrite each other's entry in the matrix. Giving each a unique boundary node avoids this.

#### `load_detection_events(filepath, num_detectors)`

Parses a whitespace-separated list of fired detector indices and returns a `std::vector<int>` with `syndrome[i] = 1` for each fired detector.

---

### 6.2 `surface_code` — building the lattice from scratch

**Header:** [cpp/src/surface_code.h](cpp/src/surface_code.h)
**Implementation:** [cpp/src/surface_code.cpp](cpp/src/surface_code.cpp)

This module provides an alternative path for generating a surface code without a `.dem` file. It is not used at runtime in the current application but is kept for reference and testing.

#### Key data structures

```cpp
struct DataQubit {
    int index;
    int row, col;
    ErrorType error;   // NONE, X_ERROR, or Z_ERROR
    float rx, ry;      // render position
};

struct Stabilizer {
    int index;
    bool is_x_type;
    std::vector<int> data_qubits;  // which data qubits it measures
    int measurement;               // 0 or 1
    float rx, ry;
};

struct DecodingGraph {
    int num_syndrome_nodes;
    std::vector<int> syndrome_to_stabilizer;
    std::vector<std::vector<int>> edges;         // [[n0,n1], ...]
    std::vector<int> edge_to_data_qubit;         // which data qubit each edge represents
    std::vector<int> boundary_nodes;
    std::vector<std::pair<float,float>> node_positions;
};
```

#### `build_surface_code(d)` — constructing the rotated surface code

The rotated surface code of distance `d` has:
- `d × d` data qubits on a grid.
- `(d-1) × (d-1)` interior stabilizer faces in a checkerboard pattern.
- Boundary half-faces completing the code on all four edges.

**Checkerboard rule:** An interior face at row `r`, column `c` is:
- Z-type if `(r + c) % 2 == 0` (corrects X errors)
- X-type if `(r + c) % 2 == 1` (corrects Z errors)

**Boundary faces:**
- Top and bottom boundaries add Z-type half-faces (pairs of adjacent qubits along the edge row).
- Left and right boundaries add X-type half-faces.

The decoding graph is built with a lambda:

```cpp
auto build_decoding_graph = [&](const std::vector<Stabilizer>& stabs) -> DecodingGraph { ... };
```

For each data qubit, the lambda checks how many stabilizers claim it:
- **Two stabilizers** → internal edge between them.
- **One stabilizer** → boundary edge; a unique boundary node is created.

#### `compute_syndromes(sc)`

Iterates over all stabilizers and XORs (`^= 1`) the measurement for each data qubit that has an error of the matching type:
- X errors flip Z-stabilizers.
- Z errors flip X-stabilizers.

#### `compute_layout(sc, origin_x, origin_y, cell_size)`

Assigns 2D render positions:
- Data qubits on a regular grid.
- Stabilizers at the centroid of their data qubits.
- Boundary nodes reflected across the stabilizer from the data qubit they connect to.

---

### 6.3 `union_find` — the three phases of decoding

**Header:** [cpp/src/union_find.h](cpp/src/union_find.h)
**Implementation:** [cpp/src/union_find.cpp](cpp/src/union_find.cpp)

This is the mathematical heart of the program. All functions are pure (no global state); all state is passed in and out as vectors.

#### State representation

The decoder maintains **one row per cluster** in a pair of 2D arrays:

| Array | Dimensions | Meaning |
|---|---|---|
| `clusters_nodes[c][n]` | `n_clusters × n_nodes` | Node `n`'s membership in cluster `c` |
| `clusters_edges[c][e]` | `n_clusters × n_edges` | Edge `e`'s membership in cluster `c` |
| `clusters_activity[c]` | `n_clusters` | State of cluster `c` |

**Node states** (stored in `clusters_nodes`):

| Value | Meaning |
|---|---|
| `0` | Node is not in this cluster |
| `1` | Node is on the **frontier** (just joined) |
| `2` | Node is in the **interior** (joined a previous cycle) |

**Activity states** (stored in `clusters_activity`):

| Value | Meaning |
|---|---|
| `0` | Cluster is empty / absorbed |
| `1` | Cluster is **active** — still growing |
| `2` | Cluster is **inactive** — two active clusters merged (even parity, stopped) |
| `3` | Cluster has **touched the boundary** — stopped |

#### `grow_clusters(...)` — Phase 1 sub-step: GROW

Called once per cycle. For each active cluster:

**Step 1 — Advance interior nodes:** Frontier nodes (1) become interior (2). Interior nodes (2) stay interior (2). The update formula `v * (3 - v)` does this in one expression:
- `0 * 3 = 0` → 0
- `1 * 2 = 2` → 2
- `2 * 1 = 2` → 2

**Step 2 — Extend from half-grown edges:** If an edge is half-grown (`cedges[e] == 1`), activate the endpoint that is not yet in the cluster (set it to frontier, 1).

**Step 3 — Fully grow half-edges:** Same formula applied to edge states: `e * (3 - e)`. Half-grown (1) becomes fully-grown (2).

**Step 4 — Half-grow new edges:** From original frontier nodes (before the cycle started, still stored in `nodes`), half-grow (`= 1`) all adjacent edges that aren't already fully grown.

This two-step growth (half then full) ensures clusters grow by exactly one "half-edge" per cycle, so two clusters collide in the middle of a shared edge symmetrically.

#### `find_and_merge_clusters(...)` — Phase 1 sub-step: MERGE

After growing, scan for clusters that share an edge or a node (both have a nonzero claim on it). Merge them using the inner lambda `merge_pair(c0, c1)`:

- **Nodes:** `min(a + b, 2)` — any positive value stays positive (capped at 2 = interior).
- **Edges:** same formula.
- **Activity:** complex rule:
  - If either cluster has reached the boundary (activity = 3), the merged cluster is also boundary (3).
  - Otherwise, if both were active (1+1) or both inactive (2+2), they become inactive (2) — they've "paired up" and cancelled.
  - If one was active and one inactive (1+2), the merged cluster stays active (1).

The absorbed cluster `c1` has its activity set to 0 (empty).

#### `deactivate_clusters_touching_boundary(...)` — Phase 1 sub-step: DEACTIVATE

Scans every active cluster. If any boundary node index appears in `clusters_nodes[c]` with a nonzero value, the cluster is marked as boundary-touched (activity = 3) and stops growing.

#### `build_spanning_tree(...)` — Phase 2

Builds a spanning tree for one cluster using **BFS (Breadth-First Search)**.

The tree is stored in `tree[node]` = the index of the edge connecting this node to its parent. The root has `tree[root] = -1`. Unvisited nodes start with `tree[n] = -2`.

`tree_access[n]` counts how many edges in the spanning tree connect to node `n`. This is the **degree** of the node within the spanning tree, used in Phase 3 to identify leaves.

The root is chosen as a boundary node in the cluster (if one exists), otherwise the first node in the cluster.

#### `peel_leaf(...)` — Phase 3

Peels one leaf from the current tree. A **leaf** is a node with `tree_access == 1` and `tree[node] != -1` (i.e., it has exactly one spanning tree edge and is not the root).

Algorithm:
1. Pop a leaf from `leaf_stack`.
2. Verify it is still a valid leaf (may have been made non-leaf by a parent cascade).
3. If the leaf is a **defect** (`defects[leaf] != 0`):
   - Mark the edge to its parent as a correction (`edge_corrections[edge_idx] = 1`).
   - Flip the parent's defect status (`defects[parent] ^= 1`). This "passes" the defect up.
4. Decrement `tree_access` for both the leaf and its parent.
5. If the parent is now a leaf (`tree_access == 1` and not root), push it onto `leaf_stack`.

The key insight: defects propagate upward through the tree. If they meet (parent gets flipped twice back to 0), they cancel. If a defect reaches the root, it is absorbed by the boundary.

---

### 6.4 `decoder_stepper` — animating the decoder one step at a time

**Header:** [cpp/src/decoder_stepper.h](cpp/src/decoder_stepper.h)
**Implementation:** [cpp/src/decoder_stepper.cpp](cpp/src/decoder_stepper.cpp)

This class wraps the three phases and exposes them as a sequence of discrete steps that the visualizer can show one-by-one.

#### Enumerations

```cpp
enum class DecoderPhase { IDLE, SYNDROME_VALIDATION, SPANNING_FOREST, FOREST_PEELING, DONE };
enum class SyndromeSubPhase { GROW, MERGE, DEACTIVATE };
```

Each call to `step()` advances exactly one sub-step: one GROW, one MERGE, or one DEACTIVATE; or one spanning tree construction; or one leaf peel.

#### `DecoderSnapshot`

A snapshot of the complete decoder state at a single step. The renderer reads this directly to draw what is happening:

```cpp
struct DecoderSnapshot {
    DecoderPhase phase;
    SyndromeSubPhase syndrome_sub_phase;
    int cycle_number;

    // Phase 1
    std::vector<std::vector<int>> clusters_nodes;
    std::vector<std::vector<int>> clusters_edges;
    std::vector<int> clusters_activity;

    // Phase 2
    std::vector<std::vector<int>> spanning_forest;  // spanning_forest[cluster][node] = edge_idx or -2
    std::vector<std::vector<int>> forest_access;    // degree in spanning tree per node

    // Phase 3
    std::vector<int> edge_corrections;
};
```

#### `DecoderStepper` constructor

Builds two graph representations:
- **Adjacency matrix** `adjacency_[n0][n1] = edge_index` — used by `grow_clusters` to look up edges by node pairs.
- **Adjacency list** `adj_list_[node] = [(neighbor, edge_idx), ...]` — used by BFS in `build_spanning_tree`. Faster to iterate than the matrix.

Then calls `init_syndrome_validation()`.

#### Initialization functions

- `init_syndrome_validation()`: Allocates cluster arrays and seeds each cluster with its syndrome value. If there are no active clusters (no defects), immediately jumps to spanning forest.
- `init_spanning_forest()`: Allocates spanning forest arrays, skips to first active cluster.
- `init_forest_peeling()`: Resets correction vector, builds `peel_defects_` from syndrome, calls `advance_to_next_nonempty_tree()`.

#### `advance_to_next_nonempty_tree()`

Finds the next cluster whose spanning tree has at least one leaf (a non-root node). This is where the `peel_leaf_stack_` is initialized. Sets `phase = DONE` when all trees are exhausted.

#### Step dispatch

```cpp
bool step() {
    switch (snapshot_.phase) {
        case SYNDROME_VALIDATION: return step_syndrome_validation();
        case SPANNING_FOREST:     return step_spanning_forest();
        case FOREST_PEELING:      return step_forest_peeling();
        case DONE: case IDLE:     return false;
    }
}
```

Returns `true` if work was done, `false` if the decoder is finished.

---

### 6.5 `ui` — keyboard input and application state

**Header:** [cpp/src/ui.h](cpp/src/ui.h)
**Implementation:** [cpp/src/ui.cpp](cpp/src/ui.cpp)

#### `AppMode` enum

```cpp
enum class AppMode { READY, DECODING, DECODE_DONE };
```

The application has three modes:
- `READY` — graph is loaded, waiting for user to press D.
- `DECODING` — the decoder stepper is active.
- `DECODE_DONE` — all phases complete; corrections are shown.

#### `UI` class

Owns the `DecodingGraph3D`, the syndrome, and a `std::unique_ptr<DecoderStepper>`.

**Key member variables:**
- `step_timer_` / `step_interval_` — for auto-stepping: accumulates `GetFrameTime()` each frame; when it exceeds the interval, takes a step.
- `auto_stepping_` — whether to step automatically without user input.
- `show_time_planes_`, `show_graph_edges_`, `show_lattice_underlay_` — visibility toggles.

#### Keyboard controls

| Key | Action |
|---|---|
| `D` | Start decoding (creates `DecoderStepper`) |
| `Space` / `→` | Step one sub-step forward |
| `A` | Toggle auto-stepping |
| `+` / `-` | Speed up / slow down auto-stepping |
| `F` | Fast-forward to completion |
| `R` | Reset decoder, return to READY |
| `1` | Toggle time plane grid |
| `2` | Toggle graph edge cylinders |
| `3` | Toggle lattice face underlay |

---

### 6.6 `renderer` — drawing everything with Raylib

**Header:** [cpp/src/renderer.h](cpp/src/renderer.h)
**Implementation:** [cpp/src/renderer.cpp](cpp/src/renderer.cpp)

#### Coordinate systems

The program uses two coordinate systems:

| Space | X | Y | Z |
|---|---|---|---|
| DEM/Stim | spatial col | spatial row | time round |
| 3D world | `x × SPATIAL_SCALE` | `y × SPATIAL_SCALE` | `z × TIME_SCALE` |

`SPATIAL_SCALE = 2.0`, `TIME_SCALE = 3.0`. The time axis is stretched more to make rounds visually separable.

The conversion is done by `to_world(Vec3 v)`:
```cpp
return {v.x * SPATIAL_SCALE, v.y * SPATIAL_SCALE, v.z * TIME_SCALE};
```

#### Orbital camera

The camera orbits the center of the graph at a fixed distance. Two spherical angles control its position:
- `orbit_angle_h_` — horizontal (azimuth), changed by horizontal mouse drag.
- `orbit_angle_v_` — vertical (elevation), changed by vertical mouse drag. Clamped to `[0.05, 3.1]` radians to avoid flipping upside down.
- `orbit_distance_` — changed by scroll wheel.

Camera position is computed each frame:
```cpp
camera_.position = {
    center.x + distance * sin(v) * cos(h),
    center.y + distance * cos(v),
    center.z + distance * sin(v) * sin(h)
};
```

#### Rendering layers (drawn in order)

1. **Lattice underlay** — translucent colored quads showing the X (red) and Z (blue) stabilizer faces. Drawn once per time round, at `z = t * TIME_SCALE`.

2. **Time planes** — light grey grid rectangles at each round's Z position.

3. **Graph edges** — thin cylinders (`DrawCylinderEx`). Color encodes type:
   - Boundary edges: neutral grey.
   - Spatial edges (same round): cool blue-grey.
   - Temporal edges (different rounds): warm tan.
   When decoding is active, edges are dimmed (lower alpha).

4. **Cluster sheaths** — a translucent convex hull drawn around each cluster's nodes. This makes the cluster's extent visually obvious as a "cloud."

5. **Cluster regions** — per-node spheres tinted with the cluster's color. Interior nodes get larger, more opaque spheres; frontier nodes get smaller spheres with a wireframe ring.

6. **Cluster edges** — colored cylinders for edges claimed by a cluster. Half-grown edges only reach the midpoint and have a glowing dot at the tip. Inactive clusters are drawn thinner and dimmer.

7. **Spanning forest** — dark steel-blue cylinders showing which edges are in the spanning trees.

8. **Corrections** — neon green cylinders on edges selected as corrections.

9. **Detector nodes** — drawn last (on top of everything). X-type detectors are cubes; Z-type are spheres. Fired (syndrome = 1) detectors are larger and dark grey. If a node belongs to a cluster, it takes on that cluster's color and gets a dark core.

10. **Boundary nodes** — small grey spheres with wireframe.

11. **2D panel** (HUD) — drawn after `EndMode3D()`, so it is always on top. Shows mode, phase, sub-phase, cluster counts, legend, and controls.

#### Cluster color palette

12 vivid hues at roughly 30° intervals around the color wheel, chosen to contrast with the neutral grey background and structural elements:

```cpp
static const Color CLUSTER_COLORS[] = {
    {255, 55, 55, 255},   // red
    {255, 150, 0, 255},   // orange
    // ...
};
Color get_cluster_color(int index) {
    return CLUSTER_COLORS[index % NUM_CLUSTER_COLORS];
}
```

#### Convex hull rendering (`render_hull`)

For cluster sheaths, the renderer needs to draw a 3D translucent "bubble" around a set of points. This is done in three cases:

- **N = 2:** Draw a capsule (cylinder + two end spheres).
- **Coplanar points:** Project to 2D on the plane, compute a 2D convex hull (Andrew's monotone chain algorithm), then extrude the hull polygon into a thin 3D slab.
- **General 3D (N ≤ 24):** Brute-force: test all triangles `(i, j, k)` and draw those that are on the convex hull (all other points are on the same side of the plane).

#### Profiling

Each rendering layer is wrapped in `PROFILE_BEGIN` / `PROFILE_END(accumulator)` macros. Every 60 frames, the average milliseconds spent in each layer is printed to stderr. This allows identifying performance bottlenecks without a separate profiler.

---

### 6.7 `main` — the game loop

**File:** [cpp/src/main.cpp](cpp/src/main.cpp)

```
main(argc, argv)
│  Parse command-line arguments (dem_file, events_file)
│  InitWindow(1400, 900, ...)
│  SetTargetFPS(60)
│  Create UI and Renderer
│
└─ Loop until window closes:
       ui.handle_input()      ← process keyboard
       ui.update()            ← advance auto-stepping timer
       renderer.update_camera() ← handle mouse drag/scroll
       BeginDrawing()
       renderer.render(...)   ← draw everything
       EndDrawing()
       Record frame time → print stats every 60 frames
```

This is the classic **game loop** pattern:
1. Process input.
2. Update state.
3. Render.
4. Repeat.

Raylib's `BeginDrawing()` / `EndDrawing()` handle double-buffering (drawing to an offscreen buffer, then swapping it to the screen at the right moment to prevent flickering).

---

## 7. Data Flow Through the Program

```
            disk
             │
       test.dem ──────────────────────────────────────────────────────►
       test_events.txt ─────────────────────────────────────────────►   │
                                                                     │   │
                                                             parse_dem_file()
                                                             load_detection_events()
                                                                     │
                                                             DecodingGraph3D
                                                             syndrome[num_detectors]
                                                                     │
                                              ┌──────────────────────┤
                                              │                      │
                                           UI class           Renderer class
                                              │
                                       D key pressed
                                              │
                                   new DecoderStepper(
                                       graph.uf_edges,
                                       graph.uf_boundary_nodes,
                                       syndrome)
                                              │
                                       init:
                                         adjacency matrix
                                         adjacency list
                                         cluster arrays
                                              │
                                   Space/A key → step()
                                              │
                                   step_syndrome_validation():
                                     GROW → grow_clusters()
                                     MERGE → find_and_merge_clusters()
                                     DEACTIVATE → deactivate_...()
                                              │
                                   step_spanning_forest():
                                     build_spanning_tree() per cluster
                                              │
                                   step_forest_peeling():
                                     peel_leaf() per leaf per tree
                                              │
                                   DecoderSnapshot (every frame)
                                              │
                                           Renderer
                                              │
                                    draws clusters, trees,
                                    corrections, panel
                                              │
                                           Screen
```

---

## 8. State Encoding Conventions

### The `* (3 - v)` trick

Used twice in `grow_clusters` for a compact in-place state transition:

| Input `v` | `v * (3 - v)` | Output |
|---|---|---|
| 0 | `0 * 3 = 0` | 0 (absent) |
| 1 | `1 * 2 = 2` | 2 (interior) |
| 2 | `2 * 1 = 2` | 2 (interior) |

This maps: absent → absent, frontier → interior, interior → interior. No branching needed.

### Activity merging rule

When two clusters merge, the new activity is:

| c0 activity | c1 activity | Result |
|---|---|---|
| 3 (boundary) | any | 3 |
| any | 3 (boundary) | 3 |
| 1 (active) | 2 (inactive) | 1 (odd → still active) |
| 2 (inactive) | 1 (active) | 1 |
| 1 | 1 | 2 (even → stop) |
| 2 | 2 | 2 |

Code: `(a0 != a1) ? 1 : 2` — if the activities differ (one active, one inactive), the merged cluster is active; if equal, it is inactive. This implements the parity rule: an odd number of defects in a cluster keeps it growing.

### Spanning tree sentinel values

| Value | Meaning |
|---|---|
| `-2` | Node not yet visited (uninitialized) |
| `-1` | Node is the root (no parent edge) |
| `≥ 0` | Index of the edge connecting this node to its parent |

---

## 9. Visual Design Choices

| Element | Shape / Color | Reason |
|---|---|---|
| Z-type detectors | Sphere, blue-grey | Spheres are rotationally symmetric; Z is the "rounder" Pauli |
| X-type detectors | Cube, warm buff | Cubes have flat faces; X is "orthogonal" |
| Fired detector | Large, dark grey | Must stand out clearly as the error signal |
| Active cluster | Vivid saturated color from palette | Needs to pop against neutral background |
| Inactive cluster | Same color, lower alpha | Still visible but clearly stopped |
| Boundary node | Small grey sphere with wire ring | Not a real physical qubit; subtle |
| Spatial edge | Cool blue-grey cylinder | Connects nodes within the same time round |
| Temporal edge | Warm tan cylinder | Connects nodes across rounds; "different dimension" |
| Spanning tree | Dark steel-blue | Shows structure without competing with cluster colors |
| Correction | Neon green | Unmistakable; marks the answer |
| Background | Off-white `{245,245,248}` | Easier on the eyes than black; makes colors accurate |

---

## 10. Glossary

**Adjacency list** — For each node, a list of `(neighbor, edge_index)` pairs. Fast to iterate. Used by BFS.

**Adjacency matrix** — A 2D grid `adj[n0][n1] = edge_index`. Allows O(1) lookup of whether two nodes are connected. Used by `grow_clusters`. Requires O(N²) memory.

**BFS (Breadth-First Search)** — Graph traversal that explores all nodes at distance 1, then distance 2, etc. Implemented with a `std::queue`. Guarantees the shortest path tree when all edges have equal weight.

**Boundary node** — A virtual node representing the code boundary. An error on a data qubit at the boundary fires only one real stabilizer; the boundary node is its phantom partner. Clusters that touch boundary nodes stop growing.

**Cluster** — A connected set of syndrome nodes and the edges between them that the UF decoder is tracking. Initially each defect is its own cluster; clusters grow and merge.

**Convex hull** — The smallest convex shape that contains all input points. Used to draw the "sheath" around each cluster.

**DEM** — Detector Error Model. A file format (from Stim) that describes which physical errors map to which detector measurement changes, along with their probabilities.

**Defect** — A stabilizer measurement that changed between rounds, indicating a nearby physical error. Also called a syndrome node.

**Frontier node** — A node that just joined a cluster in the current growth cycle (state = 1). It will become interior next cycle.

**Interior node** — A node that has been in a cluster for more than one cycle (state = 2). Fully absorbed.

**Leaf** — In a spanning tree, a node with only one tree-edge (degree 1 in the tree) that is not the root. These are the starting points for tree peeling.

**Peeling** — Working inward from tree leaves, deciding whether each edge should be in the correction set based on whether the leaf node is a defect.

**Rotated surface code** — A specific layout of the surface code where the logical qubit is encoded in a d×d grid of data qubits, surrounded by alternating X and Z stabilizers in a checkerboard pattern.

**Spanning tree** — A subset of a graph's edges that connects all nodes with no cycles, using exactly `N-1` edges for `N` nodes.

**Stabilizer** — A multi-qubit measurement operator that returns +1 when no errors affect it. When an error occurs, the relevant stabilizers return -1 instead. The collection of -1 results is the syndrome.

**Syndrome** — A binary vector indicating which stabilizers fired (measurement = 1). The decoder's input.

**UF decoder** — Union-Find decoder. An efficient (nearly linear time) algorithm for quantum error correction that works by growing clusters around defects and using a spanning forest to determine corrections.
