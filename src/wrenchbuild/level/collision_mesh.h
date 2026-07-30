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

// Collision-specific glue between the native (ColladaScene-style) Mesh
// representation used by engine/collision.cpp and the .glb-format
// CollisionAsset. Used both by the asset (un)packer (wrenchbuild) and by
// tools that need to load or author collision data directly, such as the
// level editor and the instanced collision recovery tool (wrencheditor) --
// hence being kept in a file with minimal dependencies, separate from
// collision_asset.cpp, so it can be compiled into both executables without
// dragging in the asset (un)packer machinery.
//
// The generic Mesh <-> GLTF::Mesh conversion helpers that used to live here
// (native_mesh_to_gltf_mesh/gltf_mesh_to_native_mesh) have moved to
// core/gltf.h, since they have no collision-specific logic and other asset
// types migrating off COLLADA (ties, tfrags) will want to reuse them too.

#ifndef WRENCHBUILD_LEVEL_COLLISION_MESH_H
#define WRENCHBUILD_LEVEL_COLLISION_MESH_H

#include <core/gltf.h>
#include <assetmgr/asset_types.h>

// Reads a mesh (main mesh or hero group) from a .glb-format CollisionAsset,
// remapping its per-primitive materials to collision_id values using the
// asset's CollisionMaterialAsset children, and appends it to dest (having
// first applied matrix to every vertex).
void append_collision(Mesh& dest, const CollisionAsset& src, const glm::mat4& matrix);

#endif
