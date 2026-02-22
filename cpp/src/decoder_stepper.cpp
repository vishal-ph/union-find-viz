#include "decoder_stepper.h"
#include <algorithm>

DecoderStepper::DecoderStepper(
    const std::vector<std::vector<int>>& edges,
    const std::vector<int>& boundary_nodes,
    const std::vector<int>& syndrome)
    : boundary_nodes_(boundary_nodes), syndrome_(syndrome),
      syndrome_sub_phase_(SyndromeSubPhase::GROW),
      sf_current_cluster_(0), peel_current_tree_(0)
{
    // Convert edges from vector<vector<int>> to vector<array<int,2>>
    edges_.resize(edges.size());
    for (int i = 0; i < (int)edges.size(); i++) {
        edges_[i] = {edges[i][0], edges[i][1]};
    }

    n_edges_ = (int)edges_.size();
    n_clusters_ = (int)syndrome_.size();
    n_nodes_ = n_clusters_ + (int)boundary_nodes_.size();

    // Build adjacency matrix (used by grow_clusters)
    adjacency_.assign(n_nodes_, std::vector<int>(n_nodes_, -1));
    for (int i = 0; i < n_edges_; i++) {
        int n0 = edges_[i][0], n1 = edges_[i][1];
        adjacency_[n0][n1] = i;
        adjacency_[n1][n0] = i;
    }

    // Build adjacency list (used by build_spanning_tree)
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

    snapshot_.clusters_nodes.assign(n_clusters_, std::vector<int>(n_nodes_, 0));
    snapshot_.clusters_edges.assign(n_clusters_, std::vector<int>(n_edges_, 0));
    snapshot_.clusters_activity = syndrome_;

    for (int i = 0; i < n_clusters_; i++) {
        snapshot_.clusters_nodes[i][i] = syndrome_[i];
    }

    snapshot_.edge_corrections.assign(n_edges_, 0);

    if (!has_active_clusters()) {
        init_spanning_forest();
    }
}

void DecoderStepper::init_spanning_forest() {
    snapshot_.phase = DecoderPhase::SPANNING_FOREST;
    sf_current_cluster_ = 0;

    snapshot_.spanning_forest.assign(n_clusters_, std::vector<int>(n_nodes_, -2));
    snapshot_.forest_access.assign(n_clusters_, std::vector<int>(n_nodes_, 0));

    // Skip to first active cluster
    while (sf_current_cluster_ < n_clusters_ &&
           snapshot_.clusters_activity[sf_current_cluster_] == 0) {
        sf_current_cluster_++;
    }

    if (sf_current_cluster_ >= n_clusters_) {
        init_forest_peeling();
    }
}

void DecoderStepper::init_forest_peeling() {
    snapshot_.phase = DecoderPhase::FOREST_PEELING;
    peel_current_tree_ = 0;
    snapshot_.edge_corrections.assign(n_edges_, 0);

    // Initialize defects: syndrome extended to all nodes (boundary defects = 0)
    peel_defects_.assign(n_nodes_, 0);
    for (int i = 0; i < (int)syndrome_.size(); i++) {
        peel_defects_[i] = syndrome_[i];
    }

    // Find first non-empty tree and build its leaf stack
    advance_to_next_nonempty_tree();
}

void DecoderStepper::advance_to_next_nonempty_tree() {
    while (peel_current_tree_ < n_clusters_) {
        const auto& tree = snapshot_.spanning_forest[peel_current_tree_];
        const auto& access = snapshot_.forest_access[peel_current_tree_];
        // Build leaf stack: nodes with tree_access==1 and tree!=-1 (non-root leaves)
        peel_leaf_stack_.clear();
        for (int i = 0; i < n_nodes_; i++) {
            if (access[i] == 1 && tree[i] != -1) {
                peel_leaf_stack_.push_back(i);
            }
        }
        if (!peel_leaf_stack_.empty()) return; // found a tree with leaves

        // Also check if tree has any content at all (e.g. single root node)
        bool has_content = false;
        for (int i = 0; i < n_nodes_; i++) {
            if (tree[i] != -2) { has_content = true; break; }
        }
        if (has_content) return; // tree with root only — no peeling needed but still valid

        peel_current_tree_++;
    }

    if (peel_current_tree_ >= n_clusters_) {
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
                adjacency_,
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
    if (sf_current_cluster_ >= n_clusters_) {
        init_forest_peeling();
        return true;
    }

    build_spanning_tree(
        snapshot_.spanning_forest[sf_current_cluster_],
        snapshot_.forest_access[sf_current_cluster_],
        snapshot_.clusters_nodes[sf_current_cluster_],
        snapshot_.clusters_edges[sf_current_cluster_],
        adj_list_,
        boundary_nodes_);

    // Advance to next active cluster
    sf_current_cluster_++;
    while (sf_current_cluster_ < n_clusters_ &&
           snapshot_.clusters_activity[sf_current_cluster_] == 0) {
        sf_current_cluster_++;
    }

    if (sf_current_cluster_ >= n_clusters_) {
        init_forest_peeling();
    }
    return true;
}

bool DecoderStepper::step_forest_peeling() {
    if (peel_current_tree_ >= n_clusters_) {
        snapshot_.phase = DecoderPhase::DONE;
        return false;
    }

    // Try to peel a leaf from the current tree using the leaf stack
    bool peeled = peel_leaf(
        peel_defects_,
        snapshot_.spanning_forest[peel_current_tree_],
        snapshot_.forest_access[peel_current_tree_],
        edges_,
        snapshot_.edge_corrections,
        peel_leaf_stack_);

    if (!peeled) {
        // No more leaves — advance to next non-empty tree
        peel_current_tree_++;
        advance_to_next_nonempty_tree();

        if (peel_current_tree_ >= n_clusters_) {
            snapshot_.phase = DecoderPhase::DONE;
            return false;
        }
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
