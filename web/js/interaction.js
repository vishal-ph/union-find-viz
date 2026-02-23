import * as THREE from 'three';
import { toWorld } from './colors.js';

export class Interaction {
    constructor(scene, renderer, graph, syndrome) {
        this.threeScene = scene;
        this.vizRenderer = renderer;
        this.graph = graph;
        this.syndrome = syndrome;
        this.raycaster = new THREE.Raycaster();
        this.mouse = new THREE.Vector2();
        this.tooltip = document.getElementById('tooltip');
        this.inspectorContent = document.getElementById('inspector-content');
        this.selectedNode = -1;

        this._onMouseMove = this._onMouseMove.bind(this);
        this._onClick = this._onClick.bind(this);

        scene.canvas.addEventListener('mousemove', this._onMouseMove);
        scene.canvas.addEventListener('click', this._onClick);
    }

    _updateMouse(event) {
        const rect = this.threeScene.canvas.getBoundingClientRect();
        this.mouse.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
        this.mouse.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;
    }

    _onMouseMove(event) {
        this._updateMouse(event);

        const detMesh = this.vizRenderer.getDetectorMesh();
        if (!detMesh || !this.graph) {
            this.tooltip.classList.add('hidden');
            return;
        }

        this.raycaster.setFromCamera(this.mouse, this.threeScene.camera);
        const hits = this.raycaster.intersectObject(detMesh);

        if (hits.length > 0) {
            const instanceId = hits[0].instanceId;
            if (instanceId !== undefined && instanceId < this.graph.numDetectors) {
                const det = this.graph.detectors[instanceId];
                const fired = this.syndrome && this.syndrome[instanceId] === 1;
                const type = det.isXType ? 'X' : 'Z';

                this.tooltip.textContent = `D${det.id} (${type}) ${fired ? 'FIRED' : ''}`;
                this.tooltip.style.left = (event.clientX + 12) + 'px';
                this.tooltip.style.top = (event.clientY - 8) + 'px';
                this.tooltip.classList.remove('hidden');
                this.threeScene.canvas.style.cursor = 'pointer';
                return;
            }
        }

        this.tooltip.classList.add('hidden');
        this.threeScene.canvas.style.cursor = 'default';
    }

    _onClick(event) {
        this._updateMouse(event);

        const detMesh = this.vizRenderer.getDetectorMesh();
        if (!detMesh || !this.graph) return;

        this.raycaster.setFromCamera(this.mouse, this.threeScene.camera);
        const hits = this.raycaster.intersectObject(detMesh);

        if (hits.length > 0) {
            const instanceId = hits[0].instanceId;
            if (instanceId !== undefined && instanceId < this.graph.numDetectors) {
                this.selectedNode = instanceId;
                this._updateInspector(instanceId);
                return;
            }
        }
    }

    _updateInspector(nodeId) {
        const det = this.graph.detectors[nodeId];
        const fired = this.syndrome && this.syndrome[nodeId] === 1;
        const type = det.isXType ? 'X-type' : 'Z-type';

        let html = `
            <div class="detail-row"><span class="detail-label">ID</span><span class="detail-value">D${det.id}</span></div>
            <div class="detail-row"><span class="detail-label">Type</span><span class="detail-value">${type}</span></div>
            <div class="detail-row"><span class="detail-label">Coords</span><span class="detail-value">(${det.x}, ${det.y}, ${det.z})</span></div>
            <div class="detail-row"><span class="detail-label">Fired</span><span class="detail-value" style="color:${fired ? '#f87171' : '#6b7280'}">${fired ? 'Yes' : 'No'}</span></div>
        `;

        // Check cluster ownership from latest snapshot
        const snapshot = window._latestSnapshot;
        if (snapshot && snapshot.clusters) {
            let clusterInfo = 'None';
            for (const cluster of snapshot.clusters) {
                for (const entry of cluster.nodes) {
                    if (entry.node === nodeId) {
                        clusterInfo = `C${cluster.id} (state=${entry.state})`;
                        break;
                    }
                }
            }
            html += `<div class="detail-row"><span class="detail-label">Cluster</span><span class="detail-value">${clusterInfo}</span></div>`;
        }

        this.inspectorContent.innerHTML = html;
    }

    dispose() {
        this.threeScene.canvas.removeEventListener('mousemove', this._onMouseMove);
        this.threeScene.canvas.removeEventListener('click', this._onClick);
    }
}
