#ifndef SURFACE_CODE_H
#define SURFACE_CODE_H

#include <vector>
#include <utility>

enum class ErrorType { NONE, X_ERROR, Z_ERROR };

struct DataQubit {
    int index;
    int row, col;
    ErrorType error = ErrorType::NONE;
    float rx = 0, ry = 0;
};

struct Stabilizer {
    int index;
    bool is_x_type;
    std::vector<int> data_qubits;
    int measurement = 0;
    float rx = 0, ry = 0;
};

struct DecodingGraph {
    int num_syndrome_nodes = 0;
    std::vector<int> syndrome_to_stabilizer;
    std::vector<std::vector<int>> edges;
    std::vector<int> edge_to_data_qubit;
    std::vector<int> boundary_nodes;
    // Render positions for all nodes (syndrome nodes + boundary nodes)
    std::vector<std::pair<float, float>> node_positions;
};

struct SurfaceCode {
    int distance = 0;
    std::vector<DataQubit> data_qubits;
    std::vector<Stabilizer> x_stabilizers;
    std::vector<Stabilizer> z_stabilizers;
    DecodingGraph x_decoding_graph;
    DecodingGraph z_decoding_graph;
};

SurfaceCode build_surface_code(int distance);
void compute_syndromes(SurfaceCode& sc);
std::vector<int> get_syndrome_vector(const SurfaceCode& sc, const DecodingGraph& dg, bool is_x_type);
void compute_layout(SurfaceCode& sc, float origin_x, float origin_y, float cell_size);

#endif // SURFACE_CODE_H
