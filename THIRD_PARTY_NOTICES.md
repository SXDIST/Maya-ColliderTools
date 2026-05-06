# Third-party notices

ColliderTools includes convex-hull code derived from DDConvexHull and StanHull.

## DDConvexHull

The procedural hull node remains derived from DDConvexHull and retains its original MIT license header:

- `src/ColliderHullNode.cpp`
- `src/ColliderHullNode.h`

The former DDConvexHull utility wrapper has been refactored into the first-party ColliderTools hull API:

- `src/ColliderHull.cpp`
- `src/ColliderHull.h`

## StanHull

The ColliderTools hull core contains code derived from the StanHull implementation by John W. Ratcliff and retains its original MIT license header:

- `src/ColliderHullCore.cpp`
- `src/ColliderHullCore.h`

Keep these headers intact when editing or moving the files.
