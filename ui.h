#ifndef UI_H
#define UI_H

#include "dem_parser.h"
#include "decoder_stepper.h"
#include <string>
#include <memory>

enum class AppMode {
    READY,
    DECODING,
    DECODE_DONE
};

class UI {
public:
    UI(const std::string& dem_path, const std::string& events_path);

    void handle_input();
    void update();

    AppMode mode() const { return mode_; }
    const DecodingGraph3D& graph() const { return graph_; }
    const std::vector<int>& syndrome() const { return syndrome_; }
    const DecoderSnapshot* decoder_snapshot() const;
    std::string status_text() const;
    std::string mode_text() const;
    bool show_time_planes() const { return show_time_planes_; }
    bool show_graph_edges() const { return show_graph_edges_; }

private:
    AppMode mode_;
    DecodingGraph3D graph_;
    std::vector<int> syndrome_;
    std::unique_ptr<DecoderStepper> stepper_;

    float step_timer_;
    float step_interval_;
    bool auto_stepping_;

    bool show_time_planes_;
    bool show_graph_edges_;

    void start_decoding();
    void step_decoder();
    void reset();
};

#endif // UI_H
