// App mode state machine + keyboard/button handlers

const AppMode = {
    READY: 'READY',
    DECODING: 'DECODING',
    DONE: 'DONE'
};

const PhaseNames = ['IDLE', 'SYNDROME VALIDATION', 'SPANNING FOREST', 'FOREST PEELING', 'DONE'];
const SubPhaseNames = ['GROW', 'MERGE', 'DEACTIVATE'];

export class UIController {
    constructor(wasmModule, renderer) {
        this.wasm = wasmModule;
        this.renderer = renderer;
        this.mode = AppMode.READY;
        this.autoStepping = false;
        this.stepInterval = 0.5; // seconds
        this.stepTimer = 0;
        this.lastTime = 0;

        this._bindButtons();
        this._bindKeyboard();
        this._bindToggles();
        this._updateUI();
    }

    _bindButtons() {
        this.$decode = document.getElementById('btn-decode');
        this.$step = document.getElementById('btn-step');
        this.$auto = document.getElementById('btn-auto');
        this.$ff = document.getElementById('btn-ff');
        this.$reset = document.getElementById('btn-reset');
        this.$slower = document.getElementById('btn-slower');
        this.$faster = document.getElementById('btn-faster');
        this.$speedDisplay = document.getElementById('speed-display');

        this.$decode.addEventListener('click', () => this.startDecoding());
        this.$step.addEventListener('click', () => this.stepDecoder());
        this.$auto.addEventListener('click', () => this.toggleAuto());
        this.$ff.addEventListener('click', () => this.fastForward());
        this.$reset.addEventListener('click', () => this.reset());
        this.$slower.addEventListener('click', () => this.adjustSpeed(-0.1));
        this.$faster.addEventListener('click', () => this.adjustSpeed(0.1));
    }

    _bindKeyboard() {
        document.addEventListener('keydown', (e) => {
            // Don't capture when typing in inputs
            if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;

            switch (e.key.toLowerCase()) {
                case 'd':
                    if (this.mode === AppMode.READY) this.startDecoding();
                    break;
                case ' ':
                case 'arrowright':
                    e.preventDefault();
                    if (this.mode === AppMode.DECODING) this.stepDecoder();
                    break;
                case 'a':
                    if (this.mode === AppMode.DECODING) this.toggleAuto();
                    break;
                case 'f':
                    if (this.mode === AppMode.DECODING) this.fastForward();
                    break;
                case 'r':
                    this.reset();
                    break;
                case '=':
                case '+':
                    this.adjustSpeed(0.1);
                    break;
                case '-':
                    this.adjustSpeed(-0.1);
                    break;
                case '1':
                    this._toggleLayer('toggle-time-planes', 'timePlanes');
                    break;
                case '2':
                    this._toggleLayer('toggle-graph-edges', 'graphEdges');
                    break;
                case '3':
                    this._toggleLayer('toggle-lattice', 'lattice');
                    break;
                case '4':
                    this._toggleLayer('toggle-cluster-sheaths', 'clusterSheaths');
                    break;
                case '5':
                    this._toggleLayer('toggle-cluster-edges', 'clusterEdges');
                    break;
            }
        });
    }

    _bindToggles() {
        const toggleMap = {
            'toggle-time-planes': 'timePlanes',
            'toggle-graph-edges': 'graphEdges',
            'toggle-lattice': 'lattice',
            'toggle-cluster-sheaths': 'clusterSheaths',
            'toggle-cluster-edges': 'clusterEdges'
        };

        for (const [id, layer] of Object.entries(toggleMap)) {
            const el = document.getElementById(id);
            if (el) {
                el.addEventListener('change', () => {
                    this.renderer.setVisibility(layer, el.checked);
                });
            }
        }
    }

    _toggleLayer(checkboxId, layerName) {
        const el = document.getElementById(checkboxId);
        if (el) {
            el.checked = !el.checked;
            this.renderer.setVisibility(layerName, el.checked);
        }
    }

    startDecoding() {
        if (this.mode !== AppMode.READY) return;
        this.wasm.startDecoding();
        this.mode = AppMode.DECODING;
        this.autoStepping = false;
        this.stepTimer = 0;
        this._updateUI();
        this._updateSnapshot();
    }

    stepDecoder() {
        if (this.mode !== AppMode.DECODING) return;
        const stepped = this.wasm.stepDecoder();
        if (this.wasm.isDecoderDone()) {
            this.mode = AppMode.DONE;
            this.autoStepping = false;
        }
        this._updateSnapshot();
        this._updateUI();
    }

    toggleAuto() {
        this.autoStepping = !this.autoStepping;
        this.stepTimer = 0;
        this._updateUI();
    }

    fastForward() {
        if (this.mode !== AppMode.DECODING) return;
        this.wasm.runToCompletion();
        this.autoStepping = false;
        if (this.wasm.isDecoderDone()) {
            this.mode = AppMode.DONE;
        }
        this._updateSnapshot();
        this._updateUI();
    }

    reset() {
        this.wasm.resetDecoder();
        this.mode = AppMode.READY;
        this.autoStepping = false;
        this.renderer.resetDynamicLayers();
        this._updateUI();
        document.getElementById('phase-section').classList.add('hidden');
        document.getElementById('phase-details').innerHTML = '';
    }

    adjustSpeed(delta) {
        // delta positive = faster (decrease interval)
        this.stepInterval = Math.max(0.05, Math.min(3.0, this.stepInterval - delta));
        this.$speedDisplay.textContent = this.stepInterval.toFixed(2) + 's';
    }

    update(timestamp) {
        if (this.mode === AppMode.DECODING && this.autoStepping) {
            const dt = this.lastTime > 0 ? (timestamp - this.lastTime) / 1000 : 0;
            this.stepTimer += dt;
            if (this.stepTimer >= this.stepInterval) {
                this.stepTimer = 0;
                this.stepDecoder();
            }
        }
        this.lastTime = timestamp;
    }

    _updateSnapshot() {
        const snapshot = this.wasm.getSnapshot();
        if (snapshot) {
            this.renderer.updateDynamicLayers(snapshot);
            this._updatePhaseDetails(snapshot);
        }
    }

    _updateUI() {
        const isReady = this.mode === AppMode.READY;
        const isDecoding = this.mode === AppMode.DECODING;
        const isDone = this.mode === AppMode.DONE;

        this.$decode.disabled = !isReady;
        this.$step.disabled = !isDecoding;
        this.$auto.disabled = !isDecoding;
        this.$ff.disabled = !isDecoding;

        // Auto indicator
        if (this.autoStepping) {
            this.$auto.classList.add('btn-active-indicator');
        } else {
            this.$auto.classList.remove('btn-active-indicator');
        }

        // Mode badge
        const modeBadge = document.getElementById('mode-badge');
        modeBadge.textContent = this.mode;
        modeBadge.className = 'badge';
        if (isReady) modeBadge.classList.add('badge-ready');
        else if (isDecoding) modeBadge.classList.add('badge-decoding');
        else if (isDone) modeBadge.classList.add('badge-done');
    }

    _updatePhaseDetails(snapshot) {
        const phaseSection = document.getElementById('phase-section');
        const phaseBadge = document.getElementById('phase-badge');
        const phaseDetails = document.getElementById('phase-details');

        phaseSection.classList.remove('hidden');

        const phaseName = PhaseNames[snapshot.phase] || 'IDLE';
        phaseBadge.textContent = phaseName;
        phaseBadge.className = 'badge';

        switch (snapshot.phase) {
            case 1: phaseBadge.classList.add('badge-syndrome'); break;
            case 2: phaseBadge.classList.add('badge-forest'); break;
            case 3: phaseBadge.classList.add('badge-peeling'); break;
            case 4: phaseBadge.classList.add('badge-done'); break;
            default: phaseBadge.classList.add('badge-idle'); break;
        }

        let html = '';

        if (snapshot.phase === 1) {
            // Syndrome validation
            const subName = SubPhaseNames[snapshot.subPhase] || 'GROW';
            const dotColors = ['#1ea63c', '#c8a014', '#b43232'];
            html += `<div class="sub-phase-row">
                <span class="sub-phase-dot" style="background:${dotColors[snapshot.subPhase]}"></span>
                <span>Sub-phase: ${subName}</span>
            </div>`;
            html += `<div class="stat-row"><span class="stat-label">Cycle</span><span class="stat-value">${snapshot.cycleNumber}</span></div>`;

            // Cluster counts
            const activity = snapshot.clustersActivity;
            let nActive = 0, nInactive = 0, nBoundary = 0;
            for (const a of activity) {
                if (a === 1) nActive++;
                else if (a === 2) nInactive++;
                else if (a === 3) nBoundary++;
            }
            html += `<div class="stat-row"><span class="stat-label">Active</span><span class="stat-value">${nActive}</span></div>`;
            html += `<div class="stat-row"><span class="stat-label">Inactive</span><span class="stat-value">${nInactive}</span></div>`;
            html += `<div class="stat-row"><span class="stat-label">Boundary</span><span class="stat-value">${nBoundary}</span></div>`;

            // Active clusters detail
            if (snapshot.clusters && nActive > 0) {
                html += `<div style="margin-top:6px"><span class="stat-label">Active Clusters</span></div>`;
                let shown = 0;
                for (const cluster of snapshot.clusters) {
                    if (shown >= 8) break;
                    const a = activity[cluster.id];
                    if (a !== 1) continue;
                    const nodeCount = cluster.nodes.length;
                    const cc = getClusterColorCSS(cluster.id);
                    html += `<div class="cluster-item">
                        <span class="cluster-swatch" style="background:${cc}"></span>
                        C${cluster.id}: ${nodeCount} nodes
                    </div>`;
                    shown++;
                }
            }
        } else if (snapshot.phase === 2) {
            html += `<div class="stat-row"><span class="stat-label">Cycle</span><span class="stat-value">${snapshot.cycleNumber}</span></div>`;
            const treesBuilt = snapshot.spanningForest ? snapshot.spanningForest.length : 0;
            html += `<div class="stat-row"><span class="stat-label">Trees built</span><span class="stat-value">${treesBuilt}</span></div>`;
        } else if (snapshot.phase === 3) {
            html += `<div class="stat-row"><span class="stat-label">Corrections</span><span class="stat-value">${snapshot.corrections.length}</span></div>`;
        } else if (snapshot.phase === 4) {
            html += `<div class="stat-row"><span class="stat-label" style="color:#4ade80">Decoding complete</span><span class="stat-value"></span></div>`;
            html += `<div class="stat-row"><span class="stat-label">Total corrections</span><span class="stat-value">${snapshot.corrections.length}</span></div>`;
            html += `<div class="stat-row"><span class="stat-label">Cycles used</span><span class="stat-value">${snapshot.cycleNumber}</span></div>`;
        }

        phaseDetails.innerHTML = html;
    }
}

function getClusterColorCSS(index) {
    const COLORS = [
        'rgb(80,181,191)', 'rgb(89,150,220)', 'rgb(130,120,209)',
        'rgb(176,99,194)', 'rgb(209,94,150)', 'rgb(230,120,89)',
        'rgb(220,161,61)', 'rgb(140,186,89)', 'rgb(94,194,145)',
        'rgb(161,130,199)'
    ];
    return COLORS[index % COLORS.length];
}
