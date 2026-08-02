/*
	wrench - A set of modding tools for the Ratchet & Clank PS2 games.
	Copyright (C) 2019-2026 chaoticgd

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

#ifndef CORE_MESH_SCENE_H
#define CORE_MESH_SCENE_H

#include <core/mesh.h>
#include <core/material.h>

// The fields of a material that need to be cross-referenced with material
// assets using the name field, for meshes recovered from binary game data
// (ties, tfrags, moby, collision, etc). This type (and RecoveredScene below)
// used to double as the representation written directly to COLLADA files via
// core/collada.h's write_collada() -- hence surviving fields like
// collision_id -- before ties/tfrags/collision/moby moved to writing glTF
// instead (see wrench-roadmap.md). core/collada.h has since been deleted, as
// nothing calls read_collada()/write_collada() any more; these types were
// moved out to here (and renamed, since they're no longer COLLADA-specific)
// because they're still used throughout the codebase as the shared in-memory
// representation mesh recovery code produces before conversion to glTF.
struct RecoveredMaterial
{
	std::string name;
	MaterialSurface surface;
	s32 collision_id = -1; // Only used by the collision code.
	RecoveredMaterial() {}
	RecoveredMaterial(const Material& material)
		: name(material.name)
		, surface(material.surface) {}
	Material to_material() {
		Material material;
		material.name = name;
		material.surface = surface;
		return material;
	}
};

struct Joint
{
	s32 parent = -1;
	s32 first_child = -1;
	s32 left_sibling = -1;
	s32 right_sibling = -1;
	glm::mat4 inverse_bind_matrix;
	glm::vec3 tip;
};

// The shared in-memory representation used across the codebase for meshes
// recovered from binary game data, e.g. by recover_tie_class(),
// recover_tfrags(), recover_collision(), recover_moby_class(), or
// build_instanced_collision() (the editor's instanced collision fixer tool).
// Consumers either convert this to glTF for writing to disk (see
// core/gltf.h's native_mesh_to_gltf_mesh()), or, for asset types with the
// pack direction implemented (currently just moby), pass it directly to a
// build_X_class() function to be packed back into binary game data.
struct RecoveredScene
{
	mutable std::vector<std::string> texture_paths;
	std::vector<RecoveredMaterial> materials;
	std::vector<Mesh> meshes;
	std::vector<Joint> joints;
	
	Mesh* find_mesh(const std::string& name);
};

// Rewrite SubMesh::material indices so they index into the passed materials array.
void map_lhs_material_indices_to_rhs_list(RecoveredScene& scene, const std::vector<Material>& materials);

// Strip the collision_id field off a list of RecoveredMaterial objects, e.g.
// for passing to code that only deals in plain Material objects, such as the
// shared glTF mesh converters in core/gltf.h.
std::vector<Material> to_materials(const std::vector<RecoveredMaterial>& materials);

s32 add_joint(std::vector<Joint>& joints, Joint joint, s32 parent);

#endif
