#!/usr/bin/env python3
"""
UDP test helper for the Music:playnotes command.

Protocol recap
--------------
  Message : "Music:playnotes:<hex_payload>"
  Payload : Byte 0   = tempo in BPM  (0 → 120 BPM, max 240)
            Bytes 1+ = note pairs:
              note_byte     : bit7=1 → silence, bit7=0 → MIDI note (bits 6-0)
              duration_byte : number of 16th notes (0 treated as 1)

Note: 255 in the source arrays = silence (0xFF has bit7=1).
"""

import socket
import sys

# ── Device settings ────────────────────────────────────────────────────────────
ROBOT_IP   = "192.168.1.56"   # <-- replace with your K10 IP
UDP_PORT   = 24642

# ── Theme definitions ──────────────────────────────────────────────────────────
# Format: list of [midi_note, duration_in_16ths]
#   255 = silence (bit7 already set in 0xFF)



imperial_march_theme = [
  [67,8], [67,8], [67,16],
  [63,8], [70,8], [67,16],
  [63,8], [70,8], [67,32],
  [79,8], [79,8], [79,16],
  [75,8], [70,8], [63,16],
  [75,8], [70,8], [63,32],
  [67,8], [67,8], [67,16],
  [63,8], [70,8], [67,16],
  [63,8], [70,8], [67,32],
  [79,8], [79,8], [79,16],
  [75,8], [70,8], [63,16],
  [70,8], [63,8], [55,32],
  [82,16], [82,16], [82,16], [84,16],
  [82,16], [84,16], [82,16], [79,16],
  [75,16], [72,16], [70,16], [67,32],
  [82,16], [82,16], [82,16], [84,16],
  [82,16], [84,16], [82,16], [79,16],
  [75,16], [77,16], [75,16], [72,32],
  [67,8], [67,8], [67,16],
  [63,8], [70,8], [67,16],
  [63,8], [70,8], [67,32],
  [79,8], [79,8], [79,16],
  [75,8], [70,8], [63,16],
  [70,8], [63,8], [55,32]
]

nyan_cat_theme = [  [76,8], [74,8], [72,8], [74,8], [76,8], [76,8], [76,16],
  [74,8], [74,8], [74,16], [76,8], [80,8], [80,16],
  [76,8], [74,8], [72,8], [74,8], [76,8], [76,8], [76,16],
  [74,8], [74,8], [76,8], [74,8], [72,32],
  [80,8], [78,8], [76,8], [74,8], [76,8], [78,8], [80,8], [76,8],
  [78,8], [76,8], [74,8], [72,8], [74,8], [76,8], [74,8], [72,8],
  [69,8], [71,8], [72,8], [74,8], [72,8], [71,8], [69,8], [68,8],
  [69,8], [71,8], [72,8], [74,8], [72,8], [71,8], [69,8], [64,32],
  [76,8], [74,8], [72,8], [74,8], [76,8], [76,8], [76,16],
  [74,8], [74,8], [74,16], [76,8], [80,8], [80,16],
  [76,8], [74,8], [72,8], [74,8], [76,8], [76,8], [76,16],
  [74,8], [74,8], [76,8], [74,8], [72,32]
]


theme=imperial_march_theme

def build_playnotes_payload(notes: list, bpm: int = 120) -> str:
    """Encode a note list into the hex payload expected by Music:playnotes."""
    bpm = max(1, min(240, bpm))          # clamp to valid range
    payload = f"{bpm:02X}"
    for note, dur in notes:
        note_byte = note & 0xFF          # 255 → 0xFF (silence bit already set)
        dur_byte  = max(1, dur) & 0xFF
        payload  += f"{note_byte:02X}{dur_byte:02X}"
    return payload


def send_playnotes(notes: list, bpm: int, ip: str = ROBOT_IP, port: int = UDP_PORT) -> None:
    payload = build_playnotes_payload(notes, bpm)
    message = f"Music:playnotes:{payload}"
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(2.0)
        sock.sendto(message.encode(), (ip, port))
        print(f"→ Sent to {ip}:{port}")
        print(f"  BPM      : {bpm}")
        print(f"  Notes    : {len(notes)}")
        print(f"  Payload  : {payload}")
        print(f"  Msg len  : {len(message)} chars")
        try:
            data, addr = sock.recvfrom(256)
            print(f"← Reply    : {data.decode(errors='replace')} (from {addr})")
        except socket.timeout:
            print("← (no reply within 2 s — normal for long sequences)")


# ── Main ───────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    ip   = sys.argv[1] if len(sys.argv) > 1 else ROBOT_IP
    bpm  = int(sys.argv[2]) if len(sys.argv) > 2 else 240

    if ip == "192.168.1.XXX":
        print("ERROR: set ROBOT_IP in this script or pass the IP as first argument.")
        print("  Usage: python3 test_playnotes.py <ip> [bpm]")
        sys.exit(1)

    send_playnotes(theme, bpm=bpm, ip=ip)
