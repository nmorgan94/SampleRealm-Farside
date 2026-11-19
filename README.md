# SampleRealm: Reverb

A send-style reverb with a filtered send path and a live spectrum display

## Features

### Signal Chain

The dry signal is never filtered. Only the reverb send is:

```
dry ───────────────────────────────────────────┐
                                               ├─→ out
in ──→ highpass ──→ send gain ──→ reverb ──────┘
```

The reverb itself runs 100% wet, and the dry/wet blend is handled separately, so **Mix** stays predictable no matter how hard the send is driven.

### Highpass

- 24 dB/oct, two cascaded 12 dB/oct IIR stages
- 20 Hz to 20 kHz, default 1 kHz, skewed for finer control at the low end
- Cuts only what reaches the reverb, so the dry low end stays intact - the usual fix for a reverb that turns muddy on bass-heavy material
- Bypassed automatically at 20 Hz, so parking the knob fully left costs nothing

### Send Gain

- 0 to 2× (up to +6 dB), default 1.0
- Sets how hard the signal hits the reverb, independent of the wet/dry blend
- Applied after the highpass, so the filter shapes what gets driven

### Reverb

- Room Size, 0 to 1, default 0.80
- Damping, 0 to 1, default 0.80 - high-frequency absorption as the tail decays
- Width, 0 to 1, default 0.50 - stereo spread of the wet signal

### Mix

- Dry/wet blend, 0 to 1, default 0.50
- Linear crossfade: the dry signal falls as the wet rises

### Spectrum Analyzer

- 2048-point FFT, 512 display points, redrawn at 30 Hz
- Logarithmic frequency mapping across 20 Hz to 20 kHz
- Normalized per frame against its own peak, so it shows balance across the spectrum rather than absolute level

#### Spectrum Bypass

**ANALYSER**, in the top right, toggles a `spectrumBypass` parameter that clears the display and stops the audio thread writing to the FIFO, for hosts where the analyzer isn't worth the CPU. Hosts can't automate it.


## System Requirements

**macOS**

- macOS 12.0 (Monterey) or later
- Universal 2 - runs natively on Intel and Apple Silicon
- VST3, AU, or Standalone

**Windows**

- Windows 10 or later, 64-bit
- VST3 or Standalone

On both, mono or stereo, and the input layout must match the output.

## Build Requirements

- CMake 3.25+
- A C++23 capable compiler
  - **macOS**: Xcode or its Command Line Tools (the `xcode` preset needs the full Xcode app)
  - **Windows**: Visual Studio 2022 or later with the "Desktop development with C++" workload
- Git

JUCE is fetched automatically at configure time via CPM into `.cpm-cache/`, shared by every preset and by `wasm/` -
there is no submodule to initialize. A build folder made before the cache needs its `_deps/juce-*` removed first.

The build helpers, core headers, parameter layout, `LabeledSlider`, WebView editor, demo frame, worklet,
Vite config and test helpers come from [SampleRealm-Modules](https://github.com/nmorgan94/SampleRealm-Modules).
One line in `webui/package.json` says which copy the plugin, wasm and web builds all use:

```json
"@samplerealm/modules": "github:nmorgan94/SampleRealm-Modules#<full commit id>"
"@samplerealm/modules": "file:../../SampleRealm-Modules"
```

The first is what's committed, never a branch; the second uses a local folder. Run `npm install` in `webui/` after changing it.
The web demo also needs Emscripten (`brew install emscripten`) and Node 24+.

## Commands

Every command is the same on both platforms, except the rows marked **macOS** or **Windows**. Each
workflow configures, builds and, for tests, runs in one go.

| Task | Command |
|---|---|
| Build the plugin (Debug) | `cmake --workflow --preset debug` |
| Build the plugin (Release) | `cmake --workflow --preset release` |
| Run the unit tests | `cmake --workflow --preset unit` |
| Run the unit tests and pluginval | `cmake --workflow --preset test` |
| Run all tests against a Release build | `cmake --workflow --preset test-release` |
| Build the plugin with the WebView editor (Release) | `cmake --workflow --preset webview` |
| Launch the Standalone app (Debug) | `cmake --build --preset run` |
| Install for your DAW: **macOS** | Automatic on every build |
| Install for your DAW: **Windows** | `cmake --build --preset install-windows`, from an administrator prompt |
| Open in an IDE for debugging: **macOS** | `cmake --preset xcode`, then open `build-xcode/Reverb.xcodeproj` |
| Open in an IDE for debugging: **Windows** | Open `build-debug\Reverb.sln` in Visual Studio after a Debug workflow |

`run` and `install-windows` use the Debug build, so run `cmake --workflow --preset debug` once first.
Windows installs on demand rather than after every build because its VST3 folder needs
administrator rights; `-DPLUGIN_COPY_AFTER_BUILD=ON` opts back in.

The individual steps still work on their own, for example to pass a `-D` option:

```bash
cmake --preset debug -DPLUGINVAL_STRICTNESS=5   # configure
cmake --build --preset test                     # build
ctest --preset debug                            # test
```

`cmake --list-presets=all` lists the presets for your platform: `xcode` only appears on macOS, and
`install-windows` only on Windows.

## Unit Tests

Unit tests live in `Tests/` and use [Catch2](https://github.com/catchorg/Catch2). They build into a
`Reverb_Tests` executable that links the plugin's code and drives the processor directly, checking
a snapshot of every parameter as hosts see it, bus layouts, state save and restore, that the output
stays finite across block sizes and parameter settings, that `Effect` matches the `juce_dsp`
chain it replaced, and that the editor's knobs follow their parameters.

Add a `.cpp` file to `Tests/` and reconfigure to pick it up. SampleRealm-Modules' `PluginTestHelpers.h`,
included by `Tests/TestHelpers.h`, has helpers for setting parameters and processing buffers. Set `-DENABLE_UNIT_TESTS=OFF` to skip them entirely.

Like pluginval, the tests are not part of a plain build: the `debug` workflow builds only the plugin.

## Validating the Plugin

[pluginval](https://github.com/Tracktion/pluginval) loads the built plugin as a host would and
tests it for stability. It is built from source on demand, so there is nothing to install.

```bash
cmake --build --preset debug --target validate   # builds, then validates
ctest --preset debug                             # validates an existing build
```

Logs land in `build-debug/pluginval-logs/`. Strictness defaults to 10; use
`-DPLUGINVAL_STRICTNESS=5` (range 1 to 10) for a faster run, or `-DENABLE_PLUGINVAL=OFF` to skip
pluginval entirely.

On macOS the AU test validates the installed component in `~/Library/Audio/Plug-Ins/Components`,
since macOS resolves Audio Units through its registry rather than by path, so it needs
`PLUGIN_COPY_AFTER_BUILD` left on. Steinberg's VST3 conformance validator is off by default, as it
pulls ~300MB of SDK for one extra test; enable it with `-DPLUGINVAL_VST3_VALIDATOR=ON`.

## CI

[`.github/workflows/ci.yml`](.github/workflows/ci.yml) builds the Release plugin on macOS and
Windows and runs the unit tests and pluginval. It runs on every push to `main`, or by hand:
**Actions → CI → Run workflow** on GitHub, or `gh workflow run ci.yml`.

Each run's page lists the plugins under **Artifacts** for 30 days: `Reverb-macOS.tar.gz` (VST3 and
AU) and `Reverb-Windows.tar.gz` (VST3). They are unsigned.

## Parameters

`Source/shared/ParameterSpecs.h` is the only parameter list, read by the plugin, the wasm engine and
the web UI. The snapshot test in `Tests/ReverbTests.cpp` pins what hosts and saved sessions see.

## Web Demo and WebView Editor

| Task | Command |
|---|---|
| Build and test the demo (engine and UI tests) | `cd wasm && cmake --workflow --preset wasm` |
| Serve the demo with live reload at http://localhost:8099 | `cd webui && npm run serve` |
| Typecheck and test the UI on their own | `cd webui && npm run typecheck && npm test` |
| Build the plugin with the WebView editor | `cmake --workflow --preset webview` |

- The demo builds to `wasm/build-wasm/SampleRealm-Reverb-<version>/`, named from `COMPANY_NAME`,
  `project()` and `VERSION` in `CMakeLists.txt`. Upload that folder; the site embeds its `index.html` in an iframe.
  [`wasm.yml`](.github/workflows/wasm.yml) builds the same folder on GitHub as an artifact of that name, on every push
  to `main` or by hand: **Actions → Web demo → Run workflow**, or `gh workflow run wasm.yml`.
- The engine's tests are `webui/test/engine.test.ts`, run by the wasm workflow's `ui` test.
- The demo's input is made by `webui/src/demo/loop.ts`. For a recording, add it to
  `webui/public/demo/` as AAC (`.m4a`) so Safari can decode it, and decode it in `startAudio`.
- The demo's spectrum comes from a Web Audio `AnalyserNode`, so it is windowed and looks smoother than the plugin's.
- The `webview` build installs over the native one; run the `release` workflow to switch back.
- The wasm build patches JUCE (`cmake/patches/juce-wasm.patch` in SampleRealm-Modules) until JUCE fixes its wasm support.

## Debugging in Xcode

### 1. Generate Xcode Project

```bash
cmake --preset xcode
open build-xcode/Reverb.xcodeproj
```

### 2. Configure Debugging

1. Select your plugin target from the scheme dropdown
2. Go to **Product → Scheme → Edit Scheme**
3. Click **Run** on the left sidebar
4. Under **Executable**, choose **Other** and navigate to executable.

### 3. Build and Run

1. Press **Cmd+B** to build the plugin
2. Press **Cmd+R** to run with AudioPluginHost
3. Load your plugin in AudioPluginHost
