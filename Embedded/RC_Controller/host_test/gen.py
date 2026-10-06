import sys
b=bytes.fromhex(open(sys.argv[1]).read().strip())
open("eeprom.bin","wb").write(b[2:])           # det som ligger i kortet (388 byte)
open("rcs.bin","wb").write(b[2:230])           # RControlStations Write (228 byte)
g=(b"$GPGSV,3,1,12,05,51,251,48,07,44,077,51,08,18,064,42,09,06,119,36,0*6A\r\n\x01"*20)[:1024]
open("rx_old.bin","wb").write(g)               # gammalt innehåll i mottagningsbufferten

import struct
e=bytearray(open("eeprom.bin","rb").read())
open("gunnar.bin","wb").write(bytes(e[:186]))   # äldre RControlStation: t.o.m. deadband
f=bytearray(e[:228])                              # fullt paket med giltiga sensorer/loopar
f+=struct.pack(">H",2)+b"".join(struct.pack(">HHHH",0,i,1,0) for i in range(4))
f+=struct.pack(">H",2)
for i in range(4):
    f+=struct.pack(">HHH",0,1,4 if i==0 else 1)+struct.pack(">ffffff",0.0,1.0,0.1,0.0,-1.0,1.0)+bytes([1])
open("full.bin","wb").write(bytes(f))
