#ifndef RENDERER_H
#define RENDERER_H

#include "raylib.h"
#include "dem_parser.h"
#include "decoder_stepper.h"
#include <string>
#include <vector>

Color get_cluster_color(int cluster_index);

class Renderer {
public:
    Renderer(int screen_width, int screen_height);

    void render(
        const DecodingGraph3D& graph,
        const std::vector<int>& syndrome,
        const DecoderSnapshot* snapshot,
        const std::string& status_text,
        const std::string& mode_text,
        bool show_time_planes,
        bool show_graph_edges,
        bool show_lattice_underlay
    );

    void update_camera();

    Camera3D& camera() { return camera_; }

private:
    Camera3D camera_;
    int screen_width_, screen_height_;

    // Computed from graph
    Vector3 graph_center_;
    float graph_radius_;
    bool layout_computed_ = false;

    // Panel font
    Font panel_font_;

    // Orbital camera state
    float orbit_angle_h_ = 0.7f;   // horizontal angle (radians)
    float orbit_angle_v_ = 0.6f;   // vertical angle (radians)
    float orbit_distance_ = 30.0f;
    Vector2 prev_mouse_ = {0, 0};
    bool dragging_ = false;

    void compute_layout(const DecodingGraph3D& graph);
    Vector3 to_world(const Vec3& v) const;

    // 3D rendering layers
    void render_time_planes(const DecodingGraph3D& graph);
    void render_lattice_underlay(const DecodingGraph3D& graph);
    void render_graph_edges(const DecodingGraph3D& graph, bool dim);
    void render_detector_nodes(const DecodingGraph3D& graph, const std::vector<int>& syndrome,
                               const DecoderSnapshot* snapshot);
    void render_boundary_nodes(const DecodingGraph3D& graph);

    // Decoder visualization layers
    void render_cluster_sheaths(const DecodingGraph3D& graph, const DecoderSnapshot& snap);
    void render_cluster_regions(const DecodingGraph3D& graph, const DecoderSnapshot& snap);
    void render_cluster_edges(const DecodingGraph3D& graph, const DecoderSnapshot& snap);
    void render_spanning_forest(const DecodingGraph3D& graph, const DecoderSnapshot& snap);
    void render_corrections(const DecodingGraph3D& graph, const DecoderSnapshot& snap);

    // 2D overlay
    void render_panel(const DecodingGraph3D& graph, const DecoderSnapshot* snap,
                      const std::string& status, const std::string& mode);
};

#endif // RENDERER_H
