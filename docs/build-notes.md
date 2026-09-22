# Build notes

## Run 1 — green

`rattler-build` resolved and built the recipe on `ubuntu-latest` in about
90 seconds end to end (including `setup-pixi` and the Pages deploy). No
patches, no retries.

Resolution worth recording:

| | |
| --- | --- |
| cross Qt | `qt6-main 6.11.2 jspi_hf2ac257_3` from `https://repo.prefix.dev/emscripten-forge-4x` (16.8 MB compressed) |
| native host Qt | `qt6-main 6.11.2` from conda-forge — conda-forge happens to carry the exact same version, which matters because Qt refuses a `QT_HOST_PATH` whose version does not match the cross build |
| compiler | `emscripten_emscripten-wasm32 4.0.9`, exceptions via `-fwasm-exceptions` |
| output | `qt-wasm-demo-0.1.0-*.tar.bz2` containing `share/qt-wasm-demo/{qt-wasm-demo.wasm,qt-wasm-demo.js,qt-wasm-demo.html,qtloader.js}` |

Bundle:

```
app.html                            2.9 KiB
index.html                          2.5 KiB
qt-wasm-demo.js                   217.6 KiB
qt-wasm-demo.wasm               13759.8 KiB
qtloader.js                        11.9 KiB
TOTAL                             13.67 MiB
```

13.4 MiB of `.wasm` for a five-widget app is the headline number: Qt is
statically linked and there is no dead-code-stripping miracle. It gzips well
(GitHub Pages serves it compressed), but it is not a "small page".

## Gotchas hit while writing this

* **`QtPublicWasmToolchainHelpers.cmake` must be `include()`d by hand.**
  Without it CMake happily produces `qt-wasm-demo.wasm` and `.js` but never
  runs Qt's wasm target finalization, so there is no `qtloader.js` and no
  generated HTML — and `package_contents` tests fail on files that "should"
  be there. `qt-cmake` normally injects this; emscripten-forge calls `cmake`
  with `Emscripten.cmake` directly.
* **`$EMSDK/.emscripten` does not exist.** That helper reads it to locate
  LLVM and binaryen. emscripten-forge ships emscripten as a conda package
  with no emsdk layout, so `recipe/build.sh` writes a synthetic one pointing
  at `$EMSCRIPTEN_FORGE_EMSDK_DIR`. Straight from emscripten-forge's own
  qt6 recipe.
* **JSPI is a build-string variant, not a package.** `qt6-main-jspi` still
  exists on the channel but is the deprecated shape; the current spec is
  `qt6-main[build=jspi*]`, and the variant is deliberately down-prioritized
  so a bare `qt6-main` resolves to `nojspi`. A `nojspi` build links fine and
  then hangs the browser tab the moment `QApplication::exec()` is reached.
* **`--package-format tar-bz2` is not cosmetic.** emscripten-forge's
  `/qtapp/` runner unpacks packages in the browser and only understands
  `.tar.bz2`; `.conda` (zstd) is unsupported.
* **Qt's generated shell references `qtlogo.svg`**, which the link step does
  not emit and the recipe does not install — a 404 inside the loading
  spinner. `scripts/bundle.py` strips the `<img>`.
* **No cross-origin isolation needed.** This surprised me: the standard
  Qt-for-WebAssembly advice is "you need COOP/COEP for `SharedArrayBuffer`".
  With JSPI there are no threads, so the bundle is ordinary static files and
  GitHub Pages — which cannot set response headers — is sufficient. No
  `coi-serviceworker.js` shim is shipped.

## Verification

`screenshot.yml` (manual `workflow_dispatch`) drives Playwright's Chromium
153 against the live Pages URL, waits 25 s for the wasm module to
instantiate, and uploads `screenshot.png`. That run is green and the image is
in `docs/`. Chromium 153 ships JSPI on by default; no flag was needed, and
no COOP/COEP headers were set anywhere.

Boot on a warm cache is a couple of seconds; the first load has to pull
13.4 MiB of `.wasm` over the wire.
