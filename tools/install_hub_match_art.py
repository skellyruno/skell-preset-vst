"""Install the original SkellyMotion Hub assets without touching other artwork."""
import argparse
import hashlib
import pathlib
import zipfile

EXPECTED={"skelly-radar.png":"993ee9c5254542a397b0e4cf3207df7444b281c38b9127d42df7b19ec8c724d8",
          "skelly-skull.png":"9faacd04ea4daf5086e1779d945d3285c0ab4da29b3da0e6bb9c87a44798cf8a"}

def install(source, root):
    root=pathlib.Path(root).resolve()
    if not (root/"Source/ui/SkellMatchPanel.h").is_file():raise ValueError("Choose the Skell Preset checkout")
    data={}
    with zipfile.ZipFile(source) as z:
        for name,digest in EXPECTED.items():
            info=z.getinfo("assets/"+name)
            if info.file_size>1024*1024:raise ValueError("Oversized artwork")
            raw=z.read(info)
            if hashlib.sha256(raw).hexdigest()!=digest:raise ValueError("Original artwork hash mismatch: "+name)
            data[name]=raw
    target=root/"assets/hub";target.mkdir(parents=True,exist_ok=True)
    for name,raw in data.items():
        out=target/name
        if out.exists() and out.read_bytes()!=raw:raise ValueError("Refusing to overwrite different artwork: "+name)
    for name,raw in data.items():
        (target/name).write_bytes(raw)
    print("Verified both exact original Hub PNGs and installed build inputs.")
if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("archive");p.add_argument("checkout");a=p.parse_args();install(a.archive,a.checkout)
