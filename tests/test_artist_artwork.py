"""Build-side artwork regression: no network, private recordings or artist data."""
import base64
import hashlib
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import artist_artwork as a
from PIL import Image


def test_image():
    im = Image.new('RGB', (128, 96), (22, 140, 220))
    b = io.BytesIO()
    im.save(b, format='PNG')
    return b.getvalue()


class ArtistArtwork(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.catalog = {'artists': {'skellyruno': {'name': 'skellyruno'}}}
        self.cache = self.root/'cache'
    def tearDown(self):
        self.tmp.cleanup()
    def test_offline_missing_and_required(self):
        with patch.object(a, 'fetch', side_effect=AssertionError('network in offline mode')):
            self.assertEqual(a.prepare_artwork(self.root, self.catalog, self.cache, offline=True), [])
            with self.assertRaises(ValueError):
                a.prepare_artwork(self.root, self.catalog, self.cache, offline=True, required=True)
    def test_artist_cached_once(self):
        normalized = a.normalize(test_image())
        with patch.object(a, 'fetch', return_value=normalized) as fetch:
            first = a.prepare_artwork(self.root, self.catalog, self.cache, required=True)
            self.assertEqual(fetch.call_count, 1)
        with patch.object(a, 'fetch', side_effect=AssertionError('cache missed')):
            second = a.prepare_artwork(self.root, self.catalog, self.cache, offline=True, required=True)
        self.assertEqual(first, second)
        self.assertEqual(first[0]['artistId'], 'skellyruno')
        raw = base64.b64decode(first[0]['payload'])
        self.assertEqual(hashlib.sha256(raw).hexdigest(), first[0]['sha256'])
        with Image.open(io.BytesIO(raw)) as im:
            self.assertEqual(im.size, (512, 512))
    def test_owner_override_and_integrity(self):
        raw = test_image()
        h = a.sha(raw)
        self.catalog['artists']['skellyruno']['cover_sha'] = h
        p = self.root/'artwork'/f'{h}.png'
        p.parent.mkdir()
        p.write_bytes(raw)
        with patch.object(a, 'fetch', side_effect=AssertionError('owner override ignored')):
            self.assertEqual(a.prepare_artwork(self.root, self.catalog, self.cache)[0]['source']['kind'], 'owner-cover')
        p.write_bytes(b'not image')
        with self.assertRaises(ValueError):
            a.prepare_artwork(self.root, self.catalog, self.cache)
    def test_bad_source_and_image(self):
        self.assertFalse(a.allowed('http://i.scdn.co/image/abc'))
        self.assertFalse(a.allowed('https://i.scdn.co.evil.example/image/abc'))
        self.assertFalse(a.allowed('https://evil.example@i.scdn.co/image/abc'))
        with self.assertRaises((ValueError, OSError)):
            a.normalize(b'not image')
    def test_fallback_source(self):
        normalized = a.normalize(test_image())
        with patch.object(a, 'fetch', side_effect=[OSError('first source unavailable'), normalized]) as fetch:
            result = a.prepare_artwork(self.root, self.catalog, self.cache, required=True)
        self.assertEqual(fetch.call_count, 2)
        self.assertEqual(result[0]['source']['kind'], 'official-social-avatar')

if __name__ == '__main__':
    unittest.main()
