"""
Pre-build script: gzip-compress web assets in data/ before uploadfs.

The ESP32 WebServer will transparently serve file.js.gz when a browser
requests file.js, so no HTML/JS changes are needed.

Files already smaller than their compressed version are left as-is.
"""

import gzip
import os
import shutil

Import("env")  # noqa: F821  (PlatformIO SCons environment)

DATA_DIR = os.path.join(env["PROJECT_DIR"], "data")  # noqa: F821
EXTENSIONS = (".html", ".js", ".css", ".json")


def compress_assets(source, target, env):  # noqa: F821
    if not os.path.isdir(DATA_DIR):
        print(f"compress_data: data dir not found ({DATA_DIR}), skipping.")
        return

    compressed = 0
    skipped = 0

    for root, _dirs, files in os.walk(DATA_DIR):
        for name in files:
            # Skip files that are already gzip archives
            if name.endswith(".gz"):
                continue
            if not name.endswith(EXTENSIONS):
                continue

            src_path = os.path.join(root, name)
            gz_path  = src_path + ".gz"

            raw = open(src_path, "rb").read()
            gz_data = gzip.compress(raw, compresslevel=9)

            if len(gz_data) >= len(raw):
                skipped += 1
                # Remove stale .gz if the original is now smaller
                if os.path.exists(gz_path):
                    os.remove(gz_path)
                continue

            with open(gz_path, "wb") as f:
                f.write(gz_data)

            ratio = 100 * len(gz_data) // len(raw)
            rel   = os.path.relpath(src_path, DATA_DIR)
            print(f"  [gz] {rel:40s}  {len(raw):6d} → {len(gz_data):5d} bytes ({ratio}%)")
            compressed += 1

    print(f"compress_data: {compressed} file(s) compressed, {skipped} skipped (already optimal).")


# Hook runs before the filesystem image is built
env.AddPreAction("buildfs", compress_assets)  # noqa: F821
env.AddPreAction("uploadfs", compress_assets)  # noqa: F821
