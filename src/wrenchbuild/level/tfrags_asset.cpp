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

#include "tfrags_asset.h"

#include <core/gltf.h>
#include <toolwads/wads.h>
#include <assetmgr/material_asset.h>
#include <engine/tfrag_high.h>

static void unpack_tfrags(TfragsAsset& dest, InputStream& src, BuildConfig config, const char* hint);
static void pack_tfrags_simple(OutputStream& dest, const TfragsAsset& src, BuildConfig config, const char* hint);
static bool test_tfrags(std::vector<u8>& src, AssetType type, BuildConfig config, const char* hint, AssetTestMode mode);

on_load(Tfrags, []() {
	TfragsAsset::funcs.unpack_rac1 = wrap_hint_unpacker_func<TfragsAsset>(unpack_tfrags);
	TfragsAsset::funcs.unpack_rac2 = wrap_hint_unpacker_func<TfragsAsset>(unpack_tfrags);
	TfragsAsset::funcs.unpack_rac3 = wrap_hint_unpacker_func<TfragsAsset>(unpack_tfrags);
	TfragsAsset::funcs.unpack_dl = wrap_hint_unpacker_func<TfragsAsset>(unpack_tfrags);
	
	TfragsAsset::funcs.pack_rac1 = wrap_hint_packer_func<TfragsAsset>(pack_tfrags_simple);
	TfragsAsset::funcs.pack_rac2 = wrap_hint_packer_func<TfragsAsset>(pack_tfrags_simple);
	TfragsAsset::funcs.pack_rac3 = wrap_hint_packer_func<TfragsAsset>(pack_tfrags_simple);
	TfragsAsset::funcs.pack_dl = wrap_hint_packer_func<TfragsAsset>(pack_tfrags_simple);
	
	TfragsCoreAsset::funcs.test_rac = new AssetTestFunc(test_tfrags);
	TfragsCoreAsset::funcs.test_gc  = new AssetTestFunc(test_tfrags);
	TfragsCoreAsset::funcs.test_uya = new AssetTestFunc(test_tfrags);
	TfragsCoreAsset::funcs.test_dl  = new AssetTestFunc(test_tfrags);
})

static void unpack_tfrags(TfragsAsset& dest, InputStream& src, BuildConfig config, const char* hint)
{
	if (g_asset_unpacker.dump_binaries) {
		if (!dest.has_core()) {
			unpack_asset_impl(dest.core<TfragsCoreAsset>(), src, nullptr, config);
		}
		return;
	}
	
	unpack_asset_impl(dest.core<BinaryAsset>(), src, nullptr, config);
	
	std::vector<u8> buffer = src.read_multiple<u8>(0, src.size());
	Tfrags tfrags = read_tfrags(buffer, config.game());
	RecoveredScene scene = recover_tfrags(tfrags, TFRAG_NO_FLAGS);
	if (scene.meshes.empty()) {
		// recover_tfrags() only emits a mesh if tfrags.fragments is non-empty;
		// with no fragments there's nothing to write out, so just leave
		// editor_mesh unset (mirrors how the editor/packer already treat a
		// TfragsAsset with no editor_mesh -- see Level::read/pack_tfrags).
		return;
	}
	
	// Tfrags were migrated off COLLADA (see wrench-roadmap.md, Phase 1 item
	// 4), mirroring the tie migration in tie_class.cpp: recover_tfrags() still
	// returns a RecoveredScene (it's shared groundwork for the eventual tfrag
	// build/pack side too), but here we convert its single mesh into a glTF
	// one using the same shared conversion helpers the collision/tie code
	// uses (core/gltf.h), and write mesh.glb instead of mesh.dae via COLLADA
	// XML.
	auto [gltf, gltf_scene] = GLTF::create_default_scene(get_versioned_application_name("Wrench Build Tool"));
	
	std::vector<Material> gltf_materials = to_materials(scene.materials);
	GLTF::Mesh converted_mesh = native_mesh_to_gltf_mesh(gltf, scene.meshes.at(0), gltf_materials);
	std::string mesh_name = converted_mesh.name.has_value() ? *converted_mesh.name : "mesh";
	
	gltf_scene->nodes.emplace_back((s32) gltf.nodes.size());
	GLTF::Node& node = gltf.nodes.emplace_back();
	node.name = mesh_name;
	node.mesh = (s32) gltf.meshes.size();
	gltf.meshes.emplace_back(std::move(converted_mesh));
	
	std::vector<u8> glb = GLTF::write_glb(gltf);
	auto [stream, ref] = dest.file().open_binary_file_for_writing("mesh.glb");
	stream->write_v(glb);
	
	MeshAsset& editor_mesh = dest.editor_mesh();
	editor_mesh.set_name(mesh_name);
	editor_mesh.set_src(ref);
}

static void pack_tfrags_simple(
	OutputStream& dest, const TfragsAsset& src, BuildConfig config, const char* hint)
{
	pack_tfrags(dest, nullptr, src, nullptr, config);
}

ByteRange pack_tfrags(
	OutputStream& bin_dest,
	std::vector<Mesh>* tfrags_dest,
	const TfragsAsset& src,
	u16* next_occlusion_index,
	BuildConfig config)
{
	if (g_asset_packer_dry_run) {
		return {0, 0};
	}
	
	s64 ofs = bin_dest.tell();
	
	if (src.get_core().logical_type() == BinaryAsset::ASSET_TYPE) {
		// Since the tfrags were provided as a binary file, we need to unpack
		// that file so we can generate occlusion data for the level.
		std::vector<u8> input_buffer;
		MemoryOutputStream input_stream(input_buffer);
		pack_asset_impl(input_stream, nullptr, nullptr, src.get_core(), config, nullptr);
		Tfrags tfrags = read_tfrags(input_buffer, config.game());
		
		RecoveredScene scene = recover_tfrags(tfrags, TFRAG_SEPARATE_MESHES);
		if (tfrags_dest) {
			*tfrags_dest = std::move(scene.meshes);
		}
		
		// Rewrite all the occlusion indices so they're equal to what the
		// occlusion code expects.
		if (next_occlusion_index) {
			for (Tfrag& tfrag : tfrags.fragments) {
				tfrag.occl_index = (*next_occlusion_index)++;
			}
		}
		
		allocate_tfrags_vu(tfrags);
		std::vector<u8> output_buffer;
		write_tfrags(output_buffer, tfrags, config.game());
		bin_dest.write_v(output_buffer);
	} else {
		verify_not_reached_fatal("Not yet implemented.");
	}
	
	s64 end_ofs = bin_dest.tell();
	return {(s32) ofs, (s32) (end_ofs - ofs)};
}

static bool test_tfrags(
	std::vector<u8>& src,
	AssetType type,
	BuildConfig config,
	const char* hint,
	AssetTestMode mode)
{
	Tfrags tfrags_original = read_tfrags(src, config.game());
	
	Tfrags tfrags_reallocated = tfrags_original;
	allocate_tfrags_vu(tfrags_reallocated);
	
	// Test that the data is being allocated in VU memory correctly. We do this
	// sepearately so that more helpful error messages can be generated.
	for (s32 i = 0; i < (s32) tfrags_original.fragments.size(); i++) {
		bool matching_allocation = false;
		#define COMPARE(field) \
			if (tfrags_original.fragments[i].memory_map.field != tfrags_reallocated.fragments[i].memory_map.field) { \
				fprintf(stderr, "Field " #field " for tfrag %d doesn't match. Original is 0x%x, reallocated is 0x%x.\n", \
					i, tfrags_original.fragments[i].memory_map.field, tfrags_reallocated.fragments[i].memory_map.field); \
				matching_allocation = true; \
			}
		COMPARE(header_common_addr);
		COMPARE(ad_gifs_common_addr);
		COMPARE(positions_common_addr);
		COMPARE(positions_lod_01_addr);
		COMPARE(positions_lod_0_addr);
		COMPARE(vertex_info_common_addr);
		COMPARE(vertex_info_lod_01_addr);
		COMPARE(vertex_info_lod_0_addr);
		COMPARE(parent_indices_lod_01_addr);
		COMPARE(parent_indices_lod_0_addr);
		COMPARE(indices_addr);
		COMPARE(strips_addr);
		if (matching_allocation) {
			return false;
		}
	}
	
	std::vector<u8> dest;
	write_tfrags(dest, tfrags_reallocated, config.game());
	
	// Padding is inserted so that the tfrags block for each chunk is the same size.
	strip_trailing_padding_from_lhs(src, dest, -1);
	
	return diff_buffers(src, dest, 0, DIFF_REST_OF_BUFFER, mode == AssetTestMode::PRINT_DIFF_ON_FAIL);
}
