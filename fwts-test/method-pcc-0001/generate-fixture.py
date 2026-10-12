#!/usr/bin/env python3
"""Regenerate the PCC fixture; requires iasl only when regenerating the dump."""

from pathlib import Path
import struct
import subprocess
import tempfile


base = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as tmp:
    output = Path(tmp) / "pcc"
    subprocess.run(["iasl", "-p", str(output), str(base / "pcc.asl")], check=True)
    aml = output.with_suffix(".aml").read_bytes()

# Hardware-reduced FADT: DSDT at 0x2000, no physical register access needed.
fadt = bytearray(276)
struct.pack_into("<4sIBB6s8sI4sI", fadt, 0, b"FACP", len(fadt), 6, 0,
                 b"FWTS  ", b"PCCTEST ", 1, b"INTL", 0)
struct.pack_into("<I", fadt, 40, 0x2000)
struct.pack_into("<I", fadt, 112, 1 << 20)
fadt[9] = -sum(fadt) & 0xff

with (base / "acpidump-0001.log").open("w") as dump:
    for name, address, data in [("FACP", 0x1000, fadt), ("DSDT", 0x2000, aml)]:
        dump.write(f"{name} @ 0x{address:08x}\n")
        for offset in range(0, len(data), 16):
            chunk = data[offset:offset + 16]
            dump.write(f"  {offset:04x}: " + " ".join(f"{b:02x}" for b in chunk) + "\n")
        dump.write("\n")
