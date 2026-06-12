#!/usr/bin/env python3

import argparse
import csv
import json
import os
import statistics
import time
from pathlib import Path

import psutil
from ultralytics import YOLO


DEFAULT_MODELS = ("yolo11n.pt", "yolo11s.pt", "yolo11m.pt", "yolo11l.pt")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Benchmark Ultralytics YOLO detection models on one image."
    )
    parser.add_argument("image", type=Path)
    parser.add_argument("--models", nargs="+", default=DEFAULT_MODELS)
    parser.add_argument("--imgsz", type=int, default=640)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--output", type=Path, default=Path("yolo-benchmark"))
    return parser.parse_args()


def rss_mb() -> float:
    return psutil.Process(os.getpid()).memory_info().rss / (1024 * 1024)


def main() -> None:
    args = parse_args()
    args.image = args.image.expanduser().resolve()
    args.output = args.output.expanduser().resolve()
    args.output.mkdir(parents=True, exist_ok=True)

    if not args.image.is_file():
        raise SystemExit(f"Image not found: {args.image}")
    if args.runs < 1:
        raise SystemExit("--runs must be at least 1")

    summary = []
    for model_name in args.models:
        print(f"\n=== {model_name} ===", flush=True)
        memory_before = rss_mb()
        load_started = time.perf_counter()
        model = YOLO(model_name)
        load_ms = (time.perf_counter() - load_started) * 1000

        warmup_started = time.perf_counter()
        model.predict(
            source=args.image,
            imgsz=args.imgsz,
            device="cpu",
            verbose=False,
        )
        warmup_ms = (time.perf_counter() - warmup_started) * 1000

        wall_times = []
        inference_times = []
        detections = 0
        first_result = None
        for run_index in range(args.runs):
            started = time.perf_counter()
            results = model.predict(
                source=args.image,
                imgsz=args.imgsz,
                device="cpu",
                verbose=False,
            )
            wall_ms = (time.perf_counter() - started) * 1000
            result = results[0]
            if first_result is None:
                first_result = result
            detections = len(result.boxes)
            wall_times.append(wall_ms)
            inference_times.append(float(result.speed["inference"]))
            print(
                f"run {run_index + 1}/{args.runs}: "
                f"wall={wall_ms:.1f} ms, "
                f"inference={result.speed['inference']:.1f} ms, "
                f"detections={detections}",
                flush=True,
            )

        annotated_path = args.output / f"{Path(model_name).stem}-annotated.jpg"
        first_result.save(filename=str(annotated_path))
        record = {
            "model": model_name,
            "image": str(args.image),
            "imgsz": args.imgsz,
            "runs": args.runs,
            "load_ms": round(load_ms, 1),
            "warmup_ms": round(warmup_ms, 1),
            "wall_mean_ms": round(statistics.mean(wall_times), 1),
            "wall_median_ms": round(statistics.median(wall_times), 1),
            "wall_min_ms": round(min(wall_times), 1),
            "wall_max_ms": round(max(wall_times), 1),
            "inference_mean_ms": round(statistics.mean(inference_times), 1),
            "detections": detections,
            "rss_delta_mb": round(rss_mb() - memory_before, 1),
            "annotated_image": str(annotated_path),
        }
        summary.append(record)
        print(json.dumps(record, indent=2), flush=True)
        del model

    json_path = args.output / "results.json"
    csv_path = args.output / "results.csv"
    json_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    with csv_path.open("w", newline="", encoding="utf-8") as csv_file:
        writer = csv.DictWriter(csv_file, fieldnames=summary[0].keys())
        writer.writeheader()
        writer.writerows(summary)

    print(f"\nJSON results: {json_path}")
    print(f"CSV results:  {csv_path}")


if __name__ == "__main__":
    main()
