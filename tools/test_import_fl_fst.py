import unittest
import struct
from import_fl_fst import records,eq_settings,package

def event(e,b):
    n=len(b);out=bytearray([e])
    while n>=128:out.append((n&127)|128);n>>=7
    out.append(n);return bytes(out)+b

class TestImport(unittest.TestCase):
    def test_eq(self):
        b=bytearray(354)
        p=[0]*36
        p[0]=-600;p[7]=32768;p[14]=32768;p[21]=6
        struct.pack_into("<37i",b,0,8,*p)
        struct.pack_into("<I",b,0x13e,127)
        name="Fruity Parametric EQ 2"
        stream=event(201,(name+"\0").encode("utf-16le"))+event(213,b)
        fst=b"FLhd\x06\x00\x00\x00"+b"\0"*6+b"FLdt"+struct.pack("<I",len(stream))+stream
        self.assertEqual(records(fst),[(name,b)])
        v,main,warnings=eq_settings(b)
        self.assertAlmostEqual(v["params"][1],1/3)
        self.assertTrue(v["studioEqOn"][0]);self.assertFalse(v["studioEqOn"][1])
        self.assertEqual(main,0)
        doc=package("Test",v,main)
        self.assertEqual(doc["chain"][0]["type"],34)
        self.assertEqual(len(doc["chain"]),16)
        for n in range(len(fst)):
            with self.assertRaises(ValueError):records(fst[:n])
        struct.pack_into("<i",b,4+21*4,2)
        self.assertEqual(eq_settings(b)[0]["studioEqExtraShape"][0],2)
    def test_unbound(self):
        stream=event(213,b"\0"*32)
        fst=b"FLhd\x06\x00\x00\x00"+b"\0"*6+b"FLdt"+struct.pack("<I",len(stream))+stream
        self.assertEqual(records(fst)[0][0],"Unnamed single-effect payload")

if __name__=="__main__":unittest.main()
