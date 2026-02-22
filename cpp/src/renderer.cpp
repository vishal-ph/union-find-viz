#include "renderer.h"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <sstream>

// Cluster palette: 10 harmonious colors, avoiding pure red/blue (reserved for X/Z node types).
static const Color CLUSTER_COLORS[] = {
    {80, 180, 190, 255},   // teal
    {90, 150, 220, 255},   // steel blue
    {130, 120, 210, 255},  // soft indigo
    {175, 100, 195, 255},  // orchid
    {210, 95, 150, 255},   // mauve
    {230, 120, 90, 255},   // coral
    {220, 160, 60, 255},   // amber
    {140, 185, 90, 255},   // olive green
    {95, 195, 145, 255},   // mint
    {160, 130, 200, 255},  // lavender
};
static const int NUM_CLUSTER_COLORS = 10;

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

    // Load a crisp TTF font for panel text
    panel_font_ = LoadFontEx("/System/Library/Fonts/SFNS.ttf", 32, 0, 250);
    if (panel_font_.glyphCount == 0)
        panel_font_ = LoadFontEx("/System/Library/Fonts/Helvetica.ttc", 32, 0, 250);
    if (panel_font_.glyphCount == 0)
        panel_font_ = GetFontDefault();
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
    bool show_graph_edges,
    bool show_lattice_underlay)
{
    if (!layout_computed_) {
        compute_layout(graph);
    }

    BeginMode3D(camera_);

    // Layer 0: Lattice underlay (stabilizer faces + data qubits)
    if (show_lattice_underlay && graph.lattice.valid) {
        render_lattice_underlay(graph);
    }

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
        render_cluster_sheaths(graph, *snapshot);
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

    Color grid_color = {180, 180, 200, 80};

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

void Renderer::render_lattice_underlay(const DecodingGraph3D& graph) {
    const LatticeInfo& lat = graph.lattice;

    Color x_face_fill = {220, 120, 120, 18};   // very faint red wash
    Color z_face_fill = {120, 120, 220, 18};   // very faint blue wash
    Color x_face_edge = {200, 100, 100, 60};   // subtle red outline
    Color z_face_edge = {100, 100, 200, 60};   // subtle blue outline

    for (int t = 0; t < graph.num_rounds; t++) {
        float wz = (float)t * TIME_SCALE;

        for (auto& face : lat.faces) {
            float cx = face.center.x * SPATIAL_SCALE;
            float cy = face.center.y * SPATIAL_SCALE;
            float hw = face.half_w * SPATIAL_SCALE;
            float hh = face.half_h * SPATIAL_SCALE;

            Vector3 v0 = {cx - hw, cy - hh, wz};
            Vector3 v1 = {cx + hw, cy - hh, wz};
            Vector3 v2 = {cx + hw, cy + hh, wz};
            Vector3 v3 = {cx - hw, cy + hh, wz};

            Color fill = face.is_x_type ? x_face_fill : z_face_fill;
            Color edge = face.is_x_type ? x_face_edge : z_face_edge;

            // Very faint fill
            DrawTriangle3D(v0, v3, v2, fill);
            DrawTriangle3D(v0, v2, v1, fill);
            DrawTriangle3D(v0, v2, v3, fill);
            DrawTriangle3D(v0, v1, v2, fill);

            // Outline edges of the face
            DrawLine3D(v0, v1, edge);
            DrawLine3D(v1, v2, edge);
            DrawLine3D(v2, v3, edge);
            DrawLine3D(v3, v0, edge);
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
            col = {160, 160, 160, (unsigned char)(dim ? 35 : 180)}; // neutral grey
        } else {
            float dz = std::abs(graph.node_positions[ge.node0].z -
                                graph.node_positions[ge.node1].z);
            if (dz > 0.1f) {
                col = {170, 150, 120, (unsigned char)(dim ? 35 : 200)}; // warm grey (temporal)
            } else {
                col = {120, 140, 175, (unsigned char)(dim ? 35 : 200)}; // cool grey (spatial)
            }
        }

        float r = dim ? 0.015f : 0.025f;
        DrawCylinderEx(p0, p1, r, r, 4, col);
    }
}

void Renderer::render_detector_nodes(const DecodingGraph3D& graph,
                                      const std::vector<int>& syndrome,
                                      const DecoderSnapshot* snapshot) {
    for (int i = 0; i < graph.num_detectors; i++) {
        Vector3 pos = to_world(graph.node_positions[i]);
        bool fired = (i < (int)syndrome.size() && syndrome[i] == 1);
        bool is_x = graph.detectors[i].is_x_type;

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

        bool in_cluster = (owning_cluster >= 0);

        // Base color by stabilizer type
        Color base_col;
        if (is_x) {
            base_col = {230, 70, 60, 255};   // red for X-type
        } else {
            base_col = {60, 130, 230, 255};   // blue for Z-type
        }

        // Size and opacity by state
        float radius;
        if (fired) {
            radius = in_cluster ? 0.32f : 0.28f;
            base_col.a = 255;
        } else {
            radius = in_cluster ? 0.14f : 0.09f;
            base_col.a = in_cluster ? 200 : 55;
        }

        // All nodes are spheres
        DrawSphere(pos, radius, base_col);

        // Cluster ownership indicator: wireframe ring in cluster color
        if (in_cluster) {
            Color ring_col = get_cluster_color(owning_cluster);
            DrawSphereWires(pos, radius + 0.06f, 6, 6, ring_col);
        }

        // Fired nodes get a bright white specular core
        if (fired) {
            Color core = {255, 255, 255, 180};
            DrawSphere(pos, radius * 0.3f, core);
        }
    }
}

void Renderer::render_boundary_nodes(const DecodingGraph3D& graph) {
    float radius = 0.14f;

    for (int bi : graph.boundary_node_indices) {
        if (bi >= (int)graph.node_positions.size()) continue;
        Vector3 pos = to_world(graph.node_positions[bi]);
        DrawSphere(pos, radius, Color{140, 140, 140, 220});
        DrawSphereWires(pos, radius + 0.01f, 4, 4, Color{100, 100, 100, 160});
    }
}

// ---------------------------------------------------------------------------
// Convex hull helpers for cluster sheaths
// ---------------------------------------------------------------------------

// 2D cross product of vectors OA and OB
static float cross2d(float ox, float oy, float ax, float ay, float bx, float by) {
    return (ax - ox) * (by - oy) - (ay - oy) * (bx - ox);
}

// 2D convex hull (Andrew's monotone chain), returns indices in CCW order
static std::vector<int> convex_hull_2d(const std::vector<float>& xs,
                                        const std::vector<float>& ys) {
    int n = (int)xs.size();
    if (n < 3) {
        std::vector<int> r(n);
        std::iota(r.begin(), r.end(), 0);
        return r;
    }

    std::vector<int> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&](int a, int b) {
        return xs[a] < xs[b] || (xs[a] == xs[b] && ys[a] < ys[b]);
    });

    std::vector<int> hull;
    for (int i : idx) {
        while ((int)hull.size() >= 2 &&
               cross2d(xs[hull[hull.size()-2]], ys[hull[hull.size()-2]],
                       xs[hull[hull.size()-1]], ys[hull[hull.size()-1]],
                       xs[i], ys[i]) <= 0)
            hull.pop_back();
        hull.push_back(i);
    }
    int lower = (int)hull.size() + 1;
    for (int i = n - 2; i >= 0; i--) {
        while ((int)hull.size() >= lower &&
               cross2d(xs[hull[hull.size()-2]], ys[hull[hull.size()-2]],
                       xs[hull[hull.size()-1]], ys[hull[hull.size()-1]],
                       xs[idx[i]], ys[idx[i]]) <= 0)
            hull.pop_back();
        hull.push_back(idx[i]);
    }
    hull.pop_back();
    return hull;
}

// Render a translucent convex hull around a set of 3D points.
// Handles coplanar sets (extruded 2D hull) and full 3D sets (brute-force hull).
static void render_hull(const std::vector<Vector3>& points, Color col, float inflate) {
    int n = (int)points.size();
    if (n < 2) return;

    // Centroid
    Vector3 cen = {0, 0, 0};
    for (auto& p : points) { cen.x += p.x; cen.y += p.y; cen.z += p.z; }
    cen.x /= n; cen.y /= n; cen.z /= n;

    // Inflate each point outward from centroid
    std::vector<Vector3> pts(n);
    for (int i = 0; i < n; i++) {
        float dx = points[i].x - cen.x;
        float dy = points[i].y - cen.y;
        float dz = points[i].z - cen.z;
        float len = sqrtf(dx * dx + dy * dy + dz * dz);
        if (len > 0.001f) {
            float s = inflate / len;
            pts[i] = {points[i].x + dx * s, points[i].y + dy * s, points[i].z + dz * s};
        } else {
            pts[i] = points[i];
        }
    }

    // --- N == 2: capsule ---
    if (n == 2) {
        DrawCylinderEx(pts[0], pts[1], inflate * 0.6f, inflate * 0.6f, 8, col);
        DrawSphere(pts[0], inflate * 0.6f, col);
        DrawSphere(pts[1], inflate * 0.6f, col);
        return;
    }

    // --- Determine coplanarity ---
    Vector3 normal = {0, 0, 0};
    bool has_normal = false;
    for (int i = 1; i < n && !has_normal; i++) {
        for (int j = i + 1; j < n && !has_normal; j++) {
            float abx = pts[i].x - pts[0].x, aby = pts[i].y - pts[0].y, abz = pts[i].z - pts[0].z;
            float acx = pts[j].x - pts[0].x, acy = pts[j].y - pts[0].y, acz = pts[j].z - pts[0].z;
            normal = {aby * acz - abz * acy, abz * acx - abx * acz, abx * acy - aby * acx};
            float nlen = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            if (nlen > 0.01f) {
                normal.x /= nlen; normal.y /= nlen; normal.z /= nlen;
                has_normal = true;
            }
        }
    }

    bool coplanar = true;
    if (has_normal) {
        float d = normal.x * pts[0].x + normal.y * pts[0].y + normal.z * pts[0].z;
        for (auto& p : pts) {
            if (std::abs(normal.x * p.x + normal.y * p.y + normal.z * p.z - d) > 0.5f) {
                coplanar = false;
                break;
            }
        }
    }

    // --- Coplanar: extruded 2D convex hull ---
    if (coplanar && has_normal) {
        // Build local 2D axes on the plane
        float ux = pts[1].x - pts[0].x, uy = pts[1].y - pts[0].y, uz = pts[1].z - pts[0].z;
        float ulen = sqrtf(ux * ux + uy * uy + uz * uz);
        if (ulen < 0.001f) return;
        ux /= ulen; uy /= ulen; uz /= ulen;
        // v = normal x u
        float vx = normal.y * uz - normal.z * uy;
        float vy = normal.z * ux - normal.x * uz;
        float vz = normal.x * uy - normal.y * ux;

        std::vector<float> xs(n), ys(n);
        for (int i = 0; i < n; i++) {
            float dx = pts[i].x - cen.x, dy = pts[i].y - cen.y, dz = pts[i].z - cen.z;
            xs[i] = ux * dx + uy * dy + uz * dz;
            ys[i] = vx * dx + vy * dy + vz * dz;
        }

        auto hull = convex_hull_2d(xs, ys);
        int hn = (int)hull.size();
        if (hn < 3) return;

        // Extrude along normal for thickness
        float thick = inflate * 0.4f;
        Vector3 off = {normal.x * thick, normal.y * thick, normal.z * thick};

        for (int i = 0; i < hn; i++) {
            int j = (i + 1) % hn;
            Vector3 pit = {pts[hull[i]].x + off.x, pts[hull[i]].y + off.y, pts[hull[i]].z + off.z};
            Vector3 pjt = {pts[hull[j]].x + off.x, pts[hull[j]].y + off.y, pts[hull[j]].z + off.z};
            Vector3 pib = {pts[hull[i]].x - off.x, pts[hull[i]].y - off.y, pts[hull[i]].z - off.z};
            Vector3 pjb = {pts[hull[j]].x - off.x, pts[hull[j]].y - off.y, pts[hull[j]].z - off.z};
            Vector3 ct = {cen.x + off.x, cen.y + off.y, cen.z + off.z};
            Vector3 cb = {cen.x - off.x, cen.y - off.y, cen.z - off.z};

            // Top face (fan from centroid)
            DrawTriangle3D(ct, pit, pjt, col);
            DrawTriangle3D(ct, pjt, pit, col);
            // Bottom face
            DrawTriangle3D(cb, pjb, pib, col);
            DrawTriangle3D(cb, pib, pjb, col);
            // Side quad
            DrawTriangle3D(pit, pib, pjb, col);
            DrawTriangle3D(pit, pjb, pjt, col);
            DrawTriangle3D(pit, pjb, pib, col);
            DrawTriangle3D(pit, pjt, pjb, col);
        }
        return;
    }

    // --- 3D convex hull (brute-force, small N only) ---
    if (n > 24) return;  // safety limit

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            for (int k = j + 1; k < n; k++) {
                Vector3 a = pts[i], b = pts[j], cc = pts[k];
                float abx = b.x - a.x, aby = b.y - a.y, abz = b.z - a.z;
                float acx = cc.x - a.x, acy = cc.y - a.y, acz = cc.z - a.z;
                float nx = aby * acz - abz * acy;
                float ny = abz * acx - abx * acz;
                float nz = abx * acy - aby * acx;
                if (nx * nx + ny * ny + nz * nz < 1e-10f) continue;

                bool all_pos = true, all_neg = true;
                for (int m = 0; m < n; m++) {
                    if (m == i || m == j || m == k) continue;
                    float dot = nx * (pts[m].x - a.x) + ny * (pts[m].y - a.y) + nz * (pts[m].z - a.z);
                    if (dot > 1e-5f) all_neg = false;
                    if (dot < -1e-5f) all_pos = false;
                    if (!all_pos && !all_neg) break;
                }
                if (all_pos || all_neg) {
                    DrawTriangle3D(a, b, cc, col);
                    DrawTriangle3D(a, cc, b, col);
                }
            }
        }
    }
}

void Renderer::render_cluster_sheaths(const DecodingGraph3D& graph,
                                       const DecoderSnapshot& snap) {
    float inflate = 0.5f;

    bool post_validation = (snap.phase == DecoderPhase::SPANNING_FOREST ||
                            snap.phase == DecoderPhase::FOREST_PEELING ||
                            snap.phase == DecoderPhase::DONE);

    for (int c = 0; c < (int)snap.clusters_activity.size(); c++) {
        if (snap.clusters_activity[c] == 0) continue;

        bool active = (snap.clusters_activity[c] == 1);
        Color col = get_cluster_color(c);
        col.a = post_validation ? 12 : (active ? 45 : 14);

        // Collect world positions of all nodes in this cluster
        std::vector<Vector3> node_pos;
        for (int nd = 0; nd < (int)snap.clusters_nodes[c].size(); nd++) {
            if (snap.clusters_nodes[c][nd] == 0) continue;
            if (nd >= (int)graph.node_positions.size()) continue;
            node_pos.push_back(to_world(graph.node_positions[nd]));
        }

        render_hull(node_pos, col, inflate);
    }
}

void Renderer::render_cluster_regions(const DecodingGraph3D& graph,
                                       const DecoderSnapshot& snap) {
    // Only draw thin wireframe rings on frontier nodes (state=1) as growth-boundary indicators.
    // Interior nodes are skipped — sheaths + edges already convey cluster extent.
    for (int c = 0; c < (int)snap.clusters_activity.size(); c++) {
        if (snap.clusters_activity[c] == 0) continue;

        Color col = get_cluster_color(c);
        col.a = 100;

        for (int n = 0; n < (int)snap.clusters_nodes[c].size(); n++) {
            int state = snap.clusters_nodes[c][n];
            if (state != 1 || n >= (int)graph.node_positions.size()) continue;

            Vector3 pos = to_world(graph.node_positions[n]);
            DrawSphereWires(pos, 0.22f, 4, 4, col);
        }
    }
}

// Draw a dashed cylinder between two points (n_dashes short segments with gaps)
static void draw_dashed_cylinder(Vector3 p0, Vector3 p1, float radius, int n_dashes, Color color) {
    float total = 2 * n_dashes - 1; // segments + gaps
    for (int i = 0; i < n_dashes; i++) {
        float t0 = (2.0f * i) / total;
        float t1 = (2.0f * i + 1.0f) / total;
        Vector3 a = {
            p0.x + (p1.x - p0.x) * t0,
            p0.y + (p1.y - p0.y) * t0,
            p0.z + (p1.z - p0.z) * t0
        };
        Vector3 b = {
            p0.x + (p1.x - p0.x) * t1,
            p0.y + (p1.y - p0.y) * t1,
            p0.z + (p1.z - p0.z) * t1
        };
        DrawCylinderEx(a, b, radius, radius, 6, color);
    }
}

void Renderer::render_cluster_edges(const DecodingGraph3D& graph,
                                     const DecoderSnapshot& snap) {
    bool post_validation = (snap.phase == DecoderPhase::SPANNING_FOREST ||
                            snap.phase == DecoderPhase::FOREST_PEELING ||
                            snap.phase == DecoderPhase::DONE);

    for (int c = 0; c < (int)snap.clusters_activity.size(); c++) {
        if (snap.clusters_activity[c] == 0) continue;

        Color col = get_cluster_color(c);
        bool active = (snap.clusters_activity[c] == 1);

        // Dim during post-validation so spanning tree/corrections stand out
        if (!active || post_validation) {
            col.a = post_validation ? 50 : 70;
        }

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
                float r = post_validation ? 0.015f : (active ? 0.03f : 0.02f);
                draw_dashed_cylinder(p0, p1, r, 5, col);
            } else {
                float r = post_validation ? 0.025f : (active ? 0.055f : 0.04f);
                DrawCylinderEx(p0, p1, r, r, 6, col);
            }
        }
    }
}

void Renderer::render_spanning_forest(const DecodingGraph3D& graph,
                                       const DecoderSnapshot& snap) {
    Color tree_color = {40, 40, 55, 255}; // dark charcoal — high contrast on light bg

    // Disable depth test so tree draws on top of cluster edges (same geometry, Z-fights otherwise)
    rlDisableDepthTest();

    // Track which nodes participate in trees (for joint markers)
    std::vector<bool> in_tree(graph.node_positions.size(), false);

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

            DrawCylinderEx(p0, p1, 0.08f, 0.08f, 6, tree_color);
            in_tree[n0] = true;
            in_tree[n1] = true;
        }

        // Draw root node marker (tree[n] == -1 means root)
        for (int n = 0; n < (int)snap.spanning_forest[t].size(); n++) {
            if (snap.spanning_forest[t][n] == -1 && n < (int)graph.node_positions.size()) {
                Vector3 pos = to_world(graph.node_positions[n]);
                DrawSphere(pos, 0.14f, Color{255, 255, 255, 200});
                DrawSphereWires(pos, 0.16f, 6, 6, tree_color);
                in_tree[n] = true;
            }
        }
    }

    // Draw small joint spheres at tree nodes for visual connectivity
    Color joint_color = {60, 60, 75, 180};
    for (int i = 0; i < (int)in_tree.size(); i++) {
        if (!in_tree[i]) continue;
        Vector3 pos = to_world(graph.node_positions[i]);
        DrawSphere(pos, 0.06f, joint_color);
    }

    rlEnableDepthTest();
}

void Renderer::render_corrections(const DecodingGraph3D& graph,
                                   const DecoderSnapshot& snap) {
    Color corr_color = {50, 255, 100, 255}; // neon green
    Color glow_color = {100, 255, 140, 140}; // softer green glow

    // Disable depth test so corrections draw on top of cluster edges and spanning tree
    rlDisableDepthTest();

    for (int e = 0; e < (int)snap.edge_corrections.size() &&
                     e < (int)graph.uf_edges.size(); e++) {
        if (snap.edge_corrections[e] == 0) continue;

        int n0 = graph.uf_edges[e][0];
        int n1 = graph.uf_edges[e][1];
        if (n0 >= (int)graph.node_positions.size() ||
            n1 >= (int)graph.node_positions.size()) continue;

        Vector3 p0 = to_world(graph.node_positions[n0]);
        Vector3 p1 = to_world(graph.node_positions[n1]);

        // Thick correction edge
        DrawCylinderEx(p0, p1, 0.10f, 0.10f, 8, corr_color);

        // Glowing endpoint spheres
        DrawSphere(p0, 0.13f, glow_color);
        DrawSphere(p1, 0.13f, glow_color);
    }

    rlEnableDepthTest();
}

void Renderer::render_panel(const DecodingGraph3D& graph, const DecoderSnapshot* snap,
                             const std::string& status, const std::string& mode) {
    float px = screen_width_ * 0.78f;
    float py = 20.0f;
    float panel_w = screen_width_ - px + 10;

    // Helper: draw text using panel font and return the y-advance
    auto draw_line = [&](const char* text, float x, float y, float size, Color col) -> float {
        DrawTextEx(panel_font_, text, {x, y}, size, 1, col);
        return size + 4.0f;
    };

    // Helper: draw a horizontal divider
    auto draw_divider = [&](float y) -> float {
        DrawLine((int)px, (int)y + 4, (int)(px + panel_w - 20), (int)y + 4, Color{180, 180, 195, 180});
        return 12.0f;
    };

    Color info_col = {80, 80, 100, 255};
    Color heading_col = {100, 100, 120, 255};

    // Background panel
    DrawRectangle((int)px - 10, 0, (int)panel_w + 10, screen_height_,
                  Color{235, 235, 240, 240});
    DrawLine((int)px - 10, 0, (int)px - 10, screen_height_, Color{190, 190, 200, 255});

    // Title
    py += draw_line("UNION-FIND DECODER", px, py, 22, Color{30, 30, 40, 255});
    py += 4;
    py += draw_divider(py);

    // Graph stats
    py += draw_line(TextFormat("Detectors: %d", graph.num_detectors), px, py, 16, info_col);
    py += draw_line(TextFormat("Rounds: %d", graph.num_rounds), px, py, 16, info_col);
    py += draw_line(TextFormat("Edges: %d", (int)graph.graph_edges.size()), px, py, 16, info_col);

    // Count fired detectors
    int fired_count = 0;
    if (snap) {
        // Count from clusters_nodes — any cluster that contains fired detectors
        // We can approximate from the graph by checking syndrome indirectly.
        // For simplicity, count nodes that are in any cluster with activity > 0
        for (int c = 0; c < (int)snap->clusters_activity.size(); c++) {
            if (snap->clusters_activity[c] == 0) continue;
            for (int n = 0; n < (int)snap->clusters_nodes[c].size(); n++) {
                if (snap->clusters_nodes[c][n] != 0 && n < graph.num_detectors) {
                    // We count distinct fired nodes below more accurately
                }
            }
        }
    }
    // We don't have direct access to syndrome here, so show mode instead
    py += draw_line(mode.c_str(), px, py, 16, Color{200, 100, 40, 255});
    py += draw_divider(py);

    // Phase-specific content
    if (snap && snap->phase != DecoderPhase::IDLE) {
        // Phase badge with colored background
        const char* phase_str = "Idle";
        Color phase_col = {120, 120, 120, 255};
        Color badge_bg = {200, 200, 200, 60};
        switch (snap->phase) {
            case DecoderPhase::SYNDROME_VALIDATION:
                phase_str = "SYNDROME VALIDATION";
                phase_col = Color{200, 130, 20, 255};
                badge_bg = Color{200, 130, 20, 40};
                break;
            case DecoderPhase::SPANNING_FOREST:
                phase_str = "SPANNING FOREST";
                phase_col = Color{40, 50, 70, 255};
                badge_bg = Color{40, 50, 70, 40};
                break;
            case DecoderPhase::FOREST_PEELING:
                phase_str = "FOREST PEELING";
                phase_col = Color{20, 160, 60, 255};
                badge_bg = Color{20, 160, 60, 40};
                break;
            case DecoderPhase::DONE:
                phase_str = "DONE";
                phase_col = Color{30, 150, 30, 255};
                badge_bg = Color{30, 150, 30, 40};
                break;
            default: break;
        }

        // Draw badge background
        Vector2 badge_size = MeasureTextEx(panel_font_, phase_str, 16, 1);
        DrawRectangle((int)px - 2, (int)py - 1, (int)badge_size.x + 18, (int)badge_size.y + 6, badge_bg);
        DrawRectangle((int)px - 2, (int)py - 1, 4, (int)badge_size.y + 6, phase_col); // left accent bar
        py += draw_line(phase_str, px + 8, py + 1, 16, phase_col);
        py += 4;

        // Syndrome Validation details
        if (snap->phase == DecoderPhase::SYNDROME_VALIDATION) {
            // Sub-phase with colored dot
            const char* sub_str = "GROW";
            Color sub_col = Color{30, 150, 30, 255};
            switch (snap->syndrome_sub_phase) {
                case SyndromeSubPhase::GROW:
                    sub_str = "GROW"; sub_col = Color{30, 150, 30, 255}; break;
                case SyndromeSubPhase::MERGE:
                    sub_str = "MERGE"; sub_col = Color{200, 160, 20, 255}; break;
                case SyndromeSubPhase::DEACTIVATE:
                    sub_str = "DEACTIVATE"; sub_col = Color{180, 50, 50, 255}; break;
            }
            DrawCircle((int)px + 4, (int)py + 8, 5, sub_col);
            py += draw_line(TextFormat("Sub-phase: %s", sub_str), px + 14, py, 16, info_col);

            py += draw_line(TextFormat("Cycle: %d", snap->cycle_number), px, py, 16, info_col);

            // Cluster counts
            int n_active = 0, n_inactive = 0, n_boundary = 0;
            for (int c = 0; c < (int)snap->clusters_activity.size(); c++) {
                switch (snap->clusters_activity[c]) {
                    case 1: n_active++; break;
                    case 2: n_inactive++; break;
                    case 3: n_boundary++; break;
                    default: break;
                }
            }
            py += draw_line(TextFormat("Active: %d  Inact: %d", n_active, n_inactive), px, py, 14, info_col);
            py += draw_line(TextFormat("Boundary: %d", n_boundary), px, py, 14, info_col);

            // Active clusters section
            py += 4;
            py += draw_line("--- Active Clusters ---", px, py, 14, heading_col);

            int shown = 0;
            for (int c = 0; c < (int)snap->clusters_activity.size() && shown < 8; c++) {
                if (snap->clusters_activity[c] != 1) continue;
                int cluster_size = 0;
                for (int i = 0; i < (int)snap->clusters_nodes[c].size(); i++) {
                    if (snap->clusters_nodes[c][i] != 0) cluster_size++;
                }
                Color cc = get_cluster_color(c);
                DrawRectangle((int)px, (int)py + 3, 10, 10, cc);
                py += draw_line(TextFormat("C%d: %d nodes", c, cluster_size), px + 14, py, 14, info_col);
                shown++;
            }
        }

        // Spanning Forest details
        if (snap->phase == DecoderPhase::SPANNING_FOREST) {
            py += draw_line(TextFormat("Cycle: %d", snap->cycle_number), px, py, 16, info_col);

            int trees_built = 0;
            int total_active = 0;
            for (int c = 0; c < (int)snap->clusters_activity.size(); c++) {
                if (snap->clusters_activity[c] == 1) total_active++;
            }
            for (int t = 0; t < (int)snap->spanning_forest.size(); t++) {
                bool has_edges = false;
                for (int n = 0; n < (int)snap->spanning_forest[t].size(); n++) {
                    if (snap->spanning_forest[t][n] >= 0) { has_edges = true; break; }
                }
                if (has_edges) trees_built++;
            }
            py += draw_line(TextFormat("Trees built: %d / %d", trees_built, total_active), px, py, 16, info_col);
        }

        // Forest Peeling details
        if (snap->phase == DecoderPhase::FOREST_PEELING) {
            int corr_count = 0;
            for (int e = 0; e < (int)snap->edge_corrections.size(); e++) {
                if (snap->edge_corrections[e] != 0) corr_count++;
            }
            py += draw_line(TextFormat("Corrections found: %d", corr_count), px, py, 16, info_col);
        }

        // Done
        if (snap->phase == DecoderPhase::DONE) {
            int corr_count = 0;
            for (int e = 0; e < (int)snap->edge_corrections.size(); e++) {
                if (snap->edge_corrections[e] != 0) corr_count++;
            }
            py += draw_line("Decoding complete", px, py, 16, Color{30, 150, 30, 255});
            py += draw_line(TextFormat("Total corrections: %d", corr_count), px, py, 16, info_col);
            py += draw_line(TextFormat("Cycles used: %d", snap->cycle_number), px, py, 16, info_col);
        }

        py += draw_divider(py);
    }

    // Status text (split by newlines)
    {
        std::istringstream iss(status);
        std::string line;
        while (std::getline(iss, line)) {
            py += draw_line(line.c_str(), px, py, 14, Color{40, 80, 160, 255});
        }
    }
    py += 8;
    py += draw_divider(py);

    // Legend
    py += draw_line("--- Legend ---", px, py, 14, heading_col);

    auto legend_dot = [&](Color col, const char* label) {
        DrawCircle((int)px + 6, (int)py + 7, 5, col);
        py += draw_line(label, px + 16, py, 14, info_col);
    };

    auto legend_line = [&](Color col, const char* label) {
        DrawRectangle((int)px, (int)py + 6, 14, 3, col);
        py += draw_line(label, px + 18, py, 14, info_col);
    };

    legend_dot(Color{60, 130, 230, 255}, "Z-detector (blue)");
    legend_dot(Color{230, 70, 60, 255}, "X-detector (red)");
    legend_dot(Color{60, 130, 230, 180}, "Fired (large)");
    legend_line(Color{40, 50, 70, 220}, "Spanning tree");
    legend_line(Color{50, 255, 100, 255}, "Correction");

    py += 8;
    py += draw_divider(py);

    // Controls
    py += draw_line("--- Controls ---", px, py, 14, heading_col);

    const char* controls[] = {
        "D: Start decode",
        "Space/Right: Step",
        "A: Auto-step",
        "+/-: Step speed",
        "F: Fast-forward",
        "R: Reset",
        "1: Time planes",
        "2: Graph edges",
        "3: Lattice underlay",
        "Mouse: Rotate",
        "Scroll: Zoom",
    };
    for (const char* c : controls) {
        py += draw_line(c, px, py, 13, heading_col);
    }
}
