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

// See the comment on native_mesh_to_gltf_mesh's declaration in
// collision_mesh.h -- glTF has no native quad primitive mode, so quads are
// split into two triangles here. This is a one-way simplification: once a
// collision mesh has been round-tripped through the new format, faces that
// used to be packed as quads in the octree will always be rebuilt as pairs of
// triangles instead. That only costs a little bit of storage efficiency (see
// reduce_quads_to_tris/optimise_collision in engine/collision.cpp) -- it does
// not change the collision behaviour in game, since the octree accepts either
// representation.
GLTF::Mesh native_mesh_to_gltf_mesh(
	GLTF::ModelFile& gltf, const Mesh& mesh, const std::vector<ColladaMaterial>& materials)
{
	GLTF::Mesh gltf_mesh;
	gltf_mesh.name = mesh.name;
	gltf_mesh.vertices = mesh.vertices;
	
	for (const SubMesh& submesh : mesh.submeshes) {
		if (submesh.faces.empty()) {
			continue;
		}
		
		const std::string& material_name = materials.at(submesh.material).name;
		
		s32 material_index = -1;
		for (size_t i = 0; i < gltf.materials.size(); i++) {
			if (gltf.materials[i].name == material_name) {
				material_index = (s32) i;
				break;
			}
		}
		if (material_index == -1) {
			material_index = (s32) gltf.materials.size();
			GLTF::Material& material = gltf.materials.emplace_back();
			material.name = material_name;
		}
		
		GLTF::MeshPrimitive& primitive = gltf_mesh.primitives.emplace_back();
		primitive.attributes_bitfield = GLTF::POSITION;
		primitive.mode = GLTF::TRIANGLES;
		primitive.material = material_index;
		
		for (const Face& face : submesh.faces) {
			primitive.indices.emplace_back(face.v0);
			primitive.indices.emplace_back(face.v1);
			primitive.indices.emplace_back(face.v2);
			if (face.is_quad()) {
				primitive.indices.emplace_back(face.v0);
				primitive.indices.emplace_back(face.v2);
				primitive.indices.emplace_back(face.v3);
			}
		}
	}
	
	return gltf_mesh;
}

// The inverse of native_mesh_to_gltf_mesh, minus material handling -- used for
// hero collision groups, where the material assigned to each face doesn't
// matter (see build_hero_collision_groups in engine/collision.cpp).
Mesh gltf_mesh_to_native_mesh(const GLTF::Mesh& mesh)
{
	Mesh native_mesh;
	native_mesh.name = mesh.name.has_value() ? *mesh.name : "";
	native_mesh.vertices = mesh.vertices;
	
	for (const GLTF::MeshPrimitive& primitive : mesh.primitives) {
		verify(!primitive.mode.has_value() || *primitive.mode == GLTF::TRIANGLES,
			"Hero collision group meshes must be triangulated.");
		verify(primitive.indices.size() % 3 == 0,
			"Hero collision group mesh has a non-triangular index count.");
		
		SubMesh& submesh = native_mesh.submeshes.emplace_back();
		for (size_t i = 0; i < primitive.indices.size(); i += 3) {
			submesh.faces.emplace_back(primitive.indices[i + 0], primitive.indices[i + 1], primitive.indices[i + 2]);
		}
	}
	
	return native_mesh;
}
