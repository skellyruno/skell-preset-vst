#!/usr/bin/env python3
"""Same-take diagnostic sweep for Alchemist Factory 54's low-end contour.

The renderer applies temporary APVTS overrides to Factory 54 only. No factory
preset, program index, automation ID, or saved user state is changed.
"""
from __future__ import annotations

import argparse
import json
import math
import pathlib
import zipfile

import numpy as np
from scipy import signal

import alchemist_reference_ab as ab


SEGMENT_START = 19.0
SEGMENT_DURATION = 15.0
BAND_CENTERS_HZ = (31.5, 40.0, 50.0, 63.0, 80.0, 100.0, 125.0, 160.0, 200.0)

CANDIDATES = (
    ("factory54", "Factory 54 baseline (30 Hz HPF, 70 Hz 0 dB)"),
    ("hpf20", "20 Hz HPF, 70 Hz 0 dB"),
    ("hpf20_70minus1", "20 Hz HPF, 70 Hz -1 dB"),
    ("hpf20_70minus2", "20 Hz HPF, 70 Hz -2 dB"),
)

PARAMETER_OVERRIDES = {
    "factory54": {
        "factory_index": 54,
        "classic_eq_hpf_hz": 30.0,
        "classic_eq_hpf_normalized": 0.04,
        "band1_center_hz": 70.0,
        "band1_gain_db": 0.0,
        "band1_gain_normalized": 0.5,
    },
    "hpf20": {
        "factory_index": 54,
        "classic_eq_hpf_hz": 20.0,
        "classic_eq_hpf_normalized": 0.0,
        "band1_center_hz": 70.0,
        "band1_gain_db": 0.0,
        "band1_gain_normalized": 0.5,
    },
    "hpf20_70minus1": {
        "factory_index": 54,
        "classic_eq_hpf_hz": 20.0,
        "classic_eq_hpf_normalized": 0.0,
        "band1_center_hz": 70.0,
        "band1_gain_db": -1.0,
        "band1_gain_normalized": 0.4666667,
    },
    "hpf20_70minus2": {
        "factory_index": 54,
        "classic_eq_hpf_hz": 20.0,
        "classic_eq_hpf_normalized": 0.0,
        "band1_center_hz": 70.0,
        "band1_gain_db": -2.0,
        "band1_gain_normalized": 0.4333333,
    },
}


def sha256(path: pathlib.Path) -> str:
    return ab.sha256(path)


def third_octave_levels_db(audio: np.ndarray, sample_rate: int) -> dict[str, float | None]:
    """Welch mid-channel power in fixed one-third-octave diagnostic bands."""
    if audio.ndim != 2 or audio.shape[1] not in (1, 2) or not np.isfinite(audio).all():
        raise ValueError("Band analysis requires finite mono/stereo audio")
    mid = audio.mean(axis=1)
    nperseg = min(32768, len(mid))
    if nperseg < 1024:
        raise ValueError("Audio is too short for low-end band analysis")
    frequencies, power = signal.welch(
        mid, fs=sample_rate, nperseg=nperseg, noverlap=nperseg // 2,
        scaling="density",
    )
    result: dict[str, float | None] = {}
    for center in BAND_CENTERS_HZ:
        low = center / (2.0 ** (1.0 / 6.0))
        high = center * (2.0 ** (1.0 / 6.0))
        mask = (frequencies >= low) & (frequencies < high)
        if not np.any(mask):
            result[f"{center:g}Hz"] = None
            continue
        band_power = max(float(np.mean(power[mask])), 1e-30)
        result[f"{center:g}Hz"] = float(10.0 * math.log10(band_power))
    return result


def band_diagnostics(
    target: np.ndarray,
    candidate: np.ndarray,
    sample_rate: int,
    rms_gain_db: float,
) -> dict:
    target_bands = third_octave_levels_db(target, sample_rate)
    raw_bands = third_octave_levels_db(candidate, sample_rate)
    gain = 10.0 ** (float(rms_gain_db) / 20.0)
    matched_bands = third_octave_levels_db(candidate * gain, sample_rate)

    def differences(other: dict[str, float | None]) -> dict[str, float | None]:
        result: dict[str, float | None] = {}
        for key, target_value in target_bands.items():
            candidate_value = other.get(key)
            result[key] = (
                float(target_value - candidate_value)
                if target_value is not None and candidate_value is not None
                else None
            )
        return result

    return {
        "target_minus_candidate_raw_db": differences(raw_bands),
        "target_minus_candidate_rms_matched_db": differences(matched_bands),
        "rms_diagnostic_gain_db": float(rms_gain_db),
    }


def _candidate_identity(path: pathlib.Path) -> dict:
    return {"file": path.name, "bytes": path.stat().st_size, "sha256": sha256(path)}


def analyze(args: argparse.Namespace) -> dict:
    out_dir = pathlib.Path(args.out_dir)
    zip_path = out_dir.parent / f"{out_dir.name}.zip"
    if out_dir.exists() or zip_path.exists():
        raise FileExistsError("Refusing to overwrite an existing low-end sweep artifact")

    archive_root = pathlib.Path(args.archive_root)
    manifest_path = pathlib.Path(args.manifest)
    input_identity = ab.verify_manifest_asset(
        manifest_path, archive_root, "data/alchemist/audio/input_rec_printed.flac"
    )
    reference_identity = ab.verify_manifest_asset(
        manifest_path, archive_root, "data/alchemist/audio/main_effects.flac"
    )
    fst_path = pathlib.Path(args.fst)
    fst_report_path = pathlib.Path(args.fst_report)
    fst_identity = ab.verify_manifest_asset(
        manifest_path, archive_root, "data/alchemist/presets/MAIN.fst"
    )
    fst_report_identity = ab.verify_manifest_asset(
        manifest_path, archive_root, "data/alchemist/research_reports/MAIN_FST_Structure.json"
    )
    fst_chain_manifest_path = pathlib.Path(args.fst_chain_manifest)

    input_audio, sample_rate = ab.read_audio(pathlib.Path(args.input_flac))
    reference_audio, reference_rate = ab.read_audio(pathlib.Path(args.reference_flac))
    if reference_rate != sample_rate or reference_audio.shape != input_audio.shape:
        raise ValueError("Alchemist source and target must have identical rate, shape and sample count")

    candidate_paths = {
        "factory54": pathlib.Path(args.factory54),
        "hpf20": pathlib.Path(args.hpf20),
        "hpf20_70minus1": pathlib.Path(args.hpf20_70minus1),
        "hpf20_70minus2": pathlib.Path(args.hpf20_70minus2),
    }
    candidates: dict[str, np.ndarray] = {}
    candidate_identities: dict[str, dict] = {}
    for candidate_id, path in candidate_paths.items():
        candidate, candidate_rate = ab.read_audio(path)
        if candidate_rate != sample_rate or candidate.shape != input_audio.shape:
            raise ValueError(f"{candidate_id} render rate, channels or sample count differ from input")
        candidates[candidate_id] = candidate
        candidate_identities[candidate_id] = _candidate_identity(path)

    target_lag, target_confidence, target_polarity = ab.estimate_delay(
        input_audio, reference_audio, sample_rate, args.max_lag,
        SEGMENT_START, SEGMENT_DURATION,
    )
    if abs(target_lag - args.expected_target_delay) > args.max_expected_delay_delta:
        raise ValueError("FL target delay differs too far from the hash-bound prior estimate")

    delays: dict[str, dict] = {
        "FL target": {
            "delay_samples": target_lag,
            "delay_ms": target_lag * 1000.0 / sample_rate,
            "normalized_correlation_magnitude": target_confidence,
            "correlation_polarity": target_polarity,
            "analysis_start_seconds": SEGMENT_START,
            "analysis_duration_seconds": SEGMENT_DURATION,
        }
    }
    common_lags = [(len(reference_audio), target_lag)]
    candidate_polarities: dict[str, int] = {}
    for candidate_id, candidate in candidates.items():
        lag, confidence, polarity = ab.estimate_delay(
            input_audio, candidate, sample_rate, args.max_lag,
            SEGMENT_START, SEGMENT_DURATION,
        )
        candidate_polarities[candidate_id] = polarity
        delays[candidate_id] = {
            "delay_samples": lag,
            "delay_ms": lag * 1000.0 / sample_rate,
            "normalized_correlation_magnitude": confidence,
            "correlation_polarity": polarity,
            "analysis_start_seconds": SEGMENT_START,
            "analysis_duration_seconds": SEGMENT_DURATION,
        }
        common_lags.append((len(candidate), lag))

    common_start, common_end = ab.common_input_window(len(input_audio), common_lags)
    start = round(SEGMENT_START * sample_rate)
    end = round((SEGMENT_START + SEGMENT_DURATION) * sample_rate)
    if start < common_start or end > common_end:
        raise ValueError("15-second audition is outside the aligned common input timeline")

    input_segment = input_audio[start:end]
    reference_segment = reference_audio[start + target_lag:end + target_lag]
    candidate_segments = {
        candidate_id: candidate[
            start + delays[candidate_id]["delay_samples"]:
            end + delays[candidate_id]["delay_samples"]
        ]
        for candidate_id, candidate in candidates.items()
    }
    expected_samples = round(SEGMENT_DURATION * sample_rate)
    if any(len(track) != expected_samples for track in [
        input_segment, reference_segment, *candidate_segments.values()
    ]):
        raise ValueError("Aligned audition segments do not contain exactly 15 seconds")

    metrics: dict[str, dict] = {
        "Input (printed recording)": {
            "lufs": ab.integrated_lufs(input_segment, sample_rate),
            "sample_peak_dbfs": ab.db(float(np.max(np.abs(input_segment)))),
            "true_peak_dbtp_4x_estimate": ab.db(ab.true_peak_linear(input_segment)),
            **ab.stereo_metrics(input_segment),
        },
        "FL target": {
            "lufs": ab.integrated_lufs(reference_segment, sample_rate),
            "sample_peak_dbfs": ab.db(float(np.max(np.abs(reference_segment)))),
            "true_peak_dbtp_4x_estimate": ab.db(ab.true_peak_linear(reference_segment)),
            **ab.stereo_metrics(reference_segment),
        },
    }
    low_end_metrics: dict[str, dict] = {
        "input": band_diagnostics(
            reference_segment, input_segment, sample_rate,
            ab.track_metrics(reference_segment, input_segment, sample_rate)["rms_diagnostic_gain_db"],
        )
    }
    for candidate_id, candidate_segment in candidate_segments.items():
        label = dict(CANDIDATES)[candidate_id]
        comparison = ab.track_metrics(
            reference_segment, candidate_segment, sample_rate,
            relative_polarity=target_polarity * candidate_polarities[candidate_id],
        )
        metrics[label] = {
            "lufs": ab.integrated_lufs(candidate_segment, sample_rate),
            "sample_peak_dbfs": ab.db(float(np.max(np.abs(candidate_segment)))),
            "true_peak_dbtp_4x_estimate": ab.db(ab.true_peak_linear(candidate_segment)),
            "comparison_to_FL_target": comparison,
            **ab.stereo_metrics(candidate_segment),
        }
        low_end_metrics[candidate_id] = band_diagnostics(
            reference_segment, candidate_segment, sample_rate,
            comparison["rms_diagnostic_gain_db"],
        )

    track_specs = [
        ("input", "Input (printed recording)", input_segment),
        ("reference", "FL target", reference_segment),
        *[
            (candidate_id, dict(CANDIDATES)[candidate_id], candidate_segments[candidate_id])
            for candidate_id, _ in CANDIDATES
        ],
    ]
    lufs_by_track = {
        track_id: metrics[label]["lufs"]
        for track_id, label, _ in track_specs
    }
    level_match_target_lufs, attenuation_db = ab.attenuation_only_gains(lufs_by_track)
    level_matched_segments = {
        track_id: audio * (10.0 ** (attenuation_db[track_id] / 20.0))
        for track_id, _, audio in track_specs
    }

    all_segments = [audio for _, _, audio in track_specs]
    max_peak = max(ab.true_peak_linear(track) for track in all_segments)
    safe_peak = 10.0 ** (-1.0 / 20.0)
    common_gain = min(1.0, safe_peak / max(max_peak, 1e-12))
    common_gain_db = ab.db(common_gain)

    out_dir.mkdir(parents=True, exist_ok=False)
    raw_dir = out_dir / "audio" / "raw"
    matched_dir = out_dir / "audio" / "level-matched"
    raw_dir.mkdir(parents=True)
    matched_dir.mkdir(parents=True)
    rows = []
    for track_id, label, raw_audio in track_specs:
        matched_audio = level_matched_segments[track_id]
        raw_file = f"audio/raw/{track_id}.flac"
        matched_file = f"audio/level-matched/{track_id}.flac"
        ab._write_flac(out_dir / raw_file, raw_audio * common_gain, sample_rate)
        ab._write_flac(out_dir / matched_file, matched_audio * common_gain, sample_rate)
        rows.append({
            "id": track_id,
            "label": label,
            "file_raw": raw_file,
            "file_level_matched": matched_file,
            "lufs_raw": metrics[label]["lufs"],
            "level_matched_attenuation_db": attenuation_db[track_id],
            "lufs_level_matched": ab.integrated_lufs(matched_audio, sample_rate),
            "sample_peak_dbfs": metrics[label]["sample_peak_dbfs"],
            "true_peak_dbtp_4x_estimate": metrics[label]["true_peak_dbtp_4x_estimate"],
            "level_matched_true_peak_dbtp_4x_estimate": ab.db(
                ab.true_peak_linear(matched_audio * common_gain)
            ),
            "metrics": metrics[label].get("comparison_to_FL_target"),
        })

    fst_chain = ab.parse_fst_chain(fst_path, fst_chain_manifest_path)
    if tuple(fst_chain) != ab.EXPECTED_MAIN_FST_CHAIN:
        raise ValueError(f"Unexpected hash-bound MAIN.fst chain: {fst_chain}")
    fst_chain_manifest = json.loads(fst_chain_manifest_path.read_text(encoding="utf-8"))
    renderer_path = pathlib.Path(args.renderer)
    if not renderer_path.is_file():
        raise FileNotFoundError(renderer_path)

    provenance = {
        "plugin_commit": args.plugin_commit,
        "archive_commit": args.archive_commit,
        "archive_branch": args.archive_branch,
        "sample_rate": sample_rate,
        "channels": int(input_audio.shape[1]),
        "source_duration_seconds": len(input_audio) / sample_rate,
        "segment_start_seconds": SEGMENT_START,
        "segment_duration_seconds": SEGMENT_DURATION,
        "alignment_analysis_window_seconds": [SEGMENT_START, SEGMENT_START + SEGMENT_DURATION],
        "alignment": delays,
        "common_input_timeline_samples": [common_start, common_end],
        "common_preview_safety_gain_db": common_gain_db,
        "level_matched_preview_target_lufs": level_match_target_lufs,
        "level_matched_preview_attenuation_db": attenuation_db,
        "preview_true_peak_ceiling_dbtp_4x_estimate": -1.0,
        "source_assets": {
            "input": input_identity,
            "FL_target": reference_identity,
            "MAIN_fst": fst_identity,
            "MAIN_fst_structure_report": fst_report_identity,
            "MAIN_fst_sha256": sha256(fst_path),
            "MAIN_fst_embedded_plugin_report": ab.parse_fst_report(fst_report_path),
            "MAIN_fst_verified_slot_chain": fst_chain,
            "MAIN_fst_slot_manifest": {
                "file": fst_chain_manifest_path.name,
                "bytes": fst_chain_manifest_path.stat().st_size,
                "sha256": sha256(fst_chain_manifest_path),
                "source_manifest": fst_chain_manifest.get("source_manifest"),
            },
        },
        "renderer_binary": _candidate_identity(renderer_path),
        "candidate_renders": candidate_identities,
        "parameter_overrides": PARAMETER_OVERRIDES,
        "limitations": [
            "The input is a REC-processed print, not untreated dry vocal.",
            "The candidate audio is rendered through the compiled processBlock, not an external VST3 host or FL Studio.",
            "The 4x interpolated peak is an estimate, not certified BS.1770 true peak.",
            "Low-end band values are Welch mid-channel diagnostics, not exact Fruity EQ knob positions.",
            "Temporary overrides are diagnostic only; no factory preset or user state is changed.",
            "Level-matched previews use attenuation only; no track is boosted.",
            "No candidate is automatically promoted and no user listening approval is inferred.",
        ],
    }
    report = {
        "title": "Skell Preset Alchemist low-end parameter sweep",
        "metrics": metrics,
        "low_end_band_diagnostics_db": low_end_metrics,
        "provenance": provenance,
        "preview_tracks": rows,
    }
    (out_dir / "metrics.json").write_text(
        json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8"
    )
    ab._make_html(out_dir, rows, common_gain_db, level_match_target_lufs, provenance)
    readme = [
        "# Alchemist low-end parameter sweep",
        "",
        "This is a diagnostic-only same-take sweep of Factory 54. It does not modify the factory bank or promote a preset.",
        "",
        f"- Source: REC-processed print; target: MAIN-effects wet print.",
        f"- Segment: {SEGMENT_START:.3f}–{SEGMENT_START + SEGMENT_DURATION:.3f} s; {sample_rate} Hz; {input_audio.shape[1]} channels.",
        f"- Shared preview safety attenuation: {common_gain_db:.3f} dB.",
        f"- Level-matched mode attenuates to {level_match_target_lufs:.3f} LUFS; no track is boosted.",
        "- Main chain identity is hash-bound to the owner's seven-slot MAIN.fst manifest.",
        "- Native renderer: compiled SkellPresetAudioProcessor::processBlock; not FL Studio or an external VST3 host.",
        "- 4× true-peak values are estimates, not certified BS.1770 measurements.",
        "",
        "## Temporary parameter variants",
        "",
        "| Track | Classic EQ HPF | 70 Hz band gain |",
        "|---|---:|---:|",
        "| Factory 54 baseline | 30 Hz | 0 dB |",
        "| HPF20 | 20 Hz | 0 dB |",
        "| HPF20 / -1 dB | 20 Hz | -1 dB |",
        "| HPF20 / -2 dB | 20 Hz | -2 dB |",
        "",
        "## Low-end band diagnostics",
        "",
        "metrics.json records target-minus-candidate Welch mid-channel band differences at 31.5, 40, 50, 63, 80, 100, 125, 160 and 200 Hz, both raw and RMS-level-matched.",
        "",
        "No factory is promoted. Owner listening and FL Studio session/routing validation remain required.",
        "",
    ]
    (out_dir / "README.md").write_text("\n".join(readme), encoding="utf-8")

    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(out_dir.rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(out_dir.parent))
    summary = {
        "out_dir": str(out_dir),
        "zip": str(zip_path),
        "sample_rate": sample_rate,
        "segment_seconds": [SEGMENT_START, SEGMENT_START + SEGMENT_DURATION],
        "candidate_labels": [label for _, label in CANDIDATES],
        "common_safety_gain_db": common_gain_db,
        "level_match_target_lufs": level_match_target_lufs,
        "tracks": [row["label"] for row in rows],
    }
    print(json.dumps(summary, indent=2))
    return summary


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-flac", required=True)
    parser.add_argument("--reference-flac", required=True)
    parser.add_argument("--factory54", required=True)
    parser.add_argument("--hpf20", required=True)
    parser.add_argument("--hpf20-70minus1", required=True)
    parser.add_argument("--hpf20-70minus2", required=True)
    parser.add_argument("--renderer", required=True)
    parser.add_argument("--fst", required=True)
    parser.add_argument("--fst-report", required=True)
    parser.add_argument("--fst-chain-manifest", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--archive-root", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--plugin-commit", default="unknown")
    parser.add_argument("--archive-commit", default="unknown")
    parser.add_argument("--archive-branch", default="unknown")
    parser.add_argument("--max-lag", type=int, default=1024)
    parser.add_argument("--expected-target-delay", type=int, default=55)
    parser.add_argument("--max-expected-delay-delta", type=int, default=128)
    args = parser.parse_args()
    analyze(args)


if __name__ == "__main__":
    main()
