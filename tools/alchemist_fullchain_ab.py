#!/usr/bin/env python3
"""Offline, same-take Alchemist A/B diagnostics. No audio is stored or modified.

Usage:
  python tools/alchemist_fullchain_ab.py DRY.wav ORIGINAL_WET.wav [NATIVE_RENDER.wav]
Requires numpy, scipy and soundfile (analysis-only; never VST3 runtime deps).
The first two WAVs describe the original chain. ONLY a separately rendered
third file measures a candidate Skell Preset chain. Correlation is not a %
sound-match score, especially when a phase-changing compressor/EQ is involved.
"""
import argparse
import json
from pathlib import Path
import numpy as np
import soundfile as sf
from scipy import signal


BANDS = ((40, 80), (80, 150), (150, 300), (300, 600),
         (600, 1200), (1200, 2400), (2400, 4500),
         (4500, 8000), (8000, 14000))


def load(path):
    data, rate = sf.read(str(path), dtype="float64", always_2d=True)
    if not np.isfinite(data).all():
        raise ValueError(f"{path}: non-finite audio samples")
    if not len(data) or data.shape[1] not in (1, 2):
        raise ValueError(f"{path}: expected nonempty mono/stereo WAV")
    return data, int(rate)


def info(audio, rate):
    peak = float(np.max(np.abs(audio)))
    rms = float(np.sqrt(np.mean(audio ** 2)))
    # True peak is calculated over a four-times oversampled copy, not
    # simply inferred from the peak sample. This is a diagnostic estimate.
    tp = float(np.max(np.abs(signal.resample_poly(audio, 4, 1, axis=0))))
    return {
        "duration_s": round(len(audio) / rate, 4),
        "channels": audio.shape[1],
        "rms_dbfs": round(20 * np.log10(max(rms, 1e-12)), 3),
        "sample_peak_dbfs": round(20 * np.log10(max(peak, 1e-12)), 3),
        "estimated_true_peak_dbtp": round(20 * np.log10(max(tp, 1e-12)), 3),
        "samples_at_or_above_full_scale": int(np.count_nonzero(np.abs(audio) >= 1.0))
    }


def align(reference, candidate, rate, max_shift_ms=100):
    """Maximize absolute signed correlation, allowing polarity inversion.

    A single global lag is meaningful only for same-take renders; it does
    NOT eliminate frequency-dependent phase distortion.
    """
    x = np.mean(reference, axis=1)
    y = np.mean(candidate, axis=1)
    maxlag = min(int(rate * max_shift_ms / 1000), (len(x) - 1) // 3)
    cross = signal.correlate(y, x, mode="full", method="fft")
    midpoint = len(x) - 1
    around = cross[midpoint - maxlag:midpoint + maxlag + 1]
    lag = int(np.argmax(np.abs(around)) - maxlag)
    if lag >= 0:
        x, y = x[:len(x) - lag], y[lag:]
    else:
        x, y = x[-lag:], y[:len(y) + lag]
    denom = float(np.linalg.norm(x) * np.linalg.norm(y))
    corr = float(np.dot(x, y) / denom) if denom > 1e-16 else 0.0
    return lag, corr


def compare(reference, candidate, rate):
    """Aligned, loudness-normalized errors over matched active frames."""
    lag, corr = align(reference, candidate, rate)
    if lag >= 0:
        reference, candidate = reference[:len(reference)-lag], candidate[lag:]
    else:
        reference, candidate = reference[-lag:], candidate[:len(candidate)+lag]
    n = min(len(reference), len(candidate))
    reference, candidate = reference[:n], candidate[:n]
    target_rms = np.sqrt(np.mean(reference ** 2))
    candidate_rms = np.sqrt(np.mean(candidate ** 2))
    gain = target_rms / max(candidate_rms, 1e-12)
    scaled = candidate * gain

    def envelope_error(ms):
        size = max(1, int(rate * ms / 1000))
        frames = len(reference) // size
        if not frames:
            return None
        target = reference[:frames*size].reshape(frames, size, -1)
        proposed = scaled[:frames*size].reshape(frames, size, -1)
        t = np.sqrt(np.mean(target ** 2, axis=(1, 2)))
        p = np.sqrt(np.mean(proposed ** 2, axis=(1, 2)))
        active = t > 10 ** (-40 / 20)
        if not active.any():
            return None
        return round(float(np.mean(np.abs(20 * np.log10(
            np.maximum(t[active], 1e-10) / np.maximum(p[active], 1e-10))))), 4)

    f, target_psd = signal.welch(reference, fs=rate, nperseg=min(8192,n), axis=0)
    _, candidate_psd = signal.welch(scaled, fs=rate, nperseg=min(8192,n), axis=0)
    bands = {}
    for low, high in BANDS:
        mask = (f >= low) & (f < high)
        if not mask.any():
            continue
        a = max(float(target_psd[mask].sum()), 1e-18)
        b = max(float(candidate_psd[mask].sum()), 1e-18)
        bands[f"{low}-{high}Hz"] = round(10 * np.log10(b / a), 3)

    return {
        "lag_samples": lag, "lag_ms": round(lag * 1000 / rate, 4),
        "signed_aligned_correlation": round(corr, 5),
        "absolute_aligned_correlation": round(abs(corr), 5),
        "gain_to_equal_rms_db": round(20*np.log10(max(gain, 1e-12)), 4),
        "active_20ms_envelope_mae_db": envelope_error(20),
        "active_200ms_envelope_mae_db": envelope_error(200),
        "candidate_minus_target_spectral_band_db": bands,
        "mean_absolute_band_error_db": round(
            float(np.mean(np.abs(list(bands.values())))), 4) if bands else None,
        "important": "This compares audio, not proprietary algorithm equivalence."
    }


def run(dry_path, reference_path, candidate_path=None, start=19.0, end=35.0):
    dry, sr = load(dry_path)
    target, tr = load(reference_path)
    candidate, cr = load(candidate_path) if candidate_path else (None, sr)
    if sr != tr or sr != cr:
        raise ValueError("All WAV sample rates must match; resample before A/B")
    if dry.shape[1] != target.shape[1] or (candidate is not None and candidate.shape[1] != target.shape[1]):
        raise ValueError("All WAV channel counts must match")
    arrays = [dry, target] + ([candidate] if candidate is not None else [])
    length = min(map(len, arrays))
    if length < sr // 2:
        raise ValueError("Need at least 0.5 seconds of common audio")
    i = max(0, round(start * sr))
    j = min(length, round(end * sr))
    if j <= i or j - i < sr // 2:
        i, j = 0, length
    selection = [a[i:j] for a in arrays]
    result = {
        "analysis_kind": "same-take waveform and spectral diagnostics",
        "window_seconds": [round(i/sr, 3), round(j/sr, 3)],
        "sample_rate": sr,
        "dry": info(selection[0], sr),
        "original_wet": info(selection[1], sr),
        "original_wet_vs_dry": compare(selection[0], selection[1], sr),
        "native_match_claim": False
    }
    if candidate is not None:
        result["candidate"] = info(selection[2], sr)
        result["candidate_vs_original_wet"] = compare(selection[1], selection[2], sr)
        result["candidate_filename"] = Path(candidate_path).name
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dry", type=Path)
    parser.add_argument("target", type=Path)
    parser.add_argument("candidate", type=Path, nargs="?")
    parser.add_argument("--start", type=float, default=19.0)
    parser.add_argument("--end", type=float, default=35.0)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    data = run(args.dry, args.target, args.candidate, args.start, args.end)
    payload = json.dumps(data, indent=2)
    if args.output:
        args.output.write_text(payload + "\n", encoding="utf-8")
    print(payload)


if __name__ == "__main__":
    main()
