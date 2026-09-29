# World picking BVH

`FWorldRaycastAccelerationStructure` builds world AABBs using 32-bin SAH and stores one component reference per leaf. Coincident centroids and unsplittable bounds fall back to splitting the input range in half. Hidden and inactive components remain in the tree so toggling either state does not require a rebuild. Expired references are ignored at query time.

Traversal visits nearer bounds first and performs exact component picking at each leaf. Meshes receive the current closest hit distance. Billboards use camera-independent conservative bounds during build and the current camera's quad at query time. Ordinary primitive picking reports distance zero when the ray starts inside its bounds.

Building is explicit only, through `RebuildAccelerationStructure()`. Queries and registration changes never trigger a build. No production build call site or rebuild policy is installed. Before an explicit build, queries return no hit; afterward, queries use the last built tree. Registration, unregistration, transform, geometry and billboard-size changes do not update that snapshot. There is no automatic linear fallback.

After building Debug x64, run `Scripts\RunWorldPickingTests.cmd` from the repository root. The test executable links the real engine libraries and uses a D3D11 WARP device for a real torus mesh. It checks BVH results against linear OBB/mesh/quad picking, random rotated/scaled boxes, empty trees, coincident/point bounds, visibility and activity changes, expired handles, registration changes, explicit rebuild, billboard camera rotation, and pruning a farther mesh before its narrow-phase query.

Validation: Debug x64 solution build and all WorldPickingTests passed. No interactive UI smoke test or performance benchmark was run for this change. Equal-distance overlapping components may be visited in a different order than the former registration-order scan.
