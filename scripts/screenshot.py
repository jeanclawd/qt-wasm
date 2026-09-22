#!/usr/bin/env python3
"""Screenshot the deployed demo with a JSPI-capable Chromium.

Run from CI (GitHub-hosted runners have a recent Chrome); this box's browser
is too old and too large to install. Usage:

    python scripts/screenshot.py https://jeanclawd.github.io/qt-wasm/ out.png
"""
import sys
from playwright.sync_api import sync_playwright

url = sys.argv[1]
out = sys.argv[2]

with sync_playwright() as p:
    browser = p.chromium.launch(args=["--enable-features=WebAssemblyExperimentalJSPI"])
    page = browser.new_page(viewport={"width": 1100, "height": 760})
    msgs = []
    page.on("console", lambda m: msgs.append(f"{m.type}: {m.text}"))
    page.goto(url, wait_until="networkidle", timeout=120_000)
    # Qt boots inside the iframe; give the wasm module time to instantiate.
    page.wait_for_timeout(25_000)
    page.screenshot(path=out)
    print("\n".join(msgs[-40:]))
    browser.close()
