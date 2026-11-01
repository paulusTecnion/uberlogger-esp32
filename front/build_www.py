#!/usr/bin/env python3
"""Stage the web UI for the logger's SPIFFS image and gzip the text assets.

Copies front/www to <dst>, leaving out dev-only files (dev server mock data in
ajax/, Python caches, PyInstaller spec), then gzips every .html/.css/.js file
(name.ext -> name.ext.gz). The firmware serves name.ext.gz for a request of
name.ext with "Content-Encoding: gzip" and the content type of name.ext.

Usage: build_www.py <front/www> <staging dir>
Stdlib only; output is reproducible (no timestamps in the gzip headers).
"""

import gzip
import os
import shutil
import sys

COMPRESS = (".html", ".css", ".js")
SKIP_DIRS = {"__pycache__", "ajax"}
SKIP_EXT = (".pyc", ".spec")


def main():
    src, dst = sys.argv[1], sys.argv[2]
    if os.path.isdir(dst):
        shutil.rmtree(dst)
    before = after = 0
    for root, dirs, files in os.walk(src):
        dirs[:] = sorted(d for d in dirs if d not in SKIP_DIRS)
        rel = os.path.relpath(root, src)
        out_dir = os.path.join(dst, rel) if rel != "." else dst
        os.makedirs(out_dir, exist_ok=True)
        for name in sorted(files):
            if name.endswith(SKIP_EXT):
                continue
            with open(os.path.join(root, name), "rb") as f:
                data = f.read()
            before += len(data)
            if name.endswith(COMPRESS):
                data = gzip.compress(data, compresslevel=9, mtime=0)
                name += ".gz"
            with open(os.path.join(out_dir, name), "wb") as f:
                f.write(data)
            after += len(data)
    print(f"web UI staged in {dst}: {before / 1024:.0f} KB -> {after / 1024:.0f} KB")


if __name__ == "__main__":
    main()
