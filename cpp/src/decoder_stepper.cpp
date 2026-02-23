#include "decoder_stepper.h"
#include <algorithm>

DecoderStepper::DecoderStepper(
    const std::vector<std::vector<int>>& edges,
    const std::vector<int>& boundary_nodes,
    const std::vector<int>& syndrome)
    : boundary_nodes_(boundary_nodes), syndrome_(syndrome),
      syndrome_sub_phase_(SyndromeSubPhase::GROW)
{
    // Convert edges from vector<vector<int>> to vector<array<int,2>>
    edges_.resize(edges.size());
    for (int i = 0; i < (int)edges.size(); i++) {
        edges_[i] = {edges[i][0], edges[i][1]};
    }

    n_edges_ = (int)edges_.size();
    n_nodes_ = (int)syndrome_.size() + (int)boundary_nodes_.size();

    // Build adjacency list (used by grow_clusters and build_spanning_tree)
    adj_list_.resize(n_nodes_);
    for (int i = 0; i < n_edges_; i++) {
        int n0 = edges_[i][0], n1 = edges_[i][1];
        adj_list_[n0].emplace_back(n1, i);
        adj_list_[n1].emplace_back(n0, i);
    }

    init_syndrome_validation();
}

void DecoderStepper::init_syndrome_validation() {
    snapshot_.phase = DecoderPhase::SYNDROME_VALIDATION;
    snapshot_.cycle_number = 0;
    syndrome_sub_phase_ = SyndromeSubPhase::GROW;
    snapshot_.syndrome_sub_phase = SyndromeSubPhase::GROW;

    // Only create clusters for fired detectors
    std::vector<int> fired;
    for (int i = 0; i < (int)syndrome_.size(); i++) {
        if (syndrome_[i] == 1) fired.push_back(i);
    }

    n_clusters_ = (int)fired.size();
    snapshot_.cluster_detector_ids = fired;

    snapshot_.clusters_nodes.assign(n_clusters_, SparseStateMap());
    snapshot_.clusters_edges.assign(n_clusters_, SparseStateMap());
    snapshot_.clusters_activity.assign(n_clusters_, 1); // all active

    // Each cluster c starts with one entry: the fired detector
    for (int c = 0; c < n_clusters_; c++) {
        snapshot_.clusters_nodes[c][fired[c]] = 1;
    }

    snapshot_.edge_corrections.assign(n_edges_, 0);

    if (!has_active_clusters()) {
        init_spanning_forest();
    }
}

void DecoderStepper::init_spanning_forest() {
    snapshot_.phase = DecoderPhase::SPANNING_FOREST;

    snapshot_.spanning_forest.assign(n_clusters_, SparseStateMap());
    snapshot_.forest_access.assign(n_clusters_, SparseStateMap());

    // If no active clusters, skip directly to peeling
    bool any_active = false;
    for (int c = 0; c < n_clusters_; c++) {
        if (snapshot_.clusters_activity[c] != 0) { any_active = true; break; }
    }
    if (!any_active) {
        init_forest_peeling();
    }
}

void DecoderStepper::init_forest_peeling() {
    snapshot_.phase = DecoderPhase::FOREST_PEELING;
    snapshot_.edge_corrections.assign(n_edges_, 0);

    // Initialize defects: syndrome extended to all nodes (boundary defects = 0)
    peel_defects_.assign(n_nodes_, 0);
    for (int i = 0; i < (int)syndrome_.size(); i++) {
        peel_defects_[i] = syndrome_[i];
    }

    // Build leaf stacks for ALL trees at once (parallel peeling)
    peel_leaf_stacks_.resize(n_clusters_);
    for (int t = 0; t < n_clusters_; t++) {
        peel_leaf_stacks_[t].clear();
        const auto& tree = snapshot_.spanning_forest[t];
        const auto& access = snapshot_.forest_access[t];
        for (auto& [node, edge_idx] : tree) {
            auto ait = access.find(node);
            int acc = (ait != access.end()) ? ait->second : 0;
            if (acc == 1 && edge_idx != -1) {
                peel_leaf_stacks_[t].push_back(node);
            }
        }
    }

    // If no trees have leaves to peel, we're done
    bool any_leaves = false;
    for (auto& stack : peel_leaf_stacks_) {
        if (!stack.empty()) { any_leaves = true; break; }
    }
    if (!any_leaves) {
        snapshot_.phase = DecoderPhase::DONE;
    }
}

bool DecoderStepper::has_active_clusters() const {
    return std::any_of(
        snapshot_.clusters_activity.begin(),
        snapshot_.clusters_activity.end(),
        [](int a) { return a == 1; });
}

bool DecoderStepper::step() {
    switch (snapshot_.phase) {
        case DecoderPhase::SYNDROME_VALIDATION:
            return step_syndrome_validation();
        case DecoderPhase::SPANNING_FOREST:
            return step_spanning_forest();
        case DecoderPhase::FOREST_PEELING:
            return step_forest_peeling();
        case DecoderPhase::DONE:
        case DecoderPhase::IDLE:
            return false;
    }
    return false;
}

bool DecoderStepper::step_syndrome_validation() {
    switch (syndrome_sub_phase_) {
        case SyndromeSubPhase::GROW: {
            grow_clusters(
                snapshot_.clusters_nodes,
                snapshot_.clusters_edges,
                snapshot_.clusters_activity,
                adj_list_,
                edges_);
            syndrome_sub_phase_ = SyndromeSubPhase::MERGE;
            snapshot_.syndrome_sub_phase = SyndromeSubPhase::MERGE;
            break;
        }
        case SyndromeSubPhase::MERGE: {
            find_and_merge_clusters(
                snapshot_.clusters_nodes,
                snapshot_.clusters_edges,
                snapshot_.clusters_activity);
            syndrome_sub_phase_ = SyndromeSubPhase::DEACTIVATE;
            snapshot_.syndrome_sub_phase = SyndromeSubPhase::DEACTIVATE;
            break;
        }
        case SyndromeSubPhase::DEACTIVATE: {
            deactivate_clusters_touching_boundary(
                snapshot_.clusters_nodes,
                snapshot_.clusters_activity,
                boundary_nodes_);
            snapshot_.cycle_number++;
            syndrome_sub_phase_ = SyndromeSubPhase::GROW;
            snapshot_.syndrome_sub_phase = SyndromeSubPhase::GROW;

            if (!has_active_clusters()) {
                init_spanning_forest();
            }
            break;
        }
    }
    return true;
}

bool DecoderStepper::step_spanning_forest() {
    // Build ALL spanning trees in one step (independent per cluster)
    for (int c = 0; c < n_clusters_; c++) {
        if (snapshot_.clusters_activity[c] == 0) continue;
        build_spanning_tree(
            snapshot_.spanning_forest[c],
            snapshot_.forest_access[c],
            snapshot_.clusters_nodes[c],
            snapshot_.clusters_edges[c],
            adj_list_,
            boundary_nodes_);
    }
    init_forest_peeling();
    return true;
}

bool DecoderStepper::step_forest_peeling() {
    // Peel one leaf from EACH tree in parallel
    bool any_peeled = false;
    for (int t = 0; t < n_clusters_; t++) {
        bool peeled = peel_leaf(
            peel_defects_,
            snapshot_.spanning_forest[t],
            snapshot_.forest_access[t],
            edges_,
            snapshot_.edge_corrections,
            peel_leaf_stacks_[t]);
        if (peeled) any_peeled = true;
    }

    if (!any_peeled) {
        snapshot_.phase = DecoderPhase::DONE;
        return false;
    }
    return true;
}

void DecoderStepper::run_to_completion() {
    while (step()) {}
}

void DecoderStepper::reset(const std::vector<int>& syndrome) {
    syndrome_ = syndrome;
    init_syndrome_validation();
}
