"""Build-only artist artwork cache. Never fetch from the plugin/audio thread.

Sources are curated per stable artist identity, not blind name searches.
Keep image bytes in the private catalog/build cache, not public Git.
"""
import base64
import hashlib
import io
import json
from pathlib import Path
import re
import sys
from urllib.parse import urlsplit
from urllib.request import Request, urlopen

LIMIT = 8 * 1024 * 1024
ALLOWED_HOSTS = {'i.scdn.co', 'ugc.production.linktr.ee', 'is1-ssl.mzstatic.com',
                 'is2-ssl.mzstatic.com', 'is3-ssl.mzstatic.com', 'is4-ssl.mzstatic.com',
                 'is5-ssl.mzstatic.com', 'i1.sndcdn.com'}
KEY = re.compile(r'[a-z0-9_-]{1,64}')
HEX = re.compile(r'[0-9a-f]{64}')


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def normalize(raw):
    from PIL import Image, ImageOps
    if not 0 < len(raw) <= LIMIT:
        raise ValueError('invalid image byte count')
    with Image.open(io.BytesIO(raw)) as img:
        if img.width > 4096 or img.height > 4096 or min(img.width, img.height) < 32:
            raise ValueError('invalid artwork dimensions')
        if img.format not in ('JPEG', 'PNG', 'WEBP'):
            raise ValueError('unsupported artwork format')
        img = ImageOps.fit(ImageOps.exif_transpose(img).convert('RGB'), (512, 512),
                           method=Image.Resampling.LANCZOS)
        out = io.BytesIO()
        img.save(out, format='PNG', optimize=True)
        result = out.getvalue()
    if len(result) > 1024*1024:
        raise ValueError('normalized artwork too large')
    return result


def allowed(url):
    u = urlsplit(url)
    return u.scheme == 'https' and u.hostname in ALLOWED_HOSTS and not u.username and not u.password and u.port in (None, 443)


def fetch(url):
    if not allowed(url):
        raise ValueError('unapproved artwork host')
    # Refuse redirects to unknown hosts before they are followed.
    from urllib.request import build_opener, HTTPRedirectHandler
    class RestrictedRedirect(HTTPRedirectHandler):
        def redirect_request(self, req, fp, code, msg, headers, newurl):
            if not allowed(newurl):
                raise ValueError('unapproved artwork redirect')
            return super().redirect_request(req, fp, code, msg, headers, newurl)
    opener = build_opener(RestrictedRedirect())
    with opener.open(Request(url, headers={'User-Agent': 'SkellPresetArtwork/1.0'}), timeout=8) as resp:
        if not allowed(resp.geturl()):
            raise ValueError('unapproved artwork response')
        raw = resp.read(LIMIT + 1)
    return normalize(raw)


def atomic(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    from tempfile import NamedTemporaryFile
    with NamedTemporaryFile(dir=path.parent, delete=False) as f:
        tmp = Path(f.name)
        f.write(data)
    try:
        tmp.replace(path)
    finally:
        tmp.unlink(missing_ok=True)


def prepare_artwork(root, catalog, cache=None, *, offline=False, required=False):
    cache = Path(cache) if cache is not None else root/'artist-artwork-cache'
    sources = json.loads(Path(__file__).with_name('artist_artwork_sources.json').read_text(encoding='utf-8'))['artists']
    rows = []
    for aid, artist in sorted(catalog['artists'].items()):
        if not KEY.fullmatch(aid):
            raise ValueError('invalid artist ID')
        name = artist['name']
        image = None
        source = None
        override = artist.get('cover_sha')
        if override:
            if not HEX.fullmatch(override):
                raise ValueError('invalid artist cover identity')
            raw = (root/'artwork'/f'{override}.png').read_bytes()
            if sha(raw) != override:
                raise ValueError('artist cover checksum mismatch')
            image, source = normalize(raw), {'kind':'owner-cover', 'sha256':override}
        else:
            for item in sources.get(aid, []):
                url = item['url']
                if not allowed(url):
                    raise ValueError('unapproved artist image source')
                dest = cache/aid/(sha(url.encode())+'.png')
                try:
                    if dest.is_file():
                        try:
                            image = normalize(dest.read_bytes())
                        except (OSError, ValueError, ImportError):
                            if offline:
                                raise
                            image = None
                    if image is None:
                        if offline:
                            continue
                        image = fetch(url)
                        atomic(dest, image)
                    source = item
                    break
                except (OSError, ValueError, ImportError) as e:
                    print(f'Artwork unavailable for {aid} ({item["kind"]}): {e}', file=sys.stderr)
                    image = None
        if image is None:
            if required:
                raise ValueError(f'no verified artwork bytes for {aid}')
            print(f'Artwork fallback for {aid}: native cover motif', file=sys.stderr)
            continue
        rows.append(dict(artist=name, artistId=aid, sha256=sha(image),
                         source=source, payload=base64.b64encode(image).decode('ascii')))
    if len(rows)>128 or sum(len(x['payload']) for x in rows)>6*1024*1024:
        raise ValueError('artist artwork bank size limit')
    return rows
