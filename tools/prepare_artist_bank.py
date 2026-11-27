"""Export private Sound Match catalog to a build-only native bank; no runtime Python."""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import re
import zipfile
from artist_artwork import prepare_artwork

KEY = re.compile(r'[a-z0-9_-]{1,64}')
HEX = re.compile(r'[0-9a-f]{64}')


def prepare(root: Path, generation: int, *, artwork_cache=None, offline=False, require_artwork=False):
    if not 0 < generation <= 2**31-1:
        raise ValueError('invalid release generation')
    catalog = json.loads((root/'catalog.json').read_text(encoding='utf-8'))
    if catalog['schema'] != 1:
        raise ValueError('unknown catalog schema')
    rows = []
    for pid, entry in sorted(catalog['presets'].items()):
        latest = entry.get('latest')
        if not latest:
            continue
        aid = entry['artist_id']
        pair = entry.get('pair', pid)
        if not all(KEY.fullmatch(s) for s in (pid, aid, pair)):
            raise ValueError('invalid stable identity')
        revisions = entry['revisions']
        if len(revisions) > 256:
            raise ValueError('revision limit')
        history = []
        for rev in revisions:
            h = rev['sha256']
            if not HEX.fullmatch(h) or h in history:
                raise ValueError('invalid revision hash')
            p = root/'revisions'/pid/(h+'.skellpreset')
            raw = p.read_bytes()
            if hashlib.sha256(raw).hexdigest() != h:
                raise ValueError('revision checksum mismatch')
            with zipfile.ZipFile(p) as z:
                if z.namelist() != ['preset.json'] or z.getinfo('preset.json').file_size > 256*1024 or z.testzip():
                    raise ValueError('invalid native preset archive')
            history.append(h)
        if latest not in history:
            raise ValueError('latest revision missing')
        raw = (root/'revisions'/pid/(latest+'.skellpreset')).read_bytes()
        rows.append(dict(id=pid, artistId=aid, artist=catalog['artists'][aid]['name'],
                         title=entry['name'], type=entry['category'], pair=pair,
                         sha256=latest, history=history, payload=base64.b64encode(raw).decode('ascii')))
    if not 0 < len(rows) <= 1000:
        raise ValueError('invalid bank size')
    artwork = prepare_artwork(root, catalog, artwork_cache, offline=offline, required=require_artwork)
    return dict(schema=1, generation=generation, entries=rows, artwork=artwork)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('catalog', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--generation', required=True, type=int)
    p.add_argument('--artwork-cache', type=Path)
    p.add_argument('--offline', action='store_true', help='Use cached/owner artwork only')
    p.add_argument('--require-artwork', action='store_true', help='Fail private release when an artist has no verified image')
    a = p.parse_args()
    bank = prepare(a.catalog, a.generation, artwork_cache=a.artwork_cache, offline=a.offline, require_artwork=a.require_artwork)
    data = json.dumps(bank, ensure_ascii=False, separators=(',', ':')).encode('utf-8')
    if len(data) > 8*1024*1024:
        raise ValueError('embedded bank exceeds size limit')
    a.output.parent.mkdir(parents=True, exist_ok=True)
    tmp = a.output.with_suffix('.tmp')
    tmp.write_bytes(data)
    tmp.replace(a.output)
    print(f'Prepared {len(bank["entries"])} native artist entries (generation {a.generation})')

if __name__ == '__main__':
    main()
