#include "union_find.h"
#include <algorithm>
#include <numeric>
#include <queue>

// ============================================================
// Phase 1: Syndrome Validation
// ============================================================

void grow_clusters(
    std::vector<SparseStateMap>& clusters_nodes,
    std::vector<SparseStateMap>& clusters_edges,
    const std::vector<int>& clusters_activity,
    const std::vector<std::vector<std::pair<int,int>>>& adj_list,
    const std::vector<std::array<int,2>>& edges)
{
    int n_clusters = (int)clusters_nodes.size();

    for (int c = 0; c < n_clusters; c++) {
        if (clusters_activity[c] != 1) continue;

        auto& nodes = clusters_nodes[c];
        auto& cedges = clusters_edges[c];

        // Step 1: Capture frontier nodes (state==1) before modifications
        std::vector<int> frontier;
        for (auto& [node, state] : nodes) {
            if (state == 1) frontier.push_back(node);
        }

        // Step 2: Promote all existing nodes to interior (state=2)
        for (auto& [node, state] : nodes) {
            state = 2;
        }

        // Step 3: Activate absent endpoint of half-grown edges (state=1)
        for (auto& [e, estate] : cedges) {
            if (estate == 1) {
                int n0 = edges[e][0], n1 = edges[e][1];
                if (nodes.find(n0) == nodes.end()) nodes[n0] = 1;
                else if (nodes.find(n1) == nodes.end()) nodes[n1] = 1;
            }
        }

        // Step 4: Fully grow half-edges (all map entries become 2)
        for (auto& [e, estate] : cedges) {
            estate = 2;
        }

        // Step 5: Half-grow edges adjacent to original frontier nodes
        for (int node : frontier) {
            for (auto [neighbor, edge_idx] : adj_list[node]) {
                if (cedges.find(edge_idx) == cedges.end()) {
                    cedges[edge_idx] = 1;
                }
            }
        }
    }
}

void find_and_merge_clusters(
    std::vector<SparseStateMap>& clusters_nodes,
    std::vector<SparseStateMap>& clusters_edges,
    std::vector<int>& clusters_activity)
{
    int n_clusters = (int)clusters_nodes.size();
    if (n_clusters == 0) return;

    // Lightweight union-find for identifying merge groups
    std::vector<int> parent(n_clusters);
    std::iota(parent.begin(), parent.end(), 0);

    auto find = [&](int x) -> int {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };

    auto unite = [&](int a, int b) {
        int ra = find(a), rb = find(b);
        if (ra != rb) parent[rb] = ra;
    };

    // Build reverse index: edge -> [clusters claiming it]
    std::unordered_map<int, std::vector<int>> edge_to_clusters;
    for (int c = 0; c < n_clusters; c++) {
        if (clusters_activity[c] == 0) continue;
        for (auto& [e, state] : clusters_edges[c]) {
            if (state >= 1) {
                edge_to_clusters[e].push_back(c);
            }
        }
    }

    // Merge clusters sharing edges
    for (auto& [e, cs] : edge_to_clusters) {
        for (int i = 1; i < (int)cs.size(); i++) {
            unite(cs[0], cs[i]);
        }
    }

    // Build reverse index: node -> [clusters claiming it]
    std::unordered_map<int, std::vector<int>> node_to_clusters;
    for (int c = 0; c < n_clusters; c++) {
        if (clusters_activity[c] == 0) continue;
        for (auto& [n, state] : clusters_nodes[c]) {
            if (state >= 1) {
                node_to_clusters[n].push_back(c);
            }
        }
    }

    // Merge clusters sharing nodes
    for (auto& [n, cs] : node_to_clusters) {
        for (int i = 1; i < (int)cs.size(); i++) {
            unite(cs[0], cs[i]);
        }
    }

    // Flatten parent array
    for (int c = 0; c < n_clusters; c++) {
        parent[c] = find(c);
    }

    // Group clusters by root
    std::unordered_map<int, std::vector<int>> groups;
    for (int c = 0; c < n_clusters; c++) {
        if (clusters_activity[c] == 0) continue;
        groups[parent[c]].push_back(c);
    }

    // Merge each group: absorb all members into the first
    for (auto& [root, members] : groups) {
        if (members.size() <= 1) continue;

        int target = members[0];
        for (int i = 1; i < (int)members.size(); i++) {
            int src = members[i];

            // Merge nodes
            for (auto& [n, state] : clusters_nodes[src]) {
                clusters_nodes[target][n] = std::min(clusters_nodes[target][n] + state, 2);
            }
            clusters_nodes[src].clear();

            // Merge edges
            for (auto& [e, state] : clusters_edges[src]) {
                clusters_edges[target][e] = std::min(clusters_edges[target][e] + state, 2);
            }
            clusters_edges[src].clear();

            // Merge activity (parity logic)
            int a0 = clusters_activity[target], a1 = clusters_activity[src];
            if (a0 == 3 || a1 == 3)
                clusters_activity[target] = 3;
            else
                clusters_activity[target] = (a0 != a1) ? 1 : 2;
            clusters_activity[src] = 0;
        }
    }
}

void deactivate_clusters_touching_boundary(
    const std::vector<SparseStateMap>& clusters_nodes,
    std::vector<int>& clusters_activity,
    const std::vector<int>& boundary_nodes)
{
    int n_clusters = (int)clusters_nodes.size();
    for (int c = 0; c < n_clusters; c++) {
        if (clusters_activity[c] != 1) continue;
        for (int b : boundary_nodes) {
            auto it = clusters_nodes[c].find(b);
            if (it != clusters_nodes[c].end() && it->second > 0) {
                clusters_activity[c] = 3;
                break;
            }
        }
    }
}

// ============================================================
// Phase 2: Spanning Forest
// ============================================================

void build_spanning_tree(
    SparseStateMap& tree,
    SparseStateMap& tree_access,
    const SparseStateMap& cluster_nodes,
    const SparseStateMap& cluster_edges,
    const std::vector<std::vector<std::pair<int,int>>>& adj_list,
    const std::vector<int>& boundary_nodes)
{
    // Find root: prefer boundary node in cluster, else first node in cluster
    int root = -1;
    for (int b : boundary_nodes) {
        if (cluster_nodes.find(b) != cluster_nodes.end()) {
            root = b;
            break;
        }
    }
    if (root == -1) {
        if (!cluster_nodes.empty()) {
            root = cluster_nodes.begin()->first;
        }
    }
    if (root == -1) return; // empty cluster

    // BFS from root using adjacency list
    tree[root] = -1;
    std::queue<int> q;
    q.push(root);

    while (!q.empty()) {
        int node = q.front();
        q.pop();
        for (auto [neighbor, edge_idx] : adj_list[node]) {
            if (cluster_edges.find(edge_idx) == cluster_edges.end()) continue;
            if (cluster_nodes.find(neighbor) == cluster_nodes.end()) continue;
            if (tree.find(neighbor) != tree.end()) continue; // already visited

            tree[neighbor] = edge_idx;
            tree_access[node]++;
            tree_access[neighbor]++;
            q.push(neighbor);
        }
    }
}

// ============================================================
// Phase 3: Tree Peeling
// ============================================================

bool peel_leaf(
    std::vector<int>& defects,
    SparseStateMap& tree,
    SparseStateMap& tree_access,
    const std::vector<std::array<int,2>>& edges,
    std::vector<int>& edge_corrections,
    std::vector<int>& leaf_stack)
{
    auto get_access = [&](int n) -> int {
        auto it = tree_access.find(n);
        return (it != tree_access.end()) ? it->second : 0;
    };

    auto get_tree = [&](int n) -> int {
        auto it = tree.find(n);
        return (it != tree.end()) ? it->second : -2;
    };

    // Pop a leaf from the stack
    if (leaf_stack.empty()) return false;
    int leaf = leaf_stack.back();
    leaf_stack.pop_back();

    // Verify it's still a valid leaf (may have been peeled as a parent cascade)
    if (get_access(leaf) != 1 || get_tree(leaf) == -1) {
        // Skip stale entries — try the next one
        while (!leaf_stack.empty()) {
            leaf = leaf_stack.back();
            leaf_stack.pop_back();
            if (get_access(leaf) == 1 && get_tree(leaf) != -1) goto peel;
        }
        return false;
    }

peel:
    int edge_idx = get_tree(leaf);
    int parent = (edges[edge_idx][0] == leaf) ? edges[edge_idx][1] : edges[edge_idx][0];

    tree_access[leaf]--;
    tree_access[parent]--;

    if (defects[leaf] != 0) {
        edge_corrections[edge_idx] = 1;
        if (parent >= 0 && parent < (int)defects.size()) {
            defects[parent] = 1 - defects[parent];
        }
    }

    // If parent became a leaf, push it onto the stack
    if (get_access(parent) == 1 && get_tree(parent) != -1) {
        leaf_stack.push_back(parent);
    }

    return true;
}
