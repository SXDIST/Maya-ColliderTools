# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build commands

This is a Maya 2027 C++ plugin built with CMake and the Autodesk Maya devkit. The local devkit is expected at `C:/maya_devkits/2027/devkitBase`; the build also works when `DEVKIT_LOCATION` points to that directory.

Configure and build on this machine:

```bash
DEVKIT_LOCATION="C:/maya_devkits/2027/devkitBase" cmake -S . -B build -G "Visual Studio 18 2026" -A x64
DEVKIT_LOCATION="C:/maya_devkits/2027/devkitBase" cmake --build build --config Release
```

The Release plugin output is:

```text
build/Release/ColliderTools.mll
```

If Visual Studio 2022 is installed instead of Visual Studio 2026, use `-G "Visual Studio 17 2022" -A x64` with a separate build directory. Do not reuse a CMake build directory with a different generator.

There is currently no automated test suite. Verify changes by rebuilding the plugin and loading the resulting `.mll` in Maya 2027.

## Maya usage smoke tests

After loading `ColliderTools.mll` in Maya 2027, select a polygon mesh or mesh components and run:

```mel
ctCreateCollider -type box;
ctCreateCollider -type obox;
ctCreateCollider -type hull;
ctCreateCollider -type cylinder -segments 16;
ctCreateCollider -type capsule -segments 16;
ctCreateCollider -type box -combine true;
ctCreateCollider -type hull -maxVertices 64 -unreal true;
ctCreateCollider -type hull -maxVertices 64 -unreal true -history true;
ctCreateCollider -type cylinder -segments 16 -unreal true -history true;
ctCreateCollider -type capsule -segments 16 -unreal true -history true;
```

The command creates normal Maya mesh transform/shape nodes and selects the created transforms. Supported collider types are `box`, `obox`, `hull`, `cylinder`, and `capsule`. Pass `-unreal true` to generate Unreal-style collision names (`UBX_`, `UCX_`, `UCP_`). Pass `-history true` for adjustable procedural hull/cylinder/capsule colliders backed by construction-history nodes.

For history smoke tests, press `T` or run Show Manipulator Tool after creating procedural cylinder/capsule colliders. Confirm `ctColliderPrimitive` exposes in-view `radiusScale`/`lengthScale`, component selections fit only the selected mesh elements, generated colliders remain in the correct `Colliders/*` group, normals face outward, and primitive outputs have hard edges. Procedural hull In-View is currently disabled for stability; verify hull attributes through the Channel Box / Attribute Editor instead.

## Shelf UI

The Maya UI scripts live under `scripts/` and are separate from the compiled `.mll`. To install/update the shelf in Maya's Python tab:

```python
import sys
sys.path.append(r"C:/path/to/collider-tools/scripts")
import colliderTools
colliderTools.install_shelf()
```

This creates a `ColliderTools` shelf with quick buttons for `UBX Box`, `UBX OBox`, and procedural history actions for `UCX Hull`, `UCX Cylinder`, and `UCP Capsule`. The Python UI attempts to load `build/Release/ColliderTools.mll` automatically if the plugin is not already loaded.

The hull/cylinder/capsule shelf actions create construction-history colliders with editable node attributes (`maxVertices`, `segments`, `radiusScale`, `lengthScale`) and attempt to switch Maya to Show Manipulator Tool. The older Python live-preview popup code remains as fallback but is not the primary shelf path.

Shelf icons are loaded from `icons/*.png`; reinstall the shelf after icon or script changes.

The MEL bootstrap equivalent is `scripts/installColliderToolsShelf.mel`, which defines `installColliderToolsShelf()`.

## Architecture

The top-level `CMakeLists.txt` builds one plugin target named `ColliderTools`. It uses Maya's `pluginEntry.cmake`, links `OpenMaya`, `OpenMayaUI`, and `Foundation`, and compiles the ColliderTools command, nodes, manipulators, geometry helpers, and hull core.

Plugin registration is centralized in `src/ColliderToolsPlugin.cpp`. It registers:

- dependency node `ctColliderHull`, implemented by `src/ColliderHullNode.cpp`
- dependency node `ctColliderPrimitive`, implemented by `src/ColliderPrimitiveNode.cpp`
- manipulator node `ctColliderPrimitiveManip`, implemented by `src/ColliderPrimitiveManip.cpp`
- command `ctCreateCollider`, implemented by `src/ColliderCreateCmd.cpp`

`src/ColliderCreateCmd.cpp` is the public command entry point. It parses command flags (`-type`, `-name`, `-segments`, `-maxVertices`, `-skinWidth`, `-useSkinWidth`, `-combine`, `-unreal`, `-history`), reads the active Maya selection, gathers point sets, dispatches primitive fitting/generation, creates output mesh transforms, parents them under `Colliders/UBX`, `Colliders/UCX`, or `Colliders/UCP`, selects them, and returns their transform names. With `-history true`, hull/cylinder/capsule outputs are connected to procedural dependency nodes instead of baked immediately. History mode preserves component selections by storing component-list data on the history node, and uses source `outMesh` plus matched source transforms to avoid world/object-space offsets.

`src/ColliderFit.cpp` handles selection-to-points and fitting:

- accepts selected transforms, mesh shapes, and mesh components
- converts mesh points to world space for command-created static colliders
- deduplicates quantized points and filters non-finite coordinates with `cleanPointSet()`
- uses `ColliderTools::componentToVertexIds` for component selections
- computes AABB, improved candidate-axis/twist-sampled OBB, and candidate-axis radius/length fits for cylinder/capsule generation

`src/ColliderGeometry.cpp` contains deterministic mesh construction helpers for boxes, cylinders, and capsules, plus `createMeshTransform()` for creating Maya mesh transform/shape nodes. Primitive mesh faces are oriented outward before creation. New shapes are hardened with `hardenMeshEdges()`, forced into `initialShadingGroup`, mesh/object color display is disabled, and display overrides are cleared so colliders appear like default Maya primitives.

`src/ColliderPrimitiveNode.cpp` implements `ctColliderPrimitive`, a procedural construction-history node for cylinder and capsule colliders. It exposes keyable `segments`, `radiusScale`, and `lengthScale` attributes, accepts optional `inputComponents`, cleans selected points, fits cylinder/capsule output from the connected source mesh, and hardens generated mesh edges.

Convex hull generation is integrated through the ColliderTools hull module:

- `src/ColliderHull.cpp` exposes `ColliderTools::createHullMeshData()` and `ColliderTools::componentToVertexIds()`
- `src/ColliderHullCore.cpp` contains the MIT-licensed StanHull-derived algorithm behind that wrapper boundary
- `src/ColliderHullNode.cpp` implements `ctColliderHull` for node-based/procedural hull workflows, supports component-list inputs, cleans unique points before hull generation, and exposes `maxVertices`

`src/ColliderPrimitiveManip.cpp` provides Show Manipulator Tool / In-View Editor integration through `MPxManipContainer`. `ctColliderPrimitiveManip` exposes primitive `radiusScale` and `lengthScale` plugs and distance handles. The exact native Autodesk polyCylinder overlay is not public API, so this project uses custom manipulator containers plus `addPlugToInViewEditor()` where stable. Procedural hull In-View integration is disabled for production stability until its crash source is isolated.

The old standalone DDConvexHull project files and sample assets are not part of this repository layout; `ctCreateCollider` is the command implementation.

## Important constraints

Preserve the DDConvexHull and StanHull license headers when editing or moving those files. Maya node IDs in this project use local development values and should be replaced with properly assigned Autodesk node id ranges before broad production distribution.
