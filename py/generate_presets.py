#!/usr/bin/env python3
"""Generate pre-baked preset circuits for static hosting."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from main import generate_noisy_circuit, evaluate_dem, compute_detection_events

PRESETS = [
    (3,  3,  0.01),
    (5,  5,  0.005),
    (7,  7,  0.005),
    (9,  9,  0.001),
    (25, 10, 0.0005),
]

out_dir = Path(__file__).resolve().parent.parent / "web" / "data"
out_dir.mkdir(parents=True, exist_ok=True)

for d, r, p in PRESETS:
    name = f"d{d}_r{r}"
    print(f"Generating {name} (p={p})...", end=" ", flush=True)
    circuit = generate_noisy_circuit(d, r, p)
    dem = evaluate_dem(circuit)
    # Retry until at least a few detectors fire (avoid boring empty samples)
    for _ in range(1000):
        events = compute_detection_events(circuit)
        if len(events) >= 3:
            break
    (out_dir / f"{name}.dem").write_text(str(dem))
    (out_dir / f"{name}_events.txt").write_text("\n".join(str(e) for e in events))
    print("done")

print(f"\nWrote {len(PRESETS)*2} files to {out_dir}")
