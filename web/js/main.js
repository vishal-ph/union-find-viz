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

async function loadDemoFiles() {
    try {
        const [demResponse, eventsResponse] = await Promise.all([
            fetch('/data/test.dem'),
            fetch('/data/test_events.txt')
        ]);

        if (!demResponse.ok || !eventsResponse.ok) {
            throw new Error('HTTP error');
        }

        const demContent = await demResponse.text();
        const eventsContent = await eventsResponse.text();

        wasmModule.loadDemContent(demContent, eventsContent);
        onGraphLoaded();
    } catch (err) {
        console.error('Failed to load demo files:', err);
        alert('Failed to load demo files. Make sure you are serving from the project root.');
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
// Generate circuit via server
// ============================================================

async function generateAndLoad() {
    const distanceEl = document.getElementById('gen-distance');
    const roundsEl = document.getElementById('gen-rounds');
    const pEl = document.getElementById('gen-p');
    const statusEl = document.getElementById('generate-status');
    const errorEl = document.getElementById('generate-error');
    const btnGenerate = document.getElementById('btn-generate');

    const distance = parseInt(distanceEl.value, 10);
    const rounds = parseInt(roundsEl.value, 10);
    const p = parseFloat(pEl.value);

    // Client-side validation
    if (isNaN(distance) || distance < 3 || distance > 50) {
        errorEl.textContent = 'Distance must be between 3 and 50';
        errorEl.classList.add('visible');
        return;
    }
    if (isNaN(rounds) || rounds < 1 || rounds > 100) {
        errorEl.textContent = 'Rounds must be between 1 and 100';
        errorEl.classList.add('visible');
        return;
    }
    if (isNaN(p) || p <= 0 || p >= 1) {
        errorEl.textContent = 'Error rate must be between 0 and 1 (exclusive)';
        errorEl.classList.add('visible');
        return;
    }

    // Show spinner, hide error
    errorEl.classList.remove('visible');
    statusEl.classList.add('visible');
    btnGenerate.disabled = true;

    try {
        const resp = await fetch('/api/generate', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ distance, rounds, p })
        });

        const text = await resp.text();
        let data;
        try {
            data = JSON.parse(text);
        } catch {
            throw new Error(
                resp.ok
                    ? 'Server returned invalid JSON'
                    : `Server error (${resp.status}). Is py/server.py running?`
            );
        }

        if (!resp.ok) {
            throw new Error(data.error || `Server error (${resp.status})`);
        }

        wasmModule.loadDemContent(data.dem, data.events);
        onGraphLoaded();
    } catch (err) {
        console.error('Generate failed:', err);
        errorEl.textContent = err.message;
        errorEl.classList.add('visible');
    } finally {
        statusEl.classList.remove('visible');
        btnGenerate.disabled = false;
    }
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

    // Reset loading screen buttons
    const btnDemo = document.getElementById('btn-load-demo');
    btnDemo.disabled = false;
    btnDemo.textContent = 'Load Demo';

    // Reset generate UI
    document.getElementById('generate-error').classList.remove('visible');
    document.getElementById('generate-status').classList.remove('visible');
    document.getElementById('btn-generate').disabled = false;

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

    // Load demo button
    document.getElementById('btn-load-demo').addEventListener('click', async () => {
        document.getElementById('btn-load-demo').disabled = true;
        document.getElementById('btn-load-demo').textContent = 'Loading...';
        await loadDemoFiles();
    });

    // Generate button
    document.getElementById('btn-generate').addEventListener('click', () => {
        generateAndLoad();
    });

    // Enter key in generate inputs
    for (const id of ['gen-distance', 'gen-rounds', 'gen-p']) {
        document.getElementById(id).addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
                e.preventDefault();
                generateAndLoad();
            }
        });
    }

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
