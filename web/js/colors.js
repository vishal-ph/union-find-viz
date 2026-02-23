// Cluster palette: 10 harmonious colors matching the C++ renderer
// Adjusted brighter for dark background
export const CLUSTER_COLORS = [
    [0.31, 0.71, 0.75],  // teal
    [0.35, 0.59, 0.86],  // steel blue
    [0.51, 0.47, 0.82],  // soft indigo
    [0.69, 0.39, 0.76],  // orchid
    [0.82, 0.37, 0.59],  // mauve
    [0.90, 0.47, 0.35],  // coral
    [0.86, 0.63, 0.24],  // amber
    [0.55, 0.73, 0.35],  // olive green
    [0.37, 0.76, 0.57],  // mint
    [0.63, 0.51, 0.78],  // lavender
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
