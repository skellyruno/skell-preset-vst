#!/usr/bin/env python3
"""Offline dry/wet FL Studio WAV QC and alignment report; never modifies audio.

Supports RIFF mono/stereo PCM16/24/32 and IEEE float32, including extensible
format. Uses only Python stdlib. This does NOT fit or import .skellpreset files.
"""
import argparse
import json
import math
from pathlib import Path
import struct

MAX_PREVIEW_SECONDS=3
CHUNK_FRAMES=4096

class WavFormatError(ValueError):
    pass

def inspect_wav(path):
    path=Path(path)
    total_bytes=path.stat().st_size
    with path.open('rb') as f:
        header=f.read(12)
        if len(header)!=12 or header[:4]!=b'RIFF' or header[8:]!=b'WAVE':
            raise WavFormatError(f'{path.name}: expected RIFF/WAVE')
        riff_end=struct.unpack_from('<I',header,4)[0]+8
        if riff_end>total_bytes:
            raise WavFormatError(f'{path.name}: truncated RIFF container')
        fmt=data=None
        chunk_count=0
        while f.tell()+8<=riff_end:
            chunk_count+=1
            if chunk_count>10000:
                raise WavFormatError('Too many WAV chunks')
            chunk_id,chunk_size=struct.unpack('<4sI',f.read(8))
            position=f.tell()
            if position+chunk_size>riff_end:
                raise WavFormatError(f'{path.name}: truncated WAV chunk')
            if chunk_id==b'fmt ':
                raw=f.read(min(chunk_size,64))
                if len(raw)<16:
                    raise WavFormatError('Truncated WAV format block')
                tag,channels,rate,byte_rate,align,bits=struct.unpack_from('<HHIIHH',raw)
                if tag==0xfffe:
                    if len(raw)<40:
                        raise WavFormatError('Truncated extensible WAV format block')
                    tag=struct.unpack_from('<H',raw,24)[0]
                if tag not in (1,3) or channels not in (1,2) or bits not in (16,24,32) or (tag==3 and bits!=32):
                    raise WavFormatError('Only mono/stereo PCM16/24/32 and float32 WAV supported')
                if rate<8000 or rate>384000 or align!=channels*bits//8 or byte_rate!=rate*align:
                    raise WavFormatError('Invalid sample rate or block alignment')
                fmt=(tag,channels,rate,align,bits)
            elif chunk_id==b'data' and data is None:
                data=(position,chunk_size)
            f.seek(position+chunk_size+(chunk_size&1))
        if fmt is None or data is None:
            raise WavFormatError('Missing fmt or data WAV chunk')
        tag,channels,rate,align,bits=fmt
        offset,length=data
        if length%align:
            raise WavFormatError('Unaligned audio sample data')
        frames=length//align
        if frames<1024:
            raise WavFormatError('Reference too short (<1024 frames)')
        peak=0.0
        sum_sq=0.0
        preview=[]
        limit=min(frames,rate*MAX_PREVIEW_SECONDS)
        f.seek(offset)
        for start in range(0,frames,CHUNK_FRAMES):
            count=min(CHUNK_FRAMES,frames-start)
            buf=f.read(count*align)
            if len(buf)!=count*align:
                raise WavFormatError('Truncated audio payload')
            if tag==3:
                values=(x[0] for x in struct.iter_unpack('<f',buf))
            elif bits==16:
                values=(x[0]/32768.0 for x in struct.iter_unpack('<h',buf))
            elif bits==32:
                values=(x[0]/2147483648.0 for x in struct.iter_unpack('<i',buf))
            else:
                values=(int.from_bytes(buf[i:i+3],'little',signed=True)/8388608.0 for i in range(0,len(buf),3))
            mono_sum=0.0
            ch=0
            for value in values:
                if not math.isfinite(value):
                    raise WavFormatError(f'{path.name}: NaN/Inf audio sample')
                peak=max(peak,abs(value))
                sum_sq+=value*value
                mono_sum+=value
                ch+=1
                if ch==channels:
                    ch=0
                    if start<limit and len(preview)<limit:
                        preview.append(mono_sum/channels)
                    mono_sum=0.0
        rms=math.sqrt(sum_sq/(frames*channels))
        return {
            'file':path.name,'sample_rate':rate,'channels':channels,
            'frames':frames,'seconds':round(frames/rate,5),
            'encoding':'float32' if tag==3 else f'pcm{bits}',
            'peak_dbfs':round(20*math.log10(max(peak,1e-12)),3),
            'rms_dbfs':round(20*math.log10(max(rms,1e-12)),3),
            'peak_over_0dbfs':peak>1.0,'preview':preview,
        }

def estimate_lag(a,b,sample_rate):
    if not a or not b:
        return 0,0.0
    max_lag=min(int(sample_rate*.10),4800,min(len(a),len(b))//3)
    window=min(4096,max(128,min(len(a),len(b))-2*max_lag))
    if window<128:
        return 0,0.0
    starts=range(max_lag,max(max_lag+1,len(a)-window-max_lag),max(64,window//4))
    start=max(starts,key=lambda x:sum(t*t for t in a[x:x+window:16]))
    def score(lag,stride):
        aa=a[start:start+window:stride]
        bb=b[start+lag:start+lag+window:stride]
        if len(aa)!=len(bb):
            return 0.0
        dot=sum(x*y for x,y in zip(aa,bb))
        ea=sum(x*x for x in aa)
        eb=sum(y*y for y in bb)
        if ea<1e-12 or eb<1e-12:
            return 0.0
        return abs(dot/math.sqrt(ea*eb))
    # Every INTEGER lag must be considered: coarse lag stride can miss
    # broadband consonants and select a distant periodic alias.
    coarse=max(range(-max_lag,max_lag+1),key=lambda lag:score(lag,16))
    fine=max(range(max(-max_lag,coarse-4),min(max_lag,coarse+4)+1),
             key=lambda lag:score(lag,1))
    return fine,score(fine,1)

def diagnose(dry_path,wet_path):
    dry,wet=inspect_wav(dry_path),inspect_wav(wet_path)
    warnings=[]
    if dry['sample_rate']!=wet['sample_rate']:
        raise WavFormatError('Dry and wet WAVs must have the same sample rate')
    if dry['seconds']>wet['seconds']+.02:
        warnings.append('Wet export ends before dry audio; check trimmed tails')
    if dry['rms_dbfs']<-65 or wet['rms_dbfs']<-65:
        warnings.append('Very quiet reference; alignment may be unreliable')
    if dry['peak_over_0dbfs'] or wet['peak_over_0dbfs']:
        warnings.append('Float sample above 0 dBFS; check headroom')
    if dry['channels']!=wet['channels']:
        warnings.append('Channel counts differ; timing check uses mono downmix')
    a,b=dry.pop('preview'),wet.pop('preview')
    lag,correlation=estimate_lag(a,b,dry['sample_rate'])
    if correlation<.15:
        warnings.append('Low dry/wet correlation; timing or pitch effects may prevent waveform alignment')
    return {'dry':dry,'wet':wet,'alignment':{
        'lag_samples':lag,'lag_ms':round(lag*1000/dry['sample_rate'],3),
        'absolute_correlation':round(correlation,6),
        'description':'Positive lag = wet starts later; this is NOT a sound-match percentage'},
        'warnings':warnings,'preset_created':False}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dry',type=Path)
    parser.add_argument('wet',type=Path)
    parser.add_argument('--output',type=Path,help='Optional JSON report')
    args=parser.parse_args()
    try:
        report=diagnose(args.dry,args.wet)
    except (WavFormatError,OSError) as exc:
        parser.error(str(exc))
    result=json.dumps(report,indent=2)
    if args.output:
        args.output.write_text(result+'\n',encoding='utf-8')
    print(result)

if __name__=='__main__':
    main()
