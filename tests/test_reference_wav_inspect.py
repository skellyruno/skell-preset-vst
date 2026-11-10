import importlib.util
from pathlib import Path
import json
import math
import random
import struct
import tempfile
import unittest
import wave

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('reference_wav_inspect',ROOT/'tools/reference_wav_inspect.py')
mod=importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

def write_pcm16(path,samples,sr=48000):
    with wave.open(str(path),'wb') as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(sr)
        f.writeframes(b''.join(struct.pack('<h',int(min(.999,max(-.999,s))*32767)) for s in samples))

def write_float32(path,samples,sr=48000):
    data=b''.join(struct.pack('<f',x) for x in samples)
    fmt=struct.pack('<HHIIHH',3,1,sr,sr*4,4,32)
    with open(path,'wb') as f:
        f.write(b'RIFF'+struct.pack('<I',4+8+len(fmt)+8+len(data))+b'WAVE')
        f.write(b'fmt '+struct.pack('<I',len(fmt))+fmt)
        f.write(b'data'+struct.pack('<I',len(data))+data)

class WavInspectorTests(unittest.TestCase):
    def test_pcm_float_and_delay(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            n=20000
            rng=random.Random(20261110)
            dry=[.11*(rng.random()*2-1)+.14*math.sin(i*.129) for i in range(n)]
            wet=[0.0]*137+[-.5*x for x in dry[:-137]]
            write_pcm16(root/'dry.wav',dry)
            write_float32(root/'wet.wav',wet)
            result=mod.diagnose(root/'dry.wav',root/'wet.wav')
            self.assertEqual(result['dry']['encoding'],'pcm16')
            self.assertEqual(result['wet']['encoding'],'float32')
            self.assertEqual(result['alignment']['lag_samples'],137)
            self.assertGreater(result['alignment']['absolute_correlation'],.99)
            self.assertFalse(result['preset_created'])
            json.dumps(result)

    def test_corruption(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            bad=root/'bad.wav'
            bad.write_bytes(b'garbage')
            with self.assertRaises(mod.WavFormatError):
                mod.inspect_wav(bad)
            wav=root/'nonfinite.wav'
            write_float32(wav,[.2]*2000+[float('nan')])
            with self.assertRaises(mod.WavFormatError):
                mod.inspect_wav(wav)

    def test_stereo24_extensible(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'stereo24.wav'
            channels=2
            rate=48000
            frames=3200
            fmt=struct.pack('<HHIIHHHHI',0xfffe,2,rate,rate*6,6,24,22,24,3)
            fmt+=struct.pack('<IHH8s',1,0,0x0010,b'\x80\x00\x00\xaa\x00\x38\x9b\x71')
            audio=b''.join(int(value*8388607).to_bytes(3,'little',signed=True)
                           for i in range(frames) for value in (.1*math.sin(i*.11),-.1*math.sin(i*.11)))
            with path.open('wb') as f:
                f.write(b'RIFF'+struct.pack('<I',4+8+len(fmt)+8+len(audio))+b'WAVE')
                f.write(b'fmt '+struct.pack('<I',len(fmt))+fmt)
                f.write(b'data'+struct.pack('<I',len(audio))+audio)
            info=mod.inspect_wav(path)
            self.assertEqual(info['channels'],channels)
            self.assertEqual(info['encoding'],'pcm24')
            self.assertEqual(info['frames'],frames)
            self.assertLess(abs(info['rms_dbfs']+23.01),.25)

    def test_sample_rate_mismatch(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            write_pcm16(root/'dry.wav',[.1]*5000,44100)
            write_pcm16(root/'wet.wav',[.1]*5000,48000)
            with self.assertRaises(mod.WavFormatError):
                mod.diagnose(root/'dry.wav',root/'wet.wav')

if __name__=='__main__':
    unittest.main()
