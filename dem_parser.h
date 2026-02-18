#ifndef DEM_PARSER_H
#define DEM_PARSER_H

#include <string>
#include <vector>

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

struct DetectorNode {
    int id;
    Vec3 coords;
    bool is_x_type = false;  // X-type vs Z-type stabilizer
};

struct StabilizerFace {
    Vec3 center;           // spatial position (x, y) from DEM coords
    float half_w = 1.0f;   // half-width in x
    float half_h = 1.0f;   // half-height in y
    bool is_x_type = false;
};

struct LatticeInfo {
    std::vector<Vec3> data_qubits;           // positions at odd integer coords
    std::vector<StabilizerFace> faces;       // stabilizer face rectangles
    float min_x = 0, max_x = 0;
    float min_y = 0, max_y = 0;
    bool valid = false;
};

struct GraphEdge {
    int node0, node1;       // indices into all nodes (detectors + boundary)
    double probability;
    bool is_boundary;       // true if one endpoint is a boundary node
    std::vector<int> observables;
};

struct DecodingGraph3D {
    int num_detectors = 0;
    std::vector<DetectorNode> detectors;
    std::vector<GraphEdge> graph_edges;
    std::vector<int> boundary_node_indices; // indices of boundary nodes in node_positions

    // For the UF decoder
    std::vector<std::vector<int>> uf_edges;     // [[n0,n1], ...] format
    std::vector<int> uf_boundary_nodes;

    // 3D layout
    std::vector<Vec3> node_positions;   // all nodes (detectors first, then boundary)
    int num_rounds = 1;

    // Lattice geometry inferred from detector coordinates
    LatticeInfo lattice;
};

DecodingGraph3D parse_dem_file(const std::string& filepath);
std::vector<int> load_detection_events(const std::string& filepath, int num_detectors);

#endif // DEM_PARSER_H
