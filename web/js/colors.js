// Cluster palette: 10 harmonious colors matching the C++ renderer
// Adjusted brighter for dark background
export const CLUSTER_COLORS = [
    [0.24, 0.77, 0.82],  // teal
    [0.29, 0.59, 0.92],  // steel blue
    [0.47, 0.42, 0.87],  // soft indigo
    [0.73, 0.33, 0.82],  // orchid
    [0.88, 0.31, 0.59],  // mauve
    [0.96, 0.44, 0.29],  // coral
    [0.93, 0.65, 0.17],  // amber
    [0.55, 0.80, 0.28],  // olive green
    [0.31, 0.83, 0.57],  // mint
    [0.62, 0.46, 0.83],  // lavender
];

export function getClusterColor(index) {
    return CLUSTER_COLORS[index % CLUSTER_COLORS.length];
}

export function getClusterColorHex(index) {
    const c = CLUSTER_COLORS[index % CLUSTER_COLORS.length];
    const r = Math.round(c[0] * 255);
    const g = Math.round(c[1] * 255);
    const b = Math.round(c[2] * 255);
    return (r << 16) | (g << 8) | b;
}

// Spatial layout constants
export const SPATIAL_SCALE = 2.0;
export const TIME_SCALE = 3.0;

export function toWorld(v) {
    return {
        x: v.x * SPATIAL_SCALE,
        y: v.y * SPATIAL_SCALE,
        z: v.z * TIME_SCALE
    };
}
