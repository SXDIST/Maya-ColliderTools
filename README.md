# ColliderTools

ColliderTools is an Autodesk Maya 2027 C++ plugin for quickly generating collision meshes from selected polygon meshes or mesh components. It is focused on Unreal Engine style collider workflows while keeping the generated objects as regular Maya mesh transforms.

## Features

- Axis-aligned box colliders
- Oriented box colliders
- Convex hull colliders through bundled DDConvexHull / StanHull code
- Cylinder colliders
- Capsule colliders
- Object and component selection support
- Optional Unreal-style naming and grouping:
  - `UBX_` for box and oriented box
  - `UCX_` for convex hull and cylinder
  - `UCP_` for capsule
  - output groups under `Colliders/UBX`, `Colliders/UCX`, and `Colliders/UCP`
- Optional procedural construction history for hull, cylinder, and capsule colliders
- Maya shelf buttons with icons under `scripts/` and `icons/`

## Requirements

- Autodesk Maya 2027
- Autodesk Maya 2027 devkit
- CMake 3.13 or newer
- Visual Studio generator compatible with Maya 2027 on Windows

The build expects `DEVKIT_LOCATION` to point at the Maya devkit root that contains `cmake/pluginEntry.cmake`.

## Build

Example local build:

```bash
DEVKIT_LOCATION="C:/maya_devkits/2027/devkitBase" cmake -S . -B build-vs2026-devkitbase-env -G "Visual Studio 18 2026" -A x64
DEVKIT_LOCATION="C:/maya_devkits/2027/devkitBase" cmake --build build-vs2026-devkitbase-env --config Release
```

The compiled plugin is produced at:

```text
build-vs2026-devkitbase-env/Release/ColliderTools.mll
```

If your installed Visual Studio generator differs, use a separate build directory. Do not reuse a CMake build directory with a different generator.

## Load in Maya

In Maya's Python tab, load the compiled plugin:

```python
import maya.cmds as cmds
cmds.loadPlugin(r"C:/path/to/collider-tools/build-vs2026-devkitbase-env/Release/ColliderTools.mll")
```

## Install the shelf

The shelf scripts are separate from the compiled plugin. From Maya's Python tab:

```python
import sys
sys.path.append(r"C:/path/to/collider-tools/scripts")
import colliderTools
colliderTools.install_shelf()
```

The MEL bootstrap is also available:

```mel
source "C:/path/to/collider-tools/scripts/installColliderToolsShelf.mel";
installColliderToolsShelf();
```

The Python shelf loader first checks whether `ColliderTools` is already loaded. If not, it attempts to load the local build output under `build-vs2026-devkitbase-env/Release/ColliderTools.mll` relative to the repository root.

## Command examples

Select a polygon mesh or mesh components, then run MEL commands such as:

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

Supported flags include:

- `-type` / `-t`: `box`, `obox`, `hull`, `cylinder`, or `capsule`
- `-name` / `-n`: output name override
- `-segments` / `-s`: cylinder/capsule segment count
- `-maxVertices` / `-mv`: convex hull vertex limit
- `-skinWidth` / `-sw` and `-useSkinWidth` / `-usw`: convex hull skin-width options
- `-combine` / `-c`: combine selected point sets into one collider
- `-unreal` / `-u`: use Unreal-style names and groups
- `-history` / `-hi`: create procedural history nodes for hull/cylinder/capsule

## Procedural history and manipulators

`-history true` creates editable construction-history colliders instead of only baking the initial mesh.

- `ctColliderPrimitive` drives procedural cylinder and capsule output and exposes `segments`, `radiusScale`, and `lengthScale`.
- `DDConvexHull` drives procedural hull output and exposes hull attributes such as `maxVertices`.
- `ctColliderPrimitive` has Show Manipulator Tool integration for primitive radius/length controls.

Known limitation: Maya's exact native in-view editor overlay for Autodesk primitives is not exposed as a simple public widget. ColliderTools uses public `MPxManipContainer` APIs where stable. Procedural hull In-View integration is currently treated as experimental and should remain disabled until the crash source is fully isolated.

## Production notes

Before publishing a production build, use project-owned Autodesk Maya node IDs. The current local-development `DDConvexHullNode::id` value is not suitable for broad distribution.

Generated build directories, compiled binaries, Python caches, local Claude files, and temporary image exports are intentionally excluded by `.gitignore`.

## License

ColliderTools is released under the MIT License. See `LICENSE`.

Bundled DDConvexHull and StanHull code retains its original MIT license headers. See `THIRD_PARTY_NOTICES.md`.
