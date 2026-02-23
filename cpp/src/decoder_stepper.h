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

    // Phase 1: syndrome validation (sparse)
    std::vector<SparseStateMap> clusters_nodes;
    std::vector<SparseStateMap> clusters_edges;
    std::vector<int> clusters_activity;
    std::vector<int> cluster_detector_ids;  // cluster c was seeded by detector cluster_detector_ids[c]

    // Phase 2: spanning forest (sparse)
    std::vector<SparseStateMap> spanning_forest;
    std::vector<SparseStateMap> forest_access;

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
    std::vector<std::vector<std::pair<int,int>>> adj_list_; // adj_list_[node] = [(neighbor, edge_idx), ...]

    DecoderSnapshot snapshot_;
    SyndromeSubPhase syndrome_sub_phase_;

    // Peeling state (one leaf stack per tree for parallel peeling)
    std::vector<int> peel_defects_;
    std::vector<std::vector<int>> peel_leaf_stacks_;

    void init_syndrome_validation();
    void init_spanning_forest();
    void init_forest_peeling();

    bool step_syndrome_validation();
    bool step_spanning_forest();
    bool step_forest_peeling();

    bool has_active_clusters() const;
};

#endif // DECODER_STEPPER_H
