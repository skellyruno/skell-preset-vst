from __future__ import annotations

import importlib.util
import json
import pathlib
import tempfile
import unittest

import numpy as np
import soundfile as sf
ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "alchemist_reference_ab", ROOT / "tools" / "alchemist_reference_ab.py"
)
ab = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(ab)


class AlchemistReferenceABTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.temp.name)
        self.sample_rate = 44100
        self.samples = self.sample_rate * 36
        rng = np.random.default_rng(20261117)
        mono = rng.normal(0.0, 0.025, self.samples)
        from scipy import signal
        sos = signal.butter(4, 3200, fs=self.sample_rate, btype="lowpass", output="sos")
        mono = signal.sosfilt(sos, mono)
        t = np.arange(self.samples) / self.sample_rate
        mono += 0.04 * np.sin(2 * np.pi * 180 * t) + 0.012 * np.sin(2 * np.pi * 740 * t)
        self.source = np.column_stack([mono, mono * 0.91]).astype(np.float64)

    def tearDown(self):
        self.temp.cleanup()

    def _delayed(self, lag, gain=1.0):
        output = np.zeros_like(self.source)
        if lag >= 0:
            output[lag:] = self.source[:len(self.source) - lag] * gain
        else:
            output[:lag] = self.source[-lag:] * gain
        return output

    def test_delay_estimator_sign_and_precision(self):
        delayed = self._delayed(55, 0.83)
        lag, confidence, polarity = ab.estimate_delay(
            self.source, delayed, self.sample_rate, 512
        )
        self.assertEqual(lag, 55)
        self.assertGreater(confidence, 0.9)
        self.assertEqual(polarity, 1)

    def test_alignment_keeps_same_input_timeline(self):
        target_lag = 55
        candidate_lag = 288
        common_start, common_end = ab.common_input_window(
            len(self.source),
            [(len(self.source), target_lag), (len(self.source), candidate_lag)],
        )
        self.assertEqual(common_start, 0)
        self.assertEqual(common_end, len(self.source) - candidate_lag)
        target = self._delayed(target_lag)
        candidate = self._delayed(candidate_lag)
        start, end = self.sample_rate * 19, self.sample_rate * 34
        aligned_target = target[start + target_lag:end + target_lag]
        aligned_candidate = candidate[start + candidate_lag:end + candidate_lag]
        np.testing.assert_allclose(aligned_target, aligned_candidate, atol=1e-12)

    def test_prepare_refuses_overwrite_and_preserves_float_samples(self):
        source_path = self.root / "source.flac"
        output_path = self.root / "prepared.wav"
        sf.write(source_path, self.source, self.sample_rate, format="FLAC", subtype="PCM_24")
        result = ab.write_float_wav(source_path, output_path)
        self.assertEqual(result["sample_rate"], self.sample_rate)
        info = sf.info(output_path)
        self.assertEqual(info.subtype, "FLOAT")
        self.assertEqual(info.frames, self.samples)
        with self.assertRaises(FileExistsError):
            ab.write_float_wav(source_path, output_path)

    def test_fst_chain_parser_preserves_order_and_duplicates(self):
        fst = self.root / "MAIN.fst"
        fst.write_bytes(
            b"FLhd Fruity Parametric EQ 2 Fruity Parametric EQ 2 "
            b"Fruity Limiter CLA-76 Stereo CLA-2A Stereo "
            b"Soundgoodizer Fruity Limiter"
        )
        self.assertEqual(
            ab.parse_fst_chain(fst),
            [
                "Fruity Parametric EQ 2",
                "Fruity Parametric EQ 2",
                "Fruity Limiter",
                "Waves CLA-76 Stereo",
                "Waves CLA-2A Stereo",
                "Soundgoodizer",
                "Fruity Limiter",
            ],
        )

    def test_parse_fst_chain_rejects_non_flstudio_header(self):
        fst = self.root / "invalid.fst"
        fst.write_bytes(b"NOhd Fruity Parametric EQ 2")
        with self.assertRaises(ValueError):
            ab.parse_fst_chain(fst)

    def test_manifest_verification_detects_tampering(self):
        archive_root = self.root / "archive"
        asset = archive_root / "data" / "alchemist" / "audio" / "input_rec_printed.flac"
        asset.parent.mkdir(parents=True)
        asset.write_bytes(b"original")
        manifest = self.root / "manifest.json"
        import hashlib
        manifest.write_text(json.dumps({
            "assets": [{
                "path": "data/alchemist/audio/input_rec_printed.flac",
                "bytes": len(b"original"),
                "sha256": hashlib.sha256(b"original").hexdigest(),
            }]
        }), encoding="utf-8")
        ab.verify_manifest_asset(
            manifest, archive_root, "data/alchemist/audio/input_rec_printed.flac"
        )
        asset.write_bytes(b"tampered")
        with self.assertRaises(ValueError):
            ab.verify_manifest_asset(
                manifest, archive_root, "data/alchemist/audio/input_rec_printed.flac"
            )

    def test_attenuation_only_loudness_match_never_boosts(self):
        target, gains = ab.attenuation_only_gains({
            "loud": -10.0,
            "middle": -12.0,
            "quiet": -15.0,
        })
        self.assertEqual(target, -15.0)
        self.assertEqual(gains, {"loud": -5.0, "middle": -3.0, "quiet": 0.0})
        self.assertTrue(all(value <= 0.0 for value in gains.values()))

    def test_common_preview_gain_never_boosts(self):
        hot = self.source[:self.sample_rate * 15] * 20.0
        peak = ab.true_peak_linear(hot)
        safe_peak = 10 ** (-1 / 20)
        gain = min(1.0, safe_peak / max(peak, 1e-12))
        self.assertLess(gain, 1.0)
        self.assertLessEqual(ab.true_peak_linear(hot * gain), safe_peak + 1e-6)

    def test_polarity_normalized_correlation_is_diagnostic_only(self):
        raw_signed = -0.236
        self.assertAlmostEqual(
            ab.polarity_normalized_correlation(raw_signed, -1), 0.236
        )
        self.assertAlmostEqual(
            ab.polarity_normalized_correlation(raw_signed, 1), -0.236
        )
        self.assertIsNone(ab.polarity_normalized_correlation(None, -1))
        with self.assertRaises(ValueError):
            ab.polarity_normalized_correlation(raw_signed, 0)

    def test_audition_html_uses_tight_animation_frame_sync(self):
        self.assertAlmostEqual(ab.SYNC_TOLERANCE_SECONDS, 0.010)
        out_dir = self.root / "audition"
        (out_dir / "audio" / "raw").mkdir(parents=True)
        (out_dir / "audio" / "level-matched").mkdir(parents=True)
        sample = self.source[:self.sample_rate]
        sf.write(
            out_dir / "audio" / "raw" / "input.flac",
            sample, self.sample_rate, format="FLAC", subtype="PCM_24"
        )
        sf.write(
            out_dir / "audio" / "level-matched" / "input.flac",
            sample, self.sample_rate, format="FLAC", subtype="PCM_24"
        )
        ab._make_html(
            out_dir,
            [{
                "id": "input",
                "label": "Input",
                "file_raw": "audio/raw/input.flac",
                "file_level_matched": "audio/level-matched/input.flac",
                "lufs_raw": -16.0,
                "level_matched_attenuation_db": 0.0,
                "lufs_level_matched": -16.0,
                "true_peak_dbtp_4x_estimate": -1.7,
                "metrics": None,
            }],
            -1.0,
            -16.0,
            {"test": "sync"},
        )
        page = (out_dir / "index.html").read_text(encoding="utf-8")
        self.assertIn("const syncToleranceSeconds = 0.010;", page)
        self.assertIn("requestAnimationFrame(syncFrame)", page)
        self.assertIn("cancelAnimationFrame(syncAnimationFrame)", page)
        self.assertIn("startSyncLoop();", page)
        self.assertNotIn("timeupdate", page)


if __name__ == "__main__":
    unittest.main()
