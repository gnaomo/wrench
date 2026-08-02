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

#include "mesh_scene.h"

Mesh* RecoveredScene::find_mesh(const std::string& name)
{
	for (Mesh& mesh : meshes) {
		if (mesh.name == name) {
			return &mesh;
		}
	}
	return nullptr;
}

void map_lhs_material_indices_to_rhs_list(RecoveredScene& scene, const std::vector<Material>& materials)
{
	// Generate mapping.
	std::vector<s32> mapping(scene.materials.size(), -1);
	for (size_t i = 0; i < scene.materials.size(); i++) {
		for (size_t j = 0; j < materials.size(); j++) {
			if (materials[j].name == scene.materials[i].name) {
				mapping[i] = j;
			}
		}
	}
	
	// Apply mapping.
	for (Mesh& mesh : scene.meshes) {
		for (SubMesh& submesh : mesh.submeshes) {
			verify(mapping[submesh.material] > -1,
				"Material '%s' has no corresponding asset defined for it.",
				scene.materials.at(submesh.material).name.c_str());
			submesh.material = mapping[submesh.material];
		}
	}
}

std::vector<Material> to_materials(const std::vector<RecoveredMaterial>& materials)
{
	std::vector<Material> result;
	result.reserve(materials.size());
	for (const RecoveredMaterial& material : materials) {
		Material& dest = result.emplace_back();
		dest.name = material.name;
		dest.surface = material.surface;
	}
	return result;
}

s32 add_joint(std::vector<Joint>& joints, Joint joint, s32 parent)
{
	s32 index = (s32) joints.size();
	joint.parent = parent;
	if (parent != -1) {
		s32* next = &joints[parent].first_child;
		while (*next != -1) {
			joint.left_sibling = *next;
			next = &joints[*next].right_sibling;
		}
		*next = index;
	}
	joints.push_back(joint);
	return index;
}
