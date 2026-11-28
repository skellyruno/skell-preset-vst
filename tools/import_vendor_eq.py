"""Conservative EQ setting interchange; never runs third-party binaries.

Formats: explicit Pro-Q 4 FFBS v1 component state (600 floats); Waves
PresetChunkXMLTree, including one named FST wrapper record. The destination
is an independent zero-lookahead Studio EQ, not an emulation of vendor DSP.
Reject unknown or unrepresentable configurations instead of silently dropping
bands. Export is explicit, new-file-only and always accompanied by warnings.
"""
import argparse
import json
import math
import struct
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path
from import_fl_fst import package, records

LIMIT=2*1024*1024

def bounded(v,lo,hi,label):
    if not math.isfinite(v) or not lo<=v<=hi:
        raise ValueError(f"{label} outside native range [{lo}, {hi}]")
    return v

def index(v,lo,hi,label):
    bounded(v,lo,hi,label)
    if v!=int(v):raise ValueError(f"Nonintegral {label}")
    return int(v)

def blank():
    return dict(params=[.5]*16,eqQ=[.5]*8,studioEqKind=[0]*8,
        studioEqOn=[False]*8,studioEqDynamic=[0.]*8,
        studioEqThreshold=[-24.]*8,studioEqAttack=[12.]*8,
        studioEqRelease=[110.]*8,studioEqSlope=[0.]*8,
        studioEqExtraShape=[0.]*8)

def band(settings,b,shape,hz,db,q,slope=12,warnings=None):
    if not 0<=b<8:raise ValueError("More than eight active bands")
    bounded(hz,20,20000,"frequency");bounded(db,-18,18,"gain");bounded(q,.025,40,"Q")
    if shape not in range(7):raise ValueError("Unsupported filter shape")
    settings["params"][2*b]=math.log(hz/20)/math.log(1000)
    settings["params"][2*b+1]=.5+db/36
    settings["studioEqOn"][b]=True
    settings["studioEqKind"][b]=shape if shape<5 else 0
    settings["studioEqExtraShape"][b]=shape-4 if shape>=5 else 0
    if shape==0:
        width=(2*math.asinh(1/(2*q))/math.log(2)-.1)/4.9
    elif shape in (1,2,5,6):
        width=math.log(q/.35)/math.log(12)
    else:
        # Shelf slope isn't the same quantity as a vendor Q.
        width=.5
        if warnings is not None:warnings.append(f"Band {b+1}: shelf Q approximated by native midpoint.")
    if not 0<=width<=1:
        if warnings is not None:warnings.append(f"Band {b+1}: Q clamped to native width range.")
        width=min(1,max(0,width))
    settings["eqQ"][b]=width
    if shape in (1,2):
        native=min((12,24,36),key=lambda x:abs(x-slope))
        settings["studioEqSlope"][b]=(native-12)//12
        if native!=slope and warnings is not None:warnings.append(f"Band {b+1}: {slope} dB/oct approximated by {native} dB/oct.")

def proq4(data):
    # Source research: user-provided FabFilterStates.cpp, fromProQ4.
    # Reject named FabF chunks, other versions and trailing unknown layouts.
    if len(data)!=12+600*4 or data[:4]!=b"FFBS" or struct.unpack_from("<II",data,4)!=(1,600):
        raise ValueError("Expected exact Pro-Q 4 FFBS v1, 600-float component state")
    p=struct.unpack_from("<600f",data,12)
    if not all(map(math.isfinite,p)):raise ValueError("Nonfinite Pro-Q 4 state")
    if index(p[552],0,2,"phase mode")!=0:raise ValueError("Natural/linear phase unsupported")
    if abs(p[554])>1e-5 or abs(p[555]-1)>1e-5:raise ValueError("Non-neutral character or gain scale unsupported")
    shape_map={0:0,1:3,2:1,3:4,4:2,5:5,6:6}
    slopes=(6,6,12,18,24,30,36,48,72,96,96)
    s=blank();b=0;warnings=["Settings translation only; native response/dynamics law differs.",
        "Pro-Q 4 spectral dynamics and ancillary controls are not transferred."]
    for k in range(24):
        x=p[23*k:23*k+23]
        if x[0]<.5 or x[1]<.5:continue
        if index(x[7],0,4,"placement")!=2:raise ValueError("Non-stereo active band unsupported")
        shape=index(x[5],0,9,"shape")
        if shape not in shape_map:raise ValueError("Tilt/all-pass not supported")
        bounded(x[2],math.log2(20),math.log2(20000),"log-frequency")
        q=.025*1600**bounded(x[4],0,1,"Q control")
        band(s,b,shape_map[shape],2**x[2],x[3] if shape in (0,1,3) else 0,q,
             slopes[index(x[6],0,10,"slope")],warnings)
        if x[10]>=.5 and abs(x[9])>.01:
            if shape!=0 or not -12<=x[9]<0:raise ValueError("Only dynamic bell cuts up to 12 dB supported")
            if x[11]>=.5:raise ValueError("Automatic threshold unsupported")
            v=bounded(x[12],0,1,"threshold")
            th=-90+24*v/.125 if v<.125 else -66+21*(v-.125)/.125 if v<.25 else -45+60*(v-.25)
            s["studioEqDynamic"][b]=-x[9]/12
            s["studioEqThreshold"][b]=bounded(th,-80,0,"threshold")
            attack=9.22*10**((bounded(x[13],0,100,"attack")-50)/50)
            release=268*10**((bounded(x[14],0,100,"release")-50)/50)
            s["studioEqAttack"][b]=min(200,max(.5,attack))
            s["studioEqRelease"][b]=min(2000,max(5,release))
            warnings.append(f"Band {b+1}: dynamic timing approximated from measured reference mapping; clamp if outside native range.")
        b+=1
    out=p[556]
    db=36*out if out>=-.6 else 20*math.log10(max(1e-6,.0832*((out+1)/.4)**2))
    return s,bounded(db,-24,24,"output"),warnings

def waves_xml(data):
    # Actual saved Waves FL wrapper uses this exact XML tree and ActiveSetup.
    start=b"<PresetChunkXMLTree";end=b"</PresetChunkXMLTree>"
    a=data.find(start);z=data.find(end,a)
    if a<0 or z<0 or data.find(start,a+len(start))>=0:raise ValueError("Expected exactly one Waves preset tree")
    xml=data[a:z+len(end)]
    if b"<!DOCTYPE" in xml.upper() or b"<!ENTITY" in xml.upper():raise ValueError("Unsafe XML")
    try:tree=ET.fromstring(xml)
    except ET.ParseError as exc:raise ValueError("Malformed Waves XML") from exc
    presets=tree.findall("./Preset")
    if len(presets)!=1:raise ValueError("Expected one Waves preset")
    preset=presets[0];header=preset.find("PresetHeader")
    if header is None:raise ValueError("Missing Waves header")
    name=(header.findtext("PluginName") or "").strip()
    sub=(header.findtext("PluginSubComp") or "").strip()
    active=(header.findtext("ActiveSetup") or "").strip()
    setups=[p for p in preset.findall("PresetData") if p.get("Setup")==active]
    if active not in ("SETUP_A","SETUP_B") or len(setups)!=1:raise ValueError("Ambiguous active setup")
    real=[p for p in setups[0].findall("Parameters") if p.get("Type")=="RealWorld"]
    if len(real)!=1:raise ValueError("Missing active RealWorld parameters")
    tokens=(real[0].text or "").split()
    if not 1<=len(tokens)<=4096:raise ValueError("Invalid Waves parameters")
    def val(i):
        try:v=float(tokens[i])
        except (IndexError,ValueError):raise ValueError(f"Missing numeric Waves control {i}") from None
        if not math.isfinite(v):raise ValueError("Nonfinite Waves control")
        return v
    s=blank();warnings=["Settings translation only; analog/Q/gain laws are not identical."]
    if name in ("REQ","REQ 6","Renaissance EQ","Renaissance EQ 6","Renaissance Equalizer"):
        if len(tokens)<62:raise ValueError("Unsupported REQ layout")
        for b in range(6):
            i=5*b
            if val(i)<.5:continue
            kind=index(val(i+1),0,3,"REQ shape")
            if kind==0:shape=0
            elif kind==2 and b in (0,5):shape=1 if b==0 else 2
            elif kind==2:raise ValueError("Unrecognized REQ middle-band type")
            else:shape=3 if b<=2 else 4
            band(s,b,shape,val(i+3),0 if shape in (1,2) else val(i+2),val(i+4),18,warnings)
        output=bounded(val(60),-24,24,"output")
    elif name=="API-550" and sub in ("A5AS","A5AM"):
        if len(tokens)<22 or val(0)>.5:raise ValueError("550A cut filters or unknown layout unsupported")
        for b,(gain,hz,shape) in enumerate(((val(1),val(2),0 if val(14)>.5 else 3),
                 (val(3),1000*val(4),0),(val(5),1000*val(6),0 if val(15)>.5 else 4))):
            band(s,b,shape,hz,gain,1.,12,warnings)
        output=0.
        warnings.append("550A proportional-Q, fixed analog coloration and non-band controls are not transferred.")
    else:raise ValueError("Unsupported Waves effect: "+name)
    return s,output,warnings

def convert(data,fmt,record=None):
    if len(data)>LIMIT:raise ValueError("Oversized source")
    if record is not None:
        if fmt!="waves":raise ValueError("FST wrapper extraction supported only for Waves")
        items=records(data)
        if not 0<=record<len(items) or items[record][0]!="Fruity Wrapper":raise ValueError("Select one named Fruity Wrapper record")
        data=items[record][1]
    return proq4(data) if fmt=="pro-q4" else waves_xml(data)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("source",type=Path);ap.add_argument("--format",choices=["pro-q4","waves"],required=True)
    ap.add_argument("--fst-record",type=int);ap.add_argument("--allow-approximate",action="store_true")
    ap.add_argument("--output",type=Path,required=True)
    args=ap.parse_args()
    if not args.allow_approximate or args.output.suffix.lower()!=".skellpreset":
        ap.error("Explicit --allow-approximate and NEW .skellpreset output required")
    if args.source.stat().st_size>LIMIT:ap.error("Oversized source")
    s,db,warnings=convert(args.source.read_bytes(),args.format,args.fst_record)
    doc=package(args.source.stem+" EQ",s,db);doc["author"]="EQ settings translation";doc["bank"]="Imported EQ"
    with zipfile.ZipFile(args.output,"x",compression=zipfile.ZIP_DEFLATED) as z:z.writestr("preset.json",json.dumps(doc,allow_nan=False))
    print(json.dumps({"warnings":warnings,"output":str(args.output)},indent=2))
if __name__=="__main__":
    try:main()
    except (OSError,ValueError,struct.error) as e:raise SystemExit(str(e))
