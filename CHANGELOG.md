# Changelog

> **Note (LLM-edited, unverified):** The "Unreleased" section below was
> drafted by an LLM (Claude) reading a working-tree diff, not written by a
> human maintainer after review. The changes are now committed and the
> build succeeds, but have not been reviewed, tested against edge cases,
> or verified for architectural correctness by a human. Multiple separate
> LLM sessions have been working on this codebase without cross-checking
> each other. Revise, expand, or remove these entries once someone has
> actually confirmed each change is correct -- don't treat this section as
> a finished, trustworthy changelog yet. See the "Recently committed
> changes" section at the top of TODO.md for more technical detail on each
> item below.

## Unreleased

- Use glTF (.glb) to store collision models instead of COLLADA.
  Note: quad collision faces are now always split into two triangles when
  converted to glTF (glTF has no native quad primitive), and are not
  reconstructed as quads on the way back. This does not change in-game
  collision behaviour, and per-octant vertex/quad limits in the packer
  aren't affected, but it does mean more, smaller faces per octant for
  quad-heavy meshes -- larger files, and in an extreme, unlikely case
  (very roughly 32,768+ quad faces in a single octant before the split)
  it could hit the packer's hard per-octant face-count limit and abort a
  build outright rather than degrade gracefully. See TODO.md for the full
  breakdown.
- Fixed source-file path matching (`enumerate_source_files`) on Windows
  for in-memory asset banks, which previously compared paths without
  normalising separators and so would not have matched.
- Fixed a potential buffer overflow in `OutBuffer::writesf`/`writelf`
  (`core/buffer.cpp`): formatted strings longer than a fixed 16KB stack
  buffer could silently overrun it. Replaced with a two-pass `vsnprintf`
  sizing approach.
- Added a best-effort crash handler (`core/util/crash_handler.cpp`) that
  prints a signal name, error context, and a demangled backtrace on
  SIGSEGV/SIGABRT/SIGFPE/SIGILL before re-raising. Not fully
  async-signal-safe by design (diagnostic aid, not a hardened handler).
- Default `CMAKE_BUILD_TYPE` to `RelWithDebInfo` when unspecified, and
  pass `-rdynamic` on Linux/macOS so the crash handler above can produce
  symbolised backtraces.
- Generalised the collision-specific glTF mesh conversion helpers
  (`native_mesh_to_gltf_mesh`/`gltf_mesh_to_native_mesh`) into shared code
  in `core/gltf.h`/`.cpp`, so ties and tfrags will be able to reuse them
  once they're migrated off COLLADA too. No user-visible behaviour change;
  verified by building and testing (asset extraction, level editor, ISO
  repacking).
- Use glTF (.glb) to store tie models instead of COLLADA, for the unpack
  side and the level editor (building/repacking ties from source is not
  yet implemented, unchanged from before). Fixed the shared glTF mesh
  helper above to also carry over normals/vertex colours/texture
  coordinates, not just vertex positions, which would otherwise have been
  silently dropped for tie meshes (harmless for collision meshes, which
  don't have any of those). Verified by building and testing (asset
  extraction, many levels checked in the editor, ISO repacking).
- Added a per-surface-id visibility filter for collision in the editor,
  plus a "Collision legend" window and a View > Visibility > Collision ids
  menu to drive it, replacing the previous all-or-nothing collision
  toggle for this use case (the toggle itself is unchanged). Verified
  working by a human tester.

## v0.6

- Use glTF (.glb) to store moby models instead of COLLADA.

## v0.5

- Rewrote the save editor so that it now works for all the games.
- Use glTF (.glb) to store the sky models instead of COLLADA.
- Use glTF (.glb) to store the shrub models instead of COLLADA.
- Sky models are now split up into clusters automatically, making custom skies much more doable.
- Added a collision fixer tool in the editor to recover instanced collision for ties and shrubs semi-automatically.
- Added proper 3D transformation gizmos in the editor for translation, rotation and scaling.
- The SYSTEM.CNF file is now written out onto the correct sector for R&C1.

## v0.4

New features:
- Added a new instance system and format for storing and editing instances/entities/game objects.
- Added a special-purpose C++ parser for defining pvar data types.
- Added an overlay asset bank, mounted between the game and mods, for built-in pvar data types.
- Added an experimental memory card editor (mostly Deadlocked only).
- Occlusion data for a level can now be rebuilt on demand from the level editor.
- Packed ELF files and the level code overlays can now be unpacked to a regular ELF file.
- Tfrag meshes are now unpacked with quad faces (internally tfaces are recovered, but I'm yet to develop a solution to properly export that information).
- Overhauled the documentation.

Changes:
- Tfrag and collision assets are no longer duplicated in the unpacked asset files (they are now only stored inside Chunk assets, even in the case of R&C1).
- The underlay is no longer stored in its own zip file. This should make it more convenient to modify.

Bug fixes:
- Fixed some issues with unpacking certain builds.
- Improved handling of unicode in file paths on Windows.
- A lot more.

## v0.3

New features:
- Tfrag meshes can now be unpacked and are displayed in the editor.
- Tie meshes can now be unpacked and are displayed in the editor.
- Mods can now be loaded from zip files.
- An underlay asset bank is now included, which is used by the unpacker to give the game's files and folders more human-readable names on disk.
- Moby classes stored in missions are now unpacked for Deadlocked.
- Tooltips have been added to the launcher to fix some usability issues.

Changes:
- Merged the LevelDataWad and LevelCore asset types into the LevelWad asset type.

Bug fixes:
- Fixed a crash when packing shrub meshes containing an edge connecting 3 or more faces.
- Wrench should now function correctly when it is placed in a folder with a path that contains spaces on Windows.

## v0.2

New features:
- Shrub models can now be unpacked, repacked and are now displayed in the editor.
- Sky models can now be unpacked and repacked.
- A command line option has been added to unpack loose built collision files.
- Unpacking and repacking of stashed textures (textures that are always present in GS memory) is now supported.
- Individual moby bangles (small supplementary models that can be turned on or off like destructible parts of an object) are now unpacked separately.

Bug fixes:
- Repacked builds of UYA and Deadlocked will no longer all crash on level load.
- An issue where the gameplay file was written out incorrectly for games other than R&C2 has been fixed.
- The region command line argument of wrenchbuild is now parsed correctly.
- The SYSTEM.CNF file is now written out correctly.
- Texture swizzling is now performed in more places for Deadlocked.
- The "New Mod" screen will now populate more fields of the generated gameinfo.txt file correctly.
- Improved the COLLADA parser.

## v0.1

- The first proper prerelease!
