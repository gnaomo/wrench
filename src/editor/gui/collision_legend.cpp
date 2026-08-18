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

#include "collision_legend.h"

#include <array>
#include <cstring>
#include <algorithm>
#include <unordered_map>

#include <editor/gui/imgui_includes.h>

// Human-readable names for known collision surface ids. Sourced from
// community reverse-engineering notes; ids not listed here fall back to
// "col_XX" wherever they're displayed. Feel free to extend this table as
// more ids get documented -- it's deliberately kept separate from the
// filter/legend logic below.
static const std::unordered_map<u8, const char*> COLLISION_ID_NAMES = {
	{0x00, "water"},
	{0x01, "lava bounce"},
	{0x02, "magneboots surface"},
	{0x03, "sinking mud (can be jump out of)"},
	{0x04, "bodysliding (water/quartu slide)"},
	{0x05, "hoverboard ramp"},
	{0x06, "(unused) walkable unk attr"},
	{0x07, "slippery ice"},
	{0x08, "non walkable w/ ledgegrab"},
	{0x09, "walkable w/out ledgegrab"},
	{0x0A, "walkable w/ ledgegrab"},
	{0x0B, "inescapable mud"},
	{0x0C, "non walkable w/out ledgegrab"},
	{0x0D, "insta ice cube death"},
	{0x0E, "walkable water (jesus ratchet)"},
	{0x0F, "walkable w/ ledgegrab"},
	{0x10, "water"},
	{0x11, "lava bounce"},
	{0x12, "magneboots surface"},
	{0x13, "sinking mud (can be jump out of)"},
	{0x14, "bodysliding (water/quartu slide)"},
	{0x15, "hoverboard ramp"},
	{0x16, "(unused) walkable unk attr"},
	{0x17, "slippery ice"},
	{0x18, "non walkable w/ ledgegrab"},
	{0x19, "walkable w/out ledgegrab"},
	{0x1A, "walkable w/ ledgegrab"},
	{0x1B, "inescapable mud"},
	{0x1C, "non walkable w/out ledgegrab"},
	{0x1D, "insta ice cube death"},
	{0x1E, "walkable water (jesus ratchet)"},
	{0x1F, "walkable w/ ledgegrab"},
	{0x20, "water"},
	{0x21, "lava bounce"},
	{0x22, "magneboots surface"},
	{0x23, "sinking mud (can be jump out of)"},
	{0x24, "bodysliding (water/quartu slide)"},
	{0x25, "hoverboard ramp"},
	{0x26, "(unused) walkable unk attr"},
	{0x27, "slippery ice"},
	{0x28, "non walkable w/ ledgegrab"},
	{0x29, "walkable w/out ledgegrab"},
	{0x2A, "walkable w/ ledgegrab"},
	{0x2B, "inescapable mud"},
	{0x2C, "non walkable w/out ledgegrab"},
	{0x2D, "insta ice cube death"},
	{0x2E, "walkable water (jesus ratchet)"},
	{0x2F, "walkable w/ ledgegrab"},
	{0x30, "water"},
	{0x31, "lava bounce"},
	{0x32, "magneboots surface"},
	{0x33, "sinking mud (can be jump out of)"},
	{0x34, "bodysliding (water/quartu slide)"},
	{0x35, "hoverboard ramp"},
	{0x36, "(unused) walkable unk attr"},
	{0x37, "slippery ice"},
	{0x38, "non walkable w/ ledgegrab"},
	{0x39, "walkable w/out ledgegrab"},
	{0x3A, "walkable w/ ledgegrab"},
	{0x3B, "inescapable mud"},
	{0x3C, "non walkable w/out ledgegrab"},
	{0x3D, "insta ice cube death"},
	{0x3E, "walkable water (jesus ratchet)"},
	{0x3F, "walkable w/ ledgegrab"},
	{0x40, "water"},
	{0x41, "lava bounce"},
	{0x42, "magneboots surface"},
	{0x43, "sinking mud (can be jump out of)"},
	{0x44, "bodysliding (water/quartu slide)"},
	{0x45, "hoverboard ramp"},
	{0x46, "(unused) walkable unk attr"},
	{0x47, "slippery ice"},
	{0x48, "non walkable w/ ledgegrab"},
	{0x49, "walkable w/out ledgegrab"},
	{0x4A, "walkable w/ ledgegrab"},
	{0x4B, "inescapable mud"},
	{0x4C, "non walkable w/out ledgegrab"},
	{0x4D, "insta ice cube death"},
	{0x4E, "walkable water (jesus ratchet)"},
	{0x4F, "walkable w/ ledgegrab"},
	{0x50, "water"},
	{0x51, "lava bounce"},
	{0x52, "magneboots surface"},
	{0x53, "sinking mud (can be jump out of)"},
	{0x54, "bodysliding (water/quartu slide)"},
	{0x55, "hoverboard ramp"},
	{0x56, "(unused) walkable unk attr"},
	{0x57, "slippery ice"},
	{0x58, "non walkable w/ ledgegrab"},
	{0x59, "walkable w/out ledgegrab"},
	{0x5A, "walkable w/ ledgegrab"},
	{0x5B, "inescapable mud"},
	{0x5C, "non walkable w/out ledgegrab"},
	{0x5D, "insta ice cube death"},
	{0x5E, "walkable water (jesus ratchet)"},
	{0x5F, "walkable w/ ledgegrab"},
	{0x60, "water"},
	{0x61, "lava bounce"},
	{0x62, "magneboots surface"},
	{0x63, "sinking mud (can be jump out of)"},
	{0x64, "bodysliding (water/quartu slide)"},
	{0x65, "hoverboard ramp"},
	{0x66, "(unused) walkable unk attr"},
	{0x67, "slippery ice"},
	{0x68, "non walkable w/ ledgegrab"},
	{0x69, "walkable w/out ledgegrab"},
	{0x6A, "walkable w/ ledgegrab"},
	{0x6B, "inescapable mud"},
	{0x6C, "non walkable w/out ledgegrab"},
	{0x6D, "insta ice cube death"},
	{0x6E, "walkable water (jesus ratchet)"},
	{0x6F, "walkable w/ ledgegrab"},
	{0x70, "water"},
	{0x71, "lava bounce"},
	{0x72, "magneboots surface"},
	{0x73, "sinking mud (can be jump out of)"},
	{0x74, "bodysliding (water/quartu slide)"},
	{0x75, "hoverboard ramp"},
	{0x76, "(unused) walkable unk attr"},
	{0x77, "slippery ice"},
	{0x78, "non walkable w/ ledgegrab"},
	{0x79, "walkable w/out ledgegrab"},
	{0x7A, "walkable w/ ledgegrab"},
	{0x7B, "inescapable mud"},
	{0x7C, "non walkable w/out ledgegrab"},
	{0x7D, "insta ice cube death"},
	{0x7E, "walkable water (jesus ratchet)"},
	{0x7F, "walkable w/ ledgegrab"},
	{0x80, "water"},
	{0x81, "lava bounce"},
	{0x82, "magneboots surface"},
	{0x83, "sinking mud (can be jump out of)"},
	{0x84, "bodysliding (water/quartu slide)"},
	{0x85, "hoverboard ramp"},
	{0x86, "(unused) walkable unk attr"},
	{0x87, "slippery ice"},
	{0x88, "non walkable w/ ledgegrab"},
	{0x89, "walkable w/out ledgegrab"},
	{0x8A, "walkable w/ ledgegrab"},
	{0x8B, "inescapable mud"},
	{0x8C, "non walkable w/out ledgegrab"},
	{0x8D, "insta ice cube death"},
	{0x8E, "walkable water (jesus ratchet)"},
	{0x8F, "walkable w/ ledgegrab"},
	{0x90, "water"},
	{0x91, "lava bounce"},
	{0x92, "magneboots surface"},
	{0x93, "sinking mud (can be jump out of)"},
	{0x94, "bodysliding (water/quartu slide)"},
	{0x95, "hoverboard ramp"},
	{0x96, "(unused) walkable unk attr"},
	{0x97, "slippery ice"},
	{0x98, "non walkable w/ ledgegrab"},
	{0x99, "walkable w/out ledgegrab"},
	{0x9A, "walkable w/ ledgegrab"},
	{0x9B, "inescapable mud"},
	{0x9C, "non walkable w/out ledgegrab"},
	{0x9D, "insta ice cube death"},
	{0x9E, "walkable water (jesus ratchet)"},
	{0x9F, "walkable w/ ledgegrab"},
	{0xA0, "water"},
	{0xA1, "lava bounce"},
	{0xA2, "magneboots surface"},
	{0xA3, "sinking mud (can be jump out of)"},
	{0xA4, "bodysliding (water/quartu slide)"},
	{0xA5, "hoverboard ramp"},
	{0xA6, "(unused) walkable unk attr"},
	{0xA7, "slippery ice"},
	{0xA8, "non walkable w/ ledgegrab"},
	{0xA9, "walkable w/out ledgegrab"},
	{0xAA, "walkable w/ ledgegrab"},
	{0xAB, "inescapable mud"},
	{0xAC, "non walkable w/out ledgegrab"},
	{0xAD, "insta ice cube death"},
	{0xAE, "walkable water (jesus ratchet)"},
	{0xAF, "walkable w/ ledgegrab"},
	{0xB0, "water"},
	{0xB1, "lava bounce"},
	{0xB2, "magneboots surface"},
	{0xB3, "sinking mud (can be jump out of)"},
	{0xB4, "bodysliding (water/quartu slide)"},
	{0xB5, "hoverboard ramp"},
	{0xB6, "(unused) walkable unk attr"},
	{0xB7, "slippery ice"},
	{0xB8, "non walkable w/ ledgegrab"},
	{0xB9, "walkable w/out ledgegrab"},
	{0xBA, "walkable w/ ledgegrab"},
	{0xBB, "inescapable mud"},
	{0xBC, "non walkable w/out ledgegrab"},
	{0xBD, "insta ice cube death"},
	{0xBE, "walkable water (jesus ratchet)"},
	{0xBF, "walkable w/ ledgegrab"},
	{0xC0, "water"},
	{0xC1, "lava bounce"},
	{0xC2, "magneboots surface"},
	{0xC3, "sinking mud (can be jump out of)"},
	{0xC4, "bodysliding (water/quartu slide)"},
	{0xC5, "hoverboard ramp"},
	{0xC6, "(unused) walkable unk attr"},
	{0xC7, "slippery ice"},
	{0xC8, "non walkable w/ ledgegrab"},
	{0xC9, "walkable w/out ledgegrab"},
	{0xCA, "walkable w/ ledgegrab"},
	{0xCB, "inescapable mud"},
	{0xCC, "non walkable w/out ledgegrab"},
	{0xCD, "insta ice cube death"},
	{0xCE, "walkable water (jesus ratchet)"},
	{0xCF, "walkable w/ ledgegrab"},
	{0xD0, "water"},
	{0xD1, "lava bounce"},
	{0xD2, "magneboots surface"},
	{0xD3, "sinking mud (can be jump out of)"},
	{0xD4, "bodysliding (water/quartu slide)"},
	{0xD5, "hoverboard ramp"},
	{0xD6, "(unused) walkable unk attr"},
	{0xD7, "slippery ice"},
	{0xD8, "non walkable w/ ledgegrab"},
	{0xD9, "walkable w/out ledgegrab"},
	{0xDA, "walkable w/ ledgegrab"},
	{0xDB, "inescapable mud"},
	{0xDC, "non walkable w/out ledgegrab"},
	{0xDD, "insta ice cube death"},
	{0xDE, "walkable water (jesus ratchet)"},
	{0xDF, "walkable w/ ledgegrab"},
	{0xE0, "water"},
	{0xE1, "lava bounce"},
	{0xE2, "magneboots surface"},
	{0xE3, "sinking mud (can be jump out of)"},
	{0xE4, "bodysliding (water/quartu slide)"},
	{0xE5, "hoverboard ramp"},
	{0xE6, "(unused) walkable unk attr"},
	{0xE7, "slippery ice"},
	{0xE8, "non walkable w/ ledgegrab"},
	{0xE9, "walkable w/out ledgegrab"},
	{0xEA, "walkable w/ ledgegrab"},
	{0xEB, "inescapable mud"},
	{0xEC, "non walkable w/out ledgegrab"},
	{0xED, "insta ice cube death"},
	{0xEE, "walkable water (jesus ratchet)"},
	{0xEF, "walkable w/ ledgegrab"},
	{0xF0, "water"},
	{0xF1, "lava bounce"},
	{0xF2, "magneboots surface"},
	{0xF3, "sinking mud (can be jump out of)"},
	{0xF4, "bodysliding (water/quartu slide)"},
	{0xF5, "hoverboard ramp"},
	{0xF6, "(unused) walkable unk attr"},
	{0xF7, "slippery ice"},
	{0xF8, "non walkable w/ ledgegrab"},
	{0xF9, "walkable w/out ledgegrab"},
	{0xFA, "walkable w/ ledgegrab"},
	{0xFB, "inescapable mud"},
	{0xFC, "non walkable w/out ledgegrab"},
	{0xFD, "insta ice cube death"},
	{0xFE, "walkable water (jesus ratchet)"},
	{0xFF, "walkable w/ ledgegrab"}
};

const char* collision_id_label(u8 id)
{
	auto iter = COLLISION_ID_NAMES.find(id);
	if (iter == COLLISION_ID_NAMES.end()) {
		return nullptr;
	}
	return iter->second;
}

// Union of EditorChunk::collision_ids_present across every chunk in the
// level, sorted ascending. Recomputed on demand (cheap: at most 256 ids).
static std::vector<u8> gather_ids_present(Level& lvl)
{
	std::array<bool, 256> seen{};
	std::vector<u8> ids;
	for (EditorChunk& chunk : lvl.chunks) {
		for (u8 id : chunk.collision_ids_present) {
			if (!seen[id]) {
				seen[id] = true;
				ids.push_back(id);
			}
		}
	}
	std::sort(ids.begin(), ids.end());
	return ids;
}

// The colour assigned to a given collision id is deterministic (see
// create_collision_materials in engine/collision.cpp), so it's fine to just
// grab it from whichever loaded chunk happens to have it.
static const RenderMaterial* find_collision_material(Level& lvl, u8 id)
{
	for (EditorChunk& chunk : lvl.chunks) {
		if ((size_t) id < chunk.collision_materials.size()) {
			return &chunk.collision_materials[id];
		}
	}
	return nullptr;
}

static void draw_checkbox_list(Level& lvl, RenderSettings& settings, const std::vector<u8>& ids)
{
	if (ids.empty()) {
		ImGui::TextDisabled("No collision loaded.");
		return;
	}
	
	if (ImGui::SmallButton("All")) {
		for (u8 id : ids) {
			settings.hidden_collision_ids[id] = false;
		}
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("None")) {
		for (u8 id : ids) {
			settings.hidden_collision_ids[id] = true;
		}
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Invert")) {
		for (u8 id : ids) {
			settings.hidden_collision_ids[id] = !settings.hidden_collision_ids[id];
		}
	}
	
	ImGui::Separator();
	
	for (u8 id : ids) {
		ImGui::PushID(id);
		
		bool visible = !settings.hidden_collision_ids[id];
		if (ImGui::Checkbox("##visible", &visible)) {
			settings.hidden_collision_ids[id] = !visible;
		}
		
		ImGui::SameLine();
		const RenderMaterial* mat = find_collision_material(lvl, id);
		glm::vec4 colour = mat ? mat->colour : glm::vec4(1.f, 1.f, 1.f, 1.f);
		ImGui::ColorButton(
			"##swatch",
			ImVec4(colour.r, colour.g, colour.b, 1.f),
			ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoBorder,
			ImVec2(14, 14));
		
		ImGui::SameLine();
		const char* name = collision_id_label(id);
		if (name != nullptr) {
			ImGui::Text("0x%02x", (u32) id);
			ImGui::SameLine();
			ImGui::TextUnformatted(name);
			if (strlen(name) > 24 && ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s", name);
			}
		} else {
			ImGui::TextDisabled("0x%02x  col_%02x", (u32) id, (u32) id);
		}
		
		ImGui::PopID();
	}
}

void draw_collision_id_checkboxes(Level& lvl, RenderSettings& settings)
{
	draw_checkbox_list(lvl, settings, gather_ids_present(lvl));
}

void collision_legend_window(Level& lvl, RenderSettings& settings)
{
	if (!settings.draw_collision) {
		return;
	}
	
	std::vector<u8> ids = gather_ids_present(lvl);
	if (ids.empty()) {
		return;
	}
	
	ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 260.f, 40.f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(260.f, 320.f), ImGuiCond_FirstUseEver);
	
	if (ImGui::Begin("Collision legend")) {
		ImGui::Text("%d ids", (int) ids.size());
		ImGui::Separator();
		ImGui::BeginChild("collision_id_scroll_region");
		draw_checkbox_list(lvl, settings, ids);
		ImGui::EndChild();
	}
	ImGui::End();
}
