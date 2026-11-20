from __future__ import annotations

import hashlib
import importlib.util
import json
import pathlib
import sys
import tempfile
import types
import unittest

import numpy as np
import soundfile as sf

ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))

# The production workflow installs the owner-approved skell-sound-match
# package. Keep the test runnable in a minimal local checkout as well.
try:
    import skell_sound_match.quality_gate  # noqa: F401
except ModuleNotFoundError:
    package = types.ModuleType("skell_sound_match")
    package.__path__ = []
    quality = types.ModuleType("skell_sound_match.quality_gate")

    def _measure(reference, candidate, sample_rate):
        reference = np.asarray(reference, dtype=np.float64)
        candidate = np.asarray(candidate, dtype=np.float64)
        r = reference.mean(axis=1)
        c = candidate.mean(axis=1)
        corr = float(np.corrcoef(r, c)[0, 1]) if np.std(r) > 1e-12 and np.std(c) > 1e-12 else 0.0
        rms_r = float(np.sqrt(np.mean(reference * reference)))
        rms_c = float(np.sqrt(np.mean(candidate * candidate)))
        peak_r = float(np.max(np.abs(reference)))
        peak_c = float(np.max(np.abs(candidate)))
        db = lambda value: float(20.0 * np.log10(max(float(value), 1e-12)))
        return {
            "waveform_correlation_signed": corr,
            "null_residual_db": db(np.sqrt(np.mean((reference - candidate) ** 2)) / max(rms_r, 1e-12)),
            "multires_spectral_mae_db": abs(db(rms_r) - db(rms_c)),
            "envelope_5ms_mae_db": 0.0,
            "envelope_20ms_mae_db": 0.0,
            "envelope_200ms_mae_db": 0.0,
            "reference_rms_dbfs": db(rms_r),
            "candidate_rms_dbfs": db(rms_c),
            "reference_sample_peak_dbfs": db(peak_r),
            "candidate_sample_peak_dbfs": db(peak_c),
            "candidate_true_peak_dbtp_4x_estimate": db(peak_c),
            "reference_crest_db": db(peak_r) - db(rms_r),
            "candidate_crest_db": db(peak_c) - db(rms_c),
        }

    quality.measure = _measure
    sys.modules["skell_sound_match"] = package
    sys.modules["skell_sound_match.quality_gate"] = quality

SPEC = importlib.util.spec_from_file_location(
    "alchemist_lowend_sweep", TOOLS / "alchemist_lowend_sweep.py"
)
sweep = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(sweep)


class AlchemistLowEndSweepTests(unittest.TestCase):
    def test_third_octave_diagnostics_report_target_minus_candidate(self):
        sr = 16000
        t = np.arange(sr * 2, dtype=np.float64) / sr
        target = (0.1 * np.sin(2 * np.pi * 40.0 * t))[:, None]
        candidate = target * 0.5
        measured = sweep.band_diagnostics(target, candidate, sr, 20.0 * np.log10(2.0))
        raw = measured["target_minus_candidate_raw_db"]["40Hz"]
        matched = measured["target_minus_candidate_rms_matched_db"]["40Hz"]
        self.assertAlmostEqual(raw, 6.0206, delta=0.15)
        self.assertAlmostEqual(matched, 0.0, delta=0.15)
        self.assertTrue(all(
            value is None or np.isfinite(value)
            for value in measured["target_minus_candidate_raw_db"].values()
        ))

    def _write_archive_asset(self, archive_root, relative, data, sample_rate=None, channels=None):
        path = archive_root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return {
            "path": relative,
            "bytes": path.stat().st_size,
            "sha256": hashlib.sha256(data).hexdigest(),
            "original_name": pathlib.Path(relative).name,
            "sample_rate": sample_rate,
            "channels": channels,
            "subtype": "PCM_24" if sample_rate else None,
            "sample_count": None,
        }

    @staticmethod
    def _delay(audio, samples, gain):
        out = np.zeros_like(audio)
        out[samples:] = audio[:-samples] * gain
        return out

    def test_analysis_creates_lossless_synchronized_package_without_boosts(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            archive_root = root / "archive"
            sr = 16000
            duration = 36
            count = sr * duration
            t = np.arange(count, dtype=np.float64) / sr
            rng = np.random.default_rng(20261119)
            voice = (
                0.025 * np.sin(2 * np.pi * 40.0 * t)
                + 0.020 * np.sin(2 * np.pi * 63.0 * t)
                + 0.015 * np.sin(2 * np.pi * 100.0 * t)
                + 0.018 * np.sin(2 * np.pi * 220.0 * t)
                + 0.008 * np.sin(2 * np.pi * 740.0 * t)
                + 0.002 * rng.normal(size=count)
            )
            source = np.column_stack([voice, voice * 0.97]).astype(np.float64)
            target = self._delay(source, 55, 0.92)
            base = self._delay(source, 132, 0.86)
            hpf20 = self._delay(source, 132, 0.87)
            hpf20_minus1 = self._delay(source, 132, 0.85)
            hpf20_minus2 = self._delay(source, 132, 0.84)

            input_rel = "data/alchemist/audio/input_rec_printed.flac"
            target_rel = "data/alchemist/audio/main_effects.flac"
            input_path = archive_root / input_rel
            target_path = archive_root / target_rel
            input_path.parent.mkdir(parents=True, exist_ok=True)
            sf.write(input_path, source, sr, format="FLAC", subtype="PCM_24")
            sf.write(target_path, target, sr, format="FLAC", subtype="PCM_24")

            fst_rel = "data/alchemist/presets/MAIN.fst"
            fst_report_rel = "data/alchemist/research_reports/MAIN_FST_Structure.json"
            fst_path = archive_root / fst_rel
            fst_path.parent.mkdir(parents=True, exist_ok=True)
            fst_path.write_bytes(b"FLhd test MAIN.fst")
            fst_report_path = archive_root / fst_report_rel
            fst_report_path.parent.mkdir(parents=True, exist_ok=True)
            fst_report_path.write_text(json.dumps({
                "embedded_plugins": [
                    {"name": "CLA-76", "preset_name": "In Your Face Vocal",
                     "active_setup": "SETUP_A", "bypassed_in_chunk": False,
                     "plugin_version": "15.0.70.71"},
                    {"name": "CLA-2A", "preset_name": "Vox",
                     "active_setup": "SETUP_A", "bypassed_in_chunk": False,
                     "plugin_version": "15.0.70.71"},
                ]
            }), encoding="utf-8")

            manifest_assets = []
            for relative, path, original in [
                (input_rel, input_path, "alchemist test vocals no main effects.wav"),
                (target_rel, target_path, "alchemist test main effects on .wav"),
                (fst_rel, fst_path, "MAIN.fst"),
                (fst_report_rel, fst_report_path, "MAIN_FST_Structure.json"),
            ]:
                data = path.read_bytes()
                manifest_assets.append({
                    "path": relative,
                    "bytes": len(data),
                    "sha256": hashlib.sha256(data).hexdigest(),
                    "original_name": original,
                    "sample_rate": sr if relative.endswith(".flac") else None,
                    "channels": 2 if relative.endswith(".flac") else None,
                    "subtype": "PCM_24" if relative.endswith(".flac") else None,
                    "sample_count": count if relative.endswith(".flac") else None,
                })
            manifest_path = archive_root / "archive" / "manifest.json"
            manifest_path.parent.mkdir(parents=True, exist_ok=True)
            manifest_path.write_text(json.dumps({"assets": manifest_assets}), encoding="utf-8")

            chain_manifest_path = root / "alchemist_main_fst_chain.json"
            chain_manifest_path.write_text(json.dumps({
                "schema": "skell.fst.slot-chain-manifest",
                "version": 1,
                "file": "MAIN.fst",
                "sha256": hashlib.sha256(fst_path.read_bytes()).hexdigest(),
                "source_manifest": "owner-manifest",
                "slots": [
                    {"slot": 3, "name": "Fruity Parametric EQ 2"},
                    {"slot": 4, "name": "Fruity Parametric EQ 2"},
                    {"slot": 5, "name": "Fruity Limiter"},
                    {"slot": 6, "chain_name": "Waves CLA-76 Stereo"},
                    {"slot": 7, "chain_name": "Waves CLA-2A Stereo"},
                    {"slot": 8, "name": "Soundgoodizer"},
                    {"slot": 9, "name": "Fruity Limiter"},
                ],
            }), encoding="utf-8")

            renders = {
                "factory54": base,
                "hpf20": hpf20,
                "hpf20_70minus1": hpf20_minus1,
                "hpf20_70minus2": hpf20_minus2,
            }
            render_paths = {}
            for name, audio in renders.items():
                path = root / f"{name}.wav"
                sf.write(path, audio, sr, format="WAV", subtype="FLOAT")
                render_paths[name] = path
            renderer_path = root / "renderer.exe"
            renderer_path.write_bytes(b"test renderer identity")

            out_dir = root / "alchemist-lowend-sweep"
            args = types.SimpleNamespace(
                input_flac=str(input_path),
                reference_flac=str(target_path),
                factory54=str(render_paths["factory54"]),
                hpf20=str(render_paths["hpf20"]),
                hpf20_70minus1=str(render_paths["hpf20_70minus1"]),
                hpf20_70minus2=str(render_paths["hpf20_70minus2"]),
                renderer=str(renderer_path),
                fst=str(fst_path),
                fst_report=str(fst_report_path),
                fst_chain_manifest=str(chain_manifest_path),
                manifest=str(manifest_path),
                archive_root=str(archive_root),
                out_dir=str(out_dir),
                plugin_commit="test-commit",
                archive_commit="test-archive",
                archive_branch="test-branch",
                max_lag=1024,
                expected_target_delay=55,
                max_expected_delay_delta=128,
            )
            summary = sweep.analyze(args)
            self.assertTrue(pathlib.Path(summary["zip"]).is_file())
            self.assertTrue((out_dir / "index.html").is_file())
            self.assertTrue((out_dir / "README.md").is_file())
            report = json.loads((out_dir / "metrics.json").read_text(encoding="utf-8"))
            self.assertEqual(len(report["preview_tracks"]), 6)
            self.assertEqual(report["provenance"]["alignment"]["FL target"]["delay_samples"], 55)
            self.assertEqual(report["provenance"]["parameter_overrides"]["hpf20"]["classic_eq_hpf_hz"], 20.0)
            self.assertTrue(all(value <= 1e-9 for value in report["provenance"]["level_matched_preview_attenuation_db"].values()))
            self.assertLessEqual(report["provenance"]["common_preview_safety_gain_db"], 1e-9)
            self.assertIn("low_end_band_diagnostics_db", report)
            self.assertIn("40Hz", report["low_end_band_diagnostics_db"]["hpf20"]["target_minus_candidate_raw_db"])
            for row in report["preview_tracks"]:
                raw = out_dir / row["file_raw"]
                matched = out_dir / row["file_level_matched"]
                for path in (raw, matched):
                    info = sf.info(path)
                    self.assertEqual(info.subtype, "PCM_24")
                    self.assertEqual(info.samplerate, sr)
                    self.assertEqual(info.frames, sr * 15)
            with self.assertRaises(FileExistsError):
                sweep.analyze(args)

    def test_nonfinite_band_input_is_rejected(self):
        audio = np.zeros((32000, 2), dtype=np.float64)
        audio[100, 0] = np.nan
        with self.assertRaisesRegex(ValueError, "finite"):
            sweep.third_octave_levels_db(audio, 16000)


if __name__ == "__main__":
    unittest.main()
