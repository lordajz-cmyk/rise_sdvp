#!/usr/bin/env python3
"""kort_tradar.py — läser styrkortets läge via ST-Link (openocd) utan att starta om det.

Visar om processorn lever (systemklockan ändras), avbrottsmasker, felregister,
USB-kontrollerns status och ALLA ChibiOS-trådar: namn, tillstånd och vad de väntar
på (väntobjektet slås upp mot firmwarens symboler, t.ex. send_mutex eller SDU1).
Kortet stannas bara ett ögonblick (halt/resume) per avläsning.

  sudo python3 kort_tradar.py [ELF]
Körs av kort_hangt.sh. Struct-offseten gäller ChibiOS 3.0.5 i den här firmwaren.
"""
import os
import socket
import subprocess
import sys
import time

ELF = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser(
    "~/rise_sdvp/Embedded/RC_Controller/build/fw_mactrac.elf")

# ChibiOS 3.0.5 (beräknat med offsetof mot firmwarens chconf.h)
O_RLIST_NEWER, O_RLIST_CURRENT, O_VT_SYSTIME = 16, 24, 40
O_T_PRIO, O_T_CTX_SP, O_T_NEWER, O_T_NAME, O_T_STATE, O_T_WTOBJ = 8, 12, 16, 24, 28, 36
TILLSTAND = ["READY", "CURRENT", "WTSTART", "SUSPENDED", "QUEUED", "WTSEM", "WTMTX",
             "WTCOND", "SLEEPING", "WTEXIT", "WTOREVT", "WTANDEVT", "SNDMSGQ", "SNDMSG",
             "WTMSG", "FINAL"]


def symboler():
    try:
        ut = subprocess.run(["arm-none-eabi-nm", "-n", "-S", ELF], capture_output=True, text=True).stdout
    except FileNotFoundError:
        return []
    s = []
    for rad in ut.splitlines():
        d = rad.split()
        if len(d) >= 4:
            s.append((int(d[0], 16), int(d[1], 16), d[3]))
        elif len(d) == 3:
            s.append((int(d[0], 16), 0, d[2]))
    return s


SYM = symboler()


def namn(adr):
    best = None
    for a, n, s in SYM:
        if a <= adr and (best is None or a > best[0]):
            best = (a, n, s)
    if best and (best[1] == 0 or adr < best[0] + max(best[1], 1) + 64):
        return "%s+%d" % (best[2], adr - best[0])
    return "0x%08x" % adr


def sym(s):
    for a, n, x in SYM:
        if x == s:
            return a
    return None


class OCD:
    def __init__(self):
        self.p = subprocess.Popen(["openocd", "-f", "board/stm32f4discovery.cfg", "-c",
                                   "reset_config trst_only combined", "-c", "tcl_port 6666"],
                                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(50):
            try:
                self.s = socket.create_connection(("127.0.0.1", 6666), timeout=5)
                break
            except OSError:
                time.sleep(0.1)
        self.cmd("init")

    def cmd(self, c):
        self.s.sendall(c.encode() + b"\x1a")
        buf = b""
        while not buf.endswith(b"\x1a"):
            buf += self.s.recv(4096)
        return buf[:-1].decode(errors="replace").strip()

    def w(self, adr):
        r = self.cmd("mdw 0x%x" % adr)
        return int(r.split(":")[1].split()[0], 16)

    def s_(self, adr, n=24):
        ut = b""
        for i in range(n):
            b = self.w(adr + i - (adr + i) % 4)
            ch = (b >> (8 * ((adr + i) % 4))) & 0xFF
            if ch == 0:
                break
            ut += bytes([ch])
        return ut.decode(errors="replace")

    def reg(self, r):
        v = self.cmd("reg %s" % r)
        return v.split(":")[-1].strip()

    def close(self):
        try:
            self.cmd("shutdown")
        except Exception:
            pass
        self.p.wait(timeout=5)


def main():
    o = OCD()
    ch = sym("ch")
    try:
        for varv in (1, 2):
            o.cmd("halt 2000")
            print("=== Avläsning %d ===" % varv)
            for r in ("pc", "lr", "sp", "primask", "basepri"):
                v = o.reg(r)
                print("  %-8s %s %s" % (r, v, namn(int(v, 16)) if r in ("pc", "lr") else ""))
            print("  CFSR 0x%08x  HFSR 0x%08x" % (o.w(0xE000ED28), o.w(0xE000ED2C)))
            print("  USB OTG_FS GINTSTS 0x%08x  DSTS 0x%08x (bit0 = suspend)  DCTL 0x%08x"
                  % (o.w(0x50000014), o.w(0x50000808), o.w(0x50000804)))
            if ch:
                print("  systemklocka (ch.vtlist.vt_systime): %d" % o.w(ch + O_VT_SYSTIME))
            if varv == 2 and ch:
                aktuell = o.w(ch + O_RLIST_CURRENT)
                t = o.w(ch + O_RLIST_NEWER)
                print("  Trådar:")
                for _ in range(40):
                    if t == ch or t == 0:
                        break
                    npek = o.w(t + O_T_NAME)
                    tnamn = o.s_(npek) if npek else "?"
                    st_ord = o.w(t + O_T_STATE)
                    st = st_ord & 0xFF
                    prio = o.w(t + O_T_PRIO)
                    wt = o.w(t + O_T_WTOBJ)
                    sp = o.w(t + O_T_CTX_SP)
                    # sparad LR i port_intctx (r4-r11 + lr sist, FPU s16-s31 före när påslaget)
                    lr_sparad = o.w(sp + 16 * 4 + 8 * 4) if sp else 0
                    print("   %-22s %-9s prio %3d  väntar på %-28s  %s" % (
                        tnamn, TILLSTAND[st] if st < len(TILLSTAND) else st, prio,
                        namn(wt) if st not in (0, 1) else "-",
                        "(körs nu)" if t == aktuell else "lr %s" % namn(lr_sparad)))
                    t = o.w(t + O_T_NEWER)
            o.cmd("resume")
            if varv == 1:
                time.sleep(1)
    finally:
        try:
            o.cmd("resume")
        except Exception:
            pass
        o.close()


if __name__ == "__main__":
    main()
