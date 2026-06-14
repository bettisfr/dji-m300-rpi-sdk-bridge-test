#!/usr/bin/env python3

import argparse
import time
from collections import Counter
from pathlib import Path

from ultralytics import YOLO


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run YOLO on the most recent captured JPEG."
    )
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--model", default="yolo11n.pt")
    parser.add_argument("--imgsz", type=int, default=640)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    root = args.root.expanduser().resolve()
    photos = root / "photos"
    images = sorted(photos.glob("*.JPG"), key=lambda path: path.stat().st_mtime)
    if not images:
        raise SystemExit(f"No JPG images found in {photos}")

    image = images[-1]
    model_path = Path(args.model)
    if not model_path.is_absolute():
        model_path = root / model_path
    if not model_path.is_file():
        raise SystemExit(f"Model not found: {model_path}")

    output_dir = root / "inference"
    output_dir.mkdir(parents=True, exist_ok=True)
    output_path = output_dir / "latest-annotated.jpg"

    started = time.perf_counter()
    model = YOLO(model_path)
    results = model.predict(
        source=image,
        imgsz=args.imgsz,
        device="cpu",
        verbose=False,
    )
    result = results[0]
    result.save(filename=str(output_path))
    total_ms = (time.perf_counter() - started) * 1000

    class_counts = Counter(
        result.names[int(class_id)] for class_id in result.boxes.cls.tolist()
    )
    classes = ",".join(
        f"{name}:{count}" for name, count in sorted(class_counts.items())
    )
    if not classes:
        classes = "none"

    print(
        "RESULT OK "
        f"detections={len(result.boxes)} "
        f"classes={classes} "
        f"inference_ms={result.speed['inference']:.1f} "
        f"total_ms={total_ms:.1f} "
        f"image={image.name} "
        f"output={output_path.name}",
        flush=True,
    )


if __name__ == "__main__":
    main()
