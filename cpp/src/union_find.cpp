#include "union_find.h"
#include <algorithm>
#include <queue>

// ============================================================
// Phase 1: Syndrome Validation
// ============================================================

void grow_clusters(
    std::vector<std::vector<int>>& clusters_nodes,
    std::vector<std::vector<int>>& clusters_edges,
    const std::vector<int>& clusters_activity,
    const std::vector<std::vector<int>>& adjacency,
    const std::vector<std::array<int,2>>& edges)
{
    int n_clusters = (int)clusters_nodes.size();
    int n_nodes = (int)clusters_nodes[0].size();
    int n_edges = (int)edges.size();

    for (int c = 0; c < n_clusters; c++) {
        if (clusters_activity[c] != 1) continue;

        auto& nodes = clusters_nodes[c];
        auto& cedges = clusters_edges[c];

        // Step 1: Compute new node states (frontier->interior, interior stays)
        // 0->0, 1->2, 2->2
        std::vector<int> new_nodes(n_nodes);
        for (int i = 0; i < n_nodes; i++) {
            new_nodes[i] = nodes[i] * (3 - nodes[i]);
        }

        // Step 2: Activate absent endpoint of half-grown edges
        for (int e = 0; e < n_edges; e++) {
            if (cedges[e] == 1) {
                int n0 = edges[e][0], n1 = edges[e][1];
                if (new_nodes[n0] == 0) new_nodes[n0] = 1;
                else if (new_nodes[n1] == 0) new_nodes[n1] = 1;
            }
        }

        // Step 3: Fully grow half-edges
        // 0->0, 1->2, 2->2
        for (int e = 0; e < n_edges; e++) {
            cedges[e] = cedges[e] * (3 - cedges[e]);
        }

        // Step 4: Half-grow edges adjacent to originally-frontier nodes
        for (int i = 0; i < n_nodes; i++) {
            if (nodes[i] != 1) continue; // original frontier state
            const auto& row = adjacency[i];
            for (int j = 0; j < n_nodes; j++) {
                int e = row[j];
                if (e != -1 && cedges[e] != 2) {
                    cedges[e] = 1;
                }
            }
        }

        nodes = std::move(new_nodes);
    }
}

void find_and_merge_clusters(
    std::vector<std::vector<int>>& clusters_nodes,
    std::vector<std::vector<int>>& clusters_edges,
    std::vector<int>& clusters_activity)
{
    int n_clusters = (int)clusters_nodes.size();
    int n_edges = (int)clusters_edges[0].size();
    int n_nodes = (int)clusters_nodes[0].size();

    auto merge_pair = [&](int c0, int c1) {
        // Absorb c1 into c0: element-wise min(a+b, 2), zero out c1
        for (int i = 0; i < n_nodes; i++) {
            clusters_nodes[c0][i] = std::min(clusters_nodes[c0][i] + clusters_nodes[c1][i], 2);
            clusters_nodes[c1][i] = 0;
        }
        for (int i = 0; i < n_edges; i++) {
            clusters_edges[c0][i] = std::min(clusters_edges[c0][i] + clusters_edges[c1][i], 2);
            clusters_edges[c1][i] = 0;
        }
        // Merged activity:
        //   (active + active) or (inactive + inactive) -> inactive (2)
        //   (active + inactive) -> active (1)
        //   (any + boundary) -> boundary (3)
        int a0 = clusters_activity[c0], a1 = clusters_activity[c1];
        if (a0 == 3 || a1 == 3)
            clusters_activity[c0] = 3;
        else
            clusters_activity[c0] = (a0 != a1) ? 1 : 2;
        clusters_activity[c1] = 0;
    };

    // Merge clusters that share an edge (both claim edge >= 1)
    for (int e = 0; e < n_edges; e++) {
        int first = -1;
        for (int c = 0; c < n_clusters; c++) {
            if (clusters_edges[c][e] >= 1) {
                if (first == -1) {
                    first = c;
                } else {
                    merge_pair(first, c);
                }
            }
        }
    }

    // Merge clusters that share a node (both claim node >= 1)
    for (int n = 0; n < n_nodes; n++) {
        int first = -1;
        for (int c = 0; c < n_clusters; c++) {
            if (clusters_nodes[c][n] >= 1) {
                if (first == -1) {
                    first = c;
                } else {
                    merge_pair(first, c);
                }
            }
        }
    }
}

void deactivate_clusters_touching_boundary(
    const std::vector<std::vector<int>>& clusters_nodes,
    std::vector<int>& clusters_activity,
    const std::vector<int>& boundary_nodes)
{
    int n_clusters = (int)clusters_nodes.size();
    for (int c = 0; c < n_clusters; c++) {
        if (clusters_activity[c] != 1) continue;
        for (int b : boundary_nodes) {
            if (clusters_nodes[c][b] > 0) {
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
    std::vector<int>& tree,
    std::vector<int>& tree_access,
    const std::vector<int>& cluster_nodes,
    const std::vector<int>& cluster_edges,
    const std::vector<std::vector<std::pair<int,int>>>& adj_list,
    const std::vector<int>& boundary_nodes)
{
    // Find root: prefer boundary node in cluster, else first node in cluster
    int root = -1;
    for (int b : boundary_nodes) {
        if (cluster_nodes[b] != 0) {
            root = b;
            break;
        }
    }
    if (root == -1) {
        int n_nodes = (int)cluster_nodes.size();
        for (int i = 0; i < n_nodes; i++) {
            if (cluster_nodes[i] != 0) {
                root = i;
                break;
            }
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
            if (cluster_edges[edge_idx] == 0) continue;
            if (cluster_nodes[neighbor] == 0) continue;
            if (tree[neighbor] != -2) continue; // already visited

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
    std::vector<int>& tree,
    std::vector<int>& tree_access,
    const std::vector<std::array<int,2>>& edges,
    std::vector<int>& edge_corrections,
    std::vector<int>& leaf_stack)
{
    // Pop a leaf from the stack
    if (leaf_stack.empty()) return false;
    int leaf = leaf_stack.back();
    leaf_stack.pop_back();

    // Verify it's still a valid leaf (may have been peeled as a parent cascade)
    if (tree_access[leaf] != 1 || tree[leaf] == -1) {
        // Skip stale entries — try the next one
        // Use a loop to drain any other stale entries
        while (!leaf_stack.empty()) {
            leaf = leaf_stack.back();
            leaf_stack.pop_back();
            if (tree_access[leaf] == 1 && tree[leaf] != -1) goto peel;
        }
        return false;
    }

peel:
    int edge_idx = tree[leaf];
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
    if (tree_access[parent] == 1 && tree[parent] != -1) {
        leaf_stack.push_back(parent);
    }

    return true;
}
