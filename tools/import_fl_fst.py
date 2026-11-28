"""Bounded FL Studio FST event reader and opt-in native EQ settings translator.

No proprietary binary execution. Never overwrite source files. Multi-effect
mixer presets are inventoried, not silently flattened into an incomplete rack.
"""
import argparse
import json
import math
import struct
import zipfile
from pathlib import Path

NAMES = {"Fruity Limiter", "Fruity Compressor", "Fruity Multiband Compressor",
         "Fruity Parametric EQ 2", "Fruity Stereo Enhancer", "Fruity Soft Clipper",
         "Fruity Chorus", "Fruity Flangus", "Fruity Delay", "Fruity Delay 2",
         "Fruity Delay 3", "Fruity Reeverb 2", "Soundgoodizer"}

def records(blob):
    if len(blob) < 22 or len(blob) > 32*1024*1024:
        raise ValueError("Invalid FST size")
    if blob[:8] != b"FLhd\x06\x00\x00\x00" or blob[14:18] != b"FLdt":
        raise ValueError("Unsupported FL container")
    if struct.unpack_from("<I", blob, 18)[0] != len(blob)-22:
        raise ValueError("FLdt length mismatch")
    i=22
    name=None
    result=[]
    while i<len(blob):
        event=blob[i]
        i+=1
        if event<192:
            n=1 if event<64 else 2 if event<128 else 4
        else:
            n=0
            for shift in range(0,35,7):
                if i>=len(blob): raise ValueError("Truncated variable length")
                v=blob[i];i+=1
                n|=(v&127)<<shift
                if not v&128: break
            else: raise ValueError("Oversized variable length")
        if n>len(blob)-i: raise ValueError("Truncated event")
        value=blob[i:i+n];i+=n
        if event==201:
            try: name=value.decode("utf-16-le",errors="strict").rstrip("\0")
            except UnicodeError: raise ValueError("Invalid plugin name") from None
        elif event==213:
            result.append((name or "Unnamed single-effect payload",value))
            name=None
    return result

def eq_settings(payload):
    if len(payload)!=354: raise ValueError("Unsupported EQ2 payload size")
    version,*p=struct.unpack_from("<37i",payload)
    if version not in (7,8): raise ValueError("Unsupported EQ2 version")
    design=struct.unpack_from("<I",payload,0x124)[0]
    options=payload[0x13d]
    mask=struct.unpack_from("<I",payload,0x13e)[0]
    if mask>127: raise ValueError("Unknown band mask")
    if not -1800<=p[35]<=1800: raise ValueError("Invalid EQ main gain")
    result=dict(params=[.5]*16,eqQ=[.5]*8,studioEqKind=[0]*8,studioEqOn=[False]*8,studioEqDynamic=[0.]*8,studioEqExtraShape=[0.]*8,studioEqSlope=[0.]*8)
    warnings=["Native filter design is an approximation, not a FL DSP emulation."]
    if design&1: warnings.append("Source HQ mode is not transferred.")
    if options&1: warnings.append("Source linear-phase mode is not transferred.")
    types={0:0,1:2,2:0,3:1,4:0,5:3,6:0,7:4}
    for b in range(7):
        gain,freq,width,kind,order=(p[b+7*k] for k in range(5))
        if not (-1800<=gain<=1800 and 0<=freq<=65536 and 0<=width<=65536 and 0<=kind<=7 and -3<=order<=3):
            raise ValueError("Band %d has invalid saved controls"%(b+1))
        active=bool(mask&(1<<b)) and kind!=0
        if kind in (2,4): result["studioEqExtraShape"][b]=2 if kind==2 else 1
        if active and order!=0: warnings.append("Band %d: source filter order %d not yet mapped to native slope."%(b+1,order))
        w=width/65536
        if kind in (1,2,3,4):
            q=((1-w)**3*9.9+.1)/math.sqrt(2)
            raw=math.log(q/.35)/math.log(12)
            if active and not 0<=raw<=1: warnings.append("Band %d: source cutoff Q outside native range."%(b+1))
            w=min(1,max(0,raw))
        result["params"][2*b]=freq/65536
        result["params"][2*b+1]=.5+gain/3600
        result["eqQ"][b]=w
        result["studioEqKind"][b]=types.get(kind,0)
        result["studioEqOn"][b]=active
    return result,p[35]/100,warnings

def package(name, settings, main):
    chain=[dict(slotId=i,type=0,enabled=False,wet=1.,params=[.5]*16) for i in range(16)]
    chain[0].update(settings,type=34,enabled=True)
    return dict(schemaVersion=2,activeSlotCount=10,name=name[:80],author="FL settings translation",bank="Imported EQ",
                globals=dict(mode=1,inputDb=0,mix=1,outputDb=main,macros=[.5]*8),chain=chain)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("source",type=Path)
    ap.add_argument("--eq-record",type=int,help="Zero-based record from inventory")
    ap.add_argument("--output",type=Path)
    ap.add_argument("--allow-approximate",action="store_true")
    args=ap.parse_args()
    if args.source.stat().st_size>32*1024*1024: ap.error("Oversized file")
    data=records(args.source.read_bytes())
    if args.eq_record is None:
        print(json.dumps([dict(record=i,name=n,bytes=len(b),recognized=n in NAMES,
            translation="approximate EQ settings" if n=="Fruity Parametric EQ 2" else "inspection only")
            for i,(n,b) in enumerate(data)],indent=2))
        return
    if not args.allow_approximate or args.output is None or args.output.suffix.lower()!=".skellpreset":
        ap.error("EQ translation requires --allow-approximate and --output NEW.skellpreset")
    if not 0<=args.eq_record<len(data): ap.error("Record out of range")
    name,payload=data[args.eq_record]
    if name!="Fruity Parametric EQ 2": ap.error("Not a named EQ2 record")
    settings,main_gain,warnings=eq_settings(payload)
    doc=package(args.source.stem+" EQ "+str(args.eq_record+1),settings,main_gain)
    # x mode refuses any existing file, including original or another preset.
    with zipfile.ZipFile(args.output,"x",compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr("preset.json",json.dumps(doc,allow_nan=False))
    print(json.dumps(dict(output=str(args.output),warnings=warnings,mainGainAppliedTo="standalone rack output"),indent=2))

if __name__=="__main__":
    try: main()
    except (OSError,ValueError,struct.error) as e: raise SystemExit(str(e))
