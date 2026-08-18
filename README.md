> ## ⚠️ Fork Notice: LLM-Generated Changes (branch: byte-to-byte-iso)
>
> **This branch works towards byte-for-byte accurate ISO packing: reconstructing the UDF filesystem structures Wrench packs into an ISO so the output matches a retail PS2 ISO exactly, not just a spec-compliant equivalent.**
>
> **Added / changed in this branch (relative to upstream master):**
> - `src/iso/udf_fsd_sectors.inl`, `src/iso/udf_vds_sectors.inl` — raw UDF filesystem/volume-descriptor sectors captured from a reference PS2 ISO, used as static byte arrays for exact reconstruction.
> - `src/iso/iso_packer.cpp`, `src/iso/iso_filesystem.{cpp,h}`, `src/iso/table_of_contents.{cpp,h}` — wired to reproduce the captured UDF sectors exactly during packing.
> - `src/core/util/crash_handler.{cpp,h}` — a best-effort crash handler with symbolised backtraces.
> - Small build-system/portability fixes: `CMakeLists.txt`, `cmake/wrench_compiler_flags.cmake`, `src/core/CMakeLists.txt`, `src/core/buffer.cpp`, `src/assetmgr/asset.cpp`, `src/wrenchbuild/main.cpp`.
>
> **⚠️ LLM disclosure:** All of the changes on this branch were written entirely by an LLM (AI coding assistant) and have **not been reviewed, audited, or verified by the repository owner**. Treat this code as untrusted / unreviewed. It is kept here purely as a personal backup of work-in-progress and is **not intended for upstream contribution or production use** — this fork does not send PRs back to [chaoticgd/wrench](https://github.com/chaoticgd/wrench).
>
> This branch is independent from and diverges at a different base commit than this fork's [`gltf-migration`](https://github.com/gnaomo/wrench/tree/gltf-migration) branch — they represent two separate lines of work, not sequential steps.
>
> Everything below this notice is the unmodified upstream Wrench README.

# Wrench Editor

A set of modding tools for the Ratchet & Clank PS2 games. Compatible with R&C1, R&C2, R&C3 and Deadlocked. Work in progress.

- [Releases](https://github.com/chaoticgd/wrench/releases)
- [Unstable Builds](https://github.com/chaoticgd/wrench/releases/tag/unstable)
- [Documentation](docs/README.md)

Most members of the modding community for these games use Discord. Alternatively, feel free to use the issue tracker to ask questions about Wrench or technical questions about the games.

Features currently include:
- Launcher
	- A user interface to manage mods.
	- Used to launch the level editor.
- Build Tool
	- Pack/unpack entire ISO files.
	- Pack/unpack gameplay instances as `.instances` files.
	- Unpack tfrag meshes as COLLADA files.
	- Unpack moby meshes as glTF files\*.
	- Unpack tie meshes as COLLADA files.
	- Pack/unpack collision meshes as COLLADA files.
	- **Updated in v0.5!** Pack/unpack shrub models as binary glTF files\*\*.
	- **Updated in v0.5!** Pack/unpack sky models as binary glTF files\*\*.
	- Pack/unpack textures as PNG files.
	- Pack/unpack packed executables and level overlays as ELF files.
	- Pack/unpack everything else as binary files.
- Level Editor
	- View unpacked levels.
	- Inspect objects and modify their attributes (including pvars).
	- **New in v0.5!** Recover instanced collision for ties and shrubs.
	- **New in v0.5!** Translate, rotate and scale objects using 3D transformation gizmos.
- Save Editor
	- **New in v0.5!** Works for all the games, rather than just Deadlocked.
- Asset System
	- A system to create, distribute and load mods.
	- Multiple mods can be loaded at a time.
- Instance System
	- A source format for storing object instances.
	- Use pvar data types from C++ header files (reflection).
- Visibility Tool
	- Rebuild occlusion data for levels on demand.
	- Implemented as an OpenGL renderer.

\* In version 0.5 or lower COLLADA was used instead.

\*\* In version 0.4 or lower COLLADA was used instead.

## Screenshots

![Launcher](docs/screenshots/launcher.png)
![Level Editor](docs/screenshots/editor.png)

## Building

### Linux

1.	Install the following dependencies and tools:
	- git
	- cmake
	- g++
	- zenity
	- extra-cmake-modules *
	- libwayland-dev *
	- libxkbcommon-dev *
	- wayland-protocols *
	- xorg-dev *
	
	\* Needed to build GLFW.

2.	cd into the directory above where you want Wrench to live e.g. `cd ~/programs`.

2.	Download the source code and additional dependencies using Git:
	> git clone --recursive https://github.com/chaoticgd/wrench

3.	cd into the newly created directory:
	> cd wrench

4.	Build it with cmake:
	> cmake . -B bin/ && cmake --build bin/ -j 8
	
	(in the above example 8 threads are used)

### Windows

1.	Install the following tools:
	- git
	- Visual Studio (with desktop C++/cmake support)

2.	Open a Visual Studio developer command prompt.

3.	cd into the directory above where you want Wrench to live e.g. `cd c:\programs`.

4.	Download the source code and dependencies using Git:
	> git clone --recursive https://github.com/chaoticgd/wrench

5.	cd into the newly created directory:
	> cd wrench

6.	Generate cmake files:
	> cmake . -B bin/
	
	This should generate `wrench.sln` along with a few `.vcxproj` files. 
	In case no such files are generated, you can explicitly specify usage of the Visual Studio generator by running the following command:
	> cmake . -G "Visual Studio X YYYY"
	
	where `X` is the Visual Studio version and `YYYY` is the Visual Studio year (example: `Visual Studio 16 2019`)
	A complete list can be obtained by running `cmake --help`.

7.	**(Option A)** Build the project from the command line:
	
	> cmake --build bin/ --config BUILD_TYPE
	
	where `BUILD_TYPE` is one of `Debug` (very slow - not recommended), `Release` (no symbols - not recommended), `RelWithDebInfo` (recommended) or `MinSizeRel`.
	
	**(Option B)** Build the project from Visual Studio:

	Open the newly generated `wrench.sln` in Visual Studio. In the Solution Explorer, right-click on `wrench` and click `Set as Startup Project`. You should now be able to build and debug wrench using the toolbar controls and all Visual Studio features.
