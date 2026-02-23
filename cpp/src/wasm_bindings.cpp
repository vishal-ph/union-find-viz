#include <emscripten/bind.h>
#include <emscripten/val.h>
#include "dem_parser.h"
#include "decoder_stepper.h"
#include <sstream>
#include <memory>

using namespace emscripten;

// ============================================================
// DEM Parser wrappers — accept string content instead of file paths
// ============================================================

static DecodingGraph3D g_graph;
static std::vector<int> g_syndrome;
static std::unique_ptr<DecoderStepper> g_stepper;

// Parse DEM content from a string (same logic as parse_dem_file but from text)
static DecodingGraph3D parse_dem_string(const std::string& content);

// Forward declarations for the internal parsing helpers (duplicated from dem_parser.cpp
// since they are static there — we reuse parse_dem_file by writing to a temp approach,
// but it's cleaner to just call the existing functions with a workaround)

// We'll create a simple wrapper that writes to a temporary in-memory approach.
// Actually, the cleanest way is to expose string-based parsing.
// Let's create wrapper functions that the JS side can call.

// ----------- Graph data accessors -----------

val getGraphData() {
    val result = val::object();
    result.set("numDetectors", g_graph.num_detectors);
    result.set("numRounds", g_graph.num_rounds);
    result.set("numEdges", (int)g_graph.graph_edges.size());
    result.set("numBoundaryNodes", (int)g_graph.boundary_node_indices.size());
    result.set("numNodes", (int)g_graph.node_positions.size());

    // Detector data: [id, x, y, z, isXType] flattened
    val detectors = val::array();
    for (int i = 0; i < g_graph.num_detectors; i++) {
        val det = val::object();
        det.set("id", g_graph.detectors[i].id);
        det.set("x", g_graph.detectors[i].coords.x);
        det.set("y", g_graph.detectors[i].coords.y);
        det.set("z", g_graph.detectors[i].coords.z);
        det.set("isXType", g_graph.detectors[i].is_x_type);
        detectors.call<void>("push", det);
    }
    result.set("detectors", detectors);

    // Node positions (all nodes including boundary)
    val positions = val::array();
    for (auto& p : g_graph.node_positions) {
        val pos = val::object();
        pos.set("x", p.x);
        pos.set("y", p.y);
        pos.set("z", p.z);
        positions.call<void>("push", pos);
    }
    result.set("nodePositions", positions);

    // Graph edges
    val edges = val::array();
    for (auto& ge : g_graph.graph_edges) {
        val e = val::object();
        e.set("node0", ge.node0);
        e.set("node1", ge.node1);
        e.set("probability", ge.probability);
        e.set("isBoundary", ge.is_boundary);
        val obs = val::array();
        for (int o : ge.observables) obs.call<void>("push", o);
        e.set("observables", obs);
        edges.call<void>("push", e);
    }
    result.set("graphEdges", edges);

    // UF edges
    val ufEdges = val::array();
    for (auto& ue : g_graph.uf_edges) {
        val e = val::array();
        e.call<void>("push", ue[0]);
        e.call<void>("push", ue[1]);
        ufEdges.call<void>("push", e);
    }
    result.set("ufEdges", ufEdges);

    // Boundary node indices
    val boundaryNodes = val::array();
    for (int b : g_graph.boundary_node_indices) {
        boundaryNodes.call<void>("push", b);
    }
    result.set("boundaryNodeIndices", boundaryNodes);

    // Lattice info
    if (g_graph.lattice.valid) {
        val lattice = val::object();
        lattice.set("valid", true);
        lattice.set("minX", g_graph.lattice.min_x);
        lattice.set("maxX", g_graph.lattice.max_x);
        lattice.set("minY", g_graph.lattice.min_y);
        lattice.set("maxY", g_graph.lattice.max_y);

        val faces = val::array();
        for (auto& f : g_graph.lattice.faces) {
            val face = val::object();
            face.set("centerX", f.center.x);
            face.set("centerY", f.center.y);
            face.set("halfW", f.half_w);
            face.set("halfH", f.half_h);
            face.set("isXType", f.is_x_type);
            faces.call<void>("push", face);
        }
        lattice.set("faces", faces);
        result.set("lattice", lattice);
    }

    return result;
}

val getSyndrome() {
    val arr = val::array();
    for (int s : g_syndrome) {
        arr.call<void>("push", s);
    }
    return arr;
}

// ----------- Load functions -----------

// We need a string-based version of the parser.
// The simplest approach: use a temp file via emscripten's virtual FS,
// or better, modify to accept strings. Let's write inline string-based versions.

static std::string g_dem_content;
static std::string g_events_content;

// read_lines from string content
static std::vector<std::string> read_lines_from_string(const std::string& content) {
    std::vector<std::string> lines;
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        // Trim
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t\r\n");
        lines.push_back(line.substr(start, end - start + 1));
    }
    return lines;
}

void loadDemContent(const std::string& demContent, const std::string& eventsContent) {
    g_dem_content = demContent;
    g_events_content = eventsContent;

    // Write to temp files in emscripten virtual FS and use existing parser
    // Actually, we can just write temporary files
    {
        FILE* f = fopen("/tmp/temp.dem", "w");
        if (f) { fwrite(demContent.c_str(), 1, demContent.size(), f); fclose(f); }
    }
    {
        FILE* f = fopen("/tmp/temp_events.txt", "w");
        if (f) { fwrite(eventsContent.c_str(), 1, eventsContent.size(), f); fclose(f); }
    }

    g_graph = parse_dem_file("/tmp/temp.dem");
    g_syndrome = load_detection_events("/tmp/temp_events.txt", g_graph.num_detectors);

    g_stepper.reset();
}

int getFiredCount() {
    int count = 0;
    for (int s : g_syndrome) count += s;
    return count;
}

// ----------- Decoder stepper -----------

void startDecoding() {
    g_stepper = std::make_unique<DecoderStepper>(
        g_graph.uf_edges,
        g_graph.uf_boundary_nodes,
        g_syndrome);
}

bool stepDecoder() {
    if (!g_stepper) return false;
    return g_stepper->step();
}

void runToCompletion() {
    if (g_stepper) g_stepper->run_to_completion();
}

bool isDecoderDone() {
    if (!g_stepper) return true;
    return g_stepper->is_done();
}

void resetDecoder() {
    g_stepper.reset();
}

void resetDecoderWithSyndrome() {
    if (g_stepper) {
        g_stepper->reset(g_syndrome);
    }
}

// ----------- Snapshot accessors -----------
// These return the current decoder state for visualization.
// To minimize overhead, we return flat typed arrays where possible.

int getPhase() {
    if (!g_stepper) return -1; // no decoder
    switch (g_stepper->current_phase()) {
        case DecoderPhase::IDLE: return 0;
        case DecoderPhase::SYNDROME_VALIDATION: return 1;
        case DecoderPhase::SPANNING_FOREST: return 2;
        case DecoderPhase::FOREST_PEELING: return 3;
        case DecoderPhase::DONE: return 4;
    }
    return -1;
}

int getSubPhase() {
    if (!g_stepper) return 0;
    auto& snap = g_stepper->current_snapshot();
    switch (snap.syndrome_sub_phase) {
        case SyndromeSubPhase::GROW: return 0;
        case SyndromeSubPhase::MERGE: return 1;
        case SyndromeSubPhase::DEACTIVATE: return 2;
    }
    return 0;
}

int getCycleNumber() {
    if (!g_stepper) return 0;
    return g_stepper->current_snapshot().cycle_number;
}

// Get snapshot data as a single comprehensive JS object
val getSnapshot() {
    if (!g_stepper) return val::null();

    auto& snap = g_stepper->current_snapshot();
    val result = val::object();

    result.set("phase", getPhase());
    result.set("subPhase", getSubPhase());
    result.set("cycleNumber", snap.cycle_number);

    int nClusters = (int)snap.clusters_activity.size();

    // Clusters activity
    val activity = val::array();
    for (int c = 0; c < nClusters; c++) {
        activity.call<void>("push", snap.clusters_activity[c]);
    }
    result.set("clustersActivity", activity);

    // Cluster detector IDs mapping
    val detectorIds = val::array();
    for (int id : snap.cluster_detector_ids) {
        detectorIds.call<void>("push", id);
    }
    result.set("clusterDetectorIds", detectorIds);

    // Clusters nodes — sparse format
    val clusterNodes = val::array();
    for (int c = 0; c < nClusters; c++) {
        if (snap.clusters_activity[c] == 0) continue;
        val cluster = val::object();
        cluster.set("id", c);
        val nodes = val::array();
        for (auto& [n, state] : snap.clusters_nodes[c]) {
            if (state != 0) {
                val entry = val::object();
                entry.set("node", n);
                entry.set("state", state);
                nodes.call<void>("push", entry);
            }
        }
        cluster.set("nodes", nodes);

        val edges = val::array();
        for (auto& [e, state] : snap.clusters_edges[c]) {
            if (state != 0) {
                val entry = val::object();
                entry.set("edge", e);
                entry.set("state", state);
                edges.call<void>("push", entry);
            }
        }
        cluster.set("edges", edges);
        clusterNodes.call<void>("push", cluster);
    }
    result.set("clusters", clusterNodes);

    // Spanning forest
    val forest = val::array();
    for (int t = 0; t < (int)snap.spanning_forest.size(); t++) {
        if (snap.spanning_forest[t].empty()) continue;

        val tree = val::object();
        tree.set("clusterId", t);
        val treeEdges = val::array();
        val roots = val::array();
        for (auto& [n, edgeIdx] : snap.spanning_forest[t]) {
            if (edgeIdx >= 0) {
                val entry = val::object();
                entry.set("node", n);
                entry.set("edge", edgeIdx);
                treeEdges.call<void>("push", entry);
            } else if (edgeIdx == -1) {
                roots.call<void>("push", n);
            }
        }
        tree.set("edges", treeEdges);
        tree.set("roots", roots);
        forest.call<void>("push", tree);
    }
    result.set("spanningForest", forest);

    // Edge corrections
    val corrections = val::array();
    for (int e = 0; e < (int)snap.edge_corrections.size(); e++) {
        if (snap.edge_corrections[e] != 0) {
            corrections.call<void>("push", e);
        }
    }
    result.set("corrections", corrections);

    return result;
}

// ----------- Embind module -----------

EMSCRIPTEN_BINDINGS(union_find_viz) {
    function("loadDemContent", &loadDemContent);
    function("getGraphData", &getGraphData);
    function("getSyndrome", &getSyndrome);
    function("getFiredCount", &getFiredCount);
    function("startDecoding", &startDecoding);
    function("stepDecoder", &stepDecoder);
    function("runToCompletion", &runToCompletion);
    function("isDecoderDone", &isDecoderDone);
    function("resetDecoder", &resetDecoder);
    function("getPhase", &getPhase);
    function("getSubPhase", &getSubPhase);
    function("getCycleNumber", &getCycleNumber);
    function("getSnapshot", &getSnapshot);
}
