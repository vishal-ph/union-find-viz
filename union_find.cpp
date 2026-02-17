/*
 * Copyright (c) Entropica Labs Pte Ltd 2025.
 *
 * Use, distribution and reproduction of this program in its source or compiled form is prohibited
 * without the express written consent of Entropica Labs Pte Ltd.
 *
 */

#include <algorithm>
#include <functional>
#include <iostream>
#include <vector>
#include <algorithm>
#include <iostream>
#include <algorithm>
#include <vector>
#include <algorithm>
#include <numeric>
#include <vector>
#include <numeric>
#include <algorithm>

#include "union_find.h"

// #include "utils.h"
// #include "syndrome_validation.h"
// #include "union_find.h"
// #include "spanning_forest_growth.h"
// #include "peeling.h"

auto find_first_instance(
    std::vector<int> arr,
    std::function<bool(int)> condition) -> int
{
    // Find the first index in an array that satisfies a condition.
    // If no index satisfies the condition, return -1.

    // TODO: ASK YORGOS TO VERIFY THIS FUNCTION
    auto it = std::find_if(arr.begin(), arr.end(), condition);
    if (it != arr.end())
    {
        return std::distance(arr.begin(), it);
    }
    else
    {
        return -1; // No index satisfies the condition
    }
}

auto get_other_node_from_edge(
    std::vector<int> edge, // two element vector representing an edge
    int node) -> int       // node to find the other node of
{
    // return the other node in the edge
    return (node == edge[0]) ? edge[1] : edge[0];
}
template <typename T>
T &python_style_access(std::vector<T> &vec, int index)
{
    if (index < 0)
    {
        index += vec.size();
    }
    return vec.at(index);
}

auto tree_peeling_step(State state) -> State
{
    // returns the edge corrections for the trees
    // tree is the index of the tree to be peeled
    // generate defects from syndromes with data type bool
    std::vector<int> defects = state.defects;
    std::vector<int> tree = state.tree;
    std::vector<int> tree_access = state.tree_access;
    std::vector<std::vector<int>> edges = state.edges;
    std::vector<int> edge_corrections = state.edge_corrections;

    // get an array such that its true when tree_access == 1 and false otherwise
    std::vector<int> if_tree_access_1(tree_access.size(), 0);
    std::transform(
        tree_access.cbegin(), tree_access.cend(), // range of iteration
        if_tree_access_1.begin(),                 // destination for the output
        [](int x)
        { return x == 1 ? 1 : 0; }); // condition to be applied

    std::vector<int> non_root_tree(tree.size(), 0);
    std::transform(
        tree.cbegin(), tree.cend(), // range of iteration
        non_root_tree.begin(),      // destination for the output
        [](int x)
        { return x != -1 ? 1 : 0; }); // condition to be applied

    // combine the two arrays with a logical AND
    std::vector<int> combined(if_tree_access_1.size(), 0);
    std::transform(
        if_tree_access_1.cbegin(), if_tree_access_1.cend(), // range of iteration
        non_root_tree.cbegin(),                             // destination for the output
        combined.begin(),                                   // condition to be applied
        [](int a, int b)
        { return ((a != 0) && (b != 0)) ? 1 : 0; }); // condition to be applied

    // Pop the first non-root node with access 1
    int node = find_first_instance(combined);
    int edge_idx = python_style_access(tree, node); // Get the edge index from the tree
    int parent_node = get_other_node_from_edge(edges[edge_idx], node);

    // Modify the tree access in place and add -1 at location node and parent_node
    python_style_access(tree_access, node) += -1;        // mark the node as not in the tree
    python_style_access(tree_access, parent_node) += -1; // mark the node as peeled

    // Modify the defects and edge corrections in place if node has a defect
    if (python_style_access(defects, node) != 0)
    {
        // Modify the defects
        if (parent_node >= 0 && parent_node < defects.size())
        {
            python_style_access(defects, parent_node) = 1 - python_style_access(defects, parent_node);
        }

        // Modify the edge corrections
        edge_corrections[edge_idx] = 1;
    }
    return State(defects, tree, tree_access, edges, edge_corrections);
}

auto tree_peeling(
    std::vector<int> syndrome,
    std::vector<int> tree,
    std::vector<int> tree_access,
    std::vector<int> edge_corrections,
    std::vector<std::vector<int>> edges) -> std::vector<int>
{
    // returns the edge corrections for the trees
    // tree is the index of the tree to be peeled
    // generate defects from syndromes with data type bool
    // Extend defects to cover all nodes (syndrome + boundary) with boundary defects = 0
    std::vector<int> defects(tree.size(), 0);
    for (int i = 0; i < (int)syndrome.size() && i < (int)defects.size(); i++) {
        defects[i] = syndrome[i];
    }

    // copy input tree
    std::vector<int> tree_copy(tree.size(), -1);
    std::copy(tree.begin(), tree.end(), tree_copy.begin());

    // copy input tree access
    std::vector<int> tree_access_copy(tree_access.size(), -1);
    std::copy(tree_access.begin(), tree_access.end(), tree_access_copy.begin());

    State state(defects, tree_copy, tree_access_copy, edges, edge_corrections);

    auto tree_has_a_leaf = [](const State &state) -> bool
    {
        for (int i = 0; i < state.tree_access.size(); i++)
        {
            if (state.tree_access[i] == 1 && state.tree[i] != -1)
            {
                return true; // Found a tree leaf
            }
        }
        return false;
    };

    // a while loop that checks if any tree_access == 1 and tree != -1 and continues tree_peeling_step
    while (tree_has_a_leaf(state))
    {
        state = tree_peeling_step(state);
    }

    // return edge corrections
    return state.edge_corrections;
}

// The main peeling function below
auto forest_peeling(
    std::vector<int> syndrome,
    std::vector<std::vector<int>> spanning_forest,
    std::vector<std::vector<int>> forest_access,
    std::vector<std::vector<int>> edges) -> std::vector<int>
{

    // define edge corrections
    std::vector<int> edge_corrections(edges.size(), 0);

    auto peel_non_empty_trees = [&](int tree_idx, std::vector<int> edge_corrs) -> std::vector<int>
    {
        // returns the edge corrections for the non-empty trees
        // tree_idx is the index of the tree to be peeled

        std::vector<int> tree = spanning_forest[tree_idx];
        std::vector<int> tree_access = forest_access[tree_idx];

        bool tree_has_content = std::any_of(tree.begin(), tree.end(), [](int value)
                                            { return value != -2; });

        // peel the tree if it is not empty
        if (tree_has_content)
        {
            edge_corrs = tree_peeling(syndrome, tree, tree_access, edge_corrs, edges);
        }
        return edge_corrs;
    };

    // Iterate over the spanning forest and peel each tree
    for (int i = 0; i < spanning_forest.size(); i++)
    {
        edge_corrections = peel_non_empty_trees(i, edge_corrections);
    }
    return edge_corrections;
}

auto find_root_node(
    std::vector<int> cluster_nodes,
    std::vector<int> boundary_nodes) -> int
{
    // Find the root node of the cluster.

    // Parameters
    // ----------
    // cluster_nodes : Array
    //     The nodes in the cluster.
    // boundary_nodes : _type_
    //     The boundary nodes of the graph.

    // Returns
    // -------
    // int
    //     The root node of the cluster.

    // a bool to identify whether node is in the cluster_nodes or not
    std::vector<int> is_node_in_cluster = std::vector<int>(cluster_nodes.size(), 0);
    std::transform(
        cluster_nodes.cbegin(), cluster_nodes.cend(), // range of iteration
        is_node_in_cluster.begin(),                   // destination for the output
        [](int x)
        { return (x != 0) ? 1 : 0; });

    std::vector<int> is_node_boundary = std::vector<int>(cluster_nodes.size(), 0);
    for (int i = 0; i < boundary_nodes.size(); i++)
    {
        is_node_boundary[boundary_nodes[i]] = 1;
    }

    std::vector<int> is_node_in_cluster_and_boundary = std::vector<int>(cluster_nodes.size(), 0);
    std::transform(
        is_node_in_cluster.cbegin(), is_node_in_cluster.cend(), // range of iteration
        is_node_boundary.cbegin(),                              // second iterator
        is_node_in_cluster_and_boundary.begin(),                // destination for the output
        [](bool a, bool b)
        { return ((a != 0) && (b != 0)) ? 1 : 0; }); // condition to be applied

    int root_node = find_first_instance(is_node_in_cluster_and_boundary);

    // If the cluster has no boundary nodes, then the root has not been found.
    // In this case, we can select any node in the cluster as the root.
    // We choose the first non-zero node in the cluster as the root.
    if (root_node == -1)
    {
        root_node = find_first_instance(is_node_in_cluster);
    }
    return root_node;
}

auto grow_out_from_node(
    int node,
    std::vector<int> tree,
    std::vector<int> tree_access,
    std::vector<int> nodes_to_grow,
    std::vector<std::vector<int>> graph_edge_idxs,
    std::vector<int> cluster_edges,
    std::vector<int> cluster_nodes) -> std::tuple<std::vector<int>, std::vector<int>, std::vector<int>>
{
    // Grow the tree from the node to the nodes in the cluster.

    // Parameters
    // ----------
    // node : int
    //     The node to grow from.
    // tree : Array
    //     The initialized empty tree.
    // tree_access : Array
    //     The initialized empty tree access.
    // nodes_to_grow : Array
    //     The nodes to grow to.
    // graph_edge_idxs : Array
    //     The node adjacency matrix populated with edge indices.
    // cluster_edges : Array
    //     The edges in the cluster.
    // cluster_nodes : Array
    //     The nodes in the cluster.

    // Returns
    // -------
    // tuple[Array, Array, Array]
    //     The tree, tree access and the nodes to grow.
    auto append_edge_if_conditions_are_met = [=](
                                                 int other_node,
                                                 std::vector<int> tr,
                                                 std::vector<int> tr_acs,
                                                 std::vector<int> nds_to_grow) -> std::tuple<std::vector<int>, std::vector<int>, std::vector<int>>
    {
        int edge_idx = graph_edge_idxs[node][other_node];

        bool edge_exists = edge_idx != -1;
        bool edge_is_in_cluster = edge_exists && (cluster_edges[edge_idx] != 0);
        bool other_node_in_cluster = cluster_nodes[other_node] != 0;
        bool other_node_not_in_tree = tr[other_node] == -2;

        bool append_edge_bool = edge_exists && edge_is_in_cluster && other_node_in_cluster && other_node_not_in_tree;
        if (append_edge_bool)
        {
            // Append the edge to the tree
            tr[other_node] = edge_idx;
        }

        // Update the tree access by incrementing the number of edges
        tr_acs[node] += append_edge_bool ? 1 : 0;
        tr_acs[other_node] += append_edge_bool ? 1 : 0;

        // Add the other node to the nodes to grow if they are not already to be grown
        nds_to_grow[other_node] = (nds_to_grow[other_node] != 0) || append_edge_bool;

        return std::tie(tr, tr_acs, nds_to_grow);
    };
    // Iterate over the edges of the node and append them to the tree
    for (int i = 0; i < cluster_nodes.size(); i++)
    {
        std::tie(tree, tree_access, nodes_to_grow) = append_edge_if_conditions_are_met(i, tree, tree_access, nodes_to_grow);
    }

    // return the tree, tree access and the nodes to grow
    return std::tie(tree, tree_access, nodes_to_grow);
}

auto grow_tree_step(
    std::vector<int> tree,
    std::vector<int> tree_access,
    std::vector<int> nodes_to_grow,
    std::vector<std::vector<int>> edges,
    std::vector<std::vector<int>> graph_edge_idxs,
    std::vector<int> cluster_edges,
    std::vector<int> cluster_nodes) -> std::tuple<std::vector<int>, std::vector<int>, std::vector<int>, std::vector<std::vector<int>>, std::vector<std::vector<int>>, std::vector<int>, std::vector<int>>
{
    int node = find_first_instance(nodes_to_grow);
    if (node == -1)
    {
        // Emulate Python version behavior
        node = nodes_to_grow.size() - 1;
    }
    nodes_to_grow[node] = 0;

    // Grow the tree from the node
    std::tie(tree, tree_access, nodes_to_grow) = grow_out_from_node(
        node, tree, tree_access, nodes_to_grow, graph_edge_idxs, cluster_edges, cluster_nodes);

    return std::tie(tree, tree_access, nodes_to_grow, edges, graph_edge_idxs, cluster_edges, cluster_nodes);
}

auto grow_tree_from_cluster(
    std::vector<int> tree,
    std::vector<int> tree_access,
    std::vector<int> cluster_nodes,
    std::vector<int> cluster_edges,
    std::vector<std::vector<int>> edges,
    std::vector<std::vector<int>> graph_edge_idxs,
    std::vector<int> boundary_nodes) -> std::tuple<std::vector<int>, std::vector<int>>
{
    // Grow a tree from a cluster of nodes and edges.

    // Parameters
    // ----------
    // tree : Array
    //     The initialized empty tree.
    // tree_access : Array
    //     The initialized empty tree access.
    // cluster_nodes : Array
    //     The nodes in the cluster.
    // cluster_edges : Array
    //     The edges in the cluster.
    // edges : Array
    //     The edges in the graph.
    // graph_edge_idxs : Array
    //     The node adjacency matrix populated with edge indices.
    // boundary_nodes : Array
    //     The boundary nodes of the graph.

    // Returns
    // -------
    // tuple[Array, Array]
    //     The tree and tree access.

    // check if cluster has any boundary nodes
    int root_node = find_root_node(cluster_nodes, boundary_nodes);
    if (root_node == -1)
    {
        // Emulate Python version behavior where if no root node is found,
        // FIXME: would it be better to just
        //     return std::tie(tree, tree_access);
        root_node = cluster_nodes.size() - 1;
    }

    // Initialize the tree with the root node
    tree[root_node] = -1;

    // Find edges going out of the root node
    auto nodes_to_grow = std::vector<int>(cluster_nodes.size(), 0);
    std::tie(tree, tree_access, nodes_to_grow) = grow_out_from_node(
        root_node, tree, tree_access, nodes_to_grow, graph_edge_idxs, cluster_edges, cluster_nodes);

    auto state = std::tie(tree, tree_access, nodes_to_grow, edges, graph_edge_idxs, cluster_edges, cluster_nodes);

    // while loop that checks for if any nodes_to_grow in state[2] is True it continues
    while (std::any_of(std::get<2>(state).begin(), std::get<2>(state).end(), [](bool x)
                       { return x == true; }))
    {
        std::tie(tree, tree_access, nodes_to_grow, edges, graph_edge_idxs, cluster_edges, cluster_nodes) = grow_tree_step(
            std::get<0>(state), std::get<1>(state), std::get<2>(state), std::get<3>(state), std::get<4>(state), std::get<5>(state), std::get<6>(state));

        // update the state with the new values
        state = std::tie(tree, tree_access, nodes_to_grow, edges, graph_edge_idxs, cluster_edges, cluster_nodes);
    }
    // return the tree and tree access
    return std::tie(tree, tree_access);
}

auto grow_spanning_forest(
    std::vector<int> syndrome,
    std::vector<std::vector<int>> edges,
    std::vector<int> boundary_nodes,
    std::vector<std::vector<int>> clusters_nodes,
    std::vector<std::vector<int>> clusters_edges,
    std::vector<int> clusters_activity) -> std::tuple<std::vector<std::vector<int>>, std::vector<std::vector<int>>>
{
    // Grow a spanning forest from the clusters in the graph and return the forest and
    // its leaves.

    // Each row of the spanning forest represents a tree in the forest and the
    // corresponding row of the access array of the tree.

    // Tree values:
    // * -2 : Node not in the tree
    // * -1 : Root node of the tree
    // * i >= 0 : Edge index connecting node to its parent in the tree

    // Access values:
    // * 0 : Node not in the tree
    // * i > 0 : Number of edges connected to the node

    // Parameters:
    // -----------
    // syndrome : Array
    //     A binary array indicating the syndrome.
    // edges : Array
    //     An array of edges in the graph.
    // boundary_nodes : Array
    //     An array of boundary nodes in the graph.
    // clusters_nodes : Array
    //     An array of cluster nodes.
    // clusters_edges : Array
    //     An array of cluster edges.
    // clusters_activity : Array
    //     An array of cluster activity.

    // Returns:
    // --------
    // tuple[Array, Array]
    //     The spanning forest and its access array.

    int n_nodes = syndrome.size() + boundary_nodes.size();
    int n_clusters = syndrome.size();
    int n_edges = edges.size();

    // Create a graph edge index array for quick access to the edges that a node is on
    // create graph_edge_idxs with size (n_nodes, n_nodes) and all values set to -1
    std::vector<std::vector<int>> graph_edge_idxs(n_nodes, std::vector<int>(n_nodes, -1));
    for (int i = 0; i < n_edges; i++)
    {
        int node1 = edges[i][0];
        int node2 = edges[i][1];
        graph_edge_idxs[node1][node2] = i;
        graph_edge_idxs[node2][node1] = i;
    }

    // Initialize the spanning forest and its leaves (n_clusters, n_nodes)
    std::vector<std::vector<int>> spanning_forest(n_clusters, std::vector<int>(n_nodes, -2));
    std::vector<std::vector<int>> forest_access(n_clusters, std::vector<int>(n_nodes, 0));

    // Grow a tree from each cluster
    auto grow_tree_from_cluster_if_it_exists = [=](
                                                   int c_idx,
                                                   std::vector<std::vector<int>> sp_forest,
                                                   std::vector<std::vector<int>> forest_acs) -> std::tuple<std::vector<std::vector<int>>, std::vector<std::vector<int>>>
    {
        // Grow a tree from the cluster if it exists
        // c_idx is the index of the cluster to be grown
        // sp_forest is the spanning forest
        // forest_acc is the access array of the spanning forest

        // If the cluster is not empty, grow a tree from it
        if (clusters_activity[c_idx] != 0)
        {
            std::tie(sp_forest[c_idx], forest_acs[c_idx]) = grow_tree_from_cluster(
                sp_forest[c_idx],
                forest_acs[c_idx],
                clusters_nodes[c_idx],
                clusters_edges[c_idx],
                edges,
                graph_edge_idxs,
                boundary_nodes);
        }
        return std::tie(sp_forest, forest_acs);
    };

    for (int i = 0; i < n_clusters; i++)
    {
        std::tie(spanning_forest, forest_access) = grow_tree_from_cluster_if_it_exists(i, spanning_forest, forest_access);
    }
    return std::tie(spanning_forest, forest_access);
}

// return cluster_nodes
auto activate_edge_nodes(
    std::vector<int> cluster_nodes,
    int edge_idx,
    std::vector<std::vector<int>> edges) -> std::vector<int>
{
    // Activate the node of the edge that is currently not present in the cluster.
    std::vector<int> edge = edges[edge_idx];

    bool is_node0_not_present = cluster_nodes[edge[0]] == 0;
    if (is_node0_not_present)
    {
        cluster_nodes[edge[0]] = 1;
    }
    else
    {
        cluster_nodes[edge[1]] = 1;
    }
    return cluster_nodes;
}

auto grow_out_of_node(
    std::vector<int> cluster_edges,
    int node,
    std::vector<std::vector<int>> graph_edge_idxs) -> std::vector<int>
{
    // Grow all the edges that the node is on by half,
    // except the ones that are already fully grown

    std::vector<int> node_edges = graph_edge_idxs[node];

    auto grow_half_edge = [=](int edge_idx, std::vector<int> cl_edges) -> std::vector<int>
    {
        // Grow the edge at edge_idx by half if it is not fully grown
        // 0 -> 0, 1 -> 2, 2 -> 2
        bool edge_exists = edge_idx != -1;
        if (edge_exists && cl_edges[edge_idx] != 2)
        {
            cl_edges[edge_idx] = 1;
        }
        return cl_edges;
    };

    for (int i = 0; i < node_edges.size(); i++)
    {
        int edge_idx = node_edges[i];
        cluster_edges = grow_half_edge(edge_idx, cluster_edges);
    }

    return cluster_edges;
}

auto grow_cluster(
    std::vector<int> cluster_nodes,
    std::vector<int> cluster_edges,
    std::vector<std::vector<int>> graph_edge_idxs,
    std::vector<std::vector<int>> edges) -> std::tuple<std::vector<int>, std::vector<int>>
{
    // Grow a cluster. This is done by:
    // 1. Deactivate all the active nodes.
    // 2. Activate non-present nodes that are on the edges that are half grown since in
    //    the next step they will be fully grown.
    // 3. Fully grow the edges that are half grown.
    // 4. Grow out of the INITALLY active nodes

    // #1 Deactivate all the active nodes via x * (3 - x)
    // 0 -> 0, 1 -> 2, 2 -> 2
    std::vector<int> new_cluster_nodes = std::vector<int>(cluster_nodes.size(), 0);
    for (int i = 0; i < cluster_nodes.size(); i++)
    {
        new_cluster_nodes[i] = cluster_nodes[i] * (3 - cluster_nodes[i]);
    }
    // #2 Activate non-present nodes that are on the edges that are half grown since in
    // the next step they will be fully grown.
    auto activate_half_grown_edge_nodes = [=](int edge_idx, std::vector<int> cluster_nodes) -> std::vector<int>
    {
        bool is_edge_half_grown = cluster_edges[edge_idx] == 1;
        if (is_edge_half_grown)
        {
            cluster_nodes = activate_edge_nodes(cluster_nodes, edge_idx, edges);
        }
        return cluster_nodes;
    };

    // new_cluster_nodes = std::vector<int>(edges.size(), 0);
    for (int i = 0; i < edges.size(); i++)
    {
        new_cluster_nodes = activate_half_grown_edge_nodes(i, new_cluster_nodes);
    }

    // #3 Fully grow the edges that are half grown.
    // 0 -> 0, 1 -> 2, 2 -> 2
    for (int i = 0; i < cluster_edges.size(); i++)
    {
        cluster_edges[i] = cluster_edges[i] * (3 - cluster_edges[i]);
    }

    // #4 Grow out of the INITALLY active nodes
    for (int i = 0; i < cluster_nodes.size(); i++)
    {
        bool node_equal_1 = cluster_nodes[i] == 1;
        if (node_equal_1)
        {
            cluster_edges = grow_out_of_node(cluster_edges, i, graph_edge_idxs);
        }
    }

    return std::tie(new_cluster_nodes, cluster_edges);
}

auto grow_clusters(
    std::vector<std::vector<int>> clusters_nodes,
    std::vector<std::vector<int>> clusters_edges,
    std::vector<int> clusters_activity,
    std::vector<std::vector<int>> graph_edges_idxs,
    std::vector<std::vector<int>> edges) -> std::tuple<std::vector<std::vector<int>>, std::vector<std::vector<int>>>
{
    // Grow all clusters that are active.
    int n_clusters = clusters_nodes.size();

    auto grow_active_clusters = [=](
                                    int c_idx,
                                    std::vector<std::vector<int>> cls_nodes,
                                    std::vector<std::vector<int>> cls_edges) -> std::tuple<std::vector<std::vector<int>>,
                                                                                           std::vector<std::vector<int>>>
    {
        // Grow the cluster with index c_idx if it is active
        std::vector<int> cl_nodes = cls_nodes[c_idx];
        std::vector<int> cl_edges = cls_edges[c_idx];

        bool is_cluster_active = clusters_activity[c_idx] == 1;

        if (is_cluster_active)
        {
            std::tie(cl_nodes, cl_edges) = grow_cluster(cl_nodes, cl_edges, graph_edges_idxs, edges);
        }
        // assign new computed cl_nodes and cl_edges to the cluster at index c_idx
        cls_nodes[c_idx] = cl_nodes;
        cls_edges[c_idx] = cl_edges;
        return std::tie(cls_nodes, cls_edges);
    };

    for (int i = 0; i < n_clusters; i++)
    {
        std::tie(clusters_nodes, clusters_edges) = grow_active_clusters(i, clusters_nodes, clusters_edges);
    }
    return std::tie(clusters_nodes, clusters_edges);
}

// MERGE CLUSTERS

auto deactivate_clusters_touching_boundary(
    std::vector<std::vector<int>> clusters_nodes,
    std::vector<int> clusters_activity,
    std::vector<int> boundary_nodes) -> std::vector<int>
{
    // Deactivate clusters that touch a boundary node
    int n_clusters = clusters_nodes.size();

    auto deactivate_cluster_if_it_touches_boundary = [=](int c_idx, std::vector<int> cls_activity) -> std::vector<int>
    {
        // Deactivate cluster with c_idx if it touches a boundary node

        std::vector<int> cl_nodes = clusters_nodes[c_idx];
        bool contains_boundary_node = std::any_of(boundary_nodes.begin(), boundary_nodes.end(),
                                                  [&](int boundary_node)
                                                  { return cl_nodes[boundary_node] > 0; });
        // set cluster activity to 3 if it contains a boundary node
        if (contains_boundary_node)
        {
            cls_activity[c_idx] = 3; // Deactivate cluster
        }
        return cls_activity;
    };
    for (int i = 0; i < n_clusters; i++)
    {
        clusters_activity = deactivate_cluster_if_it_touches_boundary(i, clusters_activity);
    }
    return clusters_activity;
}

auto merge_cluster_pair(
    std::vector<std::vector<int>> clusters_nodes,
    std::vector<std::vector<int>> clusters_edges,
    std::vector<int> clusters_activity,
    std::tuple<int, int> c_idxs) -> std::tuple<std::vector<std::vector<int>>,
                                               std::vector<std::vector<int>>,
                                               std::vector<int>>
{
    // set c0 to be the sum of c0 and c1
    // If the sum is greater than 2, set it to 2 (full edge)

    // add the cluster edges of c0 and c1 and store the result in c0
    auto merge_cluster_edges_and_nodes = [](std::vector<int> &c0, std::vector<int> &c1) -> void
    {
        // Merge the edges of c0 and c1
        std::transform(
            c0.cbegin(), c0.cend(),
            c1.cbegin(),
            c0.begin(),
            [](int a, int b)
            { return std::min(a + b, 2); }); // Ensure the sum does not exceed 2

        // after merging the pair, set c1 to all zeros
        std::transform(
            c1.cbegin(), c1.cend(),
            c1.begin(),
            [](int x)
            { return 0; }); // Set all edges of c1 to 0
    };

    // process cluster edges
    auto &cluster_edges0 = clusters_edges[std::get<0>(c_idxs)];
    auto &cluster_edges1 = clusters_edges[std::get<1>(c_idxs)];
    merge_cluster_edges_and_nodes(cluster_edges0, cluster_edges1);

    // process cluster nodes
    auto &cluster_nodes0 = clusters_nodes[std::get<0>(c_idxs)];
    auto &cluster_nodes1 = clusters_nodes[std::get<1>(c_idxs)];
    merge_cluster_edges_and_nodes(cluster_nodes0, cluster_nodes1);

    // Set the activity of the new cluster as follows:
    // # (1, 1) or (2, 2) -> 2 (inactive)
    // # (1, 2) or (2, 1) -> 1 (active)
    // # (3, _) or (_, 3) -> 3 (boundary)

    int cluster0_activity = clusters_activity[std::get<0>(c_idxs)];
    int cluster1_activity = clusters_activity[std::get<1>(c_idxs)];
    int new_cluster_activity = (cluster0_activity == 3 || cluster1_activity == 3) ? 3 : 2 - int(cluster0_activity != cluster1_activity);

    // set the activity of the new cluster
    clusters_activity[std::get<0>(c_idxs)] = new_cluster_activity;

    // the second cluster is now merged, so we can set it to 0
    clusters_activity[std::get<1>(c_idxs)] = 0; // Deactivate the second cluster

    return std::tie(clusters_nodes, clusters_edges, clusters_activity);
}

auto merge_multiple_clusters(
    std::vector<std::vector<int>> clusters_nodes,
    std::vector<std::vector<int>> clusters_edges,
    std::vector<int> clusters_activity,
    std::vector<int> clusters_to_merge_bool) -> std::tuple<std::vector<std::vector<int>>,
                                                           std::vector<std::vector<int>>,
                                                           std::vector<int>>
{
    // Merge multiple clusters that share an edge or a node. The clusters that are to be
    // merged are determined by the clusters_to_merge_bool array. The clusters are merged
    // into the first cluster that is to be merged.

    // Parameters:
    // -----------
    // clusters_nodes: Array
    //     An array of shape (n_clusters, n_nodes) that represents the nodes in each
    //     cluster.
    // clusters_edges: Array
    //     An array of shape (n_clusters, n_edges) that represents the edges in each
    //     cluster.
    // clusters_activity: Array
    //     An array of shape (n_clusters) that represents the activity of each cluster.
    // clusters_to_merge_bool: Array
    //     An array of shape (n_clusters) that represents which clusters are to be merged.
    //     The values are:
    //     True -> merge, False -> do not merge
    // -----------------------------------------------------------------------------------

    // Find the first cluster to be merged
    int c_idx_to_merge_into = find_first_instance(clusters_to_merge_bool);
    // check if there are clusters to merge
    bool there_are_clusters_to_merge = c_idx_to_merge_into != -1;

    auto merge_if_conditions_are_met = [=](
                                           int c_idx,
                                           std::vector<std::vector<int>> cls_nodes,
                                           std::vector<std::vector<int>> cls_edges,
                                           std::vector<int> cls_activity) -> std::tuple<std::vector<std::vector<int>>,
                                                                                        std::vector<std::vector<int>>,
                                                                                        std::vector<int>>
    {
        bool merge_bool = clusters_to_merge_bool[c_idx] != 0;
        bool different_clusters = c_idx != c_idx_to_merge_into;

        // check conditions for merging and merge the pair
        if (merge_bool && different_clusters && there_are_clusters_to_merge)
        {
            return merge_cluster_pair(cls_nodes, cls_edges, cls_activity, std::make_tuple(c_idx_to_merge_into, c_idx));
        }
        else
        {
            // If the conditions are not met, return the clusters as they are
            return std::tie(cls_nodes, cls_edges, cls_activity);
        }
    };

    // Iterate over all clusters and merge them if the conditions are met
    for (int i = 0; i < clusters_nodes.size(); i++)
    {
        std::tie(clusters_nodes, clusters_edges, clusters_activity) = merge_if_conditions_are_met(i, clusters_nodes, clusters_edges, clusters_activity);
    }

    return std::make_tuple(clusters_nodes, clusters_edges, clusters_activity);
}

auto find_and_merge_clusters(
    std::vector<std::vector<int>> clusters_nodes,
    std::vector<std::vector<int>> clusters_edges,
    std::vector<int> clusters_activity) -> std::tuple<std::vector<std::vector<int>>,
                                                      std::vector<std::vector<int>>,
                                                      std::vector<int>>

{
    // Find and merge clusters that share an edge or a node.

    // Parameters:
    // -----------
    // clusters_nodes: Array
    //     An array of shape (n_clusters, n_nodes) that represents the nodes in each
    //     cluster. The values are:
    //     0 -> not in cluster, 1 -> active, 2 -> inactive
    // clusters_edges: Array
    //     An array of shape (n_clusters, n_edges) that represents the edges in each
    //     cluster. The values are:
    //     0 -> no edge, 1 -> half edge, 2 -> full edge
    // clusters_activity: Array
    //     An array of shape (n_clusters) that represents the activity of each cluster.
    //     The values are:
    //     0 -> non-existent cluster, 1 -> active, 2 -> inactive, 3 -> touches boundary

    // Returns:
    // --------
    // tuple[Array, Array, Array]
    //     The updated clusters_nodes, clusters_edges, clusters_activity arrays.
    // -----------------------------------------------------------------------------------

    int n_edges = clusters_edges[0].size();
    int n_nodes = clusters_nodes[0].size();

    // Merge clusters that have half an edge each

    auto merge_clusters_with_shared_edge = [=](
                                               int edge_idx,
                                               std::vector<std::vector<int>> cls_nodes,
                                               std::vector<std::vector<int>> cls_edges,
                                               std::vector<int> cls_activity) -> std::tuple<std::vector<std::vector<int>>,
                                                                                            std::vector<std::vector<int>>,
                                                                                            std::vector<int>>

    {
        // Find clusters that share an edge and merge them.
        std::vector<int> edges_across_clusters = std::vector<int>(clusters_edges.size(), 0);
        std::transform(
            cls_edges.begin(), cls_edges.end(),
            edges_across_clusters.begin(),
            [&](std::vector<int> &edge)
            { return edge[edge_idx]; }); // Get the edge at edge_idx across all clusters

        std::vector<int> clusters_to_merge_bool = std::vector<int>(clusters_edges.size(), 0);
        std::transform(
            edges_across_clusters.begin(), edges_across_clusters.end(),
            clusters_to_merge_bool.begin(),
            [](int edge_value)
            { return edge_value >= 1 ? 1 : 0; }); // Mark clusters that have a half edge

        return merge_multiple_clusters(cls_nodes, cls_edges, cls_activity, clusters_to_merge_bool);
    };

    // Iterate over all edges and merge clusters that share an edge
    for (int i = 0; i < n_edges; i++)
    {
        std::tie(clusters_nodes, clusters_edges, clusters_activity) = merge_clusters_with_shared_edge(i, clusters_nodes, clusters_edges, clusters_activity);
    }

    auto merge_clusters_with_shared_node = [=](
                                               int node_idx,
                                               std::vector<std::vector<int>> cls_nodes,
                                               std::vector<std::vector<int>> cls_edges,
                                               std::vector<int> cls_activity) -> std::tuple<std::vector<std::vector<int>>,
                                                                                            std::vector<std::vector<int>>,
                                                                                            std::vector<int>>
    {
        // Find clusters that share a node and merge them.
        std::vector<int> nodes_across_clusters = std::vector<int>(clusters_nodes.size(), 0);
        std::transform(
            cls_nodes.begin(), cls_nodes.end(),
            nodes_across_clusters.begin(),
            [&](std::vector<int> &node)
            { return node[node_idx]; }); // Get the node at node_idx across all clusters

        std::vector<int> clusters_to_merge_bool = std::vector<int>(clusters_nodes.size(), 0);
        std::transform(
            nodes_across_clusters.begin(), nodes_across_clusters.end(),
            clusters_to_merge_bool.begin(),
            [](int node_value)
            { return node_value >= 1 ? 1 : 0; }); // Mark clusters that have an active node

        return merge_multiple_clusters(cls_nodes, cls_edges, cls_activity, clusters_to_merge_bool);
    };

    // Iterate over all nodes and merge clusters that share a node
    for (int i = 0; i < n_nodes; i++)
    {
        std::tie(clusters_nodes, clusters_edges, clusters_activity) = merge_clusters_with_shared_node(i, clusters_nodes, clusters_edges, clusters_activity);
    }

    return std::tie(clusters_nodes, clusters_edges, clusters_activity);
}

// VALIDATE SYNDROME

auto syndrome_validation_cycle(
    std::vector<std::vector<int>> clusters_nodes,
    std::vector<std::vector<int>> clusters_edges,
    std::vector<int> clusters_activity,
    std::vector<std::vector<int>> graph_edges_idxs,
    std::vector<std::vector<int>> edges,
    std::vector<int> boundary_nodes) -> std::tuple<std::vector<std::vector<int>>,
                                                   std::vector<std::vector<int>>,
                                                   std::vector<int>>
{
    // Perform a single cycle of the syndrome validation. This entails growing clusters,
    // merging them and deactivating clusters touching a boundary node.

    // Parameters:
    // -----------
    // clusters_nodes: Array
    //     An array of shape (n_clusters, n_nodes) that represents the nodes in each
    //     cluster. The values are:
    //     0 -> not in cluster, 1 -> active, 2 -> inactive
    // clusters_edges: Array
    //     An array of shape (n_clusters, n_edges) that represents the edges in each
    //     cluster. The values are:
    //     0 -> no edge, 1 -> half edge, 2 -> full edge
    // clusters_activity: Array
    //     An array of shape (n_clusters) that represents the activity of each cluster.
    //     The values are:
    //     0 -> non-existent cluster, 1 -> active, 2 -> inactive, 3 -> touches boundary
    // graph_edges_idxs: Array
    //     An array of shape (n_nodes, n_nodes) where element (i, j) is the index of the
    //     edge that connects node i and node j. If there is no edge, the value is -1.
    // edges: Array
    //     An array of shape (n_edges, 2) that represents the edges in the graph. Each
    //     element is a tuple of the nodes that the edge connects.
    // boundary_nodes: Array
    //     An array of shape (n_boundary_nodes) that represents the nodes that are on the
    //     boundary of the graph.

    // Returns:
    // --------
    // tuple[Array, Array, Array]
    //     The updated clusters_nodes, clusters_edges, clusters_activity arrays.
    // -----------------------------------------------------------------------------------

    // Step 1: Grow clusters
    std::tie(clusters_nodes, clusters_edges) = grow_clusters(clusters_nodes, clusters_edges, clusters_activity, graph_edges_idxs, edges);

    // Step 2: Merge clusters
    std::tie(clusters_nodes, clusters_edges, clusters_activity) = find_and_merge_clusters(clusters_nodes, clusters_edges, clusters_activity);

    // Step 3: Deactivate clusters touching a boundary node
    clusters_activity = deactivate_clusters_touching_boundary(clusters_nodes, clusters_activity, boundary_nodes);

    return std::tie(clusters_nodes, clusters_edges, clusters_activity);
}

auto syndrome_validation(
    std::vector<int> syndrome,
    std::vector<std::vector<int>> edges,
    std::vector<int> boundary_nodes) -> std::tuple<std::vector<std::vector<int>>,
                                                   std::vector<std::vector<int>>,
                                                   std::vector<int>>
{
    // Perform the syndrome validation part of the Union-Find decoder. This entails
    // growing clusters, merging them and cycling until no more clusters can be grown.

    // A) Growing clusters:
    // 1. Deactivate all the active nodes.
    // 2. Activate non-present nodes that are on the edges that are half-grown since they
    //    will be fully grown.
    // 3. Fully grow the edges that are half grown.
    // 4. Grow out of the INITALLY active nodes

    // B) Merging clusters:
    // 1. Merge clusters that share an edge that is half-grown for both clusters or share a
    //    node.
    // 2. Calculate the resulting activity of the new cluster: The resulting cluster is
    //    active if only one of the clusters is active and the total cluster does not
    //    touch a boundary node.

    // C) Deactivate clusters touching a boundary node

    // NOTE:
    // n_clusters: The maximum number of clusters that can be grown. This is equal to the
    // size of the syndrome measurement, i.e. the number of non-boundary nodes in the
    // graph.

    // Parameters:
    // -----------
    // syndrome: Array
    //     The syndrome measurement. The values are:
    //     0 -> no error, 1 -> error
    // edges: Array
    //     An array of shape (n_edges, 2) that represents the edges in the graph. Each
    //     element is a tuple of the nodes that the edge connects.
    // boundary_nodes: Array
    //     An array of shape (n_boundary_nodes) that represents the nodes that are on the
    //     boundary of the graph.

    // Returns:
    // --------
    // tuple[Array, Array, Array]
    //     - clusters_nodes: An array of shape (n_clusters, n_nodes) that represents the
    //       nodes in each cluster. The values are:
    //     0 -> not in cluster, 1 -> active, 2 -> inactive
    //     - clusters_edges: An array of shape (n_clusters, n_edges) that represents the
    //       edges in each cluster. The values are:
    //     0 -> no edge, 1 -> half edge, 2 -> full edge
    //     - clusters_activity: An array of shape (n_clusters) that represents the activity
    //     of each cluster. The values are:
    //     0 -> non-existent cluster, 1 -> active, 2 -> inactive, 3 -> touches boundary
    // -----------------------------------------------------------------------------------

    int n_edges = edges.size();
    int n_clusters = syndrome.size();
    int n_nodes = n_clusters + boundary_nodes.size();

    // Create a graph edge index array for quick access to the edges that a node is on
    std::vector<std::vector<int>> graph_edge_idxs(n_nodes, std::vector<int>(n_nodes, -1));
    for (int i = 0; i < n_edges; i++)
    {
        int node1 = edges[i][0];
        int node2 = edges[i][1];
        graph_edge_idxs[node1][node2] = i;
        graph_edge_idxs[node2][node1] = i; // Symmetric matrix
    }

    // Initialize the clusters
    // Edges:
    // 0 -> no edge, 1 -> half edge, 2 -> full edge
    std::vector<std::vector<int>> clusters_edges(n_clusters, std::vector<int>(n_edges, 0));
    // Nodes:
    // 0 -> not in cluster, 1 -> active, 2 -> inactive
    std::vector<std::vector<int>> clusters_nodes(n_clusters, std::vector<int>(n_nodes, 0));
    // Activity:
    // 0 -> non-existent cluster, 1 -> active, 2 -> inactive, 3 -> touches boundary
    // Set initial node/cluster activity from syndrome measurement
    std::vector<int> clusters_activity = syndrome;

    for (int i = 0; i < n_clusters; i++)
    {
        clusters_nodes[i][i] = syndrome[i]; // Set the node to be active in the cluster
    }

    auto state = std::tie(
        clusters_nodes,
        clusters_edges,
        clusters_activity,
        graph_edge_idxs,
        edges,
        boundary_nodes);

    while ([&]() -> bool
           { return std::any_of(
                 std::get<2>(state).begin(), std::get<2>(state).end(),
                 [](int activity)
                 { return activity == 1; }); }())
    {
        std::tie(clusters_nodes, clusters_edges, clusters_activity) = syndrome_validation_cycle(
            std::get<0>(state),
            std::get<1>(state),
            std::get<2>(state),
            std::get<3>(state),
            std::get<4>(state),
            std::get<5>(state));

        state = std::tie(
            clusters_nodes,
            clusters_edges,
            clusters_activity,
            graph_edge_idxs,
            edges,
            boundary_nodes);
    }
    return std::tie(std::get<0>(state), std::get<1>(state), std::get<2>(state));
}

auto get_decoding_function(
    std::vector<std::vector<int>> edges,
    std::vector<int> boundary_nodes) -> std::function<std::vector<int>(std::vector<int>)>
{
    auto decode = [=](std::vector<int> syndrome) -> std::vector<int>
    {
        // Decode the syndrome using union-find algorithm
        // Parameters:
        // -----------
        // syndrome: Array
        //     An array of shape (n_clusters) that represents the activity of each cluster.
        //     The values are:
        //     0 -> non-existent cluster, 1 -> active, 2 -> inactive, 3 -> touches boundary
        // edges: Array
        //     An array of shape (n_edges, 2) that represents the edges in the graph.
        // boundary_nodes: Array
        //     An array of shape (n_boundary_nodes) that represents the nodes on the boundary.

        // 1. Grow clusters from the syndrome
        std::vector<std::vector<int>> clusters_nodes;
        std::vector<std::vector<int>> clusters_edges;
        std::vector<int> clusters_activity;

        std::tie(clusters_nodes, clusters_edges, clusters_activity) = syndrome_validation(syndrome, edges, boundary_nodes);

        // 2. Grow spanning forest and get the access of the nodes from the clusters
        std::vector<std::vector<int>> spanning_forest;
        std::vector<std::vector<int>> forest_access;
        std::tie(spanning_forest, forest_access) = grow_spanning_forest(syndrome, edges, boundary_nodes,
                                                                        clusters_nodes, clusters_edges, clusters_activity);

        // 3. Find the corrections
        std::vector<int> edge_corrections;
        edge_corrections = forest_peeling(syndrome, spanning_forest, forest_access, edges);

        return edge_corrections;
    };

    return decode;
};
