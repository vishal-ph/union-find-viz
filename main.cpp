#include "raylib.h"
#include "dem_parser.h"
#include "decoder_stepper.h"
#include "renderer.h"
#include "ui.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <dem_file> <detection_events_file>" << std::endl;
        return 1;
    }

    std::string dem_path = argv[1];
    std::string events_path = argv[2];

    const int SCREEN_WIDTH = 1400;
    const int SCREEN_HEIGHT = 900;

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Union-Find 3D Decoder Visualization");
    SetTargetFPS(60);

    UI ui(dem_path, events_path);
    Renderer renderer(SCREEN_WIDTH, SCREEN_HEIGHT);

    while (!WindowShouldClose()) {
        ui.handle_input();
        ui.update();
        renderer.update_camera();

        BeginDrawing();
        ClearBackground(Color{20, 20, 30, 255});

        renderer.render(
            ui.graph(),
            ui.syndrome(),
            ui.decoder_snapshot(),
            ui.status_text(),
            ui.mode_text(),
            ui.show_time_planes(),
            ui.show_graph_edges()
        );

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
