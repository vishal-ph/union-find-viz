#include "dem_parser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <map>
#include <set>

// Trim whitespace from both ends
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Parse parenthesized coordinate list: (x, y, z) or (x, y)
static Vec3 parse_coords(const std::string& s) {
    Vec3 v{0, 0, 0};
    size_t open = s.find('(');
    size_t close = s.find(')');
    if (open == std::string::npos || close == std::string::npos) return v;

    std::string inner = s.substr(open + 1, close - open - 1);
    std::istringstream iss(inner);
    std::string token;
    int i = 0;
    while (std::getline(iss, token, ',') && i < 3) {
        float val = std::stof(trim(token));
        if (i == 0) v.x = val;
        else if (i == 1) v.y = val;
        else if (i == 2) v.z = val;
        i++;
    }
    return v;
}

// Tokenize a line into words, respecting parentheses as a single token group
static std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    int paren_depth = 0;

    for (char c : line) {
        if (c == '(') {
            paren_depth++;
            current += c;
        } else if (c == ')') {
            paren_depth--;
            current += c;
        } else if ((c == ' ' || c == '\t') && paren_depth == 0) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

// Read all lines from a file, handling braces for repeat blocks
static std::vector<std::string> read_lines(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filepath << std::endl;
        return {};
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (!trimmed.empty()) {
            lines.push_back(trimmed);
        }
    }
    return lines;
}

struct ParseContext {
    int detector_offset = 0;
    Vec3 coord_offset{0, 0, 0};

    // All detectors collected
    std::vector<DetectorNode> detectors;

    // Edges: pair of (detector_indices or -1 for boundary) + probability + observables
    struct RawEdge {
        std::vector<int> detector_targets;  // absolute detector indices
        std::vector<int> observable_targets; // L# indices
        double probability;
    };
    std::vector<RawEdge> raw_edges;

    // Explicit detector coordinate map (detector_id -> coords)
    std::map<int, Vec3> detector_coords;
};

static void process_lines(ParseContext& ctx,
                           const std::vector<std::string>& lines,
                           int start, int end);

// Find the matching closing brace for a repeat block
static int find_closing_brace(const std::vector<std::string>& lines, int open_line) {
    int depth = 0;
    for (int i = open_line; i < (int)lines.size(); i++) {
        for (char c : lines[i]) {
            if (c == '{') depth++;
            if (c == '}') {
                depth--;
                if (depth == 0) return i;
            }
        }
    }
    return (int)lines.size() - 1;
}

static void process_lines(ParseContext& ctx,
                           const std::vector<std::string>& lines,
                           int start, int end) {
    for (int i = start; i < end; i++) {
        const std::string& line = lines[i];
        auto tokens = tokenize(line);
        if (tokens.empty()) continue;

        // Strip trailing comments
        std::string cmd = tokens[0];

        if (cmd == "detector" || (cmd.find("detector(") == 0)) {
            // detector(x, y, t) D#
            // Parse coordinates from the first token if it has parens
            Vec3 coords{0, 0, 0};
            std::string full_line = line;

            size_t paren_open = full_line.find('(');
            size_t paren_close = full_line.find(')');
            if (paren_open != std::string::npos && paren_close != std::string::npos) {
                coords = parse_coords(full_line);
            }

            // Find detector ID: D#
            for (auto& tok : tokens) {
                if (tok.size() > 1 && tok[0] == 'D' && std::isdigit(tok[1])) {
                    int local_id = std::stoi(tok.substr(1));
                    int abs_id = local_id + ctx.detector_offset;

                    Vec3 abs_coords;
                    abs_coords.x = coords.x + ctx.coord_offset.x;
                    abs_coords.y = coords.y + ctx.coord_offset.y;
                    abs_coords.z = coords.z + ctx.coord_offset.z;

                    ctx.detector_coords[abs_id] = abs_coords;

                    DetectorNode dn;
                    dn.id = abs_id;
                    dn.coords = abs_coords;
                    ctx.detectors.push_back(dn);
                }
            }
        } else if (cmd == "error" || cmd.find("error(") == 0) {
            // error(p) D0 D1 ^ D4 L0 ...
            // Split on ^ to get separate error segments
            double prob = 0.0;
            size_t paren_open = line.find('(');
            size_t paren_close = line.find(')');
            if (paren_open != std::string::npos && paren_close != std::string::npos) {
                prob = std::stod(line.substr(paren_open + 1, paren_close - paren_open - 1));
            }

            // Split tokens (after the command) into segments separated by ^
            std::vector<std::vector<std::string>> segments;
            segments.push_back({});
            for (size_t t = 1; t < tokens.size(); t++) {
                if (tokens[t] == "^") {
                    segments.push_back({});
                } else {
                    segments.back().push_back(tokens[t]);
                }
            }

            // Each segment becomes a separate edge
            for (auto& seg : segments) {
                ParseContext::RawEdge edge;
                edge.probability = prob;

                for (auto& tok : seg) {
                    if (tok.size() > 1 && tok[0] == 'D' && std::isdigit(tok[1])) {
                        int local_id = std::stoi(tok.substr(1));
                        edge.detector_targets.push_back(local_id + ctx.detector_offset);
                    } else if (tok.size() > 1 && tok[0] == 'L' && std::isdigit(tok[1])) {
                        edge.observable_targets.push_back(std::stoi(tok.substr(1)));
                    }
                }

                if (!edge.detector_targets.empty()) {
                    ctx.raw_edges.push_back(edge);
                }
            }
        } else if (cmd == "repeat") {
            // repeat N { ... }
            int count = std::stoi(tokens[1]);
            int block_start = i + 1; // line after '{'
            // The '{' might be on the same line as 'repeat N {'
            if (line.find('{') != std::string::npos) {
                block_start = i + 1;
            }
            int block_end = find_closing_brace(lines, i);

            // Collect the lines inside the block
            std::vector<std::string> block_lines;
            for (int j = block_start; j < block_end; j++) {
                std::string trimmed = trim(lines[j]);
                if (trimmed != "{" && trimmed != "}" && !trimmed.empty()) {
                    block_lines.push_back(trimmed);
                }
            }

            // Find shift_detectors in the block
            int shift_count = 0;
            Vec3 shift_coords{0, 0, 0};
            std::vector<std::string> non_shift_lines;
            for (auto& bl : block_lines) {
                if (bl.find("shift_detectors") == 0) {
                    // shift_detectors(dx, dy, dt) count
                    size_t po = bl.find('(');
                    size_t pc = bl.find(')');
                    if (po != std::string::npos && pc != std::string::npos) {
                        shift_coords = parse_coords(bl);
                    }
                    auto stokens = tokenize(bl);
                    if (stokens.size() > 1) {
                        shift_count = std::stoi(stokens.back());
                    }
                } else {
                    non_shift_lines.push_back(bl);
                }
            }

            // Process the block N times
            for (int rep = 0; rep < count; rep++) {
                process_lines(ctx, non_shift_lines, 0, (int)non_shift_lines.size());
                // Apply shift
                ctx.detector_offset += shift_count;
                ctx.coord_offset.x += shift_coords.x;
                ctx.coord_offset.y += shift_coords.y;
                ctx.coord_offset.z += shift_coords.z;
            }

            i = block_end; // skip past the closing brace
        } else if (cmd == "shift_detectors" || cmd.find("shift_detectors(") == 0) {
            // Top-level shift_detectors
            Vec3 shift{0, 0, 0};
            size_t po = line.find('(');
            size_t pc = line.find(')');
            if (po != std::string::npos && pc != std::string::npos) {
                shift = parse_coords(line);
            }
            auto stokens = tokenize(line);
            int count = 0;
            if (stokens.size() > 1) {
                count = std::stoi(stokens.back());
            }
            ctx.detector_offset += count;
            ctx.coord_offset.x += shift.x;
            ctx.coord_offset.y += shift.y;
            ctx.coord_offset.z += shift.z;
        }
        // Ignore other lines (comments, etc.)
    }
}

DecodingGraph3D parse_dem_file(const std::string& filepath) {
    DecodingGraph3D graph;

    auto lines = read_lines(filepath);
    if (lines.empty()) {
        std::cerr << "Warning: Empty DEM file" << std::endl;
        return graph;
    }

    ParseContext ctx;
    process_lines(ctx, lines, 0, (int)lines.size());

    // Sort detectors by id and deduplicate
    std::sort(ctx.detectors.begin(), ctx.detectors.end(),
              [](const DetectorNode& a, const DetectorNode& b) { return a.id < b.id; });
    ctx.detectors.erase(
        std::unique(ctx.detectors.begin(), ctx.detectors.end(),
                    [](const DetectorNode& a, const DetectorNode& b) { return a.id == b.id; }),
        ctx.detectors.end());

    graph.num_detectors = (int)ctx.detectors.size();

    // Classify X/Z type based on Stim coordinate parity:
    // (int(x/2) + int(y/2)) % 2 == 1 → X-type, == 0 → Z-type
    for (auto& det : ctx.detectors) {
        int ix = (int)(det.coords.x / 2.0f);
        int iy = (int)(det.coords.y / 2.0f);
        det.is_x_type = ((ix + iy) % 2 == 1);
    }

    graph.detectors = ctx.detectors;

    // Build a map from detector absolute id to index in [0, num_detectors)
    std::map<int, int> det_id_to_idx;
    for (int i = 0; i < graph.num_detectors; i++) {
        det_id_to_idx[graph.detectors[i].id] = i;
    }

    // Node positions: detectors first (indices 0..num_detectors-1)
    // Boundary nodes will be appended after
    graph.node_positions.resize(graph.num_detectors);
    for (int i = 0; i < graph.num_detectors; i++) {
        graph.node_positions[i] = graph.detectors[i].coords;
    }

    // Process raw edges to build graph edges
    // For boundary edges (1 detector target), create one boundary node per detector
    int next_boundary_idx = graph.num_detectors;

    // Deduplicate edges (same pair of nodes)
    std::set<std::pair<int, int>> seen_edges;
    // Map detector index → boundary node index (one boundary node per detector)
    std::map<int, int> det_to_boundary;

    for (auto& re : ctx.raw_edges) {
        if (re.detector_targets.size() == 2) {
            // Internal edge between two detectors
            auto it0 = det_id_to_idx.find(re.detector_targets[0]);
            auto it1 = det_id_to_idx.find(re.detector_targets[1]);
            if (it0 == det_id_to_idx.end() || it1 == det_id_to_idx.end()) continue;

            int n0 = it0->second;
            int n1 = it1->second;
            if (n0 == n1) continue; // self-loop
            auto key = std::make_pair(std::min(n0, n1), std::max(n0, n1));
            if (seen_edges.count(key)) continue;
            seen_edges.insert(key);

            GraphEdge ge;
            ge.node0 = n0;
            ge.node1 = n1;
            ge.probability = re.probability;
            ge.is_boundary = false;
            ge.observables = re.observable_targets;
            graph.graph_edges.push_back(ge);
        } else if (re.detector_targets.size() == 1) {
            // Boundary edge: one detector + boundary
            auto it = det_id_to_idx.find(re.detector_targets[0]);
            if (it == det_id_to_idx.end()) continue;

            int det_idx = it->second;

            // Only create one boundary node per detector
            if (det_to_boundary.count(det_idx)) continue;

            int boundary_idx = next_boundary_idx++;
            det_to_boundary[det_idx] = boundary_idx;

            graph.boundary_node_indices.push_back(boundary_idx);

            // Position boundary node near its detector, offset slightly
            Vec3 det_pos = graph.node_positions[det_idx];
            Vec3 bnd_pos;
            bnd_pos.x = det_pos.x;
            bnd_pos.y = det_pos.y - 0.5f;
            bnd_pos.z = det_pos.z;
            graph.node_positions.push_back(bnd_pos);

            GraphEdge ge;
            ge.node0 = det_idx;
            ge.node1 = boundary_idx;
            ge.probability = re.probability;
            ge.is_boundary = true;
            ge.observables = re.observable_targets;
            graph.graph_edges.push_back(ge);
        }
        // Skip errors with 0 detector targets
    }

    // Build UF-compatible edge list and boundary node list
    for (auto& ge : graph.graph_edges) {
        graph.uf_edges.push_back({ge.node0, ge.node1});
    }
    graph.uf_boundary_nodes = graph.boundary_node_indices;

    // Determine number of rounds from max z coordinate
    float max_z = 0;
    for (auto& det : graph.detectors) {
        max_z = std::max(max_z, det.coords.z);
    }
    graph.num_rounds = std::max(1, (int)(max_z + 1));

    // Infer lattice geometry from detector spatial coordinates
    {
        // Collect unique spatial positions
        std::set<std::pair<int,int>> spatial_set;
        float smin_x = 1e9f, smax_x = -1e9f;
        float smin_y = 1e9f, smax_y = -1e9f;
        for (auto& det : graph.detectors) {
            int sx = (int)std::round(det.coords.x);
            int sy = (int)std::round(det.coords.y);
            spatial_set.insert({sx, sy});
            smin_x = std::min(smin_x, (float)sx);
            smax_x = std::max(smax_x, (float)sx);
            smin_y = std::min(smin_y, (float)sy);
            smax_y = std::max(smax_y, (float)sy);
        }

        if (!spatial_set.empty()) {
            LatticeInfo& lat = graph.lattice;
            lat.min_x = smin_x;
            lat.max_x = smax_x;
            lat.min_y = smin_y;
            lat.max_y = smax_y;

            // Data qubits: odd integer coordinates within bounding box expanded by 1
            int bb_x0 = (int)smin_x - 1;
            int bb_x1 = (int)smax_x + 1;
            int bb_y0 = (int)smin_y - 1;
            int bb_y1 = (int)smax_y + 1;
            for (int qx = bb_x0; qx <= bb_x1; qx++) {
                for (int qy = bb_y0; qy <= bb_y1; qy++) {
                    if (qx % 2 != 0 && qy % 2 != 0) {
                        lat.data_qubits.push_back({(float)qx, (float)qy, 0.0f});
                    }
                }
            }

            // Stabilizer faces: one per unique spatial detector position
            for (auto& sp : spatial_set) {
                StabilizerFace face;
                face.center = {(float)sp.first, (float)sp.second, 0.0f};
                // Classify same way as detectors
                int ix = sp.first / 2;
                int iy = sp.second / 2;
                face.is_x_type = ((ix + iy) % 2 == 1);

                // Check if on boundary → make a half-face
                bool on_left   = (sp.first  == (int)smin_x);
                bool on_right  = (sp.first  == (int)smax_x);
                bool on_bottom = (sp.second == (int)smin_y);
                bool on_top    = (sp.second == (int)smax_y);

                face.half_w = 1.0f;
                face.half_h = 1.0f;

                // For boundary stabilizers, shrink to half-face
                if (on_left || on_right) face.half_w = 0.5f;
                if (on_bottom || on_top) face.half_h = 0.5f;

                // Shift center for boundary half-faces so they align to lattice edge
                if (on_left)   face.center.x += 0.5f;
                if (on_right)  face.center.x -= 0.5f;
                if (on_bottom) face.center.y += 0.5f;
                if (on_top)    face.center.y -= 0.5f;

                lat.faces.push_back(face);
            }

            lat.valid = true;
        }
    }

    return graph;
}

std::vector<int> load_detection_events(const std::string& filepath, int num_detectors) {
    std::vector<int> syndrome(num_detectors, 0);

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open events file " << filepath << std::endl;
        return syndrome;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;

        // Try parsing as space-separated or one-per-line
        std::istringstream iss(trimmed);
        int idx;
        while (iss >> idx) {
            if (idx >= 0 && idx < num_detectors) {
                syndrome[idx] = 1;
            }
        }
    }

    return syndrome;
}
