# GeoEast Integration

This folder contains the GeoEast/iEco integration path for FaultPolyTrace.

## Current Milestone

`bo_runner/` contains a minimal console bridge for the competition-critical database loop:

- read `BOMapGrid`
- convert it to the standalone `Grid2D`
- run the ridge-belt tracing core
- write closed polygons as `BOFaultPolygon`

Build it from the Visual Studio 2019 + Qt + GeoEast configured environment:

```text
geoeast_plugin/bo_runner/FaultPolyTraceGeoEast.pro
```

The normal Codex PowerShell may not contain `IECO`, `NGP`, and `GEOEAST` environment variables. That is expected; use the same Qt/GeoEast environment that successfully builds the official Training examples.

## Next Deliverables

- Qt parameter dialog.
- Ribbon/action configuration.
- GeoEast-side screenshots and verification job.
- UI result summary: polygon count, kept count, topology diagnostics, elapsed time.
## Current GeoEast Deliverables

- `bo_runner/`: GeoEast SDK runner. It connects to `ndp`, reads `BOMapGrid`, traces fault polygons, and writes `BOFaultPolygon`.
- `qt_launcher/`: Qt Widgets launcher. It provides the user-facing parameter UI and starts the runner with selected GeoEast database, grid, output name, and algorithm parameters.
- `mainframe_plugin/`: GeoEast main-control ribbon plugin skeleton following the official `05_attriDemos/myPluginsSo` pattern. It is the planned in-GeoEast entry point that launches `faultpolytrace_launcher.exe`.
- `attri_plugin_config/`: lightweight qmake configuration extracted from the official attribute plugin example for the main-control plugin skeleton.

Build the launcher with:

```powershell
.\scripts\build_qt_launcher.ps1
```

Run the verified competition case with:

```powershell
.\scripts\run_geoeast_bgpcup2026_test7.ps1
```

The script accepts path and connection parameters, and also reads `FPT_*` environment variables. This keeps the local demo one-click while avoiding source-level hardcoding.

GeoEast viewing and validation steps are documented in:

```text
docs/GeoEast使用指南.md
```

