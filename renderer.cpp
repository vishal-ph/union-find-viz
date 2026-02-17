#include "renderer.h"
#include "raymath.h"
#include <cmath>
#include <algorithm>
#include <sstream>

static const Color CLUSTER_COLORS[] = {
    {230, 25, 75, 255},   // red
    {60, 180, 75, 255},   // green
    {255, 225, 25, 255},  // yellow
    {0, 130, 200, 255},   // blue
    {245, 130, 48, 255},  // orange
    {145, 30, 180, 255},  // purple
    {70, 240, 240, 255},  // cyan
    {240, 50, 230, 255},  // magenta
    {210, 245, 60, 255},  // lime
    {250, 190, 212, 255}, // pink
    {0, 128, 128, 255},   // teal
    {170, 110, 40, 255},  // brown
};
static const int NUM_CLUSTER_COLORS = 12;

Color get_cluster_color(int cluster_index) {
    return CLUSTER_COLORS[cluster_index % NUM_CLUSTER_COLORS];
}

// Spacing multiplier for the 3D layout
static const float SPATIAL_SCALE = 2.0f;
static const float TIME_SCALE = 3.0f;

Renderer::Renderer(int screen_width, int screen_height)
    : screen_width_(screen_width), screen_height_(screen_height)
{
    camera_ = {};
    camera_.position = {15.0f, 15.0f, 15.0f};
    camera_.target = {0.0f, 0.0f, 0.0f};
    camera_.up = {0.0f, 1.0f, 0.0f};
    camera_.fovy = 45.0f;
    camera_.projection = CAMERA_PERSPECTIVE;

    graph_center_ = {0, 0, 0};
    graph_radius_ = 10.0f;
}

void Renderer::compute_layout(const DecodingGraph3D& graph) {
    if (graph.node_positions.empty()) return;

    // Compute centroid and extent
    float min_x = 1e9f, max_x = -1e9f;
    float min_y = 1e9f, max_y = -1e9f;
    float min_z = 1e9f, max_z = -1e9f;

    for (auto& p : graph.node_positions) {
        Vector3 w = to_world(p);
        min_x = std::min(min_x, w.x); max_x = std::max(max_x, w.x);
        min_y = std::min(min_y, w.y); max_y = std::max(max_y, w.y);
        min_z = std::min(min_z, w.z); max_z = std::max(max_z, w.z);
    }

    graph_center_ = {
        (min_x + max_x) * 0.5f,
        (min_y + max_y) * 0.5f,
        (min_z + max_z) * 0.5f
    };

    float dx = max_x - min_x;
    float dy = max_y - min_y;
    float dz = max_z - min_z;
    graph_radius_ = std::sqrt(dx * dx + dy * dy + dz * dz) * 0.5f;
    if (graph_radius_ < 1.0f) graph_radius_ = 5.0f;

    // Set initial orbit distance to see the full graph
    orbit_distance_ = graph_radius_ * 2.8f;
    camera_.target = graph_center_;

    layout_computed_ = true;
}

Vector3 Renderer::to_world(const Vec3& v) const {
    // X, Y are spatial dimensions (vertical plane); Z is the time axis (horizontal/depth)
    return {v.x * SPATIAL_SCALE, v.y * SPATIAL_SCALE, v.z * TIME_SCALE};
}

void Renderer::update_camera() {
    // Custom orbital camera: left-drag to rotate, scroll to zoom
    Vector2 mouse = GetMousePosition();

    // Only rotate when mouse is in the 3D viewport (left of panel)
    float panel_x = screen_width_ * 0.78f - 10;
    bool in_viewport = mouse.x < panel_x;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && in_viewport) {
        dragging_ = true;
        prev_mouse_ = mouse;
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        dragging_ = false;
    }

    if (dragging_) {
        float dx = mouse.x - prev_mouse_.x;
        float dy = mouse.y - prev_mouse_.y;
        orbit_angle_h_ -= dx * 0.005f;
        orbit_angle_v_ += dy * 0.005f;
        // Clamp vertical angle to avoid flipping
        if (orbit_angle_v_ < 0.05f) orbit_angle_v_ = 0.05f;
        if (orbit_angle_v_ > 3.1f) orbit_angle_v_ = 3.1f;
        prev_mouse_ = mouse;
    }

    if (in_viewport) {
        float scroll = GetMouseWheelMove();
        orbit_distance_ -= scroll * orbit_distance_ * 0.1f;
        if (orbit_distance_ < 1.0f) orbit_distance_ = 1.0f;
        if (orbit_distance_ > 200.0f) orbit_distance_ = 200.0f;
    }

    // Compute camera position from spherical coordinates
    camera_.position = {
        graph_center_.x + orbit_distance_ * sinf(orbit_angle_v_) * cosf(orbit_angle_h_),
        graph_center_.y + orbit_distance_ * cosf(orbit_angle_v_),
        graph_center_.z + orbit_distance_ * sinf(orbit_angle_v_) * sinf(orbit_angle_h_)
    };
    camera_.target = graph_center_;
}

void Renderer::render(
    const DecodingGraph3D& graph,
    const std::vector<int>& syndrome,
    const DecoderSnapshot* snapshot,
    const std::string& status_text,
    const std::string& mode_text,
    bool show_time_planes,
    bool show_graph_edges)
{
    if (!layout_computed_) {
        compute_layout(graph);
    }

    BeginMode3D(camera_);

    // Layer 1: Time planes (translucent horizontal quads)
    if (show_time_planes) {
        render_time_planes(graph);
    }

    // Layer 2: Graph edges
    if (show_graph_edges) {
        bool decoding_active = snapshot && snapshot->phase != DecoderPhase::IDLE;
        render_graph_edges(graph, decoding_active);
    }

    // Decoder visualization layers
    if (snapshot && snapshot->phase != DecoderPhase::IDLE) {
        render_cluster_regions(graph, *snapshot);
        render_cluster_edges(graph, *snapshot);

        if (snapshot->phase == DecoderPhase::SPANNING_FOREST ||
            snapshot->phase == DecoderPhase::FOREST_PEELING ||
            snapshot->phase == DecoderPhase::DONE) {
            render_spanning_forest(graph, *snapshot);
        }

        if (snapshot->phase == DecoderPhase::FOREST_PEELING ||
            snapshot->phase == DecoderPhase::DONE) {
            render_corrections(graph, *snapshot);
        }
    }

    // Layer: Detector nodes (on top)
    render_detector_nodes(graph, syndrome, snapshot);

    // Layer: Boundary nodes
    render_boundary_nodes(graph);

    EndMode3D();

    // 2D overlay panel
    render_panel(graph, snapshot, status_text, mode_text);
}

void Renderer::render_time_planes(const DecodingGraph3D& graph) {
    // Find spatial extent
    float min_x = 1e9f, max_x = -1e9f;
    float min_y = 1e9f, max_y = -1e9f; // y in DEM coords = z in world

    for (auto& det : graph.detectors) {
        min_x = std::min(min_x, det.coords.x);
        max_x = std::max(max_x, det.coords.x);
        min_y = std::min(min_y, det.coords.y);
        max_y = std::max(max_y, det.coords.y);
    }

    float pad = 1.0f;
    float wx0 = (min_x - pad) * SPATIAL_SCALE;
    float wx1 = (max_x + pad) * SPATIAL_SCALE;
    float wz0 = (min_y - pad) * SPATIAL_SCALE;
    float wz1 = (max_y + pad) * SPATIAL_SCALE;

    Color grid_color = {80, 80, 100, 60};

    // Time planes are vertical (XY plane at each Z = t * TIME_SCALE)
    // wx0..wx1 = X range, wy0..wy1 = Y range (from DEM y coords)
    float wy0 = wz0; // reuse the computed spatial range
    float wy1 = wz1;

    int grid_lines = 8;
    for (int t = 0; t < graph.num_rounds; t++) {
        float z = (float)t * TIME_SCALE;

        // Draw border rectangle in XY plane at this Z
        DrawLine3D({wx0, wy0, z}, {wx1, wy0, z}, grid_color);
        DrawLine3D({wx1, wy0, z}, {wx1, wy1, z}, grid_color);
        DrawLine3D({wx1, wy1, z}, {wx0, wy1, z}, grid_color);
        DrawLine3D({wx0, wy1, z}, {wx0, wy0, z}, grid_color);

        // Draw internal grid lines
        for (int g = 1; g < grid_lines; g++) {
            float frac = (float)g / grid_lines;
            float gx = wx0 + (wx1 - wx0) * frac;
            float gy = wy0 + (wy1 - wy0) * frac;
            DrawLine3D({gx, wy0, z}, {gx, wy1, z}, grid_color);
            DrawLine3D({wx0, gy, z}, {wx1, gy, z}, grid_color);
        }
    }
}

void Renderer::render_graph_edges(const DecodingGraph3D& graph, bool dim) {
    for (auto& ge : graph.graph_edges) {
        if (ge.node0 >= (int)graph.node_positions.size() ||
            ge.node1 >= (int)graph.node_positions.size()) continue;

        Vector3 p0 = to_world(graph.node_positions[ge.node0]);
        Vector3 p1 = to_world(graph.node_positions[ge.node1]);

        Color col;
        if (ge.is_boundary) {
            col = {120, 120, 120, (unsigned char)(dim ? 30 : 100)}; // gray for boundary
        } else {
            // Check if spatial (same time) or temporal (same space, different time)
            float dz = std::abs(graph.node_positions[ge.node0].z -
                                graph.node_positions[ge.node1].z);
            if (dz > 0.1f) {
                col = {255, 180, 100, (unsigned char)(dim ? 40 : 120)}; // orange for temporal
            } else {
                col = {100, 180, 255, (unsigned char)(dim ? 40 : 120)}; // blue for spatial
            }
        }

        DrawLine3D(p0, p1, col);
    }
}

void Renderer::render_detector_nodes(const DecodingGraph3D& graph,
                                      const std::vector<int>& syndrome,
                                      const DecoderSnapshot* snapshot) {
    float normal_radius = 0.15f;
    float fired_radius = 0.3f;

    for (int i = 0; i < graph.num_detectors; i++) {
        Vector3 pos = to_world(graph.node_positions[i]);
        bool fired = (i < (int)syndrome.size() && syndrome[i] == 1);

        // Check cluster membership when decoding is active
        int owning_cluster = -1;
        if (snapshot && snapshot->phase != DecoderPhase::IDLE) {
            for (int c = 0; c < (int)snapshot->clusters_nodes.size(); c++) {
                if (snapshot->clusters_activity[c] == 0) continue;
                if (i < (int)snapshot->clusters_nodes[c].size() &&
                    snapshot->clusters_nodes[c][i] != 0) {
                    owning_cluster = c;
                    break;
                }
            }
        }

        if (fired) {
            if (owning_cluster >= 0) {
                // Fired + in cluster: cluster color sphere with bright white core
                Color ccol = get_cluster_color(owning_cluster);
                DrawSphere(pos, fired_radius, ccol);
                DrawSphere(pos, fired_radius * 0.5f, WHITE);
            } else {
                DrawSphere(pos, fired_radius, RED);
            }
        } else {
            if (owning_cluster >= 0) {
                // Quiet + in cluster: cluster color, slightly larger than default
                Color ccol = get_cluster_color(owning_cluster);
                ccol.a = 220;
                DrawSphere(pos, normal_radius * 1.3f, ccol);
            } else {
                DrawSphere(pos, normal_radius, Color{200, 200, 210, 255});
            }
        }
    }
}

void Renderer::render_boundary_nodes(const DecodingGraph3D& graph) {
    float radius = 0.12f;

    for (int bi : graph.boundary_node_indices) {
        if (bi >= (int)graph.node_positions.size()) continue;
        Vector3 pos = to_world(graph.node_positions[bi]);
        DrawSphere(pos, radius, Color{100, 100, 100, 200});
    }
}

void Renderer::render_cluster_regions(const DecodingGraph3D& graph,
                                       const DecoderSnapshot& snap) {
    float r_interior = 0.5f;
    float r_frontier = 0.35f;

    for (int c = 0; c < (int)snap.clusters_activity.size(); c++) {
        if (snap.clusters_activity[c] == 0) continue;

        Color col = get_cluster_color(c);

        // Wireframe sphere halos around cluster nodes
        for (int n = 0; n < (int)snap.clusters_nodes[c].size(); n++) {
            int state = snap.clusters_nodes[c][n];
            if (state == 0 || n >= (int)graph.node_positions.size()) continue;

            Vector3 pos = to_world(graph.node_positions[n]);
            if (state == 2) {
                // Interior: medium opacity wireframe
                Color wire = col;
                wire.a = 140;
                DrawSphereWires(pos, r_interior, 6, 6, wire);
            } else {
                // Frontier: lower opacity, smaller wireframe
                Color wire = col;
                wire.a = 80;
                DrawSphereWires(pos, r_frontier, 6, 6, wire);
            }
        }
    }
}

void Renderer::render_cluster_edges(const DecodingGraph3D& graph,
                                     const DecoderSnapshot& snap) {
    for (int c = 0; c < (int)snap.clusters_activity.size(); c++) {
        if (snap.clusters_activity[c] == 0) continue;

        Color col = get_cluster_color(c);

        for (int e = 0; e < (int)snap.clusters_edges[c].size() &&
                         e < (int)graph.uf_edges.size(); e++) {
            int edge_state = snap.clusters_edges[c][e];
            if (edge_state == 0) continue;

            int n0 = graph.uf_edges[e][0], n1 = graph.uf_edges[e][1];
            if (n0 >= (int)graph.node_positions.size() ||
                n1 >= (int)graph.node_positions.size()) continue;

            Vector3 p0 = to_world(graph.node_positions[n0]);
            Vector3 p1 = to_world(graph.node_positions[n1]);

            if (edge_state == 1) {
                // Half-grown: thinner cylinder from node to midpoint + glow sphere at tip
                Vector3 mid = {
                    (p0.x + p1.x) * 0.5f,
                    (p0.y + p1.y) * 0.5f,
                    (p0.z + p1.z) * 0.5f
                };
                DrawCylinderEx(p0, mid, 0.05f, 0.05f, 6, col);
                Color glow = col;
                glow.a = 150;
                DrawSphere(mid, 0.1f, glow);
            } else {
                // Fully grown: thick cylinder
                DrawCylinderEx(p0, p1, 0.08f, 0.08f, 6, col);
            }
        }
    }
}

void Renderer::render_spanning_forest(const DecodingGraph3D& graph,
                                       const DecoderSnapshot& snap) {
    Color tree_color = {0, 200, 200, 230}; // teal

    for (int t = 0; t < (int)snap.spanning_forest.size(); t++) {
        for (int n = 0; n < (int)snap.spanning_forest[t].size(); n++) {
            int edge_idx = snap.spanning_forest[t][n];
            if (edge_idx < 0) continue;
            if (edge_idx >= (int)graph.uf_edges.size()) continue;

            int n0 = graph.uf_edges[edge_idx][0];
            int n1 = graph.uf_edges[edge_idx][1];
            if (n0 >= (int)graph.node_positions.size() ||
                n1 >= (int)graph.node_positions.size()) continue;

            Vector3 p0 = to_world(graph.node_positions[n0]);
            Vector3 p1 = to_world(graph.node_positions[n1]);

            // Draw thick cylinder for spanning tree edge
            DrawCylinderEx(p0, p1, 0.06f, 0.06f, 6, tree_color);
        }
    }
}

void Renderer::render_corrections(const DecodingGraph3D& graph,
                                   const DecoderSnapshot& snap) {
    Color corr_color = {220, 20, 220, 255}; // magenta

    for (int e = 0; e < (int)snap.edge_corrections.size() &&
                     e < (int)graph.uf_edges.size(); e++) {
        if (snap.edge_corrections[e] == 0) continue;

        int n0 = graph.uf_edges[e][0];
        int n1 = graph.uf_edges[e][1];
        if (n0 >= (int)graph.node_positions.size() ||
            n1 >= (int)graph.node_positions.size()) continue;

        Vector3 p0 = to_world(graph.node_positions[n0]);
        Vector3 p1 = to_world(graph.node_positions[n1]);

        DrawCylinderEx(p0, p1, 0.1f, 0.1f, 6, corr_color);
    }
}

void Renderer::render_panel(const DecodingGraph3D& graph, const DecoderSnapshot* snap,
                             const std::string& status, const std::string& mode) {
    float px = screen_width_ * 0.78f;
    float py = 20.0f;
    int fs = 16;
    int gap = 22;

    // Background panel
    DrawRectangle((int)px - 10, 0, screen_width_ - (int)px + 10, screen_height_,
                  Color{20, 20, 30, 220});
    DrawLine((int)px - 10, 0, (int)px - 10, screen_height_, Color{60, 60, 80, 255});

    DrawText("Union-Find 3D Decoder", (int)px, (int)py, 20, WHITE);
    py += 30;

    DrawText(TextFormat("Detectors: %d", graph.num_detectors), (int)px, (int)py, fs,
             Color{180, 180, 200, 255});
    py += gap;

    DrawText(TextFormat("Rounds: %d", graph.num_rounds), (int)px, (int)py, fs,
             Color{180, 180, 200, 255});
    py += gap;

    DrawText(TextFormat("Edges: %d", (int)graph.graph_edges.size()), (int)px, (int)py, fs,
             Color{180, 180, 200, 255});
    py += gap;

    DrawText(mode.c_str(), (int)px, (int)py, fs, Color{255, 150, 100, 255});
    py += gap + 10;

    // Phase indicator
    if (snap) {
        const char* phase_str = "Idle";
        Color phase_col = GRAY;
        switch (snap->phase) {
            case DecoderPhase::SYNDROME_VALIDATION:
                phase_str = "Syndrome Validation";
                phase_col = Color{255, 180, 60, 255};
                break;
            case DecoderPhase::SPANNING_FOREST:
                phase_str = "Spanning Forest";
                phase_col = Color{0, 200, 200, 255};
                break;
            case DecoderPhase::FOREST_PEELING:
                phase_str = "Forest Peeling";
                phase_col = Color{220, 80, 220, 255};
                break;
            case DecoderPhase::DONE:
                phase_str = "Done";
                phase_col = Color{80, 220, 80, 255};
                break;
            default: break;
        }
        DrawText(TextFormat("Phase: %s", phase_str), (int)px, (int)py, fs, phase_col);
        py += gap;
        DrawText(TextFormat("Cycle: %d", snap->cycle_number), (int)px, (int)py, fs,
                 Color{180, 180, 200, 255});
        py += gap + 10;
    }

    // Status text (split by newlines)
    {
        std::istringstream iss(status);
        std::string line;
        while (std::getline(iss, line)) {
            DrawText(line.c_str(), (int)px, (int)py, 14, Color{140, 180, 255, 255});
            py += 18;
        }
    }
    py += 15;

    // Legend
    DrawText("--- Legend ---", (int)px, (int)py, fs, Color{150, 150, 170, 255});
    py += gap;

    auto legend_item = [&](Color col, const char* label) {
        DrawRectangle((int)px, (int)py + 2, 12, 12, col);
        DrawText(label, (int)px + 18, (int)py, 14, Color{180, 180, 200, 255});
        py += 20;
    };

    legend_item(Color{200, 200, 210, 255}, "Detector (quiet)");
    legend_item(RED, "Detector (fired)");
    legend_item(Color{100, 180, 255, 200}, "Spatial edge");
    legend_item(Color{255, 180, 100, 200}, "Temporal edge");
    legend_item(Color{0, 200, 200, 230}, "Spanning tree");
    legend_item(Color{220, 20, 220, 255}, "Correction");
    py += 15;

    // Controls
    DrawText("--- Controls ---", (int)px, (int)py, fs, Color{150, 150, 170, 255});
    py += gap;

    const char* controls[] = {
        "D: Start decode",
        "Space/Right: Step",
        "A: Auto-step",
        "+/-: Step speed",
        "F: Fast-forward",
        "R: Reset",
        "1: Time planes",
        "2: Graph edges",
        "Mouse: Rotate",
        "Scroll: Zoom",
    };
    for (const char* c : controls) {
        DrawText(c, (int)px, (int)py, 13, Color{130, 130, 150, 255});
        py += 18;
    }
}
