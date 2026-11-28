"""Audit extracted-state inventory; never infer enabled effects or destination routing."""
import argparse
import hashlib
import json
import subprocess
import zipfile
from pathlib import Path

KINDS = {'Fruity Limiter', 'Fruity Compressor', 'Fruity Multiband Compressor',
         'Fruity Parametric EQ 2', 'Fruity Stereo Enhancer', 'Fruity Soft Clipper'}
LIMITER = ('gain','saturation','ceiling','attack','attack_curve','release','release_curve',
           'sustain','comp_threshold','ratio','knee','comp_attack','comp_release','comp_curve',
           'rms_window','gate_gain','gate_threshold','gate_release')
COMPRESSOR = ('threshold','ratio','gain','attack','release','type')
EQ2 = tuple(f'{group}_{b+1}' for group in ('gain','frequency','width','type','order') for b in range(7))+('main',)
NAMES = {'Fruity Limiter':LIMITER,'Fruity Compressor':COMPRESSOR,'Fruity Parametric EQ 2':EQ2,
         'Fruity Multiband Compressor':tuple(f'raw_{i}' for i in range(28)),
         'Fruity Stereo Enhancer':('pan','volume','separation','phase_offset','effects_order','inversion'),
         'Fruity Soft Clipper':('threshold','post')}

def sha(b): return hashlib.sha256(b).hexdigest()

def audit(inventory, archives, output, inspector):
    output.mkdir(parents=True, exist_ok=False)
    states=output/'states'; states.mkdir()
    cache={}; decoded={}; result=[]
    for row in json.loads(inventory.read_text(encoding='utf-8')):
        if row['name'] not in KINDS: continue
        ar,member=row['archive'],row['member']
        if Path(ar).name!=ar: raise ValueError('unsafe archive name')
        key=(ar,member)
        if key not in cache:
            with zipfile.ZipFile(archives/ar) as z:
                info=z.getinfo(member)
                if info.file_size>100_000_000: raise ValueError('oversized member')
                b=z.read(info)
            cache[key]=(b,sha(b))
        b,h=cache[key]
        if h!=row['parent_sha256']: raise ValueError('parent hash mismatch')
        at,n=row['offset'],row['size']
        if not(0<=at<=len(b) and 0<n<=4096 and at+n<=len(b)):raise ValueError('invalid payload range')
        raw=b[at:at+n];digest=sha(raw)
        if digest!=row['sha256']:raise ValueError('state hash mismatch')
        path=states/(digest+'.bin')
        if not path.exists():path.write_bytes(raw)
        dk=(row['name'],digest)
        if dk not in decoded:
            p=subprocess.run([str(inspector.resolve()),row['name'],str(path)],capture_output=True,text=True,timeout=10)
            if p.returncode==0:
                parsed=json.loads(p.stdout)
                if parsed['byte_roundtrip'] is not True:raise ValueError('roundtrip failed')
                parsed['named_raw_controls']=dict(zip(NAMES[row['name']],parsed['controls'],strict=True))
                decoded[dk]=dict(status='decoded',state=parsed)
            elif p.returncode==3:decoded[dk]=dict(status='unsupported_or_invalid',state=None)
            else:raise RuntimeError(f'inspector failure {p.returncode}')
        result.append(dict(row,**decoded[dk],routing_verified=False,enabled_verified=False,engine_parity_verified=False))
    data=dict(records=result,occurrences=len(result),unique=len(decoded),decoded_occurrences=sum(r['status']=='decoded' for r in result),decoded_unique=sum(r['status']=='decoded' for r in decoded.values()))
    (output/'report.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
    return data

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('inventory',type=Path);p.add_argument('archives',type=Path);p.add_argument('output',type=Path);p.add_argument('inspector',type=Path);a=p.parse_args()
    data=audit(a.inventory,a.archives,a.output,a.inspector)
    print({k:v for k,v in data.items() if k!='records'})
