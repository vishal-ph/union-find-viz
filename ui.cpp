#include "ui.h"
#include "raylib.h"
#include <cmath>
#include <algorithm>
#include <iostream>

UI::UI(const std::string& dem_path, const std::string& events_path)
    : mode_(AppMode::READY),
      step_timer_(0.0f),
      step_interval_(0.5f),
      auto_stepping_(false),
      show_time_planes_(true),
      show_graph_edges_(true)
{
    graph_ = parse_dem_file(dem_path);
    syndrome_ = load_detection_events(events_path, graph_.num_detectors);

    int fired = 0;
    for (int s : syndrome_) fired += s;

    std::cout << "Loaded DEM: " << graph_.num_detectors << " detectors, "
              << graph_.graph_edges.size() << " edges, "
              << graph_.boundary_node_indices.size() << " boundary nodes, "
              << graph_.num_rounds << " rounds" << std::endl;
    std::cout << "Detection events: " << fired << " fired detectors" << std::endl;
}

void UI::handle_input() {
    // Toggle visibility
    if (IsKeyPressed(KEY_ONE)) show_time_planes_ = !show_time_planes_;
    if (IsKeyPressed(KEY_TWO)) show_graph_edges_ = !show_graph_edges_;

    // R: reset
    if (IsKeyPressed(KEY_R)) {
        reset();
        return;
    }

    switch (mode_) {
        case AppMode::READY: {
            if (IsKeyPressed(KEY_D)) {
                start_decoding();
            }
            break;
        }
        case AppMode::DECODING: {
            if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_RIGHT)) {
                step_decoder();
            }

            if (IsKeyPressed(KEY_A)) {
                auto_stepping_ = !auto_stepping_;
                step_timer_ = 0;
            }

            if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
                step_interval_ = std::max(0.05f, step_interval_ - 0.1f);
            }
            if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
                step_interval_ = std::min(3.0f, step_interval_ + 0.1f);
            }

            if (IsKeyPressed(KEY_F)) {
                if (stepper_) stepper_->run_to_completion();
                auto_stepping_ = false;
                if (stepper_ && stepper_->is_done()) {
                    mode_ = AppMode::DECODE_DONE;
                }
            }
            break;
        }
        case AppMode::DECODE_DONE: {
            break;
        }
    }
}

void UI::update() {
    if (mode_ == AppMode::DECODING && auto_stepping_) {
        step_timer_ += GetFrameTime();
        if (step_timer_ >= step_interval_) {
            step_timer_ = 0;
            step_decoder();
        }
    }
}

const DecoderSnapshot* UI::decoder_snapshot() const {
    if (stepper_) return &stepper_->current_snapshot();
    return nullptr;
}

std::string UI::status_text() const {
    switch (mode_) {
        case AppMode::READY:
            return "Graph loaded. Press D to start decoding.";
        case AppMode::DECODING: {
            std::string s = "Decoding in progress.\n";
            if (auto_stepping_)
                s += "Auto-stepping (A to stop).\n";
            else
                s += "Press Space to step.\n";
            s += "Speed: " + std::to_string(step_interval_).substr(0, 4) + "s";
            return s;
        }
        case AppMode::DECODE_DONE:
            return "Decoding complete!\nPress R to reset.";
    }
    return "";
}

std::string UI::mode_text() const {
    switch (mode_) {
        case AppMode::READY: return "READY";
        case AppMode::DECODING: return "DECODING";
        case AppMode::DECODE_DONE: return "DECODE DONE";
    }
    return "";
}

void UI::start_decoding() {
    stepper_ = std::make_unique<DecoderStepper>(
        graph_.uf_edges,
        graph_.uf_boundary_nodes,
        syndrome_);
    mode_ = AppMode::DECODING;
    auto_stepping_ = false;
    step_timer_ = 0;
}

void UI::step_decoder() {
    if (stepper_ && !stepper_->is_done()) {
        stepper_->step();
        if (stepper_->is_done()) {
            mode_ = AppMode::DECODE_DONE;
        }
    }
}

void UI::reset() {
    stepper_.reset();
    mode_ = AppMode::READY;
    auto_stepping_ = false;
}
