import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { toWorld } from './colors.js';

export class Scene {
    constructor(canvas) {
        this.canvas = canvas;
        this.renderer = new THREE.WebGLRenderer({
            canvas,
            antialias: true,
            alpha: false
        });
        this.renderer.setPixelRatio(window.devicePixelRatio);
        this.renderer.setClearColor(0x060610, 1);
        this.renderer.sortObjects = true;
        this.renderer.shadowMap.enabled = true;
        this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;

        this.camera = new THREE.PerspectiveCamera(45, 1, 0.1, 500);
        this.camera.position.set(15, 15, 15);

        this.scene = new THREE.Scene();
        // Lighting
        const ambient = new THREE.AmbientLight(0x3a3a5a, 1.5);
        this.scene.add(ambient);

        // Cool key light (blue-white)
        const dir = new THREE.DirectionalLight(0x8ab4f8, 1.0);
        dir.position.set(10, 20, 15);
        dir.castShadow = true;
        dir.shadow.mapSize.set(1024, 1024);
        dir.shadow.camera.near = 0.5;
        dir.shadow.camera.far = 200;
        dir.shadow.camera.left = dir.shadow.camera.bottom = -40;
        dir.shadow.camera.right = dir.shadow.camera.top = 40;
        this.scene.add(dir);

        // Warm fill (soft orange-pink from below)
        const dir2 = new THREE.DirectionalLight(0xff6688, 0.25);
        dir2.position.set(-10, -8, -10);
        this.scene.add(dir2);

        // Controls
        this.controls = new OrbitControls(this.camera, canvas);
        this.controls.enableDamping = true;
        this.controls.dampingFactor = 0.1;
        this.controls.rotateSpeed = 0.8;
        this.controls.zoomSpeed = 1.2;
        this.controls.minDistance = 1;
        this.controls.maxDistance = 200;

        this.handleResize();
        window.addEventListener('resize', () => this.handleResize());
    }

    handleResize() {
        const container = this.canvas.parentElement;
        const w = container.clientWidth;
        const h = container.clientHeight;
        this.renderer.setSize(w, h);
        this.camera.aspect = w / h;
        this.camera.updateProjectionMatrix();
    }

    computeLayout(graph) {
        if (!graph || !graph.nodePositions || graph.nodePositions.length === 0) return;

        let minX = Infinity, maxX = -Infinity;
        let minY = Infinity, maxY = -Infinity;
        let minZ = Infinity, maxZ = -Infinity;

        for (const p of graph.nodePositions) {
            const w = toWorld(p);
            minX = Math.min(minX, w.x); maxX = Math.max(maxX, w.x);
            minY = Math.min(minY, w.y); maxY = Math.max(maxY, w.y);
            minZ = Math.min(minZ, w.z); maxZ = Math.max(maxZ, w.z);
        }

        const center = new THREE.Vector3(
            (minX + maxX) / 2,
            (minY + maxY) / 2,
            (minZ + maxZ) / 2
        );

        const dx = maxX - minX;
        const dy = maxY - minY;
        const dz = maxZ - minZ;
        let radius = Math.sqrt(dx * dx + dy * dy + dz * dz) * 0.5;
        if (radius < 1) radius = 5;

        const dist = radius * 2.8;
        this.controls.target.copy(center);

        // Animate camera entrance
        const startPos = new THREE.Vector3(
            center.x + dist * 2,
            center.y + dist * 1.5,
            center.z + dist * 2
        );
        const endPos = new THREE.Vector3(
            center.x + dist * Math.sin(0.6) * Math.cos(0.7),
            center.y + dist * Math.cos(0.6),
            center.z + dist * Math.sin(0.6) * Math.sin(0.7)
        );

        this.camera.position.copy(startPos);
        this._animTarget = endPos;
        this._animProgress = 0;
        this._animating = true;
    }

    update() {
        // Camera entrance animation
        if (this._animating && this._animTarget) {
            this._animProgress += 0.02;
            if (this._animProgress >= 1) {
                this._animProgress = 1;
                this._animating = false;
            }
            const t = 1 - Math.pow(1 - this._animProgress, 3); // ease-out cubic
            this.camera.position.lerpVectors(
                this.camera.position,
                this._animTarget,
                t * 0.08
            );
        }

        this.controls.update();
    }

    render() {
        this.renderer.render(this.scene, this.camera);
    }

    add(object) {
        this.scene.add(object);
    }

    remove(object) {
        this.scene.remove(object);
    }
}
