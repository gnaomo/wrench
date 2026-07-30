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

#include "collision_asset.h"

#include <core/gltf.h>
#include <toolwads/wads.h>
#include <assetmgr/asset_util.h>
#include <wrenchbuild/level/collision_mesh.h>

static void unpack_collision_asset(CollisionAsset& dest, InputStream& src, BuildConfig config);
static void pack_collision_asset(OutputStream& dest, const CollisionAsset& src, BuildConfig config);

on_load(Collision, []() {
	CollisionAsset::funcs.unpack_rac1 = wrap_unpacker_func<CollisionAsset>(unpack_collision_asset);
	CollisionAsset::funcs.unpack_rac2 = wrap_unpacker_func<CollisionAsset>(unpack_collision_asset);
	CollisionAsset::funcs.unpack_rac3 = wrap_unpacker_func<CollisionAsset>(unpack_collision_asset);
	CollisionAsset::funcs.unpack_dl = wrap_unpacker_func<CollisionAsset>(unpack_collision_asset);
	
	CollisionAsset::funcs.pack_rac1 = wrap_packer_func<CollisionAsset>(pack_collision_asset);
	CollisionAsset::funcs.pack_rac2 = wrap_packer_func<CollisionAsset>(pack_collision_asset);
	CollisionAsset::funcs.pack_rac3 = wrap_packer_func<CollisionAsset>(pack_collision_asset);
	CollisionAsset::funcs.pack_dl = wrap_packer_func<CollisionAsset>(pack_collision_asset);
})

static void unpack_collision_asset(CollisionAsset& dest, InputStream& src, BuildConfig config)
{
	std::vector<u8> bytes = src.read_multiple<u8>(0, src.size());
	CollisionOutput output = read_collision(bytes);
	
	auto [gltf, scene] = GLTF::create_default_scene(get_versioned_application_name("Wrench Build Tool"));
	
	// The main collision mesh is always output.scene.meshes[0]; every hero
	// group mesh that follows it gets its own node in the same file, mirroring
	// how sky shells share a single .glb.
	std::vector<Material> gltf_materials = to_materials(output.scene.materials);
	std::vector<GLTF::Mesh> converted_meshes;
	for (const Mesh& native_mesh : output.scene.meshes) {
		converted_meshes.emplace_back(native_mesh_to_gltf_mesh(gltf, native_mesh, gltf_materials));
	}
	for (size_t i = 0; i < converted_meshes.size(); i++) {
		scene->nodes.emplace_back((s32) gltf.nodes.size());
		GLTF::Node& node = gltf.nodes.emplace_back();
		node.name = converted_meshes[i].name;
		node.mesh = (s32) gltf.meshes.size();
		gltf.meshes.emplace_back(std::move(converted_meshes[i]));
	}
	
	std::vector<u8> glb = GLTF::write_glb(gltf);
	auto [stream, ref] = dest.file().open_binary_file_for_writing("mesh.glb");
	stream->write_v(glb);
	
	MeshAsset& mesh = dest.mesh<MeshAsset>();
	mesh.set_src(ref);
	mesh.set_name(output.scene.meshes.at(0).name);
	
	CollectionAsset& hero_groups = dest.hero_groups();
	s32 i = 0;
	for (const std::string& hero_group_mesh : output.hero_group_meshes) {
		MeshAsset& group_mesh = hero_groups.child<MeshAsset>(stringf("%d", i++).c_str());
		group_mesh.set_src(ref);
		group_mesh.set_name(hero_group_mesh);
	}
	
	CollectionAsset& materials = dest.materials();
	for (ColladaMaterial& material : output.scene.materials) {
		CollisionMaterialAsset& asset = materials.child<CollisionMaterialAsset>(material.name.c_str());
		asset.set_name(material.name);
		asset.set_id(material.collision_id);
	}
}

static void pack_collision_asset(OutputStream& dest, const CollisionAsset& src, BuildConfig config)
{
	if (g_asset_packer_dry_run) {
		return;
	}
	
	pack_level_collision(dest, src, nullptr, nullptr, -1);
}

void pack_level_collision(
	OutputStream& dest,
	const CollisionAsset& src,
	const LevelWadAsset* level_wad,
	const Gameplay* gameplay,
	s32 chunk)
{
	ColladaScene scene;
	
	for (s32 i = 0; i < 256; i++) {
		ColladaMaterial& material = scene.materials.emplace_back();
		material.collision_id = i;
	}
	
	Mesh& mesh = scene.meshes.emplace_back();
	mesh.name = "combined";
	
	append_collision(mesh, src, glm::mat4(1.f));
	
	if (level_wad && gameplay && gameplay->level_settings.has_value()) {
		const CollectionAsset& moby_classes = level_wad->get_moby_classes();
		for (const MobyInstance& inst : opt_iterator(gameplay->moby_instances)) {
			if (inst.has_static_collision && chunk_index_from_position(inst.transform().pos(), *gameplay->level_settings) == chunk) {
				const MobyClassAsset& class_asset = moby_classes.get_child(inst.o_class()).as<MobyClassAsset>();
				append_collision(mesh, class_asset.get_static_collision(), inst.transform().matrix());
			}
		}
		
		const CollectionAsset& tie_classes = level_wad->get_tie_classes();
		for (const TieInstance& inst : opt_iterator(gameplay->tie_instances)) {
			if (inst.has_static_collision && chunk_index_from_position(inst.transform().pos(), *gameplay->level_settings) == chunk) {
				const TieClassAsset& class_asset = tie_classes.get_child(inst.o_class()).as<TieClassAsset>();
				append_collision(mesh, class_asset.get_static_collision(), inst.transform().matrix());
			}
		}
		
		const CollectionAsset& shrub_classes = level_wad->get_shrub_classes();
		for (const ShrubInstance& inst : opt_iterator(gameplay->shrub_instances)) {
			if (inst.has_static_collision && chunk_index_from_position(inst.transform().pos(), *gameplay->level_settings) == chunk) {
				const ShrubClassAsset& class_asset = shrub_classes.get_child(inst.o_class()).as<ShrubClassAsset>();
				append_collision(mesh, class_asset.get_static_collision(), inst.transform().matrix());
			}
		}
	}
	
	CollisionInput input;
	input.main_scene = &scene;
	input.main_mesh = mesh.name;
	
	std::vector<FileReference> hero_group_refs;
	std::vector<std::string> hero_group_names;
	src.get_hero_groups().for_each_logical_child_of_type<MeshAsset>([&](const MeshAsset& mesh) {
		hero_group_refs.emplace_back(mesh.src());
		hero_group_names.emplace_back(mesh.name());
	});
	
	std::vector<std::unique_ptr<GLTF::ModelFile>> hero_group_owners;
	std::vector<GLTF::ModelFile*> hero_group_gltfs = read_glb_files(hero_group_owners, hero_group_refs);
	
	// Keep the converted native meshes alive for as long as input.hero_groups
	// (which stores plain pointers) is in use.
	std::vector<Mesh> hero_group_meshes;
	hero_group_meshes.reserve(hero_group_gltfs.size());
	for (size_t i = 0; i < hero_group_gltfs.size(); i++) {
		GLTF::Node* node = GLTF::lookup_node(*hero_group_gltfs[i], hero_group_names[i].c_str());
		verify(node, "No node '%s' for hero collision group.", hero_group_names[i].c_str());
		verify(node->mesh.has_value(), "Node '%s' has no mesh for hero collision group.", hero_group_names[i].c_str());
		hero_group_meshes.emplace_back(gltf_mesh_to_native_mesh(hero_group_gltfs[i]->meshes.at(*node->mesh)));
	}
	for (const Mesh& hero_mesh : hero_group_meshes) {
		input.hero_groups.emplace_back(&hero_mesh);
	}
	
	std::vector<u8> bytes;
	write_collision(OutBuffer(bytes), input);
	dest.write_v(bytes);
}

