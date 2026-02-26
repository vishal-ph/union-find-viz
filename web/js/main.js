import { Scene } from './scene.js';
import { Renderer } from './renderer.js';
import { UIController } from './ui-controller.js';
import { Interaction } from './interaction.js';

let scene, vizRenderer, uiController, interaction;
let wasmModule = null;
let graphLoaded = false;

// ============================================================
// Initialize Wasm module
// ============================================================

async function initWasm() {
    // DecoderModule is loaded via <script src="decoder.js"> as a global
    wasmModule = await DecoderModule();
    console.log('Wasm module loaded');
}

// ============================================================
// Load graph data and build scene
// ============================================================

function onGraphLoaded() {
    // Embind val::object()/val::array() returns are plain JS objects/arrays
    const graphData = wasmModule.getGraphData();
    const syndrome = wasmModule.getSyndrome();

    console.log(`Graph: ${graphData.numDetectors} detectors, ${graphData.numRounds} rounds, ${graphData.graphEdges.length} edges`);
    console.log(`Fired: ${wasmModule.getFiredCount()} detectors`);

    // Hide loading screen, show panels
    document.getElementById('loading-screen').classList.add('hidden');
    document.getElementById('graph-stats').classList.remove('hidden');
    document.getElementById('controls-section').classList.remove('hidden');
    document.getElementById('layers-section').classList.remove('hidden');
    document.getElementById('inspector-section').classList.remove('hidden');
    document.getElementById('legend-section').classList.remove('hidden');

    // Update stats
    document.getElementById('stat-detectors').textContent = graphData.numDetectors;
    document.getElementById('stat-rounds').textContent = graphData.numRounds;
    document.getElementById('stat-edges').textContent = graphData.graphEdges.length;
    document.getElementById('stat-fired').textContent = wasmModule.getFiredCount();

    // Build scene
    scene.computeLayout(graphData);
    vizRenderer.buildStaticLayers(graphData, syndrome);

    // Setup interaction
    interaction = new Interaction(scene, vizRenderer, graphData, syndrome);

    // Setup UI controller with a wrapper that handles snapshot conversion
    const wasmWrapper = {
        startDecoding: () => wasmModule.startDecoding(),
        stepDecoder: () => wasmModule.stepDecoder(),
        runToCompletion: () => wasmModule.runToCompletion(),
        isDecoderDone: () => wasmModule.isDecoderDone(),
        resetDecoder: () => wasmModule.resetDecoder(),
        getSnapshot: () => {
            // Returns plain JS object from Embind val
            const snap = wasmModule.getSnapshot();
            if (snap) window._latestSnapshot = snap;
            return snap;
        }
    };

    uiController = new UIController(wasmWrapper, vizRenderer);
    graphLoaded = true;
    window._latestSnapshot = null;
}

// ============================================================
// File loading
// ============================================================

const PRESETS = [
    { dem: 'data/d3_r3.dem',   events: 'data/d3_r3_events.txt'   },
    { dem: 'data/d5_r5.dem',   events: 'data/d5_r5_events.txt'   },
    { dem: 'data/d7_r7.dem',   events: 'data/d7_r7_events.txt'   },
    { dem: 'data/d9_r9.dem',   events: 'data/d9_r9_events.txt'   },
    { dem: 'data/d25_r10.dem', events: 'data/d25_r10_events.txt' },
];

async function loadPreset(idx) {
    const preset = PRESETS[idx];
    try {
        const [demResponse, eventsResponse] = await Promise.all([
            fetch(preset.dem),
            fetch(preset.events)
        ]);

        if (!demResponse.ok || !eventsResponse.ok) {
            throw new Error('HTTP error loading preset files');
        }

        const demContent = await demResponse.text();
        const eventsContent = await eventsResponse.text();

        wasmModule.loadDemContent(demContent, eventsContent);
        onGraphLoaded();
    } catch (err) {
        console.error('Failed to load preset:', err);
        alert('Failed to load preset files.');
    }
}

function handleFiles(files) {
    let demFile = null, eventsFile = null;

    for (const f of files) {
        if (f.name.endsWith('.dem')) demFile = f;
        else if (f.name.endsWith('.txt')) eventsFile = f;
    }

    if (!demFile) {
        alert('Please provide a .dem file');
        return;
    }
    if (!eventsFile) {
        alert('Please provide an events .txt file');
        return;
    }

    Promise.all([demFile.text(), eventsFile.text()]).then(([demContent, eventsContent]) => {
        wasmModule.loadDemContent(demContent, eventsContent);
        onGraphLoaded();
    });
}

// ============================================================
// Reset graph — return to loading screen
// ============================================================

function resetGraph() {
    // Dispose interaction listeners
    if (interaction) {
        interaction.dispose();
        interaction = null;
    }

    // Clear all renderer layers
    vizRenderer.clearAll();

    // Reset state
    uiController = null;
    graphLoaded = false;
    window._latestSnapshot = null;

    // Re-show loading screen, hide panel sections
    document.getElementById('loading-screen').classList.remove('hidden');
    document.getElementById('graph-stats').classList.add('hidden');
    document.getElementById('controls-section').classList.add('hidden');
    document.getElementById('layers-section').classList.add('hidden');
    document.getElementById('inspector-section').classList.add('hidden');
    document.getElementById('legend-section').classList.add('hidden');
    document.getElementById('phase-section').classList.add('hidden');
    document.getElementById('phase-details').innerHTML = '';

    // Re-enable preset buttons
    document.querySelectorAll('.preset-card').forEach(btn => {
        btn.disabled = false;
        btn.style.opacity = '';
    });

    // Reset inspector
    document.getElementById('inspector-content').innerHTML =
        '<p class="hint">Click a detector node to inspect</p>';
}

// ============================================================
// Drag and drop
// ============================================================

function setupDragDrop() {
    const overlay = document.getElementById('drop-overlay');

    document.addEventListener('dragenter', (e) => {
        e.preventDefault();
        overlay.classList.remove('hidden');
    });

    overlay.addEventListener('dragover', (e) => {
        e.preventDefault();
    });

    overlay.addEventListener('dragleave', (e) => {
        if (e.target === overlay) {
            overlay.classList.add('hidden');
        }
    });

    overlay.addEventListener('drop', (e) => {
        e.preventDefault();
        overlay.classList.add('hidden');
        handleFiles(e.dataTransfer.files);
    });
}

// ============================================================
// Animation loop
// ============================================================

function animate(timestamp) {
    requestAnimationFrame(animate);

    scene.update();

    if (uiController) {
        uiController.update(timestamp);
    }

    scene.render();
}

// ============================================================
// Init
// ============================================================

async function init() {
    const canvas = document.getElementById('viewport');
    scene = new Scene(canvas);
    vizRenderer = new Renderer(scene.scene);

    setupDragDrop();

    // File input handler
    document.getElementById('file-input').addEventListener('change', (e) => {
        handleFiles(e.target.files);
    });

    // Preset card buttons
    document.querySelectorAll('.preset-card').forEach(btn => {
        btn.addEventListener('click', async () => {
            const idx = parseInt(btn.dataset.preset, 10);
            btn.disabled = true;
            btn.style.opacity = '0.6';
            await loadPreset(idx);
        });
    });

    // New Graph button
    document.getElementById('btn-new-graph').addEventListener('click', () => {
        resetGraph();
    });

    // N key for New Graph (when graph is loaded)
    document.addEventListener('keydown', (e) => {
        if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
        if (e.key.toLowerCase() === 'n' && graphLoaded) {
            resetGraph();
        }
    });

    // Init wasm
    await initWasm();

    // Start render loop
    animate(0);
}

init().catch(console.error);
