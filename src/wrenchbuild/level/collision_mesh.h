/*
	wrench - A set of modding tools for the Ratchet & Clank PS2 games.
	Copyright (C) 2019-2023 chaoticgd

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

// Conversions between the native (ColladaScene-style) Mesh representation
// used by engine/collision.cpp and the GLTF::Mesh representation used to
// serialise collision assets to .glb files. These are used both by the asset
// (un)packer (wrenchbuild) and by tools that need to load or author
// collision data directly, such as the level editor and the instanced
// collision recovery tool (wrencheditor) -- hence being kept in a file with
// minimal dependencies, separate from collision_asset.cpp, so it can be
// compiled into both executables without dragging in the asset (un)packer
// machinery.

#ifndef WRENCHBUILD_LEVEL_COLLISION_MESH_H
#define WRENCHBUILD_LEVEL_COLLISION_MESH_H

#include <core/collada.h>
#include <core/gltf.h>
#include <assetmgr/asset_types.h>

// Reads a mesh (main mesh or hero group) from a .glb-format CollisionAsset,
// remapping its per-primitive materials to collision_id values using the
// asset's CollisionMaterialAsset children, and appends it to dest (having
// first applied matrix to every vertex).
void append_collision(Mesh& dest, const CollisionAsset& src, const glm::mat4& matrix);

// Converts a plain triangulated GLTF::Mesh (e.g. a hero collision group) into
// a native Mesh, without any material remapping.
Mesh gltf_mesh_to_native_mesh(const GLTF::Mesh& mesh);

// Converts a native (ColladaScene-style) collision mesh -- which may contain
// quads -- into a GLTF::Mesh for serialisation to a .glb file, triangulating
// any quads and adding/reusing materials in gltf as required.
GLTF::Mesh native_mesh_to_gltf_mesh(
	GLTF::ModelFile& gltf, const Mesh& mesh, const std::vector<ColladaMaterial>& materials);

#endif
