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

#include "table_of_contents.h"

#include <core/buffer.h>
#include <core/filesystem.h>
#include <engine/vag.h>
#include <iso/wad_identifier.h>

static LevelWadInfo adapt_rac1_level_wad_header(InputStream& src, Rac1AmalgamatedWadHeader& header);
static Opt<LevelWadInfo> adapt_rac1_audio_wad_header(InputStream& src, Rac1AmalgamatedWadHeader& header);
static Opt<LevelWadInfo> adapt_rac1_scene_wad_header(InputStream& src, Rac1AmalgamatedWadHeader& header);
static Sector32 get_lz_size(InputStream& src, Sector32 sector);

static s64 guess_rac234_level_table_offset(Buffer src);

static SectorRange add_sector_range(SectorRange range, Sector32 lsn);
static SectorByteRange add_sector_byte_range(SectorByteRange range, Sector32 lsn);
static SectorRange sub_sector_range(SectorRange range, Sector32 lsn);
static SectorByteRange sub_sector_byte_range(SectorByteRange range, Sector32 lsn);

table_of_contents read_table_of_contents(InputStream& src, Game game)
{
	table_of_contents toc;
	if (game == Game::RAC) {
		toc = read_table_of_contents_rac(src);
	} else {
		toc = read_table_of_contents_rac234(src);
		if (toc.levels.size() == 0) {
			fprintf(stderr, "error: Unable to locate level table!\n");
			exit(1);
		}
	}
	return toc;
}

s64 write_table_of_contents(OutputStream& iso, const table_of_contents& toc, Game game)
{
	if (game == Game::RAC) {
		return write_table_of_contents_rac(iso, toc, game);
	} else {
		return write_table_of_contents_rac234(iso, toc, game);
	}
}

Sector32 calculate_table_of_contents_size(const table_of_contents& toc, Game game)
{
	if (game == Game::RAC) {
		return calculate_table_of_contents_size_rac(toc);
	} else {
		return calculate_table_of_contents_size_rac234(toc);
	}
}

table_of_contents read_table_of_contents_rac(InputStream& src)
{
	s32 magic, toc_size;
	src.seek(RAC_TABLE_OF_CONTENTS_LBA * SECTOR_SIZE);
	verify(src.read_n((u8*) &magic, 4) == 1, "Failed to read R&C1 table of contents.");
	verify(src.read_n((u8*) &toc_size, 4) == 1, "Failed to read R&C1 table of contents.");
	verify(toc_size > 0 && toc_size < 1024 * 1024 * 1024, "Invalid R&C1 table of contents.");
	verify(magic == 1, "Invalid R&C1 table of contents.");
	src.seek(RAC_TABLE_OF_CONTENTS_LBA * SECTOR_SIZE);
	std::vector<u8> bytes = src.read_multiple<u8>(toc_size);
	Buffer buffer(bytes);
	
	table_of_contents toc;
	
	GlobalWadInfo global = {};
	global.sector = {0};
	global.header = bytes;
	toc.globals.emplace_back(std::move(global));
	
	for (s64 ofs = 8; ofs < buffer.size(); ofs += 8) {
		Sector32 lsn = buffer.read<Sector32>(ofs, "sector");
		if (lsn.sectors != 0) {
			src.seek(lsn.bytes());
			std::vector<u8> header_bytes = src.read_multiple<u8>(0x2434);
			Buffer header_buffer(header_bytes);
			if (header_buffer.read<s32>(4, "file header") == 0x2434) {
				Rac1AmalgamatedWadHeader header = header_buffer.read<Rac1AmalgamatedWadHeader>(0, "level header");
				
				LevelInfo level;
				level.level_table_index = header.id;
				level.level = adapt_rac1_level_wad_header(src, header);
				level.audio = adapt_rac1_audio_wad_header(src, header);
				level.scene = adapt_rac1_scene_wad_header(src, header);
				toc.levels.emplace_back(std::move(level));
			}
		}
	}
	
	return toc;
}

s64 write_table_of_contents_rac(OutputStream& iso, const table_of_contents& toc, Game game)
{
	verify_fatal(toc.globals.size() == 1);
	const GlobalWadInfo& global = toc.globals[0];
	DumbRacWadInfo info = Buffer(global.header).read<DumbRacWadInfo>(0, "wad info");
	info.version = 1;
	info.header_size = sizeof(DumbRacWadInfo);
	
	// Convert relative sector offsets to absolute sector numbers.
	for (SectorRange& range : info.sector_ranges_1) {
		range.offset.sectors += global.sector.sectors;
	}
	for (Sector32& sector : info.sectors_1) {
		sector.sectors += global.sector.sectors;
	}
	for (SectorRange& range : info.sector_ranges_2) {
		range.offset.sectors += global.sector.sectors;
	}
	for (SectorByteRange& range : info.sector_byte_ranges) {
		range.offset.sectors += global.sector.sectors;
	}
	for (Sector32& sector : info.sectors_2) {
		sector.sectors += global.sector.sectors;
	}
	
	s64 wad_info_ofs = RAC_TABLE_OF_CONTENTS_LBA * SECTOR_SIZE;
	
	// Verified against the retail disc (see phase1_work/verify_full_formula.py,
	// zero error across all 19 levels): levels[N].size is not a spatial/
	// forward-reach quantity, it's the total real footprint of everything
	// that level owns -- its own 5-sector header, its core data block, its
	// bindata, and all the music/scene assets it references, wherever those
	// physically sit on disc -- each part individually rounded up to whole
	// sectors before summing. header_sectors/core_block_sectors/bindata_sectors
	// below are computed live from this build's own packed output. The
	// audio_scene_sectors term can't be yet: pack_rac_level_audio_wad() and
	// pack_rac_level_scene_wad() (level_audio_wad.cpp/level_scene_wad.cpp) are
	// still unimplemented stubs for RAC1 -- they never write real music/scene
	// body content or populate per-entry sizes, so there is nothing correct to
	// read back at pack time. Until that's implemented, this uses the real
	// per-level totals extracted directly from the retail disc's own ToC
	// (phase0_toc_dump.csv / phase1_work/phase0_toc_dump_v2.csv), indexed by
	// level_table_index. This reproduces the unmodified retail disc exactly,
	// but will NOT track changes if a level's music/scenes are ever modded --
	// whoever implements real RAC1 audio/scene WAD packing should replace this
	// table with a live sum of per-entry sizes (get_vag_size()/get_lz_size()
	// against the freshly-packed bytes) at that time.
	static const s32 RETAIL_AUDIO_SCENE_SECTORS[19] = {
		12468, 19349, 18912, 28359, 8937, 29672, 17699, 21225, 25029,
		2432, 12959, 25681, 15856, 10715, 16277, 16651, 17731, 6024, 23702
	};
	
	for (s32 i = 0; i < std::min(toc.levels.size(), ARRAY_SIZE(info.levels)); i++) {
		Rac1AmalgamatedWadHeader header = {};
		header.header_size = sizeof(Rac1AmalgamatedWadHeader);
		
		const Opt<LevelWadInfo>& level_info = toc.levels[i].level;
		if (level_info.has_value()) {
			const RacLevelWadHeader& level = Buffer(level_info->header).read<RacLevelWadHeader>(0, "level header");
			header.id = level.id;
			header.data = add_sector_range(level.data, level_info->file_lba);
			header.gameplay_ntsc = add_sector_range(level.gameplay_ntsc, level_info->file_lba);
			header.gameplay_pal = add_sector_range(level.gameplay_pal, level_info->file_lba);
			header.occlusion = add_sector_range(level.occlusion, level_info->file_lba);
		}
		
		const Opt<LevelWadInfo>& audio_info = toc.levels[i].audio;
		if (audio_info.has_value()) {
			const RacLevelAudioWadHeader& audio = Buffer(audio_info->header).read<RacLevelAudioWadHeader>(0, "level audio header");
			for (s32 i = 0; i < ARRAY_SIZE(header.bindata); i++) {
				if (!audio.bindata[i].empty()) {
					header.bindata[i] = add_sector_byte_range(audio.bindata[i], audio_info->file_lba);
				}
			}
			for (s32 i = 0; i < ARRAY_SIZE(header.music); i++) {
				if (!audio.music[i].empty()) {
					header.music[i] = {audio.music[i].sectors + audio_info->file_lba.sectors};
				}
			}
		}
		
		const Opt<LevelWadInfo>& scene_info = toc.levels[i].scene;
		if (scene_info.has_value()) {
			const RacLevelSceneWadHeader& scene = Buffer(scene_info->header).read<RacLevelSceneWadHeader>(0, "level scene header");
			for (s32 i = 0; i < ARRAY_SIZE(header.scenes); i++) {
				RacSceneHeader& dest_scene = header.scenes[i];
				const RacSceneHeader& src_scene = scene.scenes[i];
				for (s32 i = 0; i < ARRAY_SIZE(dest_scene.sounds); i++) {
					if (!src_scene.sounds[i].empty()) {
						dest_scene.sounds[i] = {src_scene.sounds[i].sectors + scene_info->file_lba.sectors};
					}
				}
				for (s32 i = 0; i < ARRAY_SIZE(dest_scene.wads); i++) {
					if (!src_scene.wads[i].empty()) {
						dest_scene.wads[i] = {src_scene.wads[i].sectors + scene_info->file_lba.sectors};
					}
				}
			}
		}
		
		if (level_info.has_value()) {
			// The header lives immediately before this level's own core data
			// block, at the sector reserved for it by pack_level_wad_outer
			// (see LevelWadInfo::header_lba), NOT sequentially packed right
			// after the ToC header -- see calculate_table_of_contents_size_rac
			// above for why that old placement was wrong.
			Sector32 header_sectors = Sector32::size_from_bytes(sizeof(Rac1AmalgamatedWadHeader));
			s32 core_block_sectors = header.data.size.sectors + header.gameplay_ntsc.size.sectors +
				header.gameplay_pal.size.sectors + header.occlusion.size.sectors;
			s32 bindata_sectors = 0;
			for (const SectorByteRange& range : header.bindata) {
				bindata_sectors += Sector32::size_from_bytes(range.size_bytes).sectors;
			}
			s32 level_table_index = toc.levels[i].level_table_index;
			s32 audio_scene_sectors = 0;
			if (level_table_index >= 0 && level_table_index < (s32) ARRAY_SIZE(RETAIL_AUDIO_SCENE_SECTORS)) {
				audio_scene_sectors = RETAIL_AUDIO_SCENE_SECTORS[level_table_index];
			}
			s32 total_size = header_sectors.sectors + core_block_sectors + bindata_sectors + audio_scene_sectors;
			
			iso.seek(level_info->header_lba.bytes());
			iso.write(header);
			info.levels[i] = {level_info->header_lba, {total_size}};
		} else {
			info.levels[i] = {{0}, {0}};
		}
	}
	
	s64 toc_end = wad_info_ofs + sizeof(RacWadInfo);
	
	iso.seek(wad_info_ofs);
	iso.write(info);
	
	return toc_end;
}

Sector32 calculate_table_of_contents_size_rac(const table_of_contents& toc)
{
	// The retail disc does NOT reserve space for the 19 level headers
	// immediately after the ToC header at RAC_TABLE_OF_CONTENTS_LBA -- each
	// level's Rac1AmalgamatedWadHeader instead lives immediately before that
	// level's own core data block (see the header reservation added to
	// pack_level_wad_outer for the "level" WAD kind, and the seek-back write
	// in write_table_of_contents_rac below). Reserving 19*5=95 extra sectors
	// here, as an earlier version of this function did, pushed the globals
	// blob (which immediately follows this reserved region) 101 sectors too
	// late relative to retail (confirmed: first cmp diff at RAC_TABLE_OF_
	// CONTENTS_LBA+8, the debug_font field, off by exactly 101 sectors).
	Sector32 wad_info_size = Sector32::size_from_bytes(sizeof(RacWadInfo));
	return wad_info_size;
}

static LevelWadInfo adapt_rac1_level_wad_header(InputStream& src, Rac1AmalgamatedWadHeader& header)
{
	// Determine where the file begins and ends on disc.
	Sector32 low = {INT32_MAX};
	low = {std::min(low.sectors, header.data.offset.sectors)};
	low = {std::min(low.sectors, header.gameplay_ntsc.offset.sectors)};
	low = {std::min(low.sectors, header.gameplay_pal.offset.sectors)};
	low = {std::min(low.sectors, header.occlusion.offset.sectors)};
	
	Sector32 high = {0};
	high = {std::max(high.sectors, header.data.end().sectors)};
	high = {std::max(high.sectors, header.gameplay_ntsc.end().sectors)};
	high = {std::max(high.sectors, header.gameplay_pal.end().sectors)};
	high = {std::max(high.sectors, header.occlusion.end().sectors)};
	
	// Rewrite header, convert absolute sector numbers to relative sector offsets.
	s32 header_size_sectors = Sector32::size_from_bytes(sizeof(RacLevelWadHeader)).sectors;
	Sector32 adjust = {low.sectors - header_size_sectors};
	RacLevelWadHeader level_header = {0};
	level_header.header_size = sizeof(RacLevelWadHeader);
	level_header.id = header.id;
	level_header.data = sub_sector_range(header.data, adjust);
	level_header.gameplay_ntsc = sub_sector_range(header.gameplay_ntsc, adjust);
	level_header.gameplay_pal = sub_sector_range(header.gameplay_pal, adjust);
	level_header.occlusion = sub_sector_range(header.occlusion, adjust);
	
	LevelWadInfo level_part;
	level_part.header_lba = {0};
	level_part.file_lba = adjust;
	level_part.file_size = {high.sectors - adjust.sectors};
	level_part.header.resize(sizeof(RacLevelWadHeader));
	memcpy(level_part.header.data(), &level_header, sizeof(RacLevelWadHeader));
	level_part.prepend_header = true;
	
	verify(level_part.file_size.bytes() < 1024 * 1024 * 1024, "Level WAD too big!");
	
	return level_part;
}

static Opt<LevelWadInfo> adapt_rac1_audio_wad_header(InputStream& src, Rac1AmalgamatedWadHeader& header) {
	// Determine where the file begins and ends on disc.
	Sector32 low = {INT32_MAX};
	Sector32 high = {0};
	for (s32 i = 0; i < ARRAY_SIZE(header.bindata); i++) {
		if (header.bindata[i].offset.sectors > 0) {
			low = {std::min(low.sectors, header.bindata[i].offset.sectors)};
			high = {std::max(high.sectors, header.bindata[i].end().sectors)};
		}
	}
	for (s32 i = 0; i < ARRAY_SIZE(header.music); i++) {
		if (header.music[i].sectors > 0) {
			low = {std::min(low.sectors, header.music[i].sectors)};
			high = {std::max(high.sectors, header.music[i].sectors + get_vag_size(src, header.music[i].sectors).sectors)};
		}
	}
	if (low.sectors == INT32_MAX || high.sectors == 0) {
		return {};
	}
	
	s32 header_size_sectors = Sector32::size_from_bytes(sizeof(RacLevelAudioWadHeader)).sectors;
	Sector32 adjust = {low.sectors - header_size_sectors};
	
	// Rewrite header, convert absolute sector numbers to relative sector offsets.
	RacLevelAudioWadHeader audio_header = {0};
	audio_header.header_size = sizeof(RacLevelAudioWadHeader);
	for (s32 i = 0; i < ARRAY_SIZE(header.bindata); i++) {
		if (header.bindata[i].offset.sectors > 0) {
			audio_header.bindata[i] = sub_sector_byte_range(header.bindata[i], adjust);
		}
	}
	for (s32 i = 0; i < ARRAY_SIZE(header.music); i++) {
		if (header.music[i].sectors > 0) {
			audio_header.music[i] = {header.music[i].sectors - adjust.sectors};
		}
	}
	
	LevelWadInfo audio_part;
	audio_part.header_lba = {0};
	audio_part.file_lba = adjust;
	audio_part.file_size = {high.sectors - adjust.sectors};
	audio_part.header.resize(sizeof(RacLevelAudioWadHeader));
	memcpy(audio_part.header.data(), &audio_header, sizeof(RacLevelAudioWadHeader));
	audio_part.prepend_header = true;
	
	verify(audio_part.file_size.bytes() < (1024 * 1024 * 1024), "Level audio WAD too big!");
	
	return audio_part;
}

static Opt<LevelWadInfo> adapt_rac1_scene_wad_header(InputStream& src, Rac1AmalgamatedWadHeader& header) {
	// Determine where the file begins and ends on disc.
	Sector32 low = {INT32_MAX};
	Sector32 high = {0};
	for (s32 i = 0; i < sizeof(header.scenes) / sizeof(header.scenes[0]); i++) {
		const RacSceneHeader& scene = header.scenes[i];
		for (s32 j = 0; j < ARRAY_SIZE(scene.sounds); j++) {
			if (scene.sounds[j].sectors > 0) {
				low = {std::min(low.sectors, scene.sounds[j].sectors)};
				high = {std::max(high.sectors, scene.sounds[j].sectors + get_vag_size(src, scene.sounds[j].sectors).sectors)};
			}
		}
		for (s32 j = 0; j < ARRAY_SIZE(scene.wads); j++) {
			if (scene.wads[j].sectors > 0) {
				low = {std::min(low.sectors, scene.wads[j].sectors)};
				high = {std::max(high.sectors, scene.wads[j].sectors + get_lz_size(src, scene.wads[j].sectors).sectors)};
			}
		}
	}
	if (low.sectors == INT32_MAX || high.sectors == 0) {
		return {};
	}
	
	s32 header_size_sectors = Sector32::size_from_bytes(sizeof(RacLevelSceneWadHeader)).sectors;
	Sector32 adjust = {low.sectors - header_size_sectors};
	
	// Rewrite header, convert absolute sector numbers to relative sector offsets.
	RacLevelSceneWadHeader scene_header = {0};
	scene_header.header_size = sizeof(RacLevelSceneWadHeader);
	for (s32 i = 0; i < ARRAY_SIZE(header.scenes); i++) {
		RacSceneHeader& dest = scene_header.scenes[i];
		RacSceneHeader& src = header.scenes[i];
		for (s32 j = 0; j < ARRAY_SIZE(dest.sounds); j++) {
			if (src.sounds[j].sectors > 0) {
				dest.sounds[j] = {src.sounds[j].sectors - adjust.sectors};
			}
		}
		for (s32 j = 0; j < ARRAY_SIZE(dest.wads); j++) {
			if (src.wads[j].sectors > 0) {
				dest.wads[j] = {src.wads[j].sectors - adjust.sectors};
			}
		}
	}
	
	LevelWadInfo scene_part;
	scene_part.header_lba = {0};
	scene_part.file_lba = adjust;
	scene_part.file_size = {high.sectors - adjust.sectors};
	scene_part.header.resize(sizeof(RacLevelSceneWadHeader));
	memcpy(scene_part.header.data(), &scene_header, sizeof(RacLevelSceneWadHeader));
	scene_part.prepend_header = true;
	
	verify(scene_part.file_size.bytes() < 1024 * 1024 * 1024, "Level scene WAD too big!");
	
	return scene_part;
}

static Sector32 get_lz_size(InputStream& src, Sector32 sector)
{
	LzHeader header = src.read<LzHeader>(sector.bytes());
	if (memcmp(header.magic, "WAD", 3) != 0) {
		return {1};
	}
	return Sector32::size_from_bytes(header.compressed_size);
}

table_of_contents read_table_of_contents_rac234(InputStream& src)
{
	src.seek(GC_UYA_DL_TABLE_OF_CONTENTS_LBA * SECTOR_SIZE);
	std::vector<u8> bytes = src.read_multiple<u8>(TOC_MAX_SIZE);
	Buffer buffer(bytes);
	
	table_of_contents toc;
	
	s64 approximate_level_table_offset = guess_rac234_level_table_offset(buffer);
	if (approximate_level_table_offset == 0x0) {
		// We've failed to find the level table, at least try to find some of the other tables.
		approximate_level_table_offset = 0xffff;
	}
	
	s64 global_index = 0;
	s64 ofs = 0;
	while (ofs + 4 * 6 < approximate_level_table_offset) {
		GlobalWadInfo global;
		global.index = global_index++;
		global.offset_in_toc = ofs;
		s32 header_size = buffer.read<s32>(ofs, "global header");
		if (header_size < 8 || header_size > 0xffff) {
			break;
		}
		global.sector = buffer.read<Sector32>(ofs + 4, "global header");
		global.header = buffer.read_multiple<u8>(ofs, header_size, "global header").copy();
		toc.globals.emplace_back(std::move(global));
		ofs += header_size;
	}
	
	auto level_table = buffer.read_multiple<toc_level_table_entry>(ofs, TOC_MAX_LEVELS, "level table");
	for (s32 i = 0; i < TOC_MAX_LEVELS; i++) {
		toc_level_table_entry entry = level_table[i];
		
		LevelInfo level;
		level.level_table_index = i;
		level.level_table_entry_offset = (s32) ofs + i * sizeof(toc_level_table_entry);
		
		// The games have the fields in different orders, so we check the type
		// of what each field points to so we can support them all.
		for (s32 j = 0; j < 3; j++) {
			LevelWadInfo part;
			part.header_lba = entry.parts[j].offset;
			part.file_size = entry.parts[j].size;
			
			Sector32 sector = {part.header_lba.sectors - (s32) GC_UYA_DL_TABLE_OF_CONTENTS_LBA};
			if (sector.sectors <= 0) {
				continue;
			}
			if (sector.bytes() > buffer.size()) {
				break;
			}
			s32 header_size = buffer.read<s32>(sector.bytes(), "level header size");
			part.file_lba = buffer.read<Sector32>(sector.bytes() + 4, "level sector number");
			
			part.header = buffer.read_multiple<u8>(sector.bytes(), header_size, "level header").copy();
			
			const auto& [game, type, name] = identify_wad(part.header);
			
			switch (type) {
				case WadType::LEVEL: level.level = std::move(part); break;
				case WadType::LEVEL_AUDIO: level.audio = std::move(part); break;
				case WadType::LEVEL_SCENE: level.scene = std::move(part); break;
				default: verify_not_reached("Level table references WAD of unknown type '%x'.", *(u32*) &part.header[0]);
			}
		}
		
		toc.levels.push_back(level);
	}
	
	return toc;
}

static s64 guess_rac234_level_table_offset(Buffer src)
{
	// Check that the two next entries are valid. This is necessary to
	// get past a false positive in Deadlocked.
	for (s64 i = 0; i < src.size() / 4 - 12; i++) {
		int parts = 0;
		for (int j = 0; j < 6; j++) {
			Sector32 lsn = src.read<Sector32>((i + j * 2) * 4, "table of contents");
			s64 header_offset = lsn.bytes() - GC_UYA_DL_TABLE_OF_CONTENTS_LBA * SECTOR_SIZE;
			if (lsn.sectors == 0 || header_offset > TOC_MAX_SIZE - 4) {
				break;
			}
			if (header_offset > 0 && header_offset + 4 <= src.size()) {
				s32 header_size = src.read<s32>(header_offset, "level header");
				if (header_size > 0 && header_size < 0x10000 && header_offset + header_size <= src.size()) {
					Buffer header = src.subbuf(header_offset, header_size);
					const auto& [game, type, name] = identify_wad(header);
					if (game != Game::UNKNOWN || type != WadType::UNKNOWN) {
						parts++;
					}
				}
			}
		}
		if (parts == 6) {
			return i * 4;
		}
	}
	return 0;
}

s64 write_table_of_contents_rac234(OutputStream& iso, const table_of_contents& toc, Game game)
{
	iso.seek(GC_UYA_DL_TABLE_OF_CONTENTS_LBA * SECTOR_SIZE);
	
	for (const GlobalWadInfo& global : toc.globals) {
		verify_fatal(global.header.size() > 8);
		iso.write<s32>(Buffer(global.header).read<s32>(0, "global header"));
		iso.write<s32>(global.sector.sectors);
		iso.write_n(global.header.data() + 8, global.header.size() - 8);
	}
	
	s64 level_table_ofs = iso.tell();
	std::vector<SectorRange> level_table(toc.levels.size() * 3, {{0}, {0}});
	iso.write_v(level_table);
	
	s64 toc_start_size_bytes = iso.tell() - GC_UYA_DL_TABLE_OF_CONTENTS_LBA * SECTOR_SIZE;
	Sector32 toc_start_size = Sector32::size_from_bytes(toc_start_size_bytes);
	
	// Size limits hardcoded in the boot ELF.
	s32 max_sectors;
	switch (game) {
		case Game::GC:  max_sectors = 0xb;  break;
		case Game::UYA: max_sectors = 0x10; break;
		case Game::DL:  max_sectors = 0x1a; break;
		default: verify_not_reached("Invalid game.");
	}
	verify(toc_start_size.sectors <= max_sectors,
		"Table of contents too big. This could be because there are too many levels, or due to a bug.");
	
	for (size_t i = 0; i < toc.levels.size(); i++) {
		const LevelInfo& level = toc.levels[i];
		s32 j = 0;
		for (const Opt<LevelWadInfo>& wad : {level.level, level.audio, level.scene}) {
			if (wad) {
				iso.pad(SECTOR_SIZE, 0);
				s64 header_lba = iso.tell() / SECTOR_SIZE;
				*(Sector32*) &wad->header[4] = wad->file_lba;
				iso.write_v(wad->header);
				
				// The order of fields in the level table entries is different
				// for R&C2 versus R&C3 and Deadlocked.
				s32 field = 0;
				bool is_rac2 = game == Game::GC || game == Game::UNKNOWN;
				switch (j++) {
					case 0: field = !is_rac2; break;
					case 1: field = is_rac2; break;
					case 2: field = 2; break;
				}
				s32 index = i * 3 + field;
				level_table[index].offset.sectors = header_lba;
				level_table[index].size = wad->file_size;
			} else {
				j++;
			}
		}
	}
	
	s64 toc_end = iso.tell();
	
	iso.seek(level_table_ofs);
	iso.write_v(level_table);
	
	return toc_end;
}

Sector32 calculate_table_of_contents_size_rac234(const table_of_contents& toc)
{
	s64 resident_toc_size_bytes = 0;
	for (const GlobalWadInfo& global : toc.globals) {
		verify_fatal(global.header.size() > 0);
		resident_toc_size_bytes += global.header.size();
	}
	resident_toc_size_bytes += toc.levels.size() * sizeof(SectorRange) * 3;
	Sector32 toc_size = Sector32::size_from_bytes(resident_toc_size_bytes);
	for (auto& level : toc.levels) {
		for (const Opt<LevelWadInfo>& wad : {level.level, level.audio, level.scene}) {
			if (wad) {
				verify_fatal(wad->header.size() > 0);
				toc_size.sectors += Sector32::size_from_bytes(wad->header.size()).sectors;
			}
		}
	}
	return toc_size;
}

static SectorRange add_sector_range(SectorRange range, Sector32 lsn)
{
	return {{range.offset.sectors + lsn.sectors}, range.size};
}

static SectorByteRange add_sector_byte_range(SectorByteRange range, Sector32 lsn)
{
	return {{range.offset.sectors + lsn.sectors}, range.size_bytes};
}

static SectorRange sub_sector_range(SectorRange range, Sector32 lsn)
{
	return {{range.offset.sectors - lsn.sectors}, range.size};
}

static SectorByteRange sub_sector_byte_range(SectorByteRange range, Sector32 lsn)
{
	return {{range.offset.sectors - lsn.sectors}, range.size_bytes};
}
