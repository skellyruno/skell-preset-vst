import math
import struct
import unittest
from import_vendor_eq import convert,proq4,waves_xml

def proq(values):
    return b"FFBS"+struct.pack("<II600f",1,600,*values)
def waves(name,sub,a,b,active="SETUP_A"):
    return f'''<PresetChunkXMLTree version="3"><Preset><PresetHeader><PluginName>{name}</PluginName><PluginSubComp>{sub}</PluginSubComp><ActiveSetup>{active}</ActiveSetup></PresetHeader><PresetData Setup="SETUP_A"><Parameters Type="RealWorld">{' '.join(map(str,a))}</Parameters></PresetData><PresetData Setup="SETUP_B"><Parameters Type="RealWorld">{' '.join(map(str,b))}</Parameters></PresetData></Preset></PresetChunkXMLTree>'''.encode()
class VendorEqTests(unittest.TestCase):
    def test_proq(self):
        p=[0.]*600;p[555]=1;p[0:8]=[1,1,math.log2(2000),-4,.5,0,2,2]
        p[9:15]=[-6,1,0,.5,50,50]
        s,db,w=proq4(proq(p))
        self.assertAlmostEqual(s["params"][1],.5-4/36)
        self.assertAlmostEqual(s["studioEqDynamic"][0],.5)
        self.assertAlmostEqual(s["studioEqThreshold"][0],-30)
        self.assertAlmostEqual(s["studioEqAttack"][0],9.22)
        self.assertEqual(s["studioEqRelease"][0],268)
        self.assertEqual(db,0)
        p[7]=3
        with self.assertRaises(ValueError):proq4(proq(p))
        p[7]=2;p[9]=6
        with self.assertRaises(ValueError):proq4(proq(p))
    def test_waves_active(self):
        a=[0]*62;b=a.copy()
        a[:5]=[1,0,-4,400,1.5];b[:5]=[1,0,2,1000,2]
        s,db,w=waves_xml(waves("REQ","REQ6",a,b,"SETUP_B"))
        self.assertAlmostEqual(s["params"][1],.5+2/36)
        self.assertAlmostEqual(s["params"][0],math.log(50)/math.log(1000))
        self.assertTrue(s["studioEqOn"][0]);self.assertFalse(s["studioEqOn"][1])
        with self.assertRaises(ValueError):waves_xml(waves("RCompressor","RC",a,b))
    def test_api550(self):
        a=[0,0,50,0,.4,2,5,0,0,1,0,"*","*",0,0,0,0,30,75,1.5,2.5,0,"*","*"]
        s,db,w=waves_xml(waves("API-550","A5AS",a,a))
        self.assertAlmostEqual(s["params"][5],.5+2/36)
        self.assertEqual(s["studioEqKind"][2],4)
    def test_bad(self):
        with self.assertRaises(ValueError):proq4(b"FFBS")
        with self.assertRaises(ValueError):waves_xml(b"junk")
        p=[0.]*600;p[555]=1;p[0]=float("nan")
        with self.assertRaises(ValueError):proq4(proq(p))
if __name__=="__main__":unittest.main()
