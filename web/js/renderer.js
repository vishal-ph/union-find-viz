import * as THREE from 'three';
import { ConvexGeometry } from 'three/addons/geometries/ConvexGeometry.js';
import { toWorld, SPATIAL_SCALE, TIME_SCALE, getClusterColor, getClusterColorHex } from './colors.js';

export class Renderer {
    constructor(scene) {
        this.scene = scene;
        this.graph = null;
        this.syndrome = null;

        // Layer groups
        this.timePlanesGroup = new THREE.Group();
        this.latticeGroup = new THREE.Group();
        this.graphEdgesGroup = new THREE.Group();
        this.detectorNodesGroup = new THREE.Group();
        this.boundaryNodesGroup = new THREE.Group();
        this.clusterEdgesGroup = new THREE.Group();
        this.clusterSheathsGroup = new THREE.Group();
        this.clusterRegionsGroup = new THREE.Group();
        this.spanningForestGroup = new THREE.Group();
        this.correctionsGroup = new THREE.Group();

        scene.add(this.timePlanesGroup);
        scene.add(this.latticeGroup);
        scene.add(this.graphEdgesGroup);
        scene.add(this.clusterSheathsGroup);
        scene.add(this.clusterRegionsGroup);
        scene.add(this.clusterEdgesGroup);
        scene.add(this.spanningForestGroup);
        scene.add(this.correctionsGroup);
        scene.add(this.detectorNodesGroup);
        scene.add(this.boundaryNodesGroup);

        // Visibility
        this.layers = {
            timePlanes: true,
            graphEdges: true,
            lattice: true,
            clusterSheaths: true,
            clusterEdges: true
        };

        // Shared geometries
        this.sphereGeo = new THREE.SphereGeometry(1, 16, 12);
        this.cylinderGeo = new THREE.CylinderGeometry(1, 1, 1, 6);

        // Instanced meshes for detector nodes
        this.detectorMesh = null;
        this.boundaryMesh = null;

        // Dirty flag
        this._lastSnapshotPhase = -1;
        this._lastSnapshotCycle = -1;
        this._lastSnapshotSubPhase = -1;
    }

    setVisibility(layer, visible) {
        this.layers[layer] = visible;
        switch (layer) {
            case 'timePlanes': this.timePlanesGroup.visible = visible; break;
            case 'graphEdges': this.graphEdgesGroup.visible = visible; break;
            case 'lattice': this.latticeGroup.visible = visible; break;
            case 'clusterSheaths': this.clusterSheathsGroup.visible = visible; break;
            case 'clusterEdges': this.clusterEdgesGroup.visible = visible; break;
        }
    }

    buildStaticLayers(graph, syndrome) {
        this.graph = graph;
        this.syndrome = syndrome;

        this._buildTimePlanes();
        this._buildLatticeUnderlay();
        this._buildGraphEdges(false);
        this._buildDetectorNodes(null);
        this._buildBoundaryNodes();
    }

    updateDynamicLayers(snapshot) {
        if (!snapshot || !this.graph) return;

        const decodingActive = snapshot.phase > 0;

        // Dim graph edges during decoding
        this._clearGroup(this.graphEdgesGroup);
        this._buildGraphEdges(decodingActive);

        // Update detector nodes with cluster info
        this._buildDetectorNodes(snapshot);

        // Cluster edges
        this._clearGroup(this.clusterEdgesGroup);
        if (decodingActive) {
            this._buildClusterEdges(snapshot);
        }

        // Cluster sheaths
        this._clearGroup(this.clusterSheathsGroup);
        if (decodingActive && this.layers.clusterSheaths) {
            this._buildClusterSheaths(snapshot);
        }

        // Cluster regions (frontier wireframes)
        this._clearGroup(this.clusterRegionsGroup);
        if (decodingActive) {
            this._buildClusterRegions(snapshot);
        }

        // Spanning forest
        this._clearGroup(this.spanningForestGroup);
        if (snapshot.phase >= 2) { // SPANNING_FOREST or later
            this._buildSpanningForest(snapshot);
        }

        // Corrections
        this._clearGroup(this.correctionsGroup);
        if (snapshot.phase >= 3) { // FOREST_PEELING or DONE
            this._buildCorrections(snapshot);
        }
    }

    clearAll() {
        this._clearGroup(this.timePlanesGroup);
        this._clearGroup(this.latticeGroup);
        this._clearGroup(this.graphEdgesGroup);
        this._clearGroup(this.detectorNodesGroup);
        this._clearGroup(this.boundaryNodesGroup);
        this._clearGroup(this.clusterEdgesGroup);
        this._clearGroup(this.clusterSheathsGroup);
        this._clearGroup(this.clusterRegionsGroup);
        this._clearGroup(this.spanningForestGroup);
        this._clearGroup(this.correctionsGroup);
        this.graph = null;
        this.syndrome = null;
        this.detectorMesh = null;
        this._lastSnapshotPhase = -1;
        this._lastSnapshotCycle = -1;
        this._lastSnapshotSubPhase = -1;
    }

    resetDynamicLayers() {
        this._clearGroup(this.clusterEdgesGroup);
        this._clearGroup(this.clusterSheathsGroup);
        this._clearGroup(this.clusterRegionsGroup);
        this._clearGroup(this.spanningForestGroup);
        this._clearGroup(this.correctionsGroup);

        this._clearGroup(this.graphEdgesGroup);
        this._buildGraphEdges(false);
        this._buildDetectorNodes(null);

        this._lastSnapshotPhase = -1;
        this._lastSnapshotCycle = -1;
        this._lastSnapshotSubPhase = -1;
    }

    _clearGroup(group) {
        while (group.children.length > 0) {
            const child = group.children[0];
            group.remove(child);
            if (child.geometry) child.geometry.dispose();
            if (child.material) {
                if (Array.isArray(child.material)) {
                    child.material.forEach(m => m.dispose());
                } else {
                    child.material.dispose();
                }
            }
        }
    }

    // =============================================
    // Static layers
    // =============================================

    _buildTimePlanes() {
        this._clearGroup(this.timePlanesGroup);
        const graph = this.graph;
        if (!graph || graph.numDetectors === 0) return;

        let minX = Infinity, maxX = -Infinity;
        let minY = Infinity, maxY = -Infinity;

        for (const det of graph.detectors) {
            minX = Math.min(minX, det.x); maxX = Math.max(maxX, det.x);
            minY = Math.min(minY, det.y); maxY = Math.max(maxY, det.y);
        }

        const pad = 1.0;
        const wx0 = (minX - pad) * SPATIAL_SCALE;
        const wx1 = (maxX + pad) * SPATIAL_SCALE;
        const wy0 = (minY - pad) * SPATIAL_SCALE;
        const wy1 = (maxY + pad) * SPATIAL_SCALE;

        const gridLines = 8;
        const color = new THREE.Color(0.7, 0.7, 0.78);

        for (let t = 0; t < graph.numRounds; t++) {
            const z = t * TIME_SCALE;
            const points = [];

            // Border rectangle
            points.push(wx0, wy0, z, wx1, wy0, z);
            points.push(wx1, wy0, z, wx1, wy1, z);
            points.push(wx1, wy1, z, wx0, wy1, z);
            points.push(wx0, wy1, z, wx0, wy0, z);

            // Grid lines
            for (let g = 1; g < gridLines; g++) {
                const frac = g / gridLines;
                const gx = wx0 + (wx1 - wx0) * frac;
                const gy = wy0 + (wy1 - wy0) * frac;
                points.push(gx, wy0, z, gx, wy1, z);
                points.push(wx0, gy, z, wx1, gy, z);
            }

            const geo = new THREE.BufferGeometry();
            geo.setAttribute('position', new THREE.Float32BufferAttribute(points, 3));
            const mat = new THREE.LineBasicMaterial({
                color,
                transparent: true,
                opacity: 0.15,
                depthWrite: false
            });
            this.timePlanesGroup.add(new THREE.LineSegments(geo, mat));
        }
    }

    _buildLatticeUnderlay() {
        this._clearGroup(this.latticeGroup);
        const graph = this.graph;
        if (!graph || !graph.lattice || !graph.lattice.valid) return;

        const lat = graph.lattice;

        // Draw lattice once at z=0 (identical across rounds in a memory experiment)
        for (const face of lat.faces) {
            const cx = face.centerX * SPATIAL_SCALE;
            const cy = face.centerY * SPATIAL_SCALE;
            const hw = face.halfW * SPATIAL_SCALE;
            const hh = face.halfH * SPATIAL_SCALE;

            const fillColor = face.isXType ? 0xdc7878 : 0x7878dc;
            const edgeColor = face.isXType ? 0xc86464 : 0x6464c8;

            const planeGeo = new THREE.PlaneGeometry(hw * 2, hh * 2);
            const fillMat = new THREE.MeshBasicMaterial({
                color: fillColor,
                transparent: true,
                opacity: 0.26,
                side: THREE.DoubleSide,
                depthWrite: false
            });
            const plane = new THREE.Mesh(planeGeo, fillMat);
            plane.position.set(cx, cy, 0);
            this.latticeGroup.add(plane);

            // Outline
            const outlinePoints = [
                cx - hw, cy - hh, 0,
                cx + hw, cy - hh, 0,
                cx + hw, cy - hh, 0,
                cx + hw, cy + hh, 0,
                cx + hw, cy + hh, 0,
                cx - hw, cy + hh, 0,
                cx - hw, cy + hh, 0,
                cx - hw, cy - hh, 0
            ];
            const outGeo = new THREE.BufferGeometry();
            outGeo.setAttribute('position', new THREE.Float32BufferAttribute(outlinePoints, 3));
            const outMat = new THREE.LineBasicMaterial({
                color: edgeColor,
                transparent: true,
                opacity: 0.4,
                depthWrite: false
            });
            this.latticeGroup.add(new THREE.LineSegments(outGeo, outMat));
        }
    }

    _buildGraphEdges(dim) {
        const graph = this.graph;
        if (!graph) return;

        const positions = [];
        const colors = [];

        for (const ge of graph.graphEdges) {
            if (ge.node0 >= graph.nodePositions.length || ge.node1 >= graph.nodePositions.length) continue;

            const p0 = toWorld(graph.nodePositions[ge.node0]);
            const p1 = toWorld(graph.nodePositions[ge.node1]);

            positions.push(p0.x, p0.y, p0.z, p1.x, p1.y, p1.z);

            let r, g, b;
            if (ge.isBoundary) {
                r = 0.63; g = 0.63; b = 0.63;
            } else {
                const dz = Math.abs(graph.nodePositions[ge.node0].z - graph.nodePositions[ge.node1].z);
                if (dz > 0.1) {
                    r = 0.67; g = 0.59; b = 0.47; // warm (temporal)
                } else {
                    r = 0.47; g = 0.55; b = 0.69; // cool (spatial)
                }
            }

            colors.push(r, g, b, r, g, b);
        }

        const geo = new THREE.BufferGeometry();
        geo.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
        geo.setAttribute('color', new THREE.Float32BufferAttribute(colors, 3));

        const mat = new THREE.LineBasicMaterial({
            vertexColors: true,
            transparent: true,
            opacity: dim ? 0.12 : 0.7,
            depthWrite: false
        });

        this.graphEdgesGroup.add(new THREE.LineSegments(geo, mat));
    }

    _buildDetectorNodes(snapshot) {
        this._clearGroup(this.detectorNodesGroup);
        const graph = this.graph;
        if (!graph || graph.numDetectors === 0) return;

        // Build node-to-cluster ownership map from snapshot
        const nodeOwner = new Map();
        if (snapshot && snapshot.clusters) {
            for (const cluster of snapshot.clusters) {
                for (const entry of cluster.nodes) {
                    if (!nodeOwner.has(entry.node)) {
                        nodeOwner.set(entry.node, cluster.id);
                    }
                }
            }
        }

        const n = graph.numDetectors;

        // Create InstancedMesh for detectors
        const mat = new THREE.MeshPhongMaterial({ vertexColors: false });
        const mesh = new THREE.InstancedMesh(this.sphereGeo, mat, n);
        mesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);

        const dummy = new THREE.Object3D();
        const color = new THREE.Color();

        for (let i = 0; i < n; i++) {
            const det = graph.detectors[i];
            const pos = toWorld(graph.nodePositions[i]);
            const fired = this.syndrome && this.syndrome[i] === 1;
            const isX = det.isXType;
            const inCluster = nodeOwner.has(i);

            let radius;
            if (fired) {
                radius = inCluster ? 0.32 : 0.28;
            } else {
                radius = inCluster ? 0.14 : 0.09;
            }

            dummy.position.set(pos.x, pos.y, pos.z);
            dummy.scale.set(radius, radius, radius);
            dummy.updateMatrix();
            mesh.setMatrixAt(i, dummy.matrix);

            if (isX) {
                color.setRGB(0.90, 0.27, 0.24);
            } else {
                color.setRGB(0.24, 0.51, 0.90);
            }

            if (!fired && !inCluster) {
                color.multiplyScalar(0.4);
            }

            mesh.setColorAt(i, color);
        }

        mesh.instanceMatrix.needsUpdate = true;
        mesh.instanceColor.needsUpdate = true;
        this.detectorMesh = mesh;
        this.detectorNodesGroup.add(mesh);

        // Fired nodes get a bright core
        for (let i = 0; i < n; i++) {
            if (!this.syndrome || this.syndrome[i] !== 1) continue;
            const pos = toWorld(graph.nodePositions[i]);
            const inCluster = nodeOwner.has(i);
            const radius = inCluster ? 0.32 : 0.28;

            const coreMat = new THREE.MeshBasicMaterial({
                color: 0xffffff,
                transparent: true,
                opacity: 0.7
            });
            const core = new THREE.Mesh(this.sphereGeo, coreMat);
            core.position.set(pos.x, pos.y, pos.z);
            core.scale.set(radius * 0.3, radius * 0.3, radius * 0.3);
            this.detectorNodesGroup.add(core);
        }

        // Cluster ownership rings
        if (snapshot) {
            for (const [nodeId, clusterId] of nodeOwner) {
                if (nodeId >= graph.numDetectors) continue;
                const pos = toWorld(graph.nodePositions[nodeId]);
                const fired = this.syndrome && this.syndrome[nodeId] === 1;
                const inCluster = true;
                const radius = fired ? (inCluster ? 0.32 : 0.28) : (inCluster ? 0.14 : 0.09);

                const ringColor = getClusterColorHex(clusterId);
                const ringMat = new THREE.MeshBasicMaterial({
                    color: ringColor,
                    wireframe: true,
                    transparent: true,
                    opacity: 0.6
                });
                const ring = new THREE.Mesh(this.sphereGeo, ringMat);
                ring.position.set(pos.x, pos.y, pos.z);
                const rs = radius + 0.06;
                ring.scale.set(rs, rs, rs);
                this.detectorNodesGroup.add(ring);
            }
        }
    }

    _buildBoundaryNodes() {
        this._clearGroup(this.boundaryNodesGroup);
        const graph = this.graph;
        if (!graph) return;

        const radius = 0.06;
        for (const bi of graph.boundaryNodeIndices) {
            if (bi >= graph.nodePositions.length) continue;
            const pos = toWorld(graph.nodePositions[bi]);

            const mat = new THREE.MeshBasicMaterial({
                color: 0x666688,
                transparent: true,
                opacity: 0.3,
                depthWrite: false
            });
            const mesh = new THREE.Mesh(this.sphereGeo, mat);
            mesh.position.set(pos.x, pos.y, pos.z);
            mesh.scale.set(radius, radius, radius);
            this.boundaryNodesGroup.add(mesh);
        }
    }

    // =============================================
    // Dynamic decoder layers
    // =============================================

    _buildClusterEdges(snapshot) {
        const graph = this.graph;
        const postValidation = snapshot.phase >= 2;

        for (const cluster of snapshot.clusters) {
            const c = cluster.id;
            const activity = snapshot.clustersActivity[c];
            if (activity === 0) continue;

            const [cr, cg, cb] = getClusterColor(c);
            const active = activity === 1;

            for (const entry of cluster.edges) {
                const e = entry.edge;
                const edgeState = entry.state;
                if (edgeState === 0 || e >= graph.ufEdges.length) continue;

                const n0 = graph.ufEdges[e][0];
                const n1 = graph.ufEdges[e][1];
                if (n0 >= graph.nodePositions.length || n1 >= graph.nodePositions.length) continue;

                const p0 = toWorld(graph.nodePositions[n0]);
                const p1 = toWorld(graph.nodePositions[n1]);

                let opacity;
                if (postValidation) {
                    opacity = 0.2;
                } else {
                    opacity = active ? 0.9 : 0.4;
                }

                if (edgeState === 1) {
                    // Half-grown: dashed
                    this._addDashedCylinder(
                        p0, p1,
                        postValidation ? 0.015 : (active ? 0.03 : 0.02),
                        5, cr, cg, cb, opacity,
                        this.clusterEdgesGroup
                    );
                } else {
                    // Fully grown: solid
                    const r = postValidation ? 0.025 : (active ? 0.055 : 0.04);
                    this._addCylinder(p0, p1, r, cr, cg, cb, opacity, this.clusterEdgesGroup);
                }
            }
        }
    }

    _buildClusterSheaths(snapshot) {
        const graph = this.graph;
        const postValidation = snapshot.phase >= 2;

        for (const cluster of snapshot.clusters) {
            const c = cluster.id;
            const activity = snapshot.clustersActivity[c];
            if (activity === 0) continue;

            const active = activity === 1;

            // Collect world positions
            const positions = [];
            for (const entry of cluster.nodes) {
                const n = entry.node;
                if (n >= graph.nodePositions.length) continue;
                const w = toWorld(graph.nodePositions[n]);
                positions.push(new THREE.Vector3(w.x, w.y, w.z));
            }

            if (positions.length < 2) continue;

            const [cr, cg, cb] = getClusterColor(c);
            const opacity = postValidation ? 0.05 : (active ? 0.15 : 0.06);

            this._buildConvexHull(positions, cr, cg, cb, opacity, 0.5, this.clusterSheathsGroup);
        }
    }

    _buildClusterRegions(snapshot) {
        const graph = this.graph;

        for (const cluster of snapshot.clusters) {
            const c = cluster.id;
            const activity = snapshot.clustersActivity[c];
            if (activity === 0) continue;

            const [cr, cg, cb] = getClusterColor(c);

            for (const entry of cluster.nodes) {
                if (entry.state !== 1) continue; // frontier only
                const n = entry.node;
                if (n >= graph.nodePositions.length) continue;

                const pos = toWorld(graph.nodePositions[n]);
                const mat = new THREE.MeshBasicMaterial({
                    color: new THREE.Color(cr, cg, cb),
                    wireframe: true,
                    transparent: true,
                    opacity: 0.4
                });
                const mesh = new THREE.Mesh(this.sphereGeo, mat);
                mesh.position.set(pos.x, pos.y, pos.z);
                mesh.scale.set(0.22, 0.22, 0.22);
                this.clusterRegionsGroup.add(mesh);
            }
        }
    }

    _buildSpanningForest(snapshot) {
        const graph = this.graph;
        const treeColor = new THREE.Color(0x8888aa);

        const inTree = new Set();

        for (const tree of snapshot.spanningForest) {
            // Tree edges
            for (const entry of tree.edges) {
                const edgeIdx = entry.edge;
                if (edgeIdx >= graph.ufEdges.length) continue;

                const n0 = graph.ufEdges[edgeIdx][0];
                const n1 = graph.ufEdges[edgeIdx][1];
                if (n0 >= graph.nodePositions.length || n1 >= graph.nodePositions.length) continue;

                const p0 = toWorld(graph.nodePositions[n0]);
                const p1 = toWorld(graph.nodePositions[n1]);

                this._addCylinder(
                    p0, p1, 0.08,
                    treeColor.r, treeColor.g, treeColor.b, 1.0,
                    this.spanningForestGroup, true
                );
                inTree.add(n0);
                inTree.add(n1);
            }

            // Root markers
            for (const rootNode of tree.roots) {
                if (rootNode >= graph.nodePositions.length) continue;
                const pos = toWorld(graph.nodePositions[rootNode]);

                // White sphere
                const rootMat = new THREE.MeshBasicMaterial({
                    color: 0xffffff,
                    transparent: true,
                    opacity: 0.8,
                    depthTest: false
                });
                const root = new THREE.Mesh(this.sphereGeo, rootMat);
                root.position.set(pos.x, pos.y, pos.z);
                root.scale.set(0.14, 0.14, 0.14);
                root.renderOrder = 10;
                this.spanningForestGroup.add(root);

                // Dark ring
                const ringMat = new THREE.MeshBasicMaterial({
                    color: 0x8888aa,
                    wireframe: true,
                    depthTest: false
                });
                const ring = new THREE.Mesh(this.sphereGeo, ringMat);
                ring.position.set(pos.x, pos.y, pos.z);
                ring.scale.set(0.16, 0.16, 0.16);
                ring.renderOrder = 10;
                this.spanningForestGroup.add(ring);

                inTree.add(rootNode);
            }
        }

        // Joint spheres
        const jointColor = new THREE.Color(0x9999bb);
        for (const nodeId of inTree) {
            if (nodeId >= graph.nodePositions.length) continue;
            const pos = toWorld(graph.nodePositions[nodeId]);
            const jMat = new THREE.MeshBasicMaterial({
                color: jointColor,
                transparent: true,
                opacity: 0.7,
                depthTest: false
            });
            const joint = new THREE.Mesh(this.sphereGeo, jMat);
            joint.position.set(pos.x, pos.y, pos.z);
            joint.scale.set(0.06, 0.06, 0.06);
            joint.renderOrder = 10;
            this.spanningForestGroup.add(joint);
        }
    }

    _buildCorrections(snapshot) {
        const graph = this.graph;
        const corrColor = new THREE.Color(0x32ff64);
        const glowColor = new THREE.Color(0x64ff8c);

        for (const edgeIdx of snapshot.corrections) {
            if (edgeIdx >= graph.ufEdges.length) continue;

            const n0 = graph.ufEdges[edgeIdx][0];
            const n1 = graph.ufEdges[edgeIdx][1];
            if (n0 >= graph.nodePositions.length || n1 >= graph.nodePositions.length) continue;

            const p0 = toWorld(graph.nodePositions[n0]);
            const p1 = toWorld(graph.nodePositions[n1]);

            // Thick correction edge
            this._addCylinder(
                p0, p1, 0.10,
                corrColor.r, corrColor.g, corrColor.b, 1.0,
                this.correctionsGroup, true
            );

            // Glowing endpoint spheres
            for (const pos of [p0, p1]) {
                const gMat = new THREE.MeshBasicMaterial({
                    color: glowColor,
                    transparent: true,
                    opacity: 0.6,
                    depthTest: false
                });
                const glow = new THREE.Mesh(this.sphereGeo, gMat);
                glow.position.set(pos.x, pos.y, pos.z);
                glow.scale.set(0.13, 0.13, 0.13);
                glow.renderOrder = 11;
                this.correctionsGroup.add(glow);
            }
        }
    }

    // =============================================
    // Helpers
    // =============================================

    _addCylinder(p0, p1, radius, r, g, b, opacity, group, noDepthTest = false) {
        const start = new THREE.Vector3(p0.x, p0.y, p0.z);
        const end = new THREE.Vector3(p1.x, p1.y, p1.z);
        const dir = new THREE.Vector3().subVectors(end, start);
        const len = dir.length();
        if (len < 0.001) return;

        const mat = new THREE.MeshBasicMaterial({
            color: new THREE.Color(r, g, b),
            transparent: opacity < 1,
            opacity,
            depthTest: !noDepthTest,
            depthWrite: !noDepthTest
        });

        const mesh = new THREE.Mesh(this.cylinderGeo, mat);
        mesh.position.copy(start).lerp(end, 0.5);
        mesh.scale.set(radius, len, radius);

        // Orient cylinder along direction
        const axis = new THREE.Vector3(0, 1, 0);
        const quat = new THREE.Quaternion().setFromUnitVectors(axis, dir.normalize());
        mesh.quaternion.copy(quat);

        if (noDepthTest) mesh.renderOrder = 10;
        group.add(mesh);
    }

    _addDashedCylinder(p0, p1, radius, nDashes, r, g, b, opacity, group) {
        const total = 2 * nDashes - 1;
        const start = new THREE.Vector3(p0.x, p0.y, p0.z);
        const end = new THREE.Vector3(p1.x, p1.y, p1.z);

        for (let i = 0; i < nDashes; i++) {
            const t0 = (2.0 * i) / total;
            const t1 = (2.0 * i + 1.0) / total;
            const a = new THREE.Vector3().lerpVectors(start, end, t0);
            const bb = new THREE.Vector3().lerpVectors(start, end, t1);
            this._addCylinder(a, bb, radius, r, g, b, opacity, group);
        }
    }

    _buildConvexHull(points, r, g, b, opacity, inflate, group) {
        const n = points.length;
        if (n < 2) return;

        // Compute centroid
        const cen = new THREE.Vector3();
        for (const p of points) cen.add(p);
        cen.divideScalar(n);

        // Inflate points outward from centroid
        const inflated = points.map(p => {
            const dir = new THREE.Vector3().subVectors(p, cen);
            const len = dir.length();
            if (len > 0.001) {
                dir.multiplyScalar(inflate / len);
                return p.clone().add(dir);
            }
            return p.clone();
        });

        if (n === 2) {
            // Capsule: cylinder + end spheres
            this._addCylinder(inflated[0], inflated[1], inflate * 0.6, r, g, b, opacity, group);
            return;
        }

        try {
            const hull = new ConvexGeometry(inflated);
            const mat = new THREE.MeshBasicMaterial({
                color: new THREE.Color(r, g, b),
                transparent: true,
                opacity,
                side: THREE.DoubleSide,
                depthWrite: false
            });
            const mesh = new THREE.Mesh(hull, mat);
            group.add(mesh);
        } catch (e) {
            // Fallback: just draw edges between points
        }
    }

    // Public getter for raycasting
    getDetectorMesh() {
        return this.detectorMesh;
    }
}
