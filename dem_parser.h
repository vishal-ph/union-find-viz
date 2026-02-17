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
};

DecodingGraph3D parse_dem_file(const std::string& filepath);
std::vector<int> load_detection_events(const std::string& filepath, int num_detectors);

#endif // DEM_PARSER_H
