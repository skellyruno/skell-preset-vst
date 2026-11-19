#!/usr/bin/env python3
"""Render and measure an offline, same-take Alchemist A/B audition package.

Native audio is produced by SkellPresetAudioProcessor::processBlock. This is
not an external VST3-host or FL Studio render. Preview tracks receive one
shared safety attenuation only; no track is independently normalized.
"""
from __future__ import annotations

import argparse
import base64
import hashlib
import html
import json
import math
import pathlib
import zipfile

import numpy as np
import soundfile as sf
from scipy import signal

try:
    import pyloudnorm as pyln
    from skell_sound_match.quality_gate import measure as quality_measure
except ImportError as exc:
    raise SystemExit(
        "Install owner-approved sound-match dependencies with "
        "python -m pip install -e soundmatch before running this tool."
    ) from exc


SEGMENT_START_DEFAULT = 19.0
SEGMENT_DURATION_DEFAULT = 15.0
EXPECTED_TARGET_DELAY_DEFAULT = 55
SYNC_TOLERANCE_SECONDS = 0.010
EXPECTED_MAIN_FST_CHAIN = (
    "Fruity Parametric EQ 2",
    "Fruity Parametric EQ 2",
    "Fruity Limiter",
    "Waves CLA-76 Stereo",
    "Waves CLA-2A Stereo",
    "Soundgoodizer",
    "Fruity Limiter",
)

# Keep every prior Alchemist candidate and append the lighter serial-dynamics
# threshold/wet-blend experiment for the same-input comparison.
CANDIDATE_FACTORY_IDS = (43, 45, 47, 48, 49, 50, 51, 52, 53, 54)
CANDIDATE_FACTORY_NAMES = {
    43: "Alchemist (Native Preview)",
    45: "Alchemist Refined (Trial)",
    47: "Alchemist Peak Guard (Trial)",
    48: "Alchemist Tone Match (Trial)",
    49: "Alchemist Lighter Dynamics (Trial)",
    50: "Alchemist Fast Compressor Blend (Trial)",
    51: "Alchemist Smooth Compressor Blend (Trial)",
    52: "Alchemist Serial Dynamics (Trial)",
    53: "Alchemist Low-End Contour (Trial)",
    54: "Alchemist Serial Dynamics Blend (Trial)",
}


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_audio(path: pathlib.Path) -> tuple[np.ndarray, int]:
    if not path.is_file():
        raise FileNotFoundError(path)
    audio, sample_rate = sf.read(str(path), dtype="float64", always_2d=True)
    if audio.ndim != 2 or audio.shape[1] not in (1, 2) or len(audio) < sample_rate:
        raise ValueError(f"Unsupported audio layout or duration: {path.name}")
    if not np.isfinite(audio).all():
        raise ValueError(f"Non-finite samples in {path.name}")
    return audio, int(sample_rate)


def write_float_wav(source: pathlib.Path, destination: pathlib.Path) -> dict:
    audio, sample_rate = read_audio(source)
    if destination.exists():
        raise FileExistsError(f"Refusing to overwrite {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    sf.write(str(destination), audio, sample_rate, format="WAV", subtype="FLOAT")
    info = sf.info(str(destination))
    if info.frames != len(audio) or info.samplerate != sample_rate or info.subtype != "FLOAT":
        destination.unlink(missing_ok=True)
        raise IOError("Float WAV conversion did not preserve sample count/rate")
    return {
        "path": str(destination),
        "sample_rate": sample_rate,
        "channels": int(audio.shape[1]),
        "samples": int(len(audio)),
        "sha256": sha256(destination),
    }


def verify_manifest_asset(manifest_path: pathlib.Path, archive_root: pathlib.Path,
                          relative_path: str) -> dict:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    matches = [item for item in manifest.get("assets", [])
               if item.get("path") == relative_path]
    if len(matches) != 1:
        raise ValueError(f"Manifest must contain exactly one {relative_path}")
    item = matches[0]
    actual_path = archive_root / relative_path
    actual_hash = sha256(actual_path)
    actual_size = actual_path.stat().st_size
    if actual_size != item.get("bytes") or actual_hash != item.get("sha256"):
        raise ValueError(f"Owner archive identity mismatch: {relative_path}")
    return {
        "path": relative_path,
        "bytes": actual_size,
        "sha256": actual_hash,
        "original_name": item.get("original_name"),
        "sample_rate": item.get("sample_rate"),
        "channels": item.get("channels"),
        "subtype": item.get("subtype"),
        "sample_count": item.get("sample_count"),
    }


def parse_fst_chain(fst_path: pathlib.Path,
                    chain_manifest_path: pathlib.Path) -> list[str]:
    """Read a hash-bound slot inventory; never count plugin-name substrings.

    FL Studio .fst blobs can contain plugin names inside embedded state. A raw
    text scan therefore cannot distinguish a second rack slot from a string in
    another plugin's state. The owner slot manifest is bound to the exact FST
    SHA-256 and preserves repeated real slots (for example, two Fruity Limiters).
    """
    data = fst_path.read_bytes()
    if not data.startswith(b"FLhd"):
        raise ValueError("MAIN.fst does not have the FL Studio FLhd header")
    manifest = json.loads(chain_manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schema") != "skell.fst.slot-chain-manifest" or manifest.get("version") != 1:
        raise ValueError("Unsupported MAIN.fst slot-chain manifest schema")
    if manifest.get("file") != fst_path.name:
        raise ValueError("MAIN.fst slot-chain manifest names a different source file")
    actual_sha = sha256(fst_path)
    if manifest.get("sha256") != actual_sha:
        raise ValueError("MAIN.fst slot-chain manifest SHA-256 does not match the source")
    slots = manifest.get("slots")
    if not isinstance(slots, list) or not slots:
        raise ValueError("MAIN.fst slot-chain manifest has no slots")
    names: list[str] = []
    previous_slot = -1
    for item in slots:
        if not isinstance(item, dict):
            raise ValueError("Invalid MAIN.fst slot entry")
        slot_number = item.get("slot")
        if not isinstance(slot_number, int) or slot_number <= previous_slot:
            raise ValueError("MAIN.fst slot numbers must be strictly increasing")
        previous_slot = slot_number
        name = item.get("chain_name") or item.get("name")
        if not isinstance(name, str) or not name.strip():
            raise ValueError("MAIN.fst slot entry is missing a readable name")
        names.append(name)
    return names


def parse_fst_report(report_path: pathlib.Path) -> list[dict]:
    report = json.loads(report_path.read_text(encoding="utf-8"))
    return [
        {
            "name": plugin.get("name"),
            "preset_name": plugin.get("preset_name"),
            "active_setup": plugin.get("active_setup"),
            "bypassed_in_chunk": plugin.get("bypassed_in_chunk"),
            "plugin_version": plugin.get("plugin_version"),
        }
        for plugin in report.get("embedded_plugins", [])
    ]


def estimate_delay(source: np.ndarray, output: np.ndarray, sample_rate: int,
                   max_lag_samples: int = 1024,
                   analysis_start_seconds: float | None = None,
                   analysis_duration_seconds: float | None = None) -> tuple[int, float, int]:
    """Return output delay relative to source, confidence, and correlation polarity.

    When an analysis window is supplied, measure it inside that exact source-time
    interval. The A/B tool passes its audition interval here so a delay estimated
    from a different passage cannot shift the 15-second comparison.
    """
    count = min(len(source), len(output))
    if (analysis_start_seconds is None) != (analysis_duration_seconds is None):
        raise ValueError("Alignment analysis start and duration must be supplied together")

    if analysis_start_seconds is None:
        # Backward-compatible default for callers that need a full-recording estimate.
        analysis_start = min(round(0.8 * sample_rate), max(0, count // 10))
        analysis_end = min(round(18.5 * sample_rate), count)
        if analysis_end - analysis_start < 2 * sample_rate:
            analysis_start, analysis_end = 0, count
    else:
        start = float(analysis_start_seconds)
        duration = float(analysis_duration_seconds)
        if (not math.isfinite(start) or not math.isfinite(duration)
                or start < 0.0 or duration < 2.0):
            raise ValueError("Alignment analysis window must be finite, non-negative and at least 2 seconds")
        analysis_start = round(start * sample_rate)
        analysis_end = round((start + duration) * sample_rate)
        if analysis_end > count or analysis_end <= analysis_start:
            raise ValueError("Alignment analysis window is outside the available audio")
        if analysis_end - analysis_start < 2 * sample_rate:
            raise ValueError("Alignment analysis window must contain at least 2 seconds")

    source_mono = source[analysis_start:analysis_end].mean(axis=1)
    output_mono = output[analysis_start:analysis_end].mean(axis=1)
    cutoff = min(1500.0, sample_rate * 0.35)
    sos = signal.butter(4, cutoff, btype="lowpass", fs=sample_rate, output="sos")
    source_filtered = signal.sosfiltfilt(sos, source_mono)
    output_filtered = signal.sosfiltfilt(sos, output_mono)
    source_filtered -= np.mean(source_filtered)
    output_filtered -= np.mean(output_filtered)

    correlation = signal.correlate(
        output_filtered, source_filtered, mode="full", method="fft"
    )
    lags = signal.correlation_lags(
        len(output_filtered), len(source_filtered), mode="full"
    )
    valid = np.abs(lags) <= max_lag_samples
    denominator = float(np.linalg.norm(source_filtered) * np.linalg.norm(output_filtered))
    if denominator <= 1e-15 or not np.any(valid):
        raise ValueError("Delay estimation has insufficient signal energy")
    normalized = correlation[valid] / denominator
    valid_lags = lags[valid]
    best = int(np.argmax(np.abs(normalized)))
    confidence = float(abs(normalized[best]))
    polarity = 1 if normalized[best] >= 0 else -1
    if confidence < 0.05:
        raise ValueError(f"Delay confidence too low ({confidence:.4f})")
    if abs(int(valid_lags[best])) >= max_lag_samples:
        raise ValueError("Best delay is at the search boundary; increase max lag")
    return int(valid_lags[best]), confidence, polarity


def common_input_window(input_samples: int, outputs: list[tuple[int, int]]) -> tuple[int, int]:
    """Find the input-time interval shared by (output_length, output_delay) pairs."""
    start = max([0] + [-lag for _, lag in outputs])
    end = min([input_samples] + [length - lag for length, lag in outputs])
    if end <= start:
        raise ValueError("No common timeline remains after latency alignment")
    return start, end


def true_peak_linear(audio: np.ndarray) -> float:
    peak = 0.0
    for channel in range(audio.shape[1]):
        interpolated = signal.resample_poly(audio[:, channel], 4, 1)
        if len(interpolated):
            peak = max(peak, float(np.max(np.abs(interpolated))))
    return peak


def db(value: float) -> float:
    return 20.0 * math.log10(max(float(value), 1e-12))


def polarity_normalized_correlation(
    signed_correlation: float | None, relative_polarity: int
) -> float | None:
    """Remove a measured global polarity sign from a diagnostic scalar only.

    The audio itself is never polarity-inverted; the raw signed value remains
    available so the physical phase relationship is not hidden.
    """
    if relative_polarity not in (-1, 1):
        raise ValueError("Relative polarity must be +1 or -1")
    if signed_correlation is None:
        return None
    value = float(signed_correlation)
    if not math.isfinite(value):
        return None
    return value * relative_polarity


def integrated_lufs(audio: np.ndarray, sample_rate: int) -> float | None:
    try:
        measured = pyln.Meter(sample_rate).integrated_loudness(
            audio if audio.shape[1] > 1 else audio[:, 0]
        )
        return float(measured) if math.isfinite(float(measured)) else None
    except Exception:
        return None


def stereo_metrics(audio: np.ndarray) -> dict:
    if audio.shape[1] != 2:
        return {"stereo_correlation": None, "side_mid_db": None}
    left, right = audio[:, 0], audio[:, 1]
    if np.std(left) < 1e-12 or np.std(right) < 1e-12:
        correlation = 0.0
    else:
        correlation = float(np.corrcoef(left, right)[0, 1])
    mid = (left + right) * 0.5
    side = (left - right) * 0.5
    side_mid = db(float(np.sqrt(np.mean(side * side))) /
                  max(float(np.sqrt(np.mean(mid * mid))), 1e-12))
    return {"stereo_correlation": correlation, "side_mid_db": side_mid}


def track_metrics(reference: np.ndarray, candidate: np.ndarray,
                  sample_rate: int, relative_polarity: int = 1) -> dict:
    raw = quality_measure(reference, candidate, sample_rate)
    reference_rms = float(np.sqrt(np.mean(reference * reference)))
    candidate_rms = float(np.sqrt(np.mean(candidate * candidate)))
    if candidate_rms <= 1e-12:
        raise ValueError("Candidate is silent")
    level_gain = reference_rms / candidate_rms
    level_matched = quality_measure(reference, candidate * level_gain, sample_rate)
    reference_lufs = integrated_lufs(reference, sample_rate)
    candidate_lufs = integrated_lufs(candidate, sample_rate)
    return {
        "relative_alignment_polarity": relative_polarity,
        "waveform_correlation_polarity_normalized": polarity_normalized_correlation(
            raw.get("waveform_correlation_signed"), relative_polarity
        ),
        "level_matched_waveform_correlation_polarity_normalized": polarity_normalized_correlation(
            level_matched.get("waveform_correlation_signed"), relative_polarity
        ),
        "raw": raw,
        "rms_level_matched_diagnostic": level_matched,
        "rms_diagnostic_gain_db": db(level_gain),
        "reference_lufs": reference_lufs,
        "candidate_lufs": candidate_lufs,
        "candidate_minus_reference_lufs": (
            candidate_lufs - reference_lufs
            if candidate_lufs is not None and reference_lufs is not None else None
        ),
        **stereo_metrics(candidate),
    }



def attenuation_only_gains(lufs_by_track: dict[str, float | None]) -> tuple[float, dict[str, float]]:
    valid = {
        key: float(value) for key, value in lufs_by_track.items()
        if value is not None and math.isfinite(float(value))
    }
    if not valid:
        raise ValueError("No finite loudness values for attenuation-only matching")
    target_lufs = min(valid.values())
    gains = {key: min(0.0, target_lufs - value) for key, value in valid.items()}
    return target_lufs, gains


def _write_flac(path: pathlib.Path, audio: np.ndarray, sample_rate: int) -> None:
    sf.write(str(path), audio, sample_rate, format="FLAC", subtype="PCM_24")
    info = sf.info(str(path))
    if info.subtype != "PCM_24" or info.samplerate != sample_rate or info.frames != len(audio):
        raise IOError(f"24-bit FLAC verification failed: {path.name}")


def _make_html(out_dir: pathlib.Path, track_rows: list[dict],
               common_gain_db: float, level_match_target_lufs: float,
               provenance: dict) -> None:
    alignment_window = provenance.get("alignment_analysis_window_seconds")
    if isinstance(alignment_window, list) and len(alignment_window) == 2:
        alignment_text = (
            f"{float(alignment_window[0]):.3f}–{float(alignment_window[1]):.3f} s"
        )
    else:
        alignment_text = "the audition window"
    audio_tags = []
    track_buttons = []
    for row in track_rows:
        for mode in ("raw", "level_matched"):
            payload = base64.b64encode(
                (out_dir / row[f"file_{mode}"]).read_bytes()
            ).decode("ascii")
            element_id = f"audio-{mode}-{row['id']}"
            audio_tags.append(
                f'<audio id="{element_id}" preload="auto" '
                f'src="data:audio/flac;base64,{payload}"></audio>'
            )
        track_buttons.append(
            f'<button type="button" data-track="{row["id"]}">'
            f'{html.escape(row["label"])}</button>'
        )

    def format_cell(value: object) -> str:
        if value is None:
            return "—"
        if isinstance(value, (int, np.integer)):
            return str(int(value))
        if isinstance(value, (float, np.floating)):
            numeric = float(value)
            return f"{numeric:.3f}" if math.isfinite(numeric) else "—"
        return str(value)

    table_rows = []
    for row in track_rows:
        metrics = row.get("metrics") or {}
        raw = metrics.get("raw", {})
        table_rows.append(
            "<tr>"
            f"<th>{html.escape(row['label'])}</th>"
            f"<td>{html.escape(format_cell(row.get('lufs_raw')))}</td>"
            f"<td>{html.escape(format_cell(row.get('level_matched_attenuation_db')))}</td>"
            f"<td>{html.escape(format_cell(row.get('lufs_level_matched')))}</td>"
            f"<td>{html.escape(format_cell(row.get('true_peak_dbtp_4x_estimate')))}</td>"
            f"<td>{html.escape(format_cell(raw.get('multires_spectral_mae_db')))}</td>"
            f"<td>{html.escape(format_cell(raw.get('envelope_20ms_mae_db')))}</td>"
            f"<td>{html.escape(format_cell(raw.get('waveform_correlation_signed')))}</td>"
            f"<td>{html.escape(format_cell(metrics.get('relative_alignment_polarity')))}</td>"
            f"<td>{html.escape(format_cell(metrics.get('waveform_correlation_polarity_normalized')))}</td>"
            "</tr>"
        )

    provenance_text = html.escape(json.dumps(provenance, indent=2))
    page = f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Skell Preset — Alchemist same-take A/B</title>
<style>
:root{{color-scheme:dark;font-family:system-ui,sans-serif;background:#101312;color:#e8eee9}}
body{{max-width:1150px;margin:2rem auto;padding:0 1rem}}
h1{{font-size:1.5rem}}p{{line-height:1.5;color:#b8c5bb}}
.controls{{display:flex;gap:.6rem;flex-wrap:wrap;margin:1rem 0}}
button{{background:#1d2b23;color:#eaffed;border:1px solid #45634d;border-radius:8px;padding:.65rem .9rem;cursor:pointer}}
button[aria-pressed="true"]{{background:#2e5839;border-color:#8bd69a}}
input[type=range]{{width:min(100%,700px)}}
.table-scroll{{width:100%;max-width:100%;overflow-x:auto;overscroll-behavior-x:contain;-webkit-overflow-scrolling:touch;border:1px solid #344139;border-radius:8px}}
table{{width:100%;min-width:1280px;border-collapse:collapse;font-size:.82rem;margin:0;font-variant-numeric:tabular-nums}}
th,td{{padding:.5rem .6rem;border-bottom:1px solid #344139;text-align:left;white-space:nowrap}}
tbody th:first-child{{position:sticky;left:0;background:#101312;z-index:1}}
.table-hint{{font-size:.85rem;color:#91a196;margin:.35rem 0 .5rem}}
pre{{white-space:pre-wrap;overflow-wrap:anywhere;background:#171e1a;padding:1rem;border-radius:8px;color:#aebcb2}}
.note{{border-left:3px solid #7caa84;padding:.6rem 1rem;background:#151d18}}
</style>
</head>
<body>
<h1>Alchemist — native same-take comparison</h1>
<p>15 seconds from the same 42.6-second performance. Delay is measured inside the audition window ({alignment_text}), rather than from a different passage. All streams start together; switching changes only which synchronized stream is audible.</p>
<p class="note"><b>Original level:</b> one shared safety attenuation of {common_gain_db:.3f} dB. <b>Level-matched:</b> louder tracks are attenuated down to the quietest track ({level_match_target_lufs:.2f} LUFS); no track is boosted. Both modes retain the same shared safety attenuation. The input already contains printed recording effects; it is not untreated dry vocal.</p>
<div class="controls" id="mode-buttons">
<button type="button" data-mode="raw" aria-pressed="true">Original level</button>
<button type="button" data-mode="level_matched" aria-pressed="false">Level-matched (attenuation only)</button>
</div>
<div class="controls" id="track-buttons">{''.join(track_buttons)}</div>
<div class="controls">
<button id="play" type="button">Play synchronized</button>
<button id="pause" type="button">Pause</button>
<button id="restart" type="button">Restart all</button>
</div>
<p><label for="seek">Position <span id="time">0.00 s</span></label></p>
<input id="seek" type="range" min="0" max="15" step="0.01" value="0">
<div class="controls">
<label>Playback rate
<select id="rate"><option>1</option><option>0.75</option><option>0.5</option></select>
</label>
</div>
{''.join(audio_tags)}
<h2>Measured source and output metrics</h2>
<div class="table-scroll" role="region" aria-label="Measured source and output metrics" tabindex="0">
<table>
<thead><tr><th>Track</th><th>Raw LUFS</th><th>Attenuation-only gain dB</th><th>Matched LUFS</th><th>Raw 4× peak estimate dBTP</th><th>Spectral MAE dB</th><th>20 ms envelope MAE dB</th><th>Raw signed corr.</th><th>Relative polarity</th><th>Polarity-normalized corr.</th></tr></thead>
<tbody>{''.join(table_rows)}</tbody>
</table>
</div>
<p class="table-hint">Scroll horizontally to see all metrics. Values are rounded to 3 decimals here; full precision remains in metrics.json.</p>
<p>Raw spectral and envelope metrics compare each native render against the FL Studio wet print. RMS-level-matched metrics are recorded separately in metrics.json for diagnostics only; they are not used to create either audition mode. The raw signed correlation is preserved. The polarity-normalized correlation removes only the measured global alignment-polarity sign from that scalar; it does not invert any preview audio. The 4× peak is an estimate, not a certified true-peak measurement.</p>
<p>Render source: SkellPresetAudioProcessor::processBlock. This is not an external VST3-host render, FL Studio session test, or user listening approval. No candidate is automatically promoted.</p>
<h2>Provenance</h2>
<pre>{provenance_text}</pre>
<script>
const audio = Array.from(document.querySelectorAll('audio'));
let activeTrackId = 'input';
let activeMode = 'raw';
const seek = document.getElementById('seek');
const timeLabel = document.getElementById('time');
function activeAudioId() {{ return 'audio-' + activeMode + '-' + activeTrackId; }}
function applySelection() {{
  const selected = activeAudioId();
  audio.forEach(a => a.muted = a.id !== selected);
  document.querySelectorAll('[data-track]').forEach(b =>
    b.setAttribute('aria-pressed', String(b.dataset.track === activeTrackId)));
  document.querySelectorAll('[data-mode]').forEach(b =>
    b.setAttribute('aria-pressed', String(b.dataset.mode === activeMode)));
}}
function selectTrack(id) {{ activeTrackId = id; applySelection(); }}
function selectMode(mode) {{ activeMode = mode; applySelection(); }}
document.querySelectorAll('[data-track]').forEach(b =>
  b.addEventListener('click', () => selectTrack(b.dataset.track)));
document.querySelectorAll('[data-mode]').forEach(b =>
  b.addEventListener('click', () => selectMode(b.dataset.mode)));
const syncToleranceSeconds = {SYNC_TOLERANCE_SECONDS:.3f};
let syncAnimationFrame = 0;
function stopSyncLoop() {{
  if (syncAnimationFrame !== 0) cancelAnimationFrame(syncAnimationFrame);
  syncAnimationFrame = 0;
}}
function syncFrame() {{
  const master = document.getElementById(activeAudioId());
  if (!master || master.paused || master.ended) {{
    syncAnimationFrame = 0;
    return;
  }}
  seek.value = master.currentTime;
  timeLabel.textContent = master.currentTime.toFixed(2) + ' s';
  audio.forEach(a => {{
    if (a !== master && !a.paused
        && Math.abs(a.currentTime - master.currentTime) > syncToleranceSeconds)
      a.currentTime = master.currentTime;
  }});
  syncAnimationFrame = requestAnimationFrame(syncFrame);
}}
function startSyncLoop() {{
  if (syncAnimationFrame === 0)
    syncAnimationFrame = requestAnimationFrame(syncFrame);
}}
document.getElementById('play').addEventListener('click', async () => {{
  const active = document.getElementById(activeAudioId());
  const t = active ? active.currentTime : 0;
  audio.forEach(a => {{ a.currentTime = t; a.muted = a.id !== activeAudioId(); }});
  await Promise.all(audio.map(a => a.play().catch(() => null)));
  startSyncLoop();
}});
document.getElementById('pause').addEventListener('click', () => {{
  audio.forEach(a => a.pause());
  stopSyncLoop();
}});
document.getElementById('restart').addEventListener('click', async () => {{
  audio.forEach(a => {{ a.pause(); a.currentTime = 0; }});
  await Promise.all(audio.map(a => a.play().catch(() => null)));
  startSyncLoop();
}});
seek.addEventListener('input', () => audio.forEach(a => a.currentTime = Number(seek.value)));
document.getElementById('rate').addEventListener('change', e =>
  audio.forEach(a => a.playbackRate = Number(e.target.value)));
applySelection();
</script>
</body>
</html>
"""
    (out_dir / "index.html").write_text(page, encoding="utf-8")



def analyze(args: argparse.Namespace) -> dict:
    input_path = pathlib.Path(args.input_flac)
    reference_path = pathlib.Path(args.reference_flac)
    fst_path = pathlib.Path(args.fst)
    fst_report_path = pathlib.Path(args.fst_report)
    fst_chain_manifest_path = pathlib.Path(args.fst_chain_manifest)
    manifest_path = pathlib.Path(args.manifest)
    archive_root = pathlib.Path(args.archive_root)
    out_dir = pathlib.Path(args.out_dir)
    if out_dir.exists():
        raise FileExistsError(f"Refusing to overwrite output directory {out_dir}")

    input_identity = verify_manifest_asset(
        manifest_path, archive_root, "data/alchemist/audio/input_rec_printed.flac"
    )
    reference_identity = verify_manifest_asset(
        manifest_path, archive_root, "data/alchemist/audio/main_effects.flac"
    )
    fst_identity = verify_manifest_asset(
        manifest_path, archive_root, "data/alchemist/presets/MAIN.fst"
    )
    fst_report_identity = verify_manifest_asset(
        manifest_path, archive_root, "data/alchemist/research_reports/MAIN_FST_Structure.json"
    )
    input_audio, sample_rate = read_audio(input_path)
    reference_audio, reference_rate = read_audio(reference_path)
    if reference_rate != sample_rate or reference_audio.shape[1] != input_audio.shape[1]:
        raise ValueError("Alchemist source and target must have identical rate/channel layout")
    if len(input_audio) != len(reference_audio):
        raise ValueError("Alchemist source and target sample counts differ")

    candidate_paths = {
        factory: pathlib.Path(getattr(args, f"factory{factory}"))
        for factory in CANDIDATE_FACTORY_IDS
    }
    renderer_path = pathlib.Path(args.renderer)
    if not renderer_path.is_file():
        raise FileNotFoundError(renderer_path)
    renderer_identity = {
        "file": renderer_path.name,
        "bytes": renderer_path.stat().st_size,
        "sha256": sha256(renderer_path),
    }
    candidates: dict[int, np.ndarray] = {}
    candidate_identities: dict[str, dict] = {}
    delays: dict[str, dict] = {}
    for factory, path in candidate_paths.items():
        candidate, candidate_rate = read_audio(path)
        if candidate_rate != sample_rate or candidate.shape[1] != input_audio.shape[1]:
            raise ValueError(f"Factory {factory} output rate/channels differ from source")
        candidates[factory] = candidate
        candidate_identities[str(factory)] = {
            "file": path.name,
            "bytes": path.stat().st_size,
            "sha256": sha256(path),
        }

    if (args.alignment_start is None) != (args.alignment_duration is None):
        raise ValueError("--alignment-start and --alignment-duration must be supplied together")
    alignment_start = (
        args.segment_start if args.alignment_start is None else args.alignment_start
    )
    alignment_duration = (
        args.segment_duration
        if args.alignment_duration is None else args.alignment_duration
    )
    target_lag, target_confidence, target_polarity = estimate_delay(
        input_audio, reference_audio, sample_rate, args.max_lag,
        alignment_start, alignment_duration
    )
    delays["FL target"] = {
        "delay_samples": target_lag,
        "delay_ms": target_lag * 1000.0 / sample_rate,
        "normalized_correlation_magnitude": target_confidence,
        "correlation_polarity": target_polarity,
        "expected_delay_samples_from_prior_analysis": args.expected_target_delay,
        "difference_from_expected_samples": target_lag - args.expected_target_delay,
        "analysis_start_seconds": alignment_start,
        "analysis_duration_seconds": alignment_duration,
    }
    if abs(target_lag - args.expected_target_delay) > args.max_expected_delay_delta:
        raise ValueError(
            f"FL target delay {target_lag} differs from expected "
            f"{args.expected_target_delay}; inspect source identity/alignment"
        )

    common_lags: list[tuple[int, int]] = [(len(reference_audio), target_lag)]
    for factory, candidate in candidates.items():
        lag, confidence, polarity = estimate_delay(
            input_audio, candidate, sample_rate, args.max_lag,
            alignment_start, alignment_duration
        )
        delays[f"Factory {factory}"] = {
            "delay_samples": lag,
            "delay_ms": lag * 1000.0 / sample_rate,
            "normalized_correlation_magnitude": confidence,
            "correlation_polarity": polarity,
            "analysis_start_seconds": alignment_start,
            "analysis_duration_seconds": alignment_duration,
        }
        common_lags.append((len(candidate), lag))

    common_start, common_end = common_input_window(len(input_audio), common_lags)
    segment_start = round(args.segment_start * sample_rate)
    segment_end = round((args.segment_start + args.segment_duration) * sample_rate)
    if segment_start < common_start or segment_end > common_end:
        raise ValueError("Requested audition segment is outside the aligned common timeline")
    input_segment = input_audio[segment_start:segment_end]
    reference_segment = reference_audio[
        segment_start + target_lag:segment_end + target_lag
    ]
    candidate_segments = {
        factory: candidate[
            segment_start + delays[f"Factory {factory}"]["delay_samples"]:
            segment_end + delays[f"Factory {factory}"]["delay_samples"]
        ]
        for factory, candidate in candidates.items()
    }
    expected_samples = round(args.segment_duration * sample_rate)
    all_segments = [input_segment, reference_segment, *candidate_segments.values()]
    if any(len(track) != expected_samples for track in all_segments):
        raise ValueError("Aligned preview does not have the requested exact sample count")

    metrics = {}
    metrics["Input (printed recording)"] = {
        "lufs": integrated_lufs(input_segment, sample_rate),
        "sample_peak_dbfs": db(float(np.max(np.abs(input_segment)))),
        "true_peak_dbtp_4x_estimate": db(true_peak_linear(input_segment)),
        **stereo_metrics(input_segment),
    }
    metrics["FL target"] = {
        "lufs": integrated_lufs(reference_segment, sample_rate),
        "sample_peak_dbfs": db(float(np.max(np.abs(reference_segment)))),
        "true_peak_dbtp_4x_estimate": db(true_peak_linear(reference_segment)),
        **stereo_metrics(reference_segment),
    }
    for factory, candidate_segment in candidate_segments.items():
        metrics[f"Factory {factory}"] = {
            "lufs": integrated_lufs(candidate_segment, sample_rate),
            "sample_peak_dbfs": db(float(np.max(np.abs(candidate_segment)))),
            "true_peak_dbtp_4x_estimate": db(true_peak_linear(candidate_segment)),
            "comparison_to_FL_target": track_metrics(
                reference_segment,
                candidate_segment,
                sample_rate,
                relative_polarity=(
                    target_polarity
                    * delays[f"Factory {factory}"]["correlation_polarity"]
                ),
            ),
        }

    track_specs = [
        ("input", "Input (printed recording)", input_segment),
        ("reference", "FL target", reference_segment),
        *[
            (f"factory{factory}", f"Factory {factory}", candidate_segments[factory])
            for factory in CANDIDATE_FACTORY_IDS
        ],
    ]
    lufs_by_track = {
        track_id: metrics[label]["lufs"] for track_id, label, _ in track_specs
    }
    level_match_target_lufs, attenuation_db = attenuation_only_gains(lufs_by_track)
    level_matched_segments = {
        track_id: audio * (10.0 ** (attenuation_db[track_id] / 20.0))
        for track_id, _, audio in track_specs
    }

    max_peak = max(true_peak_linear(track) for track in all_segments)
    safe_peak = 10.0 ** (-1.0 / 20.0)
    common_gain = min(1.0, safe_peak / max(max_peak, 1e-12))
    common_gain_db = db(common_gain)

    zip_path = out_dir.parent / f"{out_dir.name}.zip"
    if zip_path.exists():
        raise FileExistsError(f"Refusing to overwrite {zip_path}")
    out_dir.mkdir(parents=True, exist_ok=False)
    raw_dir = out_dir / "audio" / "raw"
    level_matched_dir = out_dir / "audio" / "level-matched"
    raw_dir.mkdir(parents=True)
    level_matched_dir.mkdir(parents=True)
    rows = []
    for track_id, label, raw_audio in track_specs:
        matched_audio = level_matched_segments[track_id]
        raw_file = f"audio/raw/{track_id}.flac"
        matched_file = f"audio/level-matched/{track_id}.flac"
        _write_flac(out_dir / raw_file, raw_audio * common_gain, sample_rate)
        _write_flac(out_dir / matched_file, matched_audio * common_gain, sample_rate)
        rows.append({
            "id": track_id,
            "label": label,
            "file_raw": raw_file,
            "file_level_matched": matched_file,
            "lufs_raw": metrics[label]["lufs"],
            "level_matched_attenuation_db": attenuation_db[track_id],
            "lufs_level_matched": integrated_lufs(matched_audio, sample_rate),
            "sample_peak_dbfs": metrics[label]["sample_peak_dbfs"],
            "true_peak_dbtp_4x_estimate": metrics[label]["true_peak_dbtp_4x_estimate"],
            "level_matched_true_peak_dbtp_4x_estimate": db(
                true_peak_linear(matched_audio * common_gain)
            ),
            "metrics": metrics[label].get("comparison_to_FL_target"),
        })

    fst_chain = parse_fst_chain(fst_path, fst_chain_manifest_path)
    if tuple(fst_chain) != EXPECTED_MAIN_FST_CHAIN:
        raise ValueError(f"Unexpected MAIN.fst verified slot order: {fst_chain}")
    fst_chain_manifest = json.loads(
        fst_chain_manifest_path.read_text(encoding="utf-8")
    )
    fst_chain_manifest_identity = {
        "file": fst_chain_manifest_path.name,
        "bytes": fst_chain_manifest_path.stat().st_size,
        "sha256": sha256(fst_chain_manifest_path),
        "source_manifest": fst_chain_manifest.get("source_manifest"),
    }
    fst_plugins = parse_fst_report(fst_report_path)
    provenance = {
        "plugin_commit": args.plugin_commit,
        "archive_commit": args.archive_commit,
        "archive_branch": args.archive_branch,
        "sample_rate": sample_rate,
        "channels": int(input_audio.shape[1]),
        "source_duration_seconds": len(input_audio) / sample_rate,
        "segment_start_seconds": args.segment_start,
        "segment_duration_seconds": args.segment_duration,
        "alignment_analysis_window_seconds": [
            alignment_start, alignment_start + alignment_duration
        ],
        "alignment": delays,
        "common_input_timeline_samples": [common_start, common_end],
        "common_preview_safety_gain_db": common_gain_db,
        "level_matched_preview_target_lufs": level_match_target_lufs,
        "level_matched_preview_attenuation_db": attenuation_db,
        "preview_modes": {
            "raw": "common safety attenuation only; original relative loudness preserved",
            "level_matched": "attenuation-only to the quietest track; no per-track boosts",
        },
        "preview_true_peak_ceiling_dbtp_4x_estimate": -1.0,
        "source_assets": {
            "input": input_identity,
            "FL_target": reference_identity,
            "MAIN_fst": fst_identity,
            "MAIN_fst_structure_report": fst_report_identity,
            "MAIN_fst_sha256": sha256(fst_path),
            "MAIN_fst_embedded_plugin_report": fst_plugins,
            "MAIN_fst_verified_slot_chain": fst_chain,
            "MAIN_fst_slot_manifest": fst_chain_manifest_identity,
        },
        "renderer_binary": renderer_identity,
        "factory_names": {
            str(factory): name for factory, name in CANDIDATE_FACTORY_NAMES.items()
        },
        "candidate_renders": candidate_identities,
        "limitations": [
            "The input already contains printed recording effects; it is not untreated dry vocal.",
            "The native render uses SkellPresetAudioProcessor::processBlock, not an external VST3 host.",
            "The 4x interpolated peak is an estimate, not a certified true-peak measurement.",
            "The measured delay is an offline cross-correlation estimate; signed polarity is recorded and no audio polarity is changed.",
            "Polarity-normalized correlation adjusts only the diagnostic scalar for the measured global polarity relation; preview audio is never inverted.",
            "Level-matched diagnostics are measurement-only; audition files receive one shared safety attenuation.",
            "No candidate is automatically promoted and no user listening approval is inferred.",
        ],
    }
    report = {
        "title": "Skell Preset Alchemist native same-take comparison",
        "metrics": metrics,
        "provenance": provenance,
        "preview_tracks": rows,
    }
    (out_dir / "metrics.json").write_text(
        json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8"
    )
    _make_html(out_dir, rows, common_gain_db, level_match_target_lufs, provenance)
    readme = (
        "# Alchemist native same-take A/B\n\n"
        "Open index.html locally in a browser. The page embeds every 15-second "
        "24-bit FLAC and uses synchronized playback for switching.\n\n"
        f"- Segment: {args.segment_start:.3f}–{args.segment_start + args.segment_duration:.3f} s\n"
        f"- Delay analysis window: {alignment_start:.3f}–{alignment_start + alignment_duration:.3f} s (same-take excerpt by default).\n"
        f"- Sample rate: {sample_rate} Hz; channels: {input_audio.shape[1]}\n"
        f"- Shared safety attenuation: {common_gain_db:.3f} dB in both modes.\n"
        f"- Level-matched mode attenuates to {level_match_target_lufs:.2f} LUFS; no track is boosted.\n"
        "- Source: printed recording input and FL Studio main-chain wet print.\n"
        "- MAIN.fst chain: six hash-bound owner-manifest slots (one EQ2, two Fruity Limiters).\n"
        f"- Native candidates: factories {', '.join(str(factory) for factory in CANDIDATE_FACTORY_IDS)}.\n"
        "- Original-level and attenuation-only level-matched previews are both included.\n"
        "- Render: compiled SkellPresetAudioProcessor::processBlock; not a hosted VST3 or FL Studio run.\n"
        "- Metrics and SHA-256 provenance: metrics.json.\n"
        "- Listening approval and factory promotion remain manual.\n"
    )
    (out_dir / "README.md").write_text(readme, encoding="utf-8")
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(out_dir.rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(out_dir.parent))
    summary = {
        "out_dir": str(out_dir),
        "zip": str(zip_path),
        "segment": [args.segment_start, args.segment_start + args.segment_duration],
        "alignment_window": [alignment_start, alignment_start + alignment_duration],
        "target_delay_samples": target_lag,
        "candidate_delays": {
            key: value["delay_samples"]
            for key, value in delays.items() if key.startswith("Factory ")
        },
        "common_safety_gain_db": common_gain_db,
        "tracks": [row["label"] for row in rows],
        "fst_chain": fst_chain,
    }
    print(json.dumps(summary, indent=2))
    return summary


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)

    prepare = commands.add_parser("prepare", help="Convert the source FLAC to float WAV for the native renderer")
    prepare.add_argument("--input-flac", required=True)
    prepare.add_argument("--output-wav", required=True)

    run = commands.add_parser("analyze", help="Align, measure, package and create the 15-second audition")
    run.add_argument("--input-flac", required=True)
    run.add_argument("--reference-flac", required=True)
    run.add_argument("--factory43", required=True)
    run.add_argument("--factory45", required=True)
    run.add_argument("--factory47", required=True)
    run.add_argument("--factory48", required=True)
    run.add_argument("--factory49", required=True)
    run.add_argument("--factory50", required=True)
    run.add_argument("--factory51", required=True)
    run.add_argument("--factory52", required=True)
    run.add_argument("--factory53", required=True)
    run.add_argument("--factory54", required=True)
    run.add_argument("--renderer", required=True)
    run.add_argument("--fst", required=True)
    run.add_argument("--fst-report", required=True)
    run.add_argument("--fst-chain-manifest", required=True)
    run.add_argument("--manifest", required=True)
    run.add_argument("--archive-root", required=True)
    run.add_argument("--out-dir", required=True)
    run.add_argument("--plugin-commit", default="unknown")
    run.add_argument("--archive-commit", default="unknown")
    run.add_argument("--archive-branch", default="unknown")
    run.add_argument("--segment-start", type=float, default=SEGMENT_START_DEFAULT)
    run.add_argument("--segment-duration", type=float, default=SEGMENT_DURATION_DEFAULT)
    run.add_argument(
        "--alignment-start", type=float, default=None,
        help="Delay-analysis start in seconds (defaults to the audition segment start)"
    )
    run.add_argument(
        "--alignment-duration", type=float, default=None,
        help="Delay-analysis duration in seconds (defaults to the audition segment duration)"
    )
    run.add_argument("--max-lag", type=int, default=1024)
    run.add_argument("--expected-target-delay", type=int, default=EXPECTED_TARGET_DELAY_DEFAULT)
    run.add_argument("--max-expected-delay-delta", type=int, default=128)

    args = parser.parse_args()
    if args.command == "prepare":
        result = write_float_wav(
            pathlib.Path(args.input_flac), pathlib.Path(args.output_wav)
        )
        print(json.dumps(result, indent=2))
        return
    analyze(args)


if __name__ == "__main__":
    main()
