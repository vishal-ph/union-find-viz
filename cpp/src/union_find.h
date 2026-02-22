#ifndef UNION_FIND_H
#define UNION_FIND_H

#include <vector>
#include <array>
#include <utility>

// ============================================================
// Phase 1: Syndrome Validation
// ============================================================

// Grow all active clusters by one step
void grow_clusters(
    std::vector<std::vector<int>>& clusters_nodes,
    std::vector<std::vector<int>>& clusters_edges,
    const std::vector<int>& clusters_activity,
    const std::vector<std::vector<int>>& adjacency,
    const std::vector<std::array<int,2>>& edges);

// Find overlapping clusters and merge them
void find_and_merge_clusters(
    std::vector<std::vector<int>>& clusters_nodes,
    std::vector<std::vector<int>>& clusters_edges,
    std::vector<int>& clusters_activity);

// Deactivate clusters that contain a boundary node
void deactivate_clusters_touching_boundary(
    const std::vector<std::vector<int>>& clusters_nodes,
    std::vector<int>& clusters_activity,
    const std::vector<int>& boundary_nodes);

// ============================================================
// Phase 2: Spanning Forest
// ============================================================

// Build spanning tree for a single cluster via BFS.
// adj_list[node] = [(neighbor, edge_idx), ...] — sparse adjacency.
void build_spanning_tree(
    std::vector<int>& tree,
    std::vector<int>& tree_access,
    const std::vector<int>& cluster_nodes,
    const std::vector<int>& cluster_edges,
    const std::vector<std::vector<std::pair<int,int>>>& adj_list,
    const std::vector<int>& boundary_nodes);

// ============================================================
// Phase 3: Tree Peeling
// ============================================================

// Peel one leaf from a spanning tree using a pre-built leaf stack.
// The caller must initialize leaf_stack by scanning for leaves once
// (tree_access[i]==1 && tree[i]!=-1), then pass it here on each call.
// Returns true if a leaf was peeled, false if no leaves remain.
bool peel_leaf(
    std::vector<int>& defects,
    std::vector<int>& tree,
    std::vector<int>& tree_access,
    const std::vector<std::array<int,2>>& edges,
    std::vector<int>& edge_corrections,
    std::vector<int>& leaf_stack);

#endif // UNION_FIND_H
