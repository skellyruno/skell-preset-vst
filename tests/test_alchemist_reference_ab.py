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

    def test_delay_estimator_uses_the_requested_audition_window(self):
        # A deliberately time-varying synthetic offset proves the selected
        # 19–34s audition passage is measured directly, not via the earlier
        # default 0.8–18.5s window.
        output = np.zeros_like(self.source)
        first_end = round(18.5 * self.sample_rate)
        second_start = round(19.0 * self.sample_rate)
        second_end = round(34.0 * self.sample_rate)
        first_lag = 89
        second_lag = 40
        output[first_lag:first_end + first_lag] = self.source[:first_end]
        output[second_start + second_lag:second_end + second_lag] = self.source[second_start:second_end]

        full_recording = ab.estimate_delay(
            self.source, output, self.sample_rate, 512
        )
        audition_window = ab.estimate_delay(
            self.source, output, self.sample_rate, 512, 19.0, 15.0
        )
        self.assertEqual(full_recording[0], first_lag)
        self.assertEqual(audition_window[0], second_lag)
        self.assertGreater(audition_window[1], 0.9)

    def test_delay_estimator_rejects_partial_or_out_of_range_windows(self):
        with self.assertRaisesRegex(ValueError, "supplied together"):
            ab.estimate_delay(
                self.source, self.source, self.sample_rate, 512, 19.0, None
            )
        with self.assertRaisesRegex(ValueError, "outside"):
            ab.estimate_delay(
                self.source, self.source, self.sample_rate, 512, 35.0, 2.0
            )

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

    def _write_fst_chain_manifest(self, fst, slots, sha_override=None):
        import hashlib
        manifest = self.root / "fst-chain.json"
        manifest.write_text(json.dumps({
            "schema": "skell.fst.slot-chain-manifest",
            "version": 1,
            "file": fst.name,
            "sha256": sha_override or hashlib.sha256(fst.read_bytes()).hexdigest(),
            "slots": slots,
        }), encoding="utf-8")
        return manifest

    def test_fst_chain_parser_uses_hash_bound_slots_not_embedded_name_text(self):
        fst = self.root / "MAIN.fst"
        # Repeated plugin-name text in an embedded blob must not invent a rack slot.
        fst.write_bytes(
            b"FLhd FruityParametricEQ2 FruityParametricEQ2 "
            b"FruityLimiter CLA-76 CLA-2A Soundgoodizer FruityLimiter"
        )
        slots = [
            {"slot": 3, "name": "Fruity Parametric EQ 2"},
            {"slot": 4, "name": "Fruity Limiter"},
            {"slot": 5, "chain_name": "Waves CLA-76 Stereo"},
            {"slot": 6, "chain_name": "Waves CLA-2A Stereo"},
            {"slot": 7, "name": "Soundgoodizer"},
            {"slot": 8, "name": "Fruity Limiter"},
        ]
        manifest = self._write_fst_chain_manifest(fst, slots)
        self.assertEqual(
            ab.parse_fst_chain(fst, manifest),
            [
                "Fruity Parametric EQ 2",
                "Fruity Limiter",
                "Waves CLA-76 Stereo",
                "Waves CLA-2A Stereo",
                "Soundgoodizer",
                "Fruity Limiter",
            ],
        )

    def test_fst_chain_parser_rejects_manifest_hash_mismatch(self):
        fst = self.root / "MAIN.fst"
        fst.write_bytes(b"FLhd valid owner file")
        manifest = self._write_fst_chain_manifest(
            fst, [{"slot": 3, "name": "Fruity Parametric EQ 2"}],
            sha_override="0" * 64,
        )
        with self.assertRaisesRegex(ValueError, "SHA-256"):
            ab.parse_fst_chain(fst, manifest)

    def test_parse_fst_chain_rejects_non_flstudio_header(self):
        fst = self.root / "invalid.fst"
        fst.write_bytes(b"NOhd Fruity Parametric EQ 2")
        manifest = self._write_fst_chain_manifest(
            fst, [{"slot": 3, "name": "Fruity Parametric EQ 2"}]
        )
        with self.assertRaisesRegex(ValueError, "FL Studio FLhd"):
            ab.parse_fst_chain(fst, manifest)

    def test_owner_alchemist_chain_fixture_has_one_eq2_and_two_limiters(self):
        fixture = ROOT / "tests" / "fixtures" / "alchemist_main_fst_chain.json"
        manifest = json.loads(fixture.read_text(encoding="utf-8"))
        slots = manifest["slots"]
        names = [item.get("chain_name") or item["name"] for item in slots]
        self.assertEqual(names, list(ab.EXPECTED_MAIN_FST_CHAIN))
        self.assertEqual(names.count("Fruity Parametric EQ 2"), 1)
        self.assertEqual(names.count("Fruity Limiter"), 2)
        soundgoodizer = next(item for item in slots if item.get("name") == "Soundgoodizer")
        self.assertEqual(soundgoodizer["native_state"]["mode_index"], 0)
        self.assertEqual(soundgoodizer["native_state"]["amount_raw_integer"], 169)

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

    def test_candidate_factory_registry_includes_dynamics_blends(self):
        self.assertEqual(ab.CANDIDATE_FACTORY_IDS, (43, 45, 47, 48, 49, 50, 51, 52, 53, 54))
        self.assertEqual(
            ab.CANDIDATE_FACTORY_NAMES[49], "Alchemist Lighter Dynamics (Trial)"
        )
        self.assertEqual(
            ab.CANDIDATE_FACTORY_NAMES[50], "Alchemist Fast Compressor Blend (Trial)"
        )
        self.assertEqual(
            ab.CANDIDATE_FACTORY_NAMES[51], "Alchemist Smooth Compressor Blend (Trial)"
        )
        self.assertEqual(
            ab.CANDIDATE_FACTORY_NAMES[54], "Alchemist Serial Dynamics Blend (Trial)"
        )
        self.assertEqual(
            len(set(ab.CANDIDATE_FACTORY_NAMES.values())),
            len(ab.CANDIDATE_FACTORY_NAMES),
        )

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
            {"test": "sync", "alignment_analysis_window_seconds": [19.0, 34.0]},
        )
        page = (out_dir / "index.html").read_text(encoding="utf-8")
        self.assertIn("19.000–34.000 s", page)
        self.assertIn("The input already contains printed recording effects; it is not untreated dry vocal.", page)
        self.assertIn('class="table-scroll"', page)
        self.assertIn("overflow-x:auto", page)
        self.assertIn("Scroll horizontally to see all metrics.", page)
        self.assertIn("-16.000", page)
        self.assertIn("const syncToleranceSeconds = 0.010;", page)
        self.assertIn("requestAnimationFrame(syncFrame)", page)
        self.assertIn("cancelAnimationFrame(syncAnimationFrame)", page)
        self.assertIn("startSyncLoop();", page)
        self.assertNotIn("timeupdate", page)


if __name__ == "__main__":
    unittest.main()
