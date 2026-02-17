#include "surface_code.h"
#include <cmath>
#include <map>
#include <algorithm>

// Helper: data qubit index from row, col
static int dq_index(int row, int col, int d) {
    return row * d + col;
}

// Build the rotated surface code of distance d.
//
// Layout: d x d data qubits on a grid. Between them, (d-1) x (d-1) interior
// faces form a checkerboard of X and Z stabilizers. Boundary half-faces on
// two sides per stabilizer type complete the code.
//
// Convention (for odd d):
//   - Interior face at (r, c) with r in [0, d-2], c in [0, d-2]:
//       if (r + c) % 2 == 0: Z-stabilizer (detects X errors)
//       if (r + c) % 2 == 1: X-stabilizer (detects Z errors)
//   - Top/bottom boundary half-faces: Z-type
//   - Left/right boundary half-faces: X-type
//
// Each Z-stabilizer participates in the Z-decoding graph (corrects X errors).
// Each X-stabilizer participates in the X-decoding graph (corrects Z errors).

SurfaceCode build_surface_code(int d) {
    SurfaceCode sc;
    sc.distance = d;

    // Create data qubits
    sc.data_qubits.resize(d * d);
    for (int r = 0; r < d; r++) {
        for (int c = 0; c < d; c++) {
            int idx = dq_index(r, c, d);
            sc.data_qubits[idx].index = idx;
            sc.data_qubits[idx].row = r;
            sc.data_qubits[idx].col = c;
        }
    }

    // Build stabilizers
    // Interior faces: (d-1) x (d-1)
    for (int r = 0; r < d - 1; r++) {
        for (int c = 0; c < d - 1; c++) {
            Stabilizer stab;
            stab.is_x_type = ((r + c) % 2 == 1);
            stab.data_qubits = {
                dq_index(r, c, d),
                dq_index(r, c + 1, d),
                dq_index(r + 1, c, d),
                dq_index(r + 1, c + 1, d)
            };
            if (stab.is_x_type) {
                stab.index = (int)sc.x_stabilizers.size();
                sc.x_stabilizers.push_back(stab);
            } else {
                stab.index = (int)sc.z_stabilizers.size();
                sc.z_stabilizers.push_back(stab);
            }
        }
    }

    // Boundary half-faces
    // Top boundary (row = -0.5): Z-stabilizers acting on pairs along top edge
    // These cover data qubits (0, c) and (0, c+1) where the interior face
    // above would be Z-type. Top boundary faces exist where the top row is
    // missing a Z-face, i.e., at columns c where (0 + c) % 2 == 0 would be
    // Z-type but since r=-1 there's no interior face. We need the boundary
    // Z-faces at positions where the first interior row has X-type.
    // Top: for c where (0 + c) % 2 == 1 (the interior face at (0,c) is X-type,
    //       so the boundary face above column c is Z-type)
    // Actually, the correct way: boundary stabilizers exist to "complete" the code.
    // For the rotated surface code:
    // - Z boundary stabilizers on top and bottom edges at odd columns
    // - Z boundary stabilizers on top row at even columns (depending on convention)
    //
    // Simpler approach: For each pair of adjacent data qubits along each boundary
    // edge, if they don't already share a stabilizer of the boundary's type, add one.

    // Top boundary: Z-type half-faces
    for (int c = 0; c < d - 1; c++) {
        // Check if the interior face at (0, c) is NOT Z-type
        if ((0 + c) % 2 != 0) {
            // No Z-stabilizer covers (0,c)-(0,c+1) from the interior
            Stabilizer stab;
            stab.is_x_type = false; // Z-type
            stab.data_qubits = {dq_index(0, c, d), dq_index(0, c + 1, d)};
            stab.index = (int)sc.z_stabilizers.size();
            sc.z_stabilizers.push_back(stab);
        }
    }

    // Bottom boundary: Z-type half-faces
    for (int c = 0; c < d - 1; c++) {
        if ((d - 2 + c) % 2 != 0) {
            // The interior face at (d-2, c) is X-type, so bottom boundary needs Z
            Stabilizer stab;
            stab.is_x_type = false;
            stab.data_qubits = {dq_index(d - 1, c, d), dq_index(d - 1, c + 1, d)};
            stab.index = (int)sc.z_stabilizers.size();
            sc.z_stabilizers.push_back(stab);
        }
    }

    // Left boundary: X-type half-faces
    for (int r = 0; r < d - 1; r++) {
        if ((r + 0) % 2 != 1) {
            // The interior face at (r, 0) is Z-type, so left boundary needs X
            Stabilizer stab;
            stab.is_x_type = true;
            stab.data_qubits = {dq_index(r, 0, d), dq_index(r + 1, 0, d)};
            stab.index = (int)sc.x_stabilizers.size();
            sc.x_stabilizers.push_back(stab);
        }
    }

    // Right boundary: X-type half-faces
    for (int r = 0; r < d - 1; r++) {
        if ((r + d - 2) % 2 != 1) {
            // The interior face at (r, d-2) is Z-type, so right boundary needs X
            Stabilizer stab;
            stab.is_x_type = true;
            stab.data_qubits = {dq_index(r, d - 1, d), dq_index(r + 1, d - 1, d)};
            stab.index = (int)sc.x_stabilizers.size();
            sc.x_stabilizers.push_back(stab);
        }
    }

    // Re-index stabilizers
    for (int i = 0; i < (int)sc.x_stabilizers.size(); i++)
        sc.x_stabilizers[i].index = i;
    for (int i = 0; i < (int)sc.z_stabilizers.size(); i++)
        sc.z_stabilizers[i].index = i;

    // Build decoding graphs
    // For Z-decoding graph (corrects X errors): nodes = Z-stabilizers + boundary nodes
    // For each data qubit, find which Z-stabilizers it belongs to:
    //   - If 2: add edge between those two syndrome nodes
    //   - If 1: add edge from that syndrome node to a boundary node

    auto build_decoding_graph = [&](const std::vector<Stabilizer>& stabs) -> DecodingGraph {
        DecodingGraph dg;
        dg.num_syndrome_nodes = (int)stabs.size();

        // Map data qubit index -> list of stabilizer indices that contain it
        std::map<int, std::vector<int>> dq_to_stabs;
        for (int s = 0; s < (int)stabs.size(); s++) {
            for (int dq : stabs[s].data_qubits) {
                dq_to_stabs[dq].push_back(s);
            }
        }

        // Create one virtual boundary node per boundary edge to avoid parallel
        // edges. The UF decoder uses an adjacency matrix which can't represent
        // multigraphs, so each boundary data qubit needs its own boundary node.
        int next_boundary_id = dg.num_syndrome_nodes;

        for (auto& [dq_idx, stab_list] : dq_to_stabs) {
            if ((int)stab_list.size() == 2) {
                // Internal edge between two stabilizers
                dg.edges.push_back({stab_list[0], stab_list[1]});
                dg.edge_to_data_qubit.push_back(dq_idx);
            } else if ((int)stab_list.size() == 1) {
                // Boundary edge: create a unique boundary node for this edge
                int bnode = next_boundary_id++;
                dg.boundary_nodes.push_back(bnode);
                dg.edges.push_back({stab_list[0], bnode});
                dg.edge_to_data_qubit.push_back(dq_idx);
            }
        }

        dg.syndrome_to_stabilizer.resize(dg.num_syndrome_nodes);
        for (int i = 0; i < dg.num_syndrome_nodes; i++) {
            dg.syndrome_to_stabilizer[i] = i;
        }

        return dg;
    };

    sc.z_decoding_graph = build_decoding_graph(sc.z_stabilizers);
    sc.x_decoding_graph = build_decoding_graph(sc.x_stabilizers);

    return sc;
}

void compute_syndromes(SurfaceCode& sc) {
    // X errors flip Z-stabilizers, Z errors flip X-stabilizers
    // Y error = X + Z, so it flips both

    for (auto& stab : sc.z_stabilizers) {
        stab.measurement = 0;
        for (int dq : stab.data_qubits) {
            ErrorType e = sc.data_qubits[dq].error;
            if (e == ErrorType::X_ERROR || e == ErrorType::Z_ERROR) {
                // X errors flip Z-stabilizers
                if (e == ErrorType::X_ERROR) {
                    stab.measurement ^= 1;
                }
            }
        }
    }

    for (auto& stab : sc.x_stabilizers) {
        stab.measurement = 0;
        for (int dq : stab.data_qubits) {
            ErrorType e = sc.data_qubits[dq].error;
            if (e == ErrorType::Z_ERROR) {
                stab.measurement ^= 1;
            }
        }
    }
}

std::vector<int> get_syndrome_vector(const SurfaceCode& sc, const DecodingGraph& dg, bool is_x_type) {
    const auto& stabs = is_x_type ? sc.x_stabilizers : sc.z_stabilizers;
    std::vector<int> syndrome(dg.num_syndrome_nodes, 0);
    for (int i = 0; i < dg.num_syndrome_nodes; i++) {
        syndrome[i] = stabs[dg.syndrome_to_stabilizer[i]].measurement;
    }
    return syndrome;
}

void compute_layout(SurfaceCode& sc, float origin_x, float origin_y, float cell_size) {
    int d = sc.distance;

    // Data qubits on a regular grid
    for (auto& dq : sc.data_qubits) {
        dq.rx = origin_x + (dq.col + 1) * cell_size;
        dq.ry = origin_y + (dq.row + 1) * cell_size;
    }

    // Stabilizer positions: center of the face they cover
    auto set_stab_pos = [&](Stabilizer& stab) {
        float sx = 0, sy = 0;
        for (int dq : stab.data_qubits) {
            sx += sc.data_qubits[dq].rx;
            sy += sc.data_qubits[dq].ry;
        }
        stab.rx = sx / stab.data_qubits.size();
        stab.ry = sy / stab.data_qubits.size();
    };

    for (auto& stab : sc.x_stabilizers) set_stab_pos(stab);
    for (auto& stab : sc.z_stabilizers) set_stab_pos(stab);

    // Decoding graph node positions
    auto set_dg_positions = [&](DecodingGraph& dg, const std::vector<Stabilizer>& stabs) {
        // Total nodes = syndrome nodes + boundary nodes
        // Boundary node indices may not be contiguous starting from num_syndrome_nodes,
        // so find the max index to size the positions array.
        int max_node = dg.num_syndrome_nodes;
        for (int bi : dg.boundary_nodes) {
            if (bi >= max_node) max_node = bi + 1;
        }
        dg.node_positions.resize(max_node, {0.0f, 0.0f});

        // Syndrome nodes -> stabilizer positions
        for (int i = 0; i < dg.num_syndrome_nodes; i++) {
            int si = dg.syndrome_to_stabilizer[i];
            dg.node_positions[i] = {stabs[si].rx, stabs[si].ry};
        }

        // Boundary nodes: position each near the data qubit it represents,
        // offset outward from the lattice.
        // Find which edge each boundary node belongs to, then position accordingly.
        for (int e = 0; e < (int)dg.edges.size(); e++) {
            int n0 = dg.edges[e][0], n1 = dg.edges[e][1];
            // Check if either endpoint is a boundary node
            int bnode = -1, snode = -1;
            for (int bi : dg.boundary_nodes) {
                if (n0 == bi) { bnode = n0; snode = n1; break; }
                if (n1 == bi) { bnode = n1; snode = n0; break; }
            }
            if (bnode < 0) continue;

            // Position boundary node: take the data qubit's position and offset outward
            int dq_idx = dg.edge_to_data_qubit[e];
            float dqx = sc.data_qubits[dq_idx].rx;
            float dqy = sc.data_qubits[dq_idx].ry;
            float sx = dg.node_positions[snode].first;
            float sy = dg.node_positions[snode].second;
            // Offset = data qubit position reflected across the stabilizer
            float bx = 2.0f * dqx - sx;
            float by = 2.0f * dqy - sy;
            dg.node_positions[bnode] = {bx, by};
        }
    };

    set_dg_positions(sc.z_decoding_graph, sc.z_stabilizers);
    set_dg_positions(sc.x_decoding_graph, sc.x_stabilizers);
}
