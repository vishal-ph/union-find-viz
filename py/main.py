import argparse
from pathlib import Path

import stim

def generate_noisy_circuit(
        distance:int,
        rounds:int, p:float=0.01,
        ) -> stim.Circuit:
    noisy_circuit = stim.Circuit.generated(
        code_task='surface_code:rotated_memory_z',
        distance=distance,
        rounds=rounds,
        after_clifford_depolarization= p,
        before_round_data_depolarization = p,
        before_measure_flip_probability = p,
        after_reset_flip_probability = p)

    return noisy_circuit


def evaluate_dem(noisy_circuit:stim.Circuit,
                 decompose_errors:bool=True) -> stim.DetectorErrorModel:
    return noisy_circuit.detector_error_model(decompose_errors=decompose_errors)


def compute_detection_events(noisy_circuit: stim.Circuit) -> list[int]:
    detector_sampler = noisy_circuit.compile_detector_sampler()
    detection_events = detector_sampler.sample(shots=1, append_observables=False)

    return [i for i, val in enumerate(detection_events[0]) if val == 1]

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, required=True, help="Output directory")
    args = parser.parse_args()
    out_dir = args.out
    out_dir.mkdir(parents=True, exist_ok=True)

    distance = 5
    rounds = 5
    p = 0.01

    noisy_circuit = generate_noisy_circuit(distance, rounds, p)
    dem = evaluate_dem(noisy_circuit)
    detection_events = compute_detection_events(noisy_circuit)

    with open(out_dir / "test_events.txt", "w") as f:
        [f.write(str(event) + "\n") for event in detection_events]

    with open(out_dir / "test.dem", "w") as f:
        f.write(str(dem))