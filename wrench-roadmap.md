# Wrench Developer Roadmap

Grounded against the actual working tree at `/home/yogas1/github_cloned_repos/wrench`
(branch `master`, currently at `1f4d099d`) — not just `TODO.md`.

> **Maintenance note:** this file is meant to be kept current as work
> lands, not just written once. Each phase below is marked DONE / IN
> PROGRESS / NOT STARTED, and that status should be updated in the same
> sitting as the corresponding commits, not left to go stale like the
> "Ground truth" section used to.

## Ground truth

The authoritative, currently-maintained record of what's actually in the
working tree lives in `TODO.md` (see its top section), not here. That
section gets updated as the working tree changes; this roadmap doesn't on
its own, so treat `TODO.md` as the source of truth for "what's true right
now" and this document as the source of truth for "in what order to tackle
what's left" plus "what's already been done, in this order."

Headline facts that affect ordering below (see `TODO.md` for full detail):

- Collision mesh packing is on glTF (committed as of `5f429426`). Quad
  faces don't survive the round-trip as quads, which interacts with a hard
  per-octant limit in the packer; unlikely to bite on stock content, but
  worth knowing before assuming it's free.
- The generic `Mesh <-> GLTF::Mesh` conversion helpers
  (`native_mesh_to_gltf_mesh`/`gltf_mesh_to_native_mesh`) have been moved
  out of collision-specific code into `core/gltf.h`/`.cpp`, decoupled from
  `ColladaMaterial` in favour of the plain `Material` type (a new
  `to_materials()` helper in `core/collada.h` bridges the two at the two
  remaining call sites). This was Phase 1 item 2 below.
- Ties are **migrated on the unpack/editor side, verified working**:
  `unpack_tie_class` in `tie_class.cpp` writes a `mesh.glb` via the shared
  `native_mesh_to_gltf_mesh()` helper instead of a `mesh.dae` via
  `write_collada()`, and `load_tie_editor_class` in `editor/level.cpp` reads
  it back via `GLTF::read_glb` instead of `read_collada()` (this second fix
  was needed after the first caused a crash opening levels in the editor).
  The **pack** side (`pack_tie_class`) still stubs out with
  `verify_not_reached_fatal("Not yet implemented.")`, and `write_tie_class()`
  in `engine/tie.cpp` is itself still an empty stub, so full tie building
  remains gated on the Phase 2 tface work. Tfrags are **not yet** migrated
  — still stubs out `pack()` with `verify_not_reached_fatal("Not yet
  implemented.")` and still calls `write_collada()`.
  
  While doing this, found and fixed a real gap in the shared helper itself:
  `native_mesh_to_gltf_mesh()` (`core/gltf.h`/`.cpp`) previously only ever
  set the `POSITION` attribute bit on output primitives, silently dropping
  normals/vertex colours/texture coordinates on any mesh that had them.
  This was invisible for collision meshes (which never set those mesh
  flags) but would have silently discarded tie UVs on the round trip
  through the `.glb` file. It now sets `NORMAL`/`COLOR_0`/`TEXCOORD_0`
  based on the source mesh's `MESH_HAS_NORMALS`/`MESH_HAS_VERTEX_COLOURS`/
  `MESH_HAS_TEX_COORDS` flags, mirroring what the old COLLADA writer did.
  Worth keeping in mind for the tfrags migration too, since tfrags also
  carry texture coordinates.
  
  Built and functionally tested by the user: ISO extracted, many levels
  opened and checked in the editor, ISO repacked -- all working.
- Moby's matrix/tristrip split hasn't been started.
- Of the ~94 `fs::path` uses in `src/assetmgr`, 2 call sites
  (`enumerate_source_files` in `asset.cpp`/`zipped_asset_bank.cpp`) are
  fixed (committed as of `8db68a4c`); the rest are untouched.
- VAG has a low-level size-reading stub (`vag.cpp`); no pack/unpack
  integration. 989snd, incremental builds/asset-graph persistence, and
  build-wide multithreading all confirmed not started.

## Ordering — why this sequence

### Phase 0 — Commit what's already in flight — **DONE**
1. ~~Commit the collision COLLADA→glTF migration, the crash handler + CMake
   build-type/`-rdynamic` changes, and the `generic_string()` path fix in
   `asset.cpp`/`zipped_asset_bank.cpp`.~~ Landed as five commits rather
   than one, so the history reads as separable, reviewable changes instead
   of one undifferentiated blob:
   - `be4b083d` — core: fix stack buffer overflow in
     `OutBuffer::writesf/writelf`
   - `8db68a4c` — assetmgr: `generic_string()` cross-platform path fix
     (the confirmed Windows bug)
   - `38b29301` — core: crash handler + the CMake changes it needs
   - `5f429426` — collision: COLLADA→glTF migration
   - `1f4d099d` — docs: TODO.md/CHANGELOG.md reworded from "uncommitted"
     to "committed" now that the above landed (content unchanged)

   Build was verified to succeed both before and after this phase, on top
   of an already-clean tree (zlib submodule already restored, stray build
   dir already gone from a prior session). Nothing has been pushed to
   `origin/master` yet — these are local commits pending review.

*Rationale:* none of this blocked on anything else, and leaving it
uncommitted risked bit-rot or merge pain once Phase 1–2 work touched the
same files (`collision_asset.h`, `collision_mesh.h` were already part of
the WIP).

### Phase 1 — Finish the format migration (unlocks Phase 2) — **IN PROGRESS**
2. **Generalize** `native_mesh_to_gltf_mesh()`/`gltf_mesh_to_native_mesh()`
   out of `collision_mesh.cpp` into shared code — **done and verified.**
   Moved into `core/gltf.h`/`.cpp` (inside `namespace GLTF`, relying on ADL
   so call sites didn't need to be qualified). Along the way, changed
   `native_mesh_to_gltf_mesh`'s materials parameter from
   `std::vector<ColladaMaterial>` to the plain `std::vector<Material>`, so
   that `core/gltf.h` doesn't gain a dependency on `core/collada.h` (which
   is slated for deletion in item 5) — a new `to_materials()` helper in
   `core/collada.h`/`.cpp` bridges `ColladaScene::materials` to the plain
   type at the two call sites that still produce `ColladaMaterial`
   (`collision_asset.cpp`, `collision_fixer.cpp`). `collision_mesh.h`/`.cpp`
   now only contain the collision-specific `append_collision()` and no
   longer include `core/collada.h` at all. Built and functionally tested by
   the user: opens, extracts, level editor works, ISO repacking works.
3. Migrate **ties** off COLLADA using those shared helpers
   (`tie_class.cpp`) — **unpack side done and verified; pack side still
   blocked.** `unpack_tie_class` now converts the `ColladaScene` produced by
   `recover_tie_class()` into a `GLTF::Mesh` via `native_mesh_to_gltf_mesh()`
   and writes it out as `mesh.glb` (matching the `editor_mesh` field, same
   as before) instead of writing a `mesh.dae` via `write_collada()`. Found
   along the way that `native_mesh_to_gltf_mesh()` only ever propagated the
   `POSITION` attribute -- harmless for collision (no tex coords/normals),
   but would have dropped tie UVs silently, so fixed it to also propagate
   `NORMAL`/`COLOR_0`/`TEXCOORD_0` based on the source mesh's flags.
   
   This surfaced a second, previously-hidden bug: `load_tie_editor_class` in
   `editor/level.cpp` (one of the "two `editor/level.cpp` call sites"
   flagged in item 5 below) still assumed `editor_mesh`'s file was COLLADA
   XML and called `read_collada()` on it directly, so the editor crashed
   with `[collada.cpp:96] error: expected <` the first time it tried to load
   a level after this change (the raw glTF binary doesn't start with `<`).
   Fixed by rewriting `load_tie_editor_class` to parse the `.glb` with
   `GLTF::read_glb`/`GLTF::lookup_node`, remap materials with
   `GLTF::map_gltf_materials_to_wrench_materials`, and upload with
   `upload_gltf_mesh`/`upload_materials`, mirroring `load_moby_editor_class`/
   `load_shrub_editor_class`. Still populates `EditorClass::mesh` (the
   native CPU-side copy, via `gltf_mesh_to_native_mesh`) since
   `editor/gui/collision_fixer.cpp`'s `generate_bounding_box()` depends on
   it for the instanced-collision-fixer tool -- moby/shrub don't set that
   field since nothing else needs it for them.
   
   Note `pack_tie_class` still calls `verify_not_reached_fatal("Not yet
   implemented.")` regardless of any of this, since `write_tie_class()` in
   `engine/tie.cpp` is itself an empty stub -- that's Phase 2 item 7's job,
   not this item's.
   
   Built and functionally tested by the user: extracted the ISO, opened and
   checked many levels in the editor (tie meshes/textures render correctly),
   and repacked the ISO successfully.
4. Migrate **tfrags** off COLLADA the same way (`tfrags_asset.cpp`) — not
   started. Do this *together with* the tface source-representation design
   (Phase 2, item 6), since the glTF file is where that representation
   will live (e.g. mesh extras/custom attributes). Note: the collision
   migration's quad→triangle tradeoff (glTF has no native quad primitive,
   so quads become two triangles and don't reconstruct on the way back) is
   a property of glTF itself, not something specific to collision — it'll
   recur here and for ties, so it's worth deciding once whether that's
   acceptable rather than re-litigating it per asset type. For collision
   specifically this isn't just a file-size concern: per-octant face count
   roughly doubles for quad-heavy geometry, which feeds a hard `verify()`
   that aborts the whole build rather than degrading gracefully (see
   `TODO.md` for the specifics and why it's unlikely in practice). Worth
   checking whether tfrags/ties have an analogous fixed-size packed field
   that the same doubling could stress, before assuming this tradeoff is
   free elsewhere too.
5. Once ties, tfrags, and the two `editor/level.cpp` call sites, and the
   two `wrenchbuild/main.cpp` call sites no longer call `read_collada`/
   `write_collada`, delete `core/collada.cpp/h` entirely (including the
   `to_materials()`/`ColladaMaterial`/`ColladaScene` types added/used along
   the way — by this point ties/tfrags should no longer need them either).
   This retires the whole bug class you just hit, rather than patching
   instances of it. Not started.

*Rationale:* writing new tfrag/tie **build** logic (Phase 2) against a
format you're about to delete is wasted work. Do the migration first so
the rebuild code is written once, against glTF.

### Phase 2 — Mesh/model packing completeness (the core value) — **NOT STARTED**
6. **Tfrags**: decide the source representation for tfaces (coupled with
   step 4 above — this is really one workstream), then implement full
   tfrag building. This is flagged as the highest-impact item since most
   core gameplay geometry can't be rebuilt from scratch without it.
7. **Ties**: apply the same tface-adjacent solution once tfrags prove it
   out — TODO.md itself says ties have "similar issues as the tfrag
   renderer," so solving tfrags first de-risks ties.
8. **Moby**: split matrix allocation/scheduling out of read/write, wire
    in the existing tristrip algorithm for new submeshes. This has *no*
    dependency on 6/7 (moby's already fully on glTF since v0.6) — good
    candidate to parallelize with a second contributor while tfrag/tie
    work is ongoing.

### Phase 3 — Asset system infrastructure (interleave early, don't defer) — **NOT STARTED**
9. Do the **full** `std::filesystem::path` cleanup in `src/assetmgr` —
    2 of ~94 call sites are already fixed (`enumerate_source_files` in
    `asset.cpp`/`zipped_asset_bank.cpp`, one of which was a confirmed real
    Windows bug), the rest remain. Do this *before* persisting the asset
    graph to disk (next item) — a persisted graph format baked on top of
    inconsistent path handling just moves the bug into serialized data.
10. **Incremental builds / persist the asset graph.** This is the
    highest-leverage item for contributor experience per your own
    framing — start it early and run it in parallel with Phase 1/2 rather
    than waiting, since faster rebuilds pay back their cost across all the
    tfrag/tie/moby iteration that follows.
11. **Multithreaded builds**, after incremental caching design is settled
    (parallelizing work that caching is about to make unnecessary is
    wasted design effort). Extend the existing `thread_count` groundwork
    in `compression.cpp` out to the wider build.

### Phase 4 — Independent tracks (parallelizable any time after Phase 0) — **NOT STARTED**
12. **Texture pipeline**: convert "automatic paletted-color conversion,
    palette merging, better mipmaps, fix glitchiness" into concrete,
    reproducible bug reports first — it's explicitly vague — then
    prioritize individually. No dependency on mesh/model work.
13. **Audio pipeline**: VAG support (partial groundwork already exists in
    `vag.cpp`) then 989snd sample/grain-script support. Fully independent
    subsystem — safe to run in parallel with anything above.

### Phase 5 — Editor/GUI polish (last, on purpose) — **NOT STARTED**
14. Widget 3D format, HUD banks, instance add/remove tooling, and
    especially the **schema-driven GUI for editing asset files** — the
    last one specifically wants a stable, persisted asset graph
    representation (Phase 3, item 10) to be schema-driven against, so it
    naturally comes after asset-system infra lands. You already correctly
    flagged this whole phase as speculative/nice-to-have relative to the
    packing-completeness work.

## One-paragraph summary of the critical path

Land the in-flight WIP (done) → generalize the collision glTF helpers
(done and verified) and finish migrating ties/tfrags off COLLADA
(deleting `collada.cpp` once done) → use that to unblock full tfrag
rebuild (the actual bottleneck) → apply the same fix to ties → moby
matrix/tristrip work can run in parallel the whole time. Independently,
start the full `fs::path` cleanup and incremental-build work early since
they're pure force-multipliers for every phase after them. Texture and
audio work are self-contained side quests. Editor/GUI work comes last
because the schema-driven parts want the asset-graph work to exist first.
