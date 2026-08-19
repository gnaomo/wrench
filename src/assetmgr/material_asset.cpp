/*
	wrench - A set of modding tools for the Ratchet & Clank PS2 games.
	Copyright (C) 2019-2022 chaoticgd

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

#include "material_asset.h"

MaterialSet read_material_assets(const CollectionAsset& src)
{
	std::vector<Material> materials;
	std::vector<FileReference> textures;
	std::map<std::pair<const AssetFile*, std::string>, s32> texture_indices;
	s32 next_texture_index = 0;
	src.for_each_logical_child_of_type<MaterialAsset>([&](const MaterialAsset& asset) {
		Material& material = materials.emplace_back();
		material.name = asset.name();
		if (asset.has_diffuse()) {
			const TextureAsset& diffuse = asset.get_diffuse();
			std::string diffuse_path = asset.get_diffuse().src().path.string();
			auto key = std::make_pair(&diffuse.file(), diffuse_path);
			auto iter = texture_indices.find(key);
			if (iter == texture_indices.end()) {
				texture_indices[key] = next_texture_index;
				material.surface = MaterialSurface(next_texture_index);
				next_texture_index++;
				textures.emplace_back(diffuse.file(), asset.get_diffuse().src().path);
			} else {
				material.surface = MaterialSurface(iter->second);
			}
		} else {
			material.surface = MaterialSurface(glm::vec4(1.f, 0.f, 1.f, 1.f));
		}
		if (asset.has_wrap_mode()) {
			std::vector<std::string> wrap_mode = asset.wrap_mode();
			if (wrap_mode.size() == 2) {
				if (wrap_mode[0] == "clamp") {
					material.wrap_mode_s = WrapMode::CLAMP;
				}
				if (wrap_mode[1] == "clamp") {
					material.wrap_mode_t = WrapMode::CLAMP;
				}
			}
		}
		if (asset.has_effect_mode()) {
			for(std::string effect : asset.effect_mode()) {
				if (effect == "none") {
					material.effect_mode = MaterialEffectMode::NONE;
				}
				if (effect == "chrome") {
					material.effect_mode = MaterialEffectMode::CHROME;
				}
				if (effect == "glass") {
					material.effect_mode = MaterialEffectMode::GLASS;
				}
			}
		}
	});
	return {materials, textures};
}
