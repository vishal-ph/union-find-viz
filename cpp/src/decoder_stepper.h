#ifndef DECODER_STEPPER_H
#define DECODER_STEPPER_H

#include <vector>
#include <array>
#include <utility>
#include "union_find.h"

enum class DecoderPhase {
    IDLE,
    SYNDROME_VALIDATION,
    SPANNING_FOREST,
    FOREST_PEELING,
    DONE
};

enum class SyndromeSubPhase {
    GROW,
    MERGE,
    DEACTIVATE
};

struct DecoderSnapshot {
    DecoderPhase phase = DecoderPhase::IDLE;
    SyndromeSubPhase syndrome_sub_phase = SyndromeSubPhase::GROW;
    int cycle_number = 0;

    // Phase 1: syndrome validation
    std::vector<std::vector<int>> clusters_nodes;
    std::vector<std::vector<int>> clusters_edges;
    std::vector<int> clusters_activity;

    // Phase 2: spanning forest
    std::vector<std::vector<int>> spanning_forest;
    std::vector<std::vector<int>> forest_access;

    // Phase 3: peeling
    std::vector<int> edge_corrections;
};

class DecoderStepper {
public:
    DecoderStepper(
        const std::vector<std::vector<int>>& edges,
        const std::vector<int>& boundary_nodes,
        const std::vector<int>& syndrome
    );

    const DecoderSnapshot& current_snapshot() const { return snapshot_; }
    bool step();
    void run_to_completion();
    void reset(const std::vector<int>& syndrome);
    DecoderPhase current_phase() const { return snapshot_.phase; }
    bool is_done() const { return snapshot_.phase == DecoderPhase::DONE; }
    std::vector<int> get_corrections() const { return snapshot_.edge_corrections; }

private:
    std::vector<std::array<int,2>> edges_;
    std::vector<int> boundary_nodes_;
    std::vector<int> syndrome_;
    int n_nodes_;
    int n_edges_;
    int n_clusters_;
    std::vector<std::vector<int>> adjacency_;
    std::vector<std::vector<std::pair<int,int>>> adj_list_; // adj_list_[node] = [(neighbor, edge_idx), ...]

    DecoderSnapshot snapshot_;
    SyndromeSubPhase syndrome_sub_phase_;

    // Spanning forest construction state
    int sf_current_cluster_;

    // Peeling state
    int peel_current_tree_;
    std::vector<int> peel_defects_;
    std::vector<int> peel_leaf_stack_;

    void init_syndrome_validation();
    void init_spanning_forest();
    void init_forest_peeling();

    bool step_syndrome_validation();
    bool step_spanning_forest();
    bool step_forest_peeling();

    bool has_active_clusters() const;
    void advance_to_next_nonempty_tree();
};

#endif // DECODER_STEPPER_H
