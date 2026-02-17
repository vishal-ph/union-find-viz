#include "decoder_stepper.h"
#include <algorithm>

DecoderStepper::DecoderStepper(
    const std::vector<std::vector<int>>& edges,
    const std::vector<int>& boundary_nodes,
    const std::vector<int>& syndrome)
    : edges_(edges), boundary_nodes_(boundary_nodes), syndrome_(syndrome),
      sf_current_cluster_(0), peel_current_tree_(0), peel_state_(nullptr)
{
    n_edges_ = (int)edges_.size();
    n_clusters_ = (int)syndrome_.size();
    n_nodes_ = n_clusters_ + (int)boundary_nodes_.size();

    // Build graph edge index adjacency matrix
    graph_edge_idxs_.assign(n_nodes_, std::vector<int>(n_nodes_, -1));
    for (int i = 0; i < n_edges_; i++) {
        int n0 = edges_[i][0], n1 = edges_[i][1];
        graph_edge_idxs_[n0][n1] = i;
        graph_edge_idxs_[n1][n0] = i;
    }

    init_syndrome_validation();
}

void DecoderStepper::init_syndrome_validation() {
    snapshot_.phase = DecoderPhase::SYNDROME_VALIDATION;
    snapshot_.cycle_number = 0;

    snapshot_.clusters_nodes.assign(n_clusters_, std::vector<int>(n_nodes_, 0));
    snapshot_.clusters_edges.assign(n_clusters_, std::vector<int>(n_edges_, 0));
    snapshot_.clusters_activity = syndrome_;

    for (int i = 0; i < n_clusters_; i++) {
        snapshot_.clusters_nodes[i][i] = syndrome_[i];
    }

    snapshot_.edge_corrections.assign(n_edges_, 0);

    // If no active clusters, skip directly to spanning forest
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

    if (peel_state_) {
        delete peel_state_;
        peel_state_ = nullptr;
    }

    // Find first non-empty tree
    while (peel_current_tree_ < n_clusters_) {
        bool has_content = std::any_of(
            snapshot_.spanning_forest[peel_current_tree_].begin(),
            snapshot_.spanning_forest[peel_current_tree_].end(),
            [](int v) { return v != -2; });
        if (has_content) break;
        peel_current_tree_++;
    }

    if (peel_current_tree_ >= n_clusters_) {
        snapshot_.phase = DecoderPhase::DONE;
    } else {
        // Initialize peeling state for current tree
        // Extend defects to cover all nodes (syndrome + boundary) with boundary = 0
        std::vector<int> defects(n_nodes_, 0);
        for (int i = 0; i < (int)syndrome_.size(); i++) {
            defects[i] = syndrome_[i];
        }
        peel_state_ = new State(
            defects,
            snapshot_.spanning_forest[peel_current_tree_],
            snapshot_.forest_access[peel_current_tree_],
            edges_,
            snapshot_.edge_corrections
        );
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
    // Run one grow-merge-deactivate cycle
    std::tie(snapshot_.clusters_nodes,
             snapshot_.clusters_edges,
             snapshot_.clusters_activity) =
        syndrome_validation_cycle(
            snapshot_.clusters_nodes,
            snapshot_.clusters_edges,
            snapshot_.clusters_activity,
            graph_edge_idxs_,
            edges_,
            boundary_nodes_);

    snapshot_.cycle_number++;

    if (!has_active_clusters()) {
        init_spanning_forest();
    }
    return true;
}

bool DecoderStepper::step_spanning_forest() {
    if (sf_current_cluster_ >= n_clusters_) {
        init_forest_peeling();
        return true;
    }

    // Grow tree from this cluster
    std::tie(snapshot_.spanning_forest[sf_current_cluster_],
             snapshot_.forest_access[sf_current_cluster_]) =
        grow_tree_from_cluster(
            snapshot_.spanning_forest[sf_current_cluster_],
            snapshot_.forest_access[sf_current_cluster_],
            snapshot_.clusters_nodes[sf_current_cluster_],
            snapshot_.clusters_edges[sf_current_cluster_],
            edges_,
            graph_edge_idxs_,
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
    if (!peel_state_ || peel_current_tree_ >= n_clusters_) {
        snapshot_.phase = DecoderPhase::DONE;
        return false;
    }

    // Check if current tree has leaves to peel
    bool has_leaf = false;
    for (int i = 0; i < (int)peel_state_->tree_access.size(); i++) {
        if (peel_state_->tree_access[i] == 1 && peel_state_->tree[i] != -1) {
            has_leaf = true;
            break;
        }
    }

    if (has_leaf) {
        *peel_state_ = tree_peeling_step(*peel_state_);
        snapshot_.edge_corrections = peel_state_->edge_corrections;
    } else {
        // Move to next non-empty tree
        delete peel_state_;
        peel_state_ = nullptr;
        peel_current_tree_++;

        while (peel_current_tree_ < n_clusters_) {
            bool has_content = std::any_of(
                snapshot_.spanning_forest[peel_current_tree_].begin(),
                snapshot_.spanning_forest[peel_current_tree_].end(),
                [](int v) { return v != -2; });
            if (has_content) break;
            peel_current_tree_++;
        }

        if (peel_current_tree_ >= n_clusters_) {
            snapshot_.phase = DecoderPhase::DONE;
            return false;
        }

        // Initialize peeling for next tree with properly sized defects
        std::vector<int> defects(n_nodes_, 0);
        for (int i = 0; i < (int)syndrome_.size(); i++) {
            defects[i] = syndrome_[i];
        }
        peel_state_ = new State(
            defects,
            snapshot_.spanning_forest[peel_current_tree_],
            snapshot_.forest_access[peel_current_tree_],
            edges_,
            snapshot_.edge_corrections
        );
    }

    return true;
}

void DecoderStepper::run_to_completion() {
    while (step()) {}
}

void DecoderStepper::reset(const std::vector<int>& syndrome) {
    syndrome_ = syndrome;
    if (peel_state_) {
        delete peel_state_;
        peel_state_ = nullptr;
    }
    init_syndrome_validation();
}
