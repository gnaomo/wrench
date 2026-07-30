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

#include "collision_mesh.h"

#include <map>

void append_collision(Mesh& dest, const CollisionAsset& src, const glm::mat4& matrix)
{
	const MeshAsset& mesh_asset = src.get_mesh();
	std::unique_ptr<InputStream> stream = mesh_asset.src().open_binary_file_for_reading();
	GLTF::ModelFile gltf = GLTF::read_glb(stream->read_multiple<u8>(stream->size()));
	
	GLTF::Node* node = GLTF::lookup_node(gltf, mesh_asset.name().c_str());
	verify(node, "Cannot find node '%s' in collision model.", mesh_asset.name().c_str());
	verify(node->mesh.has_value(), "Node '%s' in collision model has no mesh.", mesh_asset.name().c_str());
	GLTF::Mesh& mesh = gltf.meshes.at(*node->mesh);
	
	// Build a name -> collision_id lookup table from the material assets. This
	// mirrors the id remapping that used to happen directly on ColladaMaterial
	// objects.
	std::map<std::string, s32> material_name_to_id;
	src.get_materials().for_each_logical_child_of_type<CollisionMaterialAsset>([&](const CollisionMaterialAsset& asset) {
		material_name_to_id[asset.name()] = asset.id();
	});
	
	s32 vertex_base = (s32) dest.vertices.size();
	for (const Vertex& vertex_src : mesh.vertices) {
		Vertex& vertex_dest = dest.vertices.emplace_back(vertex_src);
		vertex_dest.pos = matrix * glm::vec4(vertex_dest.pos, 1.f);
	}
	
	for (const GLTF::MeshPrimitive& primitive : mesh.primitives) {
		verify(!primitive.mode.has_value() || *primitive.mode == GLTF::TRIANGLES,
			"Collision meshes must be triangulated (mesh '%s').", mesh_asset.name().c_str());
		verify(primitive.material.has_value(), "Collision mesh primitive has no material.");
		
		const Opt<std::string>& material_name = gltf.materials.at(*primitive.material).name;
		verify(material_name.has_value(), "Collision material has no name.");
		
		auto id_iter = material_name_to_id.find(*material_name);
		verify(id_iter != material_name_to_id.end(), "Tried to reference collision material that doesn't exist.");
		
		SubMesh& submesh_dest = dest.submeshes.emplace_back();
		submesh_dest.material = id_iter->second;
		
		verify(primitive.indices.size() % 3 == 0,
			"Collision mesh primitive '%s' has a non-triangular index count.", mesh_asset.name().c_str());
		for (size_t i = 0; i < primitive.indices.size(); i += 3) {
			submesh_dest.faces.emplace_back(
				vertex_base + primitive.indices[i + 0],
				vertex_base + primitive.indices[i + 1],
				vertex_base + primitive.indices[i + 2]);
		}
	}
}
