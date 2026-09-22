#!/usr/bin/env python3
"""Turn the built emscripten-wasm32 conda package into a static web bundle.

The recipe installs the Qt wasm artifacts at

    share/qt-wasm-demo/{qt-wasm-demo.wasm,qt-wasm-demo.js,qt-wasm-demo.html,qtloader.js}

which is the layout emscripten-forge's /qtapp/ runner expects. For GitHub
Pages we do not need the runner: we just extract those four files plus our own
index.html into dist/ and serve them as plain static assets.
"""

from __future__ import annotations

import re
import shutil
import sys
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "output" / "emscripten-wasm32"
DIST = ROOT / "dist"
APP = "qt-wasm-demo"
MEMBER_PREFIX = f"share/{APP}/"


def find_package() -> Path:
    candidates = sorted(OUTPUT.glob(f"{APP}-*.tar.bz2"))
    if not candidates:
        sys.exit(f"no {APP}-*.tar.bz2 found under {OUTPUT}")
    return candidates[-1]


def main() -> None:
    pkg = find_package()
    print(f"bundling {pkg.name} ({pkg.stat().st_size / 1e6:.1f} MB compressed)")

    if DIST.exists():
        shutil.rmtree(DIST)
    DIST.mkdir(parents=True)

    extracted = []
    with tarfile.open(pkg, "r:bz2") as tf:
        for member in tf.getmembers():
            if not member.isfile() or not member.name.startswith(MEMBER_PREFIX):
                continue
            name = member.name[len(MEMBER_PREFIX) :]
            src = tf.extractfile(member)
            assert src is not None
            (DIST / name).write_bytes(src.read())
            extracted.append(name)

    if not extracted:
        sys.exit(f"package contained nothing under {MEMBER_PREFIX}")

    # Qt's own generated page is what actually boots the app (it knows the
    # -sEXPORT_NAME the link step used). Keep it as app.html; our index.html
    # is a landing shell that frames it.
    qt_html = DIST / f"{APP}.html"
    if qt_html.exists():
        # Qt's shell references qtlogo.svg, which the recipe does not install
        # (it lives in Qt's wasm_shell sources, not in the link output). Drop
        # the <img> rather than serve a 404 inside the loading spinner.
        text = qt_html.read_text()
        text = re.sub(r"\s*<img src=\"qtlogo\.svg\".*?</img>", "", text, flags=re.S)
        (DIST / "app.html").write_text(text)
        qt_html.unlink()

    for item in (ROOT / "web").iterdir():
        shutil.copy2(item, DIST / item.name)

    # Pages would otherwise run the bundle through Jekyll.
    (DIST / ".nojekyll").touch()

    print("dist/ contents:")
    total = 0
    for f in sorted(DIST.iterdir()):
        size = f.stat().st_size
        total += size
        print(f"  {f.name:<28} {size / 1024:>10.1f} KiB")
    print(f"  {'TOTAL':<28} {total / 1024 / 1024:>10.2f} MiB")


if __name__ == "__main__":
    main()
