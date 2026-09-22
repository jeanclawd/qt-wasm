# qt-wasm

Testing whether a real Qt desktop app can run in the browser, compiled to
WebAssembly with [emscripten-forge](https://emscripten-forge.org/).

Live demo: **https://jeanclawd.github.io/qt-wasm/**
(needs a JSPI-capable browser — Chrome 137+, Firefox 141+, Safari 18.4+)

![The demo running in Chromium 153: a Qt Widgets window with a filter field, a
Resample button and a sortable table of fake measurements](docs/screenshot.png)

That is a real `QTableView` over a `QSortFilterProxyModel`, laid out by Qt,
rendered to a canvas, in a browser tab. The status line is printed by the app
itself: `Qt 6.11.2 · wasm32 (emscripten-forge)`.

---

## What the sources say

A faithful digest of
[notebook.link/blog/qt-in-the-browser](https://notebook.link/blog/qt-in-the-browser/)
and [emscripten-forge.org/qtapp](https://emscripten-forge.org/qtapp/):

1. **emscripten-forge is a general-purpose software distribution for
   WebAssembly** — conda packages built for the `emscripten-wasm32` platform,
   distributed from `https://repo.prefix.dev/emscripten-forge-4x`.
2. Unlike npm, packages install into a **POSIX-shaped prefix** (`bin/`, `lib/`,
   `include/`, `share/`), so native build systems (CMake, pkg-config) find
   their dependencies exactly as they would on Linux.
3. The channel enforces a **consistent ABI**: one pinned emscripten version
   (4.0.9 today) and one exception model (`-fwasm-exceptions`) across every
   package, plus CI that cascades rebuilds through dependents.
4. Qt6 **6.11.2** is on the channel as `qt6-main`, `qt6-5compat`,
   `qt6-svg`, `qt6-imageformats`, `qt6-webchannel`, `qt6-websockets`.
   `qt6-main` alone is ~17 MB compressed and carries Core, Gui, Widgets,
   Network and the `qwasm` platform plugin.
5. Each `qt6-*` package ships in **two build-string variants**: `nojspi_*`
   (the default) and `jspi_*`. Widget apps need the JSPI one — select it with
   a match spec, `qt6-main[build=jspi*]`. (It used to be a separate
   `qt6-main-jspi` package family; that form is deprecated.)
6. **JSPI** (JavaScript Promise Integration) is the enabling trick. Qt is
   rebuilt with `QT_FEATURE_wasm_jspi=ON` so `QApplication::exec()` can block
   in C++ while the underlying wasm stack suspends and yields to the browser's
   event loop. No Asyncify rewrite, no `-sPTHREAD_POOL_SIZE`, no threads.
7. Consequence: **no `SharedArrayBuffer`, so no COOP/COEP headers are
   required.** The bundle is plain static files and drops straight onto GitHub
   Pages. (This is the single biggest practical difference from the classic
   "Qt for WebAssembly needs cross-origin isolation" advice.)
8. The price is browser support: JSPI landed in Chrome 137, Firefox 141,
   Safari 18.4. Older browsers show a blank canvas.
9. A built app is just a conda package. The expected layout is
   `share/<appname>/<appname>.{wasm,js,html}` plus `qtloader.js`, with the
   final link done as `-sMODULARIZE=1 -sJSPI=1`.
10. Because it is a package, emscripten-forge can run it with **zero server
    work**: `emscripten-forge.org/qtapp/` is an in-browser runner that takes a
    `.tar.bz2` URL, unpacks it client-side with
    `@emscripten-forge/untarjs`, wraps the members as blob URLs and boots
    `qtloader.js`. Only `.tar.bz2` works — `.conda` is not supported yet.
11. Known limitations the sources call out: **not all Qt subpackages are
    available**; running full Qt apps "may require custom patches";
    timer-driven Qt animations under JSPI **can starve the browser main
    thread**; dynamic side-module dependencies are unsupported; continuous
    60 fps rendering is not there yet.
12. Reference apps already on the channel (in the `-experimental` channel):
    `qt-calculator-experimental` (Qt's own calculator example) and
    `sqlitebrowser-experimental` — a 50k-line real-world Qt Widgets app.
13. Font and file access are the usual Qt-for-wasm story: Qt bundles its own
    font (`Qt` embeds DejaVu) into the wasm, and the filesystem is
    emscripten's virtual MEMFS — there is no host filesystem.
14. Build machinery is `rattler-build` driven by `pixi`, with a `variant.yaml`
    supplying `cxx_compiler = emscripten` / `cxx_compiler_version = 4.0.9`
    when `--target-platform emscripten-wasm32` is passed.

## The plan

Compile a small Qt Widgets app of our own, using the same recipe shape as
`qt-calculator-experimental`, build it on GitHub Actions (this box has 3.8 GB
of RAM and no business linking Qt), unpack the resulting conda package into a
static `dist/` and publish it with GitHub Pages.

## The app

`app/` is a Qt Widgets app with a data-science shape, since that is the point
of interest: a `QTableView` over a `QStandardItemModel` of 60 fake
measurements, a `QLineEdit` wired to a `QSortFilterProxyModel`, a "Resample"
`QPushButton`, and a status line reporting the Qt version and whether it is
running on wasm. Enough to exercise layout, fonts, model/view, sorting, text
input and signals in one screen.

```
app/CMakeLists.txt   # qt_add_executable + the wasm helper include & JSPI flags
app/main.cpp         # ~130 lines, Qt6::Widgets only
recipe/recipe.yaml   # conda recipe, host: qt6-main[build=jspi*]
recipe/build.sh      # QT_HOST_PATH + synthesised $EMSDK/.emscripten, cmake, ninja
variant.yaml         # trimmed copy of emscripten-forge's
pixi.toml            # build / bundle / serve tasks
scripts/bundle.py    # .tar.bz2 -> dist/
web/index.html       # landing shell + JSPI feature check
```

Two details are load-bearing and both are copied from upstream:

* Qt's wasm target finalization lives in `QtPublicWasmToolchainHelpers.cmake`,
  which `qt-cmake` normally injects. emscripten-forge calls `cmake` with
  `Emscripten.cmake` directly, so `app/CMakeLists.txt` `include()`s the helper
  by hand — without it you get a `.wasm` but no `.html` / `qtloader.js`.
* That helper reads `$EMSDK/.emscripten` to find LLVM and binaryen.
  emscripten-forge ships emscripten as a conda package and never writes that
  file, so `recipe/build.sh` synthesises it.

## Build and run locally

Needs [pixi](https://pixi.sh). **This is a heavy build** — it pulls ~17 MB of
compressed Qt static archives and links them; budget a few GB of RAM and
several minutes.

```bash
pixi run build     # rattler-build --target-platform emscripten-wasm32
pixi run bundle    # unpack output/emscripten-wasm32/*.tar.bz2 into dist/
pixi run serve     # http://localhost:8000  (no special headers needed)
```

`pixi run build` expands to the same invocation emscripten-forge uses:

```bash
rattler-build build \
  --package-format tar-bz2 \
  -c https://repo.prefix.dev/emscripten-forge-4x \
  -c microsoft \
  -c conda-forge \
  --target-platform emscripten-wasm32 \
  --skip-existing none \
  -m variant.yaml \
  --recipe recipe/recipe.yaml \
  --output-dir output
```

You can also skip the static hosting entirely and drop the resulting
`output/emscripten-wasm32/qt-wasm-demo-*.tar.bz2` onto
<https://emscripten-forge.org/qtapp/>, which boots it client-side.

## Deployment

`.github/workflows/build.yml` runs on every push to `main` and on
`workflow_dispatch`: `prefix-dev/setup-pixi` → `pixi run build` →
`scripts/bundle.py` → `actions/upload-pages-artifact` →
`actions/deploy-pages`. The conda package itself is also kept as a normal
workflow artifact.

`dist/` is served as-is. **No `coi-serviceworker.js` shim is included** and
none is needed: the JSPI build uses no threads and no `SharedArrayBuffer`, so
cross-origin isolation is irrelevant here. If you ever switch to a threaded
Qt build you would need COOP/COEP and GitHub Pages cannot set headers — that
is when the service-worker shim becomes necessary.

## Status

<!-- STATUS -->
| Thing | State |
| --- | --- |
| Recipe builds on GitHub Actions | **works** — green on the first run, ~90 s end to end |
| Qt version / modules pulled in | Qt **6.11.2**, `qt6-main[build=jspi*]`, `Qt6::Widgets` only |
| Pages deploy | **works** — <https://jeanclawd.github.io/qt-wasm/> returns 200 |
| Bundle size | **13.67 MiB** total; `qt-wasm-demo.wasm` 13.4 MiB, `.js` 218 KiB, `qtloader.js` 12 KiB |
| COOP/COEP headers | **not needed** — JSPI build, no threads, no `SharedArrayBuffer`. No service-worker shim shipped. |
| Demo renders in a JSPI browser | **works** — screenshotted above in Chromium 153 (`.github/workflows/screenshot.yml`); filter field, sortable header, alternating rows and Qt's own font all correct |
| Demo in a non-JSPI browser | **does not work**, by design; `index.html` feature-detects `WebAssembly.Suspending` and warns |
| Running it from emscripten-forge's `/qtapp/` runner | untested here; the package is the right shape (`share/<app>/...`, `.tar.bz2`) and is published as a workflow artifact |

Not tested, and deliberately so: touch input, `QFileDialog` against the host
filesystem, clipboard, anything with a `QTimer`-driven animation (the sources
warn this can starve the browser main thread under JSPI), and any Qt module
beyond `Qt6::Widgets`.

Full notes, including every gotcha hit on the way:
[`docs/build-notes.md`](docs/build-notes.md).

## License

MIT for the app and glue. Qt6 is LGPL-3.0; the wasm binary statically links
Qt, so the usual LGPL obligations apply to the artifact.
