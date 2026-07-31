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

#ifndef EDITOR_GUI_COLLISION_LEGEND_H
#define EDITOR_GUI_COLLISION_LEGEND_H

#include <core/util.h>
#include <editor/level.h>
#include <editor/renderer.h>

// Returns a short human-readable label for a known collision surface id, or
// nullptr if this id hasn't been documented yet (falls back to "col_XX" at
// the call site in that case).
const char* collision_id_label(u8 id);

// Draws the "All / None / Invert" buttons followed by one checkbox row per
// distinct collision id used anywhere in the level, each with a colour
// swatch and its label if known. Shared by the floating legend window and
// the View > Visibility > Collision ids menu so the two never drift out of
// sync. Toggling a checkbox flips the matching bit in settings.hidden_collision_ids.
void draw_collision_id_checkboxes(Level& lvl, RenderSettings& settings);

// Floating "Collision legend" window overlaid on the 3D viewport (top-right
// by default, draggable). Only draws anything once the level has collision
// data loaded.
void collision_legend_window(Level& lvl, RenderSettings& settings);

#endif
