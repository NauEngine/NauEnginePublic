"""Compares runs of the GPU profiler (doc/gpu_profiler.md): 1 GPU and 2 GPU runs of one scene.

Every run is a CSV (frame,pass,gpu,queue,ms,mode,features,age,bytes) with its metadata JSON next to it (same name,
.json). The first run is the baseline; the others are compared with it.

    py tools/scripts/compare_gpu_timings.py run_1gpu.csv run_2gpu.csv --label "1 GPU" --label "2 GPU"
    py tools/scripts/compare_gpu_timings.py a1.csv+a2.csv b1.csv+b2.csv --md summary.md --json summary.json

"a1.csv+a2.csv": repeated runs of one configuration, pooled into one run (frames of each file stay separate).

Frames: the profiler already drops its warmup; --skip-frames drops more. By default only the frames with the dominant
"features" of a run are used (switching modes by keys leaves a few frames of the previous mode), and --settle-frames
(default 60) frames after every change of "features" are dropped; --all-features keeps every frame. The comparison of 1 GPU and 2 GPU is based on the CPU frame time (cpu_frame); GPU times explain where the
time goes. Warnings are printed when runs differ in features, resolution, commit, local changes, vsync, preset or
configuration, adapters or drivers, or were not on mains power.
"""

import argparse
import csv
import hashlib
import json
import math
import os
import statistics
import sys
from collections import Counter, defaultdict


def percentile(values, p):
    """Linear interpolation between closest ranks (as numpy's default)."""
    if not values:
        return None
    ordered = sorted(values)
    k = (len(ordered) - 1) * p / 100.0
    lo = math.floor(k)
    hi = min(lo + 1, len(ordered) - 1)
    return ordered[lo] + (ordered[hi] - ordered[lo]) * (k - lo)


def stats(values):
    if not values:
        return None
    return {"samples": len(values), "mean_ms": statistics.fmean(values), "median_ms": statistics.median(values),
            "p95_ms": percentile(values, 95), "p99_ms": percentile(values, 99), "max_ms": max(values)}


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def load_run(spec, label, skip_frames, all_features, settle_frames):
    """spec: one CSV or several joined by '+'. Frames are keyed (file index, frame)."""
    paths = spec.split("+")
    files = []
    rows = []
    frame_features = {}
    for index, path in enumerate(paths):
        meta_path = os.path.splitext(path)[0] + ".json"
        meta = None
        if os.path.exists(meta_path):
            with open(meta_path, encoding="utf-8") as f:
                meta = json.load(f)
        files.append({"csv": path, "meta_path": meta_path if meta else None, "meta": meta})
        with open(path, newline="", encoding="utf-8") as f:
            for row in csv.DictReader(f):
                try:
                    row["frame"] = int(row["frame"])
                    row["ms"] = float(row["ms"])
                except (KeyError, ValueError):
                    continue  # a truncated last line of a killed run
                if row["frame"] <= skip_frames:
                    continue
                row["key"] = (index, row["frame"])
                rows.append(row)
                if row["gpu"] == "cpu" and row["pass"] == "cpu_frame":
                    frame_features[row["key"]] = (row["mode"], row.get("features", ""))

    variants = Counter(frame_features.values())
    dominant = variants.most_common(1)[0][0] if variants else ("1gpu", "")
    frames = set()
    previous, changed = None, None
    for key in sorted(frame_features):
        value = frame_features[key]
        if previous is None or key[0] != previous[0] or value != frame_features[previous]:
            changed = key
        previous = key
        if all_features:
            frames.add(key)
        elif value == dominant and key[1] - changed[1] >= settle_frames:
            frames.add(key)

    return {"label": label, "files": files, "meta": files[0]["meta"], "rows": rows, "frames": frames,
            "all_frames": len(frame_features), "variants": variants, "dominant": dominant}


def analyze(run):
    frames = run["frames"]
    cpu_frame, cpu_timers, passes, transfers = [], defaultdict(list), defaultdict(list), defaultdict(list)
    transfer_bytes = defaultdict(set)
    consumed = {}  # csm_atlas: frame that used an atlas -> its age
    for row in run["rows"]:
        name, gpu = row["pass"], row["gpu"]
        is_transfer = "/" in name and name.rsplit("/", 1)[1] in ("src", "dst", "sum", "span")
        if row["key"] not in frames:
            continue
        if gpu == "cpu":
            (cpu_frame if name == "cpu_frame" else cpu_timers[name]).append(row["ms"])
        elif is_transfer:
            base, half = name.rsplit("/", 1)
            transfers[(base, half, gpu, row["queue"])].append(row["ms"])
            if row.get("bytes"):
                transfer_bytes[(base, half)].add(int(row["bytes"]))
            if base == "csm_atlas" and half == "sum" and row.get("age"):
                age = int(row["age"])
                consumed[(row["key"][0], row["frame"] + age)] = age
        else:
            passes[(name, gpu, row["queue"])].append(row["ms"])

    result = {"frames": len(frames), "cpu_frame": stats(cpu_frame),
              "cpu_timers": {k: stats(v) for k, v in sorted(cpu_timers.items())},
              "passes": {k: stats(v) for k, v in passes.items()},
              "transfers": {k: stats(v) for k, v in transfers.items()},
              "transfer_bytes": {k: max(v) for k, v in transfer_bytes.items()}}

    # Shadow atlas from the second GPU: which frames got a new one, and how old the atlas on screen is in every frame.
    if consumed and run["dominant"][0] == "2gpu":
        ordered = sorted(frames)
        new_frames = [f for f in ordered if f in consumed]
        displayed = []
        for index in sorted({key[0] for key in ordered}):
            in_file = [key[1] for key in ordered if key[0] == index]
            last_frame, last_age = None, None
            for frame in range(in_file[0], in_file[-1] + 1):
                key = (index, frame)
                if key in consumed:
                    last_frame, last_age = frame, consumed[key]
                if key in frames and last_frame is not None:
                    displayed.append(last_age + frame - last_frame)
        result["atlas"] = {
            "frames": len(ordered),
            "frames_with_new_atlas": len(new_frames),
            "new_atlas_share": len(new_frames) / len(ordered),
            "age_at_use": dict(sorted(Counter(consumed[f] for f in new_frames).items())),
            "displayed_age_mean": statistics.fmean(displayed) if displayed else None,
            "displayed_age_median": statistics.median(displayed) if displayed else None,
            "displayed_age_p95": percentile(displayed, 95) if displayed else None,
            "displayed_age_max": max(displayed) if displayed else None,
        }
    return result


def file_warnings(label, file):
    meta = file["meta"]
    name = os.path.basename(file["csv"])
    if meta is None:
        return [f"{label}: no metadata JSON next to {name}"]
    out = []
    if not meta.get("complete"):
        out.append(f"{label}: metadata of {name} incomplete (the run did not shut down cleanly)")
    if meta.get("build", {}).get("local_changes"):
        out.append(f"{label}: {name} built with local changes (commit {meta['build'].get('commit', '?')[:10]} + uncommitted)")
    start = meta.get("power_at_start", "")
    end = meta.get("power_at_end", start)
    if not (start.startswith("ac") and end.startswith("ac")):
        out.append(f"{label}: {name} not on mains power the whole run (start: {start}; end: {end})")
    if len(meta.get("resolution", [])) > 1:
        out.append(f"{label}: the window of {name} was resized during the run ({meta['resolution']})")
    if meta.get("settings", {}).get("vsync") == "on":
        out.append(f"{label}: {name} with vsync on, frame times are bound to the refresh rate")
    return out


def warnings_for(runs):
    out = []
    for run in runs:
        for file in run["files"]:
            out.extend(file_warnings(run["label"], file))
        share = sum(n for v, n in run["variants"].items() if v == run["dominant"]) / max(1, run["all_frames"])
        if share < 0.98:
            out.append(f"{run['label']}: {100 * (1 - share):.1f}% of the frames had other features than "
                       f"'{run['dominant'][1]}' ({dict(run['variants'])})")

    def compare(name, getter):
        values = {}
        for run in runs:
            for index, file in enumerate(run["files"]):
                key = run["label"] if len(run["files"]) == 1 else f"{run['label']} #{index + 1}"
                try:
                    values[key] = getter({"dominant": run["dominant"], "meta": file["meta"]})
                except (KeyError, TypeError, IndexError):
                    values[key] = None
        if len({json.dumps(v, sort_keys=True) for v in values.values()}) > 1:
            out.append(f"runs differ in {name}: " + "; ".join(f"{k}: {v}" for k, v in values.items()))

    compare("features", lambda r: r["dominant"][1])
    compare("resolution", lambda r: [(x["width"], x["height"]) for x in r["meta"]["resolution"]])
    compare("commit", lambda r: r["meta"]["build"]["commit"])
    compare("local changes", lambda r: r["meta"]["build"]["local_changes"])
    compare("vsync", lambda r: r["meta"]["settings"].get("vsync"))
    compare("preset / config", lambda r: (r["meta"]["build"]["preset"], r["meta"]["build"]["config"]))
    compare("adapters / drivers", lambda r: [(a["name"], a["driver_version"]) for a in r["meta"]["adapters"]])
    return out


def fmt(value, digits=3):
    return "" if value is None else f"{value:.{digits}f}"


def delta(value, base):
    if value is None or base is None or base == 0:
        return ""
    return f"{(value - base) / base * 100:+.1f}%"


def markdown(runs, results, warns, args):
    lines = []
    base = results[0]
    lines.append("## Runs\n")
    lines.append(f"Frames: the dominant features of each run, {args.settle_frames} frames after every change of features dropped.\n")
    lines.append("| Run | Files | Mode | Features | Frames used | Of all | Resolution | vsync | Commit |")
    lines.append("| --- | ---: | --- | --- | ---: | ---: | --- | --- | --- |")
    for run, res in zip(runs, results):
        meta = run["meta"] or {}
        resolution = ", ".join(f"{x['width']}x{x['height']}" for x in meta.get("resolution", []))
        build = meta.get("build", {})
        commit = (build.get("commit", "?")[:10] + ("+local" if build.get("local_changes") else "")) if build else "?"
        lines.append(f"| {run['label']} | {len(run['files'])} | {run['dominant'][0]} | `{run['dominant'][1] or '-'}` | {res['frames']} | {run['all_frames']} | "
                     f"{resolution} | {meta.get('settings', {}).get('vsync', '?')} | `{commit}` |")

    if warns:
        lines.append("\n## Warnings\n")
        lines.extend(f"- {w}" for w in warns)

    lines.append("\n## CPU frame time (the comparison)\n")
    lines.append("| Run | Mean, ms | Median, ms | p95, ms | p99, ms | FPS (1000 / mean) | Mean vs baseline | p95 vs baseline |")
    lines.append("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    for run, res in zip(runs, results):
        s = res["cpu_frame"] or {}
        b = base["cpu_frame"] or {}
        fps = 1000.0 / s["mean_ms"] if s else None
        lines.append(f"| {run['label']} | {fmt(s.get('mean_ms'))} | {fmt(s.get('median_ms'))} | {fmt(s.get('p95_ms'))} | "
                     f"{fmt(s.get('p99_ms'))} | {fmt(fps, 1)} | {delta(s.get('mean_ms'), b.get('mean_ms'))} | "
                     f"{delta(s.get('p95_ms'), b.get('p95_ms'))} |")

    atlas_runs = [(run, res) for run, res in zip(runs, results) if "atlas" in res]
    if atlas_runs:
        lines.append("\n## Shadow atlas from the second GPU\n")
        lines.append("A new atlas reaches Nau only in some frames; the others show the previous one. Age: frames between the "
                     "frame that drew the atlas and the frame that shows it.\n")
        lines.append("| Run | Frames with a new atlas | Age when used | Age on screen: mean | median | p95 | max | FPS vs baseline |")
        lines.append("| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |")
        base_fps = 1000.0 / base["cpu_frame"]["mean_ms"] if base["cpu_frame"] else None
        for run, res in atlas_runs:
            a = res["atlas"]
            fps = 1000.0 / res["cpu_frame"]["mean_ms"] if res["cpu_frame"] else None
            used = ", ".join(f"{age}: {100 * n / a['frames_with_new_atlas']:.0f}%" for age, n in a["age_at_use"].items())
            gain = f"{fps - base_fps:+.1f}" if fps and base_fps else ""
            lines.append(f"| {run['label']} | {100 * a['new_atlas_share']:.1f}% | {used} | {fmt(a['displayed_age_mean'], 2)} | "
                         f"{fmt(a['displayed_age_median'], 1)} | {fmt(a['displayed_age_p95'], 1)} | {a['displayed_age_max']} | {gain} |")

    timers = sorted({k for res in results for k in res["cpu_timers"]})
    if timers:
        lines.append("\n## CPU timers (render thread)\n")
        lines.append("| Timer | " + " | ".join(f"{r['label']} median / p95, ms" for r in runs) + " |")
        lines.append("| --- | " + " | ".join("---:" for _ in runs) + " |")
        for t in timers:
            cells = []
            for res in results:
                s = res["cpu_timers"].get(t)
                cells.append(f"{fmt(s['median_ms'])} / {fmt(s['p95_ms'])}" if s else "")
            lines.append(f"| `{t}` | " + " | ".join(cells) + " |")

    keys = sorted({k for res in results for k in res["passes"]}, key=lambda k: (k[1], k[2], k[0] != "Frame", k[0]))
    lines.append("\n## GPU passes\n")
    lines.append(f"Passes with a median below {args.min_ms} ms in every run are left out (--min-ms).\n")
    lines.append("| Pass | GPU | Queue | " + " | ".join(f"{r['label']} mean / median / p95, ms" for r in runs) + " |")
    lines.append("| --- | --- | --- | " + " | ".join("---:" for _ in runs) + " |")
    for key in keys:
        values = [res["passes"].get(key) for res in results]
        if all(v is None or v["median_ms"] < args.min_ms for v in values):
            continue
        cells = [f"{fmt(v['mean_ms'])} / {fmt(v['median_ms'])} / {fmt(v['p95_ms'])}" if v else "" for v in values]
        lines.append(f"| `{key[0]}` | {key[1]} | {key[2]} | " + " | ".join(cells) + " |")

    tkeys = sorted({k for res in results for k in res["transfers"]},
                   key=lambda k: (k[0], ["src", "dst", "sum", "span"].index(k[1])))
    if tkeys:
        lines.append("\n## Transfers between GPUs\n")
        lines.append("`sum` = src + dst (GPU work); `span` = start of src to end of dst on the CPU clock (includes waits). "
                     "Bandwidth: payload / median.\n")
        lines.append("| Transfer | Half | GPU | Queue | Run | Samples | Mean, ms | Median, ms | p95, ms | Payload | GB/s |")
        lines.append("| --- | --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |")
        for key in tkeys:
            for run, res in zip(runs, results):
                s = res["transfers"].get(key)
                if not s:
                    continue
                payload = res["transfer_bytes"].get((key[0], key[1]))
                rate = payload / (s["median_ms"] / 1000.0) / 1e9 if payload and s["median_ms"] > 0 and key[1] != "span" else None
                size = f"{payload / 2**20:.2f} MiB" if payload else ""
                lines.append(f"| `{key[0]}` | {key[1]} | {key[2]} | {key[3]} | {run['label']} | {s['samples']} | {fmt(s['mean_ms'])} | "
                             f"{fmt(s['median_ms'])} | {fmt(s['p95_ms'])} | {size} | {fmt(rate, 2)} |")
    return "\n".join(lines) + "\n"


def to_json(runs, results, warns, args):
    out = {"skip_frames": args.skip_frames, "settle_frames": args.settle_frames, "all_features": args.all_features,
           "warnings": warns, "runs": []}
    for run, res in zip(runs, results):
        sources = []
        for file in run["files"]:
            sources.append({"file": os.path.basename(file["csv"]), "sha256": sha256(file["csv"])})
            if file["meta_path"]:
                sources.append({"file": os.path.basename(file["meta_path"]), "sha256": sha256(file["meta_path"])})
        out["runs"].append({
            "label": run["label"],
            "mode": run["dominant"][0],
            "features": run["dominant"][1],
            "frames_used": res["frames"],
            "frames_all": run["all_frames"],
            # Only the file name of the CSV: local paths do not belong in a recorded summary.
            "metadata": [dict(file["meta"], csv=os.path.basename(file["meta"].get("csv", ""))) if file["meta"] else None
                         for file in run["files"]],
            "sources": sources,
            "cpu_frame": res["cpu_frame"],
            "cpu_timers": res["cpu_timers"],
            "atlas": res.get("atlas"),
            "passes": [{"pass": k[0], "gpu": k[1], "queue": k[2], **v} for k, v in sorted(res["passes"].items()) if v],
            "transfers": [{"transfer": k[0], "half": k[1], "gpu": k[2], "queue": k[3],
                           "payload_bytes": res["transfer_bytes"].get((k[0], k[1])), **v}
                          for k, v in sorted(res["transfers"].items()) if v],
        })
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("csv", nargs="+", help="profiler CSV files; the first one is the baseline")
    parser.add_argument("--label", action="append", default=[], help="name of a run, in the order of the CSV files")
    parser.add_argument("--skip-frames", type=int, default=0, help="drop frames up to this profiler frame (in addition to the warmup)")
    parser.add_argument("--all-features", action="store_true", help="keep frames of every features value, not only the dominant one")
    parser.add_argument("--settle-frames", type=int, default=60, help="drop this many frames after every change of features (default 60)")
    parser.add_argument("--min-ms", type=float, default=0.005, help="leave out GPU passes below this median in every run")
    parser.add_argument("--md", help="write the summary as Markdown to this file")
    parser.add_argument("--json", help="write the summary as JSON to this file")
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")

    labels = args.label + [os.path.splitext(os.path.basename(p.split("+")[0]))[0] for p in args.csv[len(args.label):]]
    runs = [load_run(spec, label, args.skip_frames, args.all_features, args.settle_frames) for spec, label in zip(args.csv, labels)]
    results = [analyze(run) for run in runs]
    warns = warnings_for(runs)

    text = markdown(runs, results, warns, args)
    print(text)
    for w in warns:
        print(f"WARNING: {w}", file=sys.stderr)
    if args.md:
        with open(args.md, "w", encoding="utf-8") as f:
            f.write(text)
    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump(to_json(runs, results, warns, args), f, indent=1)


if __name__ == "__main__":
    main()
