// UNION_FIND_H
/*
 * Copyright (c) Entropica Labs Pte Ltd 2025.
 *
 * Use, distribution and reproduction of this program in its source or compiled form is prohibited
 * without the express written consent of Entropica Labs Pte Ltd.
 *
 */

#ifndef UNION_FIND_H
#define UNION_FIND_H
#include <vector>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <functional>

auto get_decoding_function(
    std::vector<std::vector<int>> edges,
    std::vector<int> boundary_nodes) -> std::function<std::vector<int>(std::vector<int>)>;

#endif // UNION_FIND_H

#ifndef SYNDROME_VALIDATION_H
#define SYNDROME_VALIDATION_H
#include <vector>

// GROW CLUSTERS

// return cluster_nodes
auto activate_edge_nodes(
    std::vector<int> cluster_nodes,
    int edge_idx,
    std::vector<std::vector<int>> edges) -> std::vector<int>;

auto grow_out_of_node(
    std::vector<int> cluster_edges,
    int node,
    std::vector<std::vector<int>> graph_edge_idxs) -> std::vector<int>;

auto grow_cluster(
    std::vector<int> cluster_nodes,
    std::vector<std::vector<int>> cluster_edges,
    std::vector<std::vector<int>> graph_edge_idxs,
    std::vector<std::vector<int>> edges) -> std::tuple<std::vector<int>, std::vector<int>>;

auto grow_clusters(
    std::vector<std::vector<int>> clusters_nodes,
    std::vector<std::vector<int>> clusters_edges,
    std::vector<int> clusters_activity,
    std::vector<std::vector<int>> graph_edges_idxs,
    std::vector<std::vector<int>> edges) -> std::tuple<std::vector<std::vector<int>>, std::vector<std::vector<int>>>;

// MERGE CLUSTERS

auto deactivate_clusters_touching_boundary(
    std::vector<std::vector<int>> clusters_nodes,
    std::vector<int> clusters_activity,
    std::vector<int> boundary_nodes) -> std::vector<int>;

auto merge_cluster_pair(
    std::vector<std::vector<int>> clusters_nodes,
    std::vector<std::vector<int>> clusters_edges,
    std::vector<int> clusters_activity,
    std::tuple<int, int> c_idxs) -> std::tuple<std::vector<std::vector<int>>,
                                               std::vector<std::vector<int>>,
                                               std::vector<int>>;

auto merge_multiple_clusters(
    std::vector<std::vector<int>> clusters_nodes,
    std::vector<std::vector<int>> clusters_edges,
    std::vector<int> clusters_activity,
    std::vector<int> clusters_to_merge_bool) -> std::tuple<std::vector<std::vector<int>>,
                                                           std::vector<std::vector<int>>,
                                                           std::vector<int>>;

auto find_and_merge_clusters(
    std::vector<std::vector<int>> clusters_nodes,
    std::vector<std::vector<int>> clusters_edges,
    std::vector<int> clusters_activity) -> std::tuple<std::vector<std::vector<int>>,
                                                      std::vector<std::vector<int>>,
                                                      std::vector<int>>;

// VALIDATE SYNDROME

auto syndrome_validation_cycle(
    std::vector<std::vector<int>> clusters_nodes,
    std::vector<std::vector<int>> clusters_edges,
    std::vector<int> clusters_activity,
    std::vector<std::vector<int>> graph_edges_idxs,
    std::vector<std::vector<int>> edges,
    std::vector<int> boundary_nodes) -> std::tuple<std::vector<std::vector<int>>,
                                                   std::vector<std::vector<int>>,
                                                   std::vector<int>>;

auto syndrome_validation(
    std::vector<int> syndrome,
    std::vector<std::vector<int>> edges,
    std::vector<int> boundary_nodes) -> std::tuple<std::vector<std::vector<int>>, std::vector<std::vector<int>>, std::vector<int>>;

#endif // SYNDROME_VALIDATION_H

#ifndef UTILS_H // ensure header file is not already defined
#define UTILS_H // define the header file

#include <vector>
#include <functional>

// Declaration (prototype) of the function:
int find_first_instance(
    std::vector<int> arr,
    std::function<bool(int)> condition = [](int x)
    { return x != 0; });

int get_other_node_from_edge(
    std::vector<int> edge,
    int node);

#endif // UTILS_H

#ifndef SPANNING_FOREST_GROWTH_H
#define SPANNING_FOREST_GROWTH_H

#include <vector>

// Declaration (prototype) of the function:

auto find_root_node(
    std::vector<int> cluster_nodes,
    std::vector<int> boundary_nodes) -> int;

auto grow_out_from_node(
    int node,
    std::vector<int> tree,
    std::vector<int> tree_access,
    std::vector<int> nodes_to_grow,
    std::vector<std::vector<int>> graph_edge_idxs,
    std::vector<int> cluster_edges,
    std::vector<int> cluster_nodes) -> std::tuple<std::vector<int>, std::vector<int>, std::vector<int>>;

auto grow_tree_step(
    std::vector<int> tree,
    std::vector<int> tree_access,
    std::vector<int> nodes_to_grow,
    std::vector<std::vector<int>> edges,
    std::vector<std::vector<int>> graph_edge_idxs,
    std::vector<int> cluster_edges,
    std::vector<int> cluster_nodes) -> std::tuple<std::vector<int>,
                                                  std::vector<int>,
                                                  std::vector<int>,
                                                  std::vector<std::vector<int>>,
                                                  std::vector<std::vector<int>>,
                                                  std::vector<int>,
                                                  std::vector<int>>;

auto grow_tree_from_cluster(
    std::vector<int> tree,
    std::vector<int> tree_access,
    std::vector<int> cluster_nodes,
    std::vector<int> cluster_edges,
    std::vector<std::vector<int>> edges,
    std::vector<std::vector<int>> graph_edge_idxs,
    std::vector<int> boundary_nodes) -> std::tuple<std::vector<int>, std::vector<int>>;

auto grow_spanning_forest(
    std::vector<int> syndrome,
    std::vector<std::vector<int>> edges,
    std::vector<int> boundary_nodes,
    std::vector<std::vector<int>> clusters_nodes,
    std::vector<std::vector<int>> clusters_edges,
    std::vector<int> clusters_activity) -> std::tuple<std::vector<std::vector<int>>,
                                                      std::vector<std::vector<int>>>;

#endif // SPANNING_FOREST_GROWTH_H

#ifndef PEELING_H // ensure header file is not already defined
#define PEELING_H // define the header file

#include <vector>

// Declaration (prototype) of the function:
struct State
{
    std::vector<int> defects;
    std::vector<int> tree;
    std::vector<int> tree_access;
    std::vector<std::vector<int>> edges;
    std::vector<int> edge_corrections;

    // 1.1 Constructor for the State struct
    State(const std::vector<int> &defects_,
          const std::vector<int> &tree_,
          const std::vector<int> &tree_access_,
          const std::vector<std::vector<int>> &edges_,
          const std::vector<int> &edge_corrections_)
        : defects(defects_),
          tree(tree_),
          tree_access(tree_access_),
          edges(edges_),
          edge_corrections(edge_corrections_)
    {
        // Body can be empty: all work is done in the initializer list.
    }
};

auto tree_peeling_step(State state) -> State;

auto tree_peeling(
    std::vector<int> syndrome,
    std::vector<int> tree,
    std::vector<int> tree_access,
    std::vector<int> edge_corrections,
    std::vector<std::vector<int>> edges) -> std::vector<int>;

auto forest_peeling(
    std::vector<int> syndrome,
    std::vector<std::vector<int>> spanning_forest,
    std::vector<std::vector<int>> forest_access,
    std::vector<std::vector<int>> edges) -> std::vector<int>;

// auto peel_non_empty_trees = [&](int &tree_idx, std::vector<int> &edge_corrs) -> std::vector<int>;

#endif // PEELING_H
