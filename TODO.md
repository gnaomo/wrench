# Todo List

> **Note (LLM-edited, unverified):** The section below and some entries
> further down were written/updated by an LLM (Claude) reading a working-
> tree diff, not by a human developer reviewing the change. As of this
> note, all changes described below have been committed (each as its own
> commit -- see the bullet list for what each one contains) and the build
> succeeds, but none of it has been reviewed, tested against edge cases,
> or verified for architectural correctness by a human or by the project
> maintainer, except where a bullet explicitly says a human built and
> functionally tested it. Multiple separate LLM sessions have been working
> on this codebase without cross-checking each other. Treat everything
> below as a claim to be verified, not a confirmed fact.

## Recently committed changes as of this note

Objective description of what the commits above contain, for future
reference (this list will go stale as the code changes further -- it
describes the commits as of when they landed, not the current tree):

- `collision_asset.cpp`/`.h`, new files `collision_mesh.cpp`/`.h`,
  `editor/gui/collision_fixer.cpp`, `editor/level.cpp`,
  `editor/instanced_collision_recovery.cpp`/`.h`: collision meshes are
  written/read as glTF (`.glb`) instead of COLLADA (`.dae`). Quad collision
  faces are converted to pairs of triangles when written to glTF, and are
  not reconstructed as quads when read back — this does not change
  collision behaviour in-game (verified against `reduce_quads_to_tris` in
  `engine/collision.cpp`, which only ever splits quads at octant
  boundaries, never merges triangles into quads). Downstream effects on
  the octant packer in `engine/collision.cpp` (itself untouched by this
  diff, so this is about what it now receives as input, not a change to
  it):
  - Per-octant vertex count is unaffected (splitting a quad into two
    triangles reuses the same 4 vertices, no new ones are added), so the
    existing 256-vertex-per-octant cap (`octant.vertices.size() >= 256`,
    silently drops the octant with a warning) isn't touched by this.
  - Every collision mesh round-tripped through the new glTF path arrives
    back with zero quad faces (all split into triangles), so the existing
    `quad_count >= 256` check (also a silent per-octant drop with a
    warning) can no longer fire for migrated content — effectively moot
    now, not a regression.
  - Per-octant face count roughly doubles for what used to be quad-heavy
    octants (1 quad face -> 2 triangle faces), and that count feeds
    `verify(octant.faces.size() < 65536, "Too many faces in octant.")` —
    a hard abort of the *entire build*, not a graceful per-octant drop
    like the two checks above it. An octant would need roughly 32,768+
    quad faces before the split to cross this after doubling, which is an
    extreme density for a single octree cell and very unlikely to occur in
    the stock game levels, but it's a real, new failure mode that wasn't
    reachable in quite this form before, most plausible with unusually
    dense custom/modded collision geometry rather than stock content.
  - Net effect on file size: collision data for quad-heavy meshes takes up
    more space than before, roughly proportional to how many real quads
    the original mesh had.
- `assetmgr/asset.cpp`, `assetmgr/zipped_asset_bank.cpp`: source-file path
  matching in `enumerate_source_files` now uses `fs::path::generic_string()`
  instead of manual backslash replacement (or, in `MemoryAssetBank`'s case,
  no normalisation at all previously). The `MemoryAssetBank` case was a
  real pre-existing bug: `get_common_source_path()`/`get_game_source_path()`
  always return forward-slash strings, so on Windows the old unnormalised
  comparison would never have matched. This fix covers those two call
  sites; the wider TODO item below about `std::filesystem::path` usage
  across `src/assetmgr` (~94 occurrences) is not otherwise addressed by it.
- `core/buffer.cpp`: `OutBuffer::writesf`/`writelf` used a fixed 16KB stack
  buffer that `vsnprintf` could silently overrun for longer formatted
  strings (the returned length was used to size a `memcpy` out of the same
  undersized buffer). Replaced with a two-pass `vsnprintf` sizing pattern.
- `core/collada.cpp`: several `%d`-format arguments were passed `size_t`
  values instead of `int`/`s32`, now cast explicitly.
- New file `core/util/crash_handler.cpp` (+ `CMakeLists.txt`/
  `wrench_compiler_flags.cmake` changes): installs a signal handler for
  SIGSEGV/SIGABRT/SIGFPE/SIGILL that prints an error-context string and a
  demangled backtrace before re-raising. Its own comments note it isn't
  fully async-signal-safe (`fprintf`/`malloc`-family calls inside a signal
  handler), by design — described as a best-effort diagnostic aid, not a
  hardened handler. `CMakeLists.txt` also now defaults `CMAKE_BUILD_TYPE`
  to `RelWithDebInfo` when unspecified.
- `src/editor/CMakeLists.txt` includes `collision_mesh.cpp` via a relative
  path into `../wrenchbuild/level/`, rather than that file being part of a
  shared library both executables link against.
- `core/gltf.h`/`.cpp`, `wrenchbuild/level/collision_mesh.h`/`.cpp`,
  `core/collada.h`/`.cpp`, `wrenchbuild/level/collision_asset.cpp`,
  `editor/gui/collision_fixer.cpp` (built and functionally verified by a
  human -- opens, extracts, level editor works, repacking an ISO works):
  `native_mesh_to_gltf_mesh()`/`gltf_mesh_to_native_mesh()` moved out of
  `collision_mesh.cpp` into `core/gltf.h`/`.cpp` (inside `namespace GLTF`,
  relying on argument-dependent lookup so call sites didn't need explicit
  qualification), so ties/tfrags can reuse them later without depending on
  collision-specific code. Along the way, `native_mesh_to_gltf_mesh`'s
  materials parameter changed from `std::vector<ColladaMaterial>` to plain
  `std::vector<Material>`, so `core/gltf.h` doesn't pick up a dependency on
  `core/collada.h` (which is slated for deletion once ties/tfrags migrate).
  A new `to_materials()` helper in `core/collada.h`/`.cpp` bridges
  `ColladaScene::materials` to the plain type at the two call sites that
  still produce `ColladaMaterial`. `collision_mesh.h`/`.cpp` now only
  contain the collision-specific `append_collision()` and no longer
  include `core/collada.h` at all. One shadowing bug was introduced and
  fixed during this change: a local `std::vector<Material> materials` in
  `unpack_collision_asset()` collided with a pre-existing
  `CollectionAsset& materials` later in the same function; renamed to
  `gltf_materials`.
- `wrenchbuild/classes/tie_class.cpp`, `core/gltf.h`/`.cpp`,
  `editor/level.cpp` (built and functionally verified by a human -- ISO
  extracted, many levels opened/checked in the editor, ISO repacked):
  `unpack_tie_class()` now writes tie meshes out as `mesh.glb` via the
  shared `native_mesh_to_gltf_mesh()` helper (see the item above) instead
  of `mesh.dae` via `write_collada()`. `recover_tie_class()` itself is
  unchanged and still returns a `ColladaScene` internally -- only the file
  written to disk changed. This is unpack-only: `pack_tie_class()` still
  calls `verify_not_reached_fatal("Not yet implemented.")` regardless,
  since `write_tie_class()` in `engine/tie.cpp` is itself an empty stub.
  Two bugs were found and fixed along the way:
  - `native_mesh_to_gltf_mesh()` only ever set the glTF `POSITION`
    attribute bit on output primitives, silently dropping normals/vertex
    colours/texture coordinates for any mesh that had them. Invisible for
    collision meshes (which never set those mesh flags), but would have
    silently discarded tie UVs on the round trip through the `.glb` file.
    Now sets `NORMAL`/`COLOR_0`/`TEXCOORD_0` based on the source mesh's
    `MESH_HAS_NORMALS`/`MESH_HAS_VERTEX_COLOURS`/`MESH_HAS_TEX_COORDS`
    flags, matching what the old COLLADA writer did. Relevant to the
    upcoming tfrags migration too, since tfrags also carry UVs.
  - `load_tie_editor_class()` in `editor/level.cpp` still assumed
    `editor_mesh`'s file was COLLADA XML and called `read_collada()`
    directly on it, which crashed the editor (`[collada.cpp:96] error:
    expected <`) the first time a level was opened after the change above
    landed, since the file is now a binary `.glb`. Rewritten to parse the
    `.glb` with `GLTF::read_glb`/`GLTF::lookup_node`, remap materials with
    `GLTF::map_gltf_materials_to_wrench_materials`, and upload with
    `upload_gltf_mesh`/`upload_materials`, mirroring
    `load_moby_editor_class`/`load_shrub_editor_class`. Still populates
    `EditorClass::mesh` (the native CPU-side copy, via
    `gltf_mesh_to_native_mesh`), unlike the moby/shrub loaders, because
    `editor/gui/collision_fixer.cpp`'s `generate_bounding_box()` depends on
    it for the instanced-collision-fixer tool.
- New files `editor/gui/collision_legend.h`/`.cpp`, `editor/renderer.h`/`.cpp`,
  `editor/level.h`/`.cpp`, `editor/gui/editor_gui.cpp`, `editor/gui/view_3d.cpp`,
  `editor/CMakeLists.txt` (built and functionally verified by a human --
  the per-id collision filter and legend UI work as intended): adds a way
  to hide/show collision faces by surface id in the editor, instead of the
  existing all-or-nothing `draw_collision` toggle.
  - `RenderSettings::hidden_collision_ids` (`std::array<bool, 256>`)
    threaded through `draw_mesh`/`draw_mesh_instanced` as an optional
    filter parameter (defaults to `nullptr`), applied only to the
    collision draw call in `draw_level()`. All other callers (moby, shrub,
    tie, tfrags, hero collision) pass no argument and so are unaffected --
    this does not touch or interact with the tie glTF migration above,
    which is a separate code path (`load_tie_editor_class` vs. this
    feature's `Level::read`/`draw_level` changes).
  - `EditorChunk::collision_ids_present`: distinct collision ids used in
    each chunk, collected at load time in `Level::read` from the same
    submesh list `collision_to_scene()` already deduplicates, so it
    doesn't need to scan all 256 possible ids.
  - `gui/collision_legend.h`/`.cpp`: shared checkbox-list logic (colour
    swatch, hex id, known name where available, All/None/Invert), a
    lookup table of documented names for ~19 identified collision ids, a
    floating "Collision legend" window over the 3D viewport, and a nested
    View > Visibility > Collision ids menu that reuses the same checkbox
    logic so the two entry points can't drift apart.
  - Not persisted across relaunches (no other `RenderSettings` flag is
    persisted either, by the commit's own note, so this follows existing
    precedent rather than being an oversight).
- `wrenchbuild/level/tfrags_asset.cpp`, `editor/level.cpp` (built and
  functionally verified by a human -- ISO extracted, many levels opened
  and checked in the editor with tfrag geometry/textures rendering
  correctly, ISO repacked): `unpack_tfrags()`
  now writes tfrag meshes out as
  `mesh.glb` via the shared `native_mesh_to_gltf_mesh()` helper instead of
  `mesh.dae` via `write_collada()`, mirroring the tie migration above.
  `recover_tfrags()` itself is unchanged and still returns a `ColladaScene`
  internally. Added a guard for the case where `recover_tfrags()` produces
  zero meshes (happens when a chunk's `Tfrags::fragments` is empty) --
  `scene.meshes.at(0)` would otherwise throw; now `editor_mesh` is simply
  left unset, matching how `Level::read`/`pack_tfrags` already treat a
  `TfragsAsset` with no editor mesh. This is unpack-only: `pack_tfrags()`
  still calls `verify_not_reached_fatal("Not yet implemented.")` when the
  source isn't already a binary asset, unrelated to this change.
  
  The tfrag-loading block in `Level::read` (`editor/level.cpp`) was
  rewritten the same way as `load_tie_editor_class` -- `GLTF::read_glb`/
  `GLTF::lookup_node` instead of `read_collada`/`ColladaScene::find_mesh`,
  `upload_gltf_mesh`/`upload_materials` instead of `upload_mesh`/
  `upload_collada_materials`. Found and fixed an ordering issue along the
  way: the old code called `upload_mesh()` on the tfrag mesh *before*
  remapping its material indices (`map_lhs_material_indices_to_rhs_list`),
  so the uploaded mesh's material indices were never actually the remapped
  ones. In practice this was harmless -- `recover_tfrags()` numbers
  materials by texture id and `unpack_level_materials()` names each
  `MaterialAsset` the same way, so the "remap" was always the identity
  mapping -- but the new code remaps before uploading regardless, matching
  the order used by the moby/tie/shrub loaders in the same file.
  
  Separately, found (not introduced by this change) that
  `wrenchvis/wrenchvis.cpp` -- a standalone occlusion-generation CLI tool,
  distinct from `wrenchbuild`/`wrencheditor` -- has its own independent
  `read_collada` call sites in `load_moby_classes` and `load_tie_classes`,
  which weren't part of the roadmap's item-5 inventory of call sites
  blocking `core/collada.cpp`'s eventual deletion. `load_moby_classes` has
  been broken (reads binary `.glb` as if it were COLLADA XML) since moby
  models moved to glTF in v0.6, entirely unrelated to any work in this or
  the previous session. `load_tie_classes` broke as a consequence of the
  tie migration above. `wrenchvis`'s tfrag loading (`load_chunks`) reads
  straight from the binary asset rather than a file, so it was never
  affected by any of this. Both `load_moby_classes`/`load_tie_classes`
  have been fixed the same way (new shared `load_editor_mesh_for_occlusion`
  helper: `GLTF::read_glb`/`GLTF::lookup_node`/`gltf_mesh_to_native_mesh`,
  materials skipped entirely since occlusion computation doesn't need
  them). Unlike the two files above, `wrenchvis.cpp` is a separate
  executable the editor/build tools never invoke, so the human build/test
  pass didn't exercise it -- confirmed to compile as part of the overall
  build, but not functionally run/tested by anyone yet.
- New files `core/mesh_scene.h`/`.cpp`, deleted `core/collada.h`/`.cpp`, and
  ~31 other files touched for includes/renames (built and functionally
  verified by a human -- full build succeeded, ISO extracted, levels
  opened and checked in the editor, ISO repacked): `read_collada()`/
  `write_collada()` had zero remaining callers anywhere after the tie/tfrag
  migrations above, but `ColladaScene`/`ColladaMaterial` (the *types*, not
  the XML functions) turned out to still be load-bearing as the shared
  in-memory "recovered mesh" representation across ~17 files
  (`engine/collision.*`, `engine/tie.*`, `engine/moby_low.*`,
  `engine/tfrag_high.*`/`tfrag_debug.*`, `gui/render_mesh.*`,
  `editor/instanced_collision_recovery.*`, `editor/gui/collision_fixer.cpp`,
  `editor/level.cpp`, three `wrenchbuild` files, `wrenchvis.cpp`) -- used by
  `recover_collision()`/`recover_tie_class()`/`recover_tfrags()`/
  `recover_moby_class()`/`build_instanced_collision()` (the editor's
  instanced collision fixer tool), even for asset types that already write
  glTF. So deleting `core/collada.cpp/h` meant renaming and relocating
  these types rather than just deleting dead code:
  - `ColladaScene` -> `RecoveredScene`, `ColladaMaterial` ->
    `RecoveredMaterial`, `upload_collada_material(s)` ->
    `upload_recovered_material(s)`, moved into new `core/mesh_scene.h`/`.cpp`
    along with `Joint`/`add_joint()`/`to_materials()`/
    `map_lhs_material_indices_to_rhs_list()`.
  - Dropped `read_collada()`/`write_collada()` and all their XML-parsing/
    -serialising-only helper functions entirely, after confirming zero
    remaining callers.
  - Found and removed two pieces of pre-existing dead code while auditing
    what to keep: a `read_collada_files()` utility in
    `assetmgr/asset_util.cpp`/`.h` with zero callers anywhere (long
    superseded by `read_glb_files()`, which sits right next to it), and a
    declared-but-never-defined `assert_collada_scenes_equal()` in the old
    header paired with a never-called, differently-named
    `verify_fatal_collada_scenes_equal()` in the old .cpp (a mismatch that
    means neither was ever actually callable/called from outside the file).
  - Finished off the last two real `read_collada`/`write_collada` call
    sites, the `extract_tfrags`/`extract_tie` debug subcommands in
    `wrenchbuild/main.cpp`, converting them to write `.glb` the same way
    `extract_moby`/`extract_shrub` already did (including usage-text
    updates).
  - Cleaned up stale comments/error strings referencing the now-deleted
    functions/file, including one in `engine/moby_low.cpp`'s
    `build_moby_class()` ("Collada file doesn't contain...") unrelated to
    any of this session's earlier work.
  - Three files (`engine/moby_packet.h`, `instancemgr/level_settings.h`,
    `wrenchbuild/level/sky_asset.cpp`) turned up with a `#include
    <core/collada.h>` that, on inspection, was already vestigial -- none of
    them use `Joint`/`RecoveredScene`/`RecoveredMaterial`/`to_materials()`
    or plain `Material`/`MaterialSurface` anywhere. Left these pointed at
    the new `core/mesh_scene.h` rather than removing the include outright,
    since that's a strictly safer edit to make without a compiler to check
    against (worst case is a harmless unused include, not a missing one).
  
  Found and fixed one real latent bug this exposed:
  `editor/instanced_collision_recovery.h` declared a function returning
  `Opt<ColladaScene>` without including the header that defined the type,
  silently relying on a transitive include path through
  `assetmgr/asset_util.h` -> `assetmgr/asset.h`. That path broke when the
  now-unused `core/collada.h` include was removed from `asset_util.h` as
  part of deleting the dead `read_collada_files()` above -- fixed by
  including `core/mesh_scene.h` directly in the header that actually uses
  the type, rather than continuing to rely on transitive luck (a
  pre-existing fragility, not something introduced by this session).
- Separately from the diff above: the `thirdparty/zlib` submodule had a
  tracked file (`zconf.h`) missing from its working directory, unrelated to
  any of the changes above and with no clear cause found. It has been
  restored (`git checkout -- zconf.h` inside the submodule). A stray,
  untracked, ~2.3GB nested build directory at `bin/bin/` was also found and
  removed (build output only, not source-affecting).

In no particular order:

- Build System/CI
	- ~~Integrate automated testing~~
- Moby Model Packing
	- Seperate out matrix allocation/scheduling from read/write functions to improve testability
	- Use the existing tristrip algorithm to build new submeshes
- Tfrag Model Packing
	- ~~Recover original tfaces~~
		- ~~Possibly compare different LOD levels to determine which strips are part of which tface~~
	- ~~Migrate tfrag mesh unpacking from COLLADA to glTF~~ (unpack side
	  only, built and functionally verified -- see "Recently committed
	  changes" above; `pack_tfrags()`'s "not yet implemented" path is
	  untouched)
	- Figure out how tfaces should be represented in the source format
	- Build new tfrags
	- Make sure to pad the tfrag blocks in the level core and the chunks to the same size
- Tie Model Packing
	- ~~Migrate tie mesh unpacking from COLLADA to glTF~~ (unpack side only --
	  see "Recently committed changes" above; `pack_tie_class()`/
	  `write_tie_class()` are still unimplemented stubs)
	- Similar issues as with the tfrag renderer for full model packing
- Occlusion System
	- ~~Generate new occlusion data during a build~~
	- ~~Possibly use OpenGL in wrenchbuild to speed things up (should be faster than raycasting)~~
- Gameplay Source Format
	- ~~Will probably be based on the Wrench Text Format~~
	- ~~System for editing/storing pvar data~~
- General Editor Improvements
	- ~~Transform tool~~
	- Tools for adding/removing instances
	- GUI for editing asset files
		- Should make use of the asset schema
- Audio Packing/Unpacking
	- VAG files
	- 989snd sound banks
		- Samples
		- IOP-side scripting with grains
		- Version used in R&C doesn't seem to support MIDI (good?)
- GUI/HUD System
	- Widget 3D file
	- HUD banks
- Asset system improvements
	- Incremental build support
	- Support for storing the asset graph to disk and only reparsing asset files that have been modified
	- Multithreading?
	- Replace all uses of std::filesystem::path for asset bank file paths (so the behaviour is consistent between platforms)
- Sky Packing
	- Automatically split up a mesh into separate clusters
- Texture Packing
	- Automatic conversion to paletted colour
	- Merge palettes together (like the original game does)
	- Generate better mipmaps
	- Fix general glitchiness
- Memory Map
	- Determine memory map during build
		- Error out if the files are too big to work
- Collsion System
	- ~~Recover instanced collision~~
	- Collision meshes are written/read as glTF instead of COLLADA -- see
	  "Recently committed changes" above for details.
- Level Packing
	- ~~Don't unpack the tfrag block for chunk 0 twice~~
