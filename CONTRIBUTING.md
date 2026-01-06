# Contributing

## Overview
This repository contains an ObjectARX/MFC C++14 plugin for AutoCAD.

## Build
- Build with Visual Studio (C++14).
- The project uses the Autodesk ObjectARX SDK via a `.props` file.

## UI/Interaction Standards
- The `CErksMapPreviewWnd` preview control should provide consistent navigation:
  - `F` or double-click: fit view to extents.
  - `Home`: reset view.
  - Mouse wheel zoom should feel smooth on trackpads (support non-detent deltas).
  - Provide minor/major layer visibility toggles (hotkeys or popup).
  - Major grid lines may show coordinate labels when sufficiently zoomed in.

## Code Style
- Keep changes minimal and compatible with C++14.
- Preserve existing code comment blocks.
- Prefer readable, self-contained helpers over complex dependencies.