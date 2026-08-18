/*
	wrench - A set of modding tools for the Ratchet & Clank PS2 games.
	Copyright (C) 2019-2021 chaoticgd

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

#include "iso_filesystem.h"

#include <core/filesystem.h>

packed_struct(IsoPathTableEntry,
	u8 identifier_length;
	u8 record_length;
	u32 lba;
	u16 parent;
)

void read_directory_record(IsoDirectory& dest, Buffer src, s64 ofs, size_t size, size_t depth);

IsoFilesystem read_iso_filesystem(InputStream& src)
{
	src.seek(0);
	std::vector<u8> filesystem_buf = src.read_multiple<u8>(MAX_FILESYSTEM_SIZE_BYTES);
	IsoFilesystem filesystem;
	if (!read_iso_filesystem(filesystem, Buffer(filesystem_buf))) {
		fprintf(stderr, "error: Missing or invalid ISO filesystem!\n");
		exit(1);
	}
	return filesystem;
}

bool read_iso_filesystem(IsoFilesystem& dest, Buffer src)
{
	IsoFilesystem filesystem;
	
	filesystem.pvd = src.read<IsoPrimaryVolumeDescriptor>(0x10 * SECTOR_SIZE, "primary volume descriptor");
	if (filesystem.pvd.volume_descriptor_type != 0x01) {
		return false;
	}
	if (memcmp(filesystem.pvd.standard_identifier, "CD001", 5) != 0) {
		return false;
	}
	if (filesystem.pvd.root_directory.data_length.lsb > 0x10000) {
		return false;
	}
	
	size_t root_dir_ofs = filesystem.pvd.root_directory.lba.lsb * SECTOR_SIZE;
	size_t root_dir_size = filesystem.pvd.root_directory.data_length.lsb;
	read_directory_record(filesystem.root, src, root_dir_ofs, root_dir_size, 0);
	
	dest = std::move(filesystem);
	
	return true;
}

void read_directory_record(IsoDirectory& dest, Buffer src, s64 ofs, size_t size, size_t depth)
{
	verify(depth <= 8, "Depth limit (8 levels) reached!");
	
	s64 end = ofs + size;
	
	size_t i;
	for (i = 0; i < 1000 && ofs < end; i++) {
		s64 record_ofs = ofs;
		auto& record = src.read<IsoDirectoryRecord>(ofs, "directory record");
		ofs += sizeof(IsoDirectoryRecord);
		if (record.record_length < 1) {
			ofs = record_ofs + 1;
			continue;
		}
		if (record.file_flags & 2) {
			if (i < 2) {
				// Skip dot and dot dot.
				ofs = record_ofs + record.record_length;
				continue;
			}
			IsoDirectory subdir;
			subdir.name = src.read_fixed_string(ofs, record.identifier_length);
			ofs += record.identifier_length;
			for (char& c : subdir.name) {
				c = tolower(c);
			}
			read_directory_record(subdir, src, record.lba.lsb * SECTOR_SIZE, record.data_length.lsb, depth + 1);
			dest.subdirs.push_back(subdir);
		} else if (record.identifier_length >= 2) {
			IsoFileRecord file;
			file.name = src.read_fixed_string(ofs, record.identifier_length);
			ofs += record.identifier_length;
			for (char& c : file.name) {
				c = tolower(c);
			}
			if (file.name.size() >= 2 && file.name[file.name.size() - 2] == ';' && file.name[file.name.size() - 1] == '1') {
				file.name = file.name.substr(0, file.name.size() - 2);
			}
			file.lba = {(s32) record.lba.lsb};
			file.size = record.data_length.lsb;
			dest.files.push_back(file);
		}
		ofs = record_ofs + record.record_length;
	}
	verify(i != 1000, "Iteration limit exceeded while reading directory!");
}

static void copy_and_pad(char* dest, const char* src, size_t size)
{
	size_t i;
	for (i = 0; i < strlen(src); i++) {
		dest[i] = src[i];
	}
	for (; i < size; i++) {
		dest[i] = ' ';
	}
}

// Per ECMA-119, an "unspecified" PVD date/time field is 16 ASCII '0'
// characters followed by a zero GMT offset byte -- NOT 17 raw zero bytes
// (which is what a naive {0}-initialized struct produces). Use this for
// both real dates and the unspecified/zero-filled fields so the encoding
// is always correct.
static IsoPvdDateTime make_pvd_date_time(
	int year, int month, int day, int hour, int minute, int second, int hundredths, s8 time_zone)
{
	IsoPvdDateTime dt;
	char buffer[17];
	snprintf(buffer, sizeof(buffer), "%04d%02d%02d%02d%02d%02d%02d",
		year, month, day, hour, minute, second, hundredths);
	memcpy(&dt, buffer, 16);
	dt.time_zone = time_zone;
	return dt;
}

static void flatten_subdirs(std::vector<IsoDirectory*>* flat_dirs, IsoDirectory* dir)
{
	for (IsoDirectory& subdir : dir->subdirs) {
		subdir.parent = dir;
		subdir.index = flat_dirs->size();
		subdir.parent_index = dir->index;
		flat_dirs->push_back(&subdir);
		flatten_subdirs(flat_dirs, &subdir);
	}
}

static void write_directory_records(OutputStream& dest, const IsoDirectory& dir);
static void write_directory_record(OutputStream& dest, const IsoFileRecord& file, u8 flags);

void write_iso_filesystem(OutputStream& dest, IsoDirectory* root_dir, const IsoPvdStrings* pvd_strings)
{
	dest.seek(16 * SECTOR_SIZE);
	
	IsoPvdDateTime zeroed_datetime = {{0}};
	
	// Write out primary volume descriptor.
	size_t pvd_pos = dest.tell();
	IsoPrimaryVolumeDescriptor pvd;
	defer([&]() {
		size_t pos = dest.tell();
		dest.seek(pvd_pos);
		dest.write(pvd);
		dest.seek(pos);
	});
	dest.seek(dest.tell() + sizeof(pvd));
	pvd.volume_descriptor_type = 0x01;
	memcpy(pvd.standard_identifier, "CD001", sizeof(pvd.standard_identifier));
	pvd.volume_descriptor_version = 1;
	pvd.unused_7 = 0;
	copy_and_pad(pvd.system_identifier,
		pvd_strings ? pvd_strings->system_identifier.c_str() : "WRENCH", sizeof(pvd.system_identifier));
	copy_and_pad(pvd.volume_identifier,
		pvd_strings ? pvd_strings->volume_identifier.c_str() : "WRENCH", sizeof(pvd.volume_identifier));
	memset(pvd.unused_48, 0, 8);
	pvd.volume_space_size = IsoLsbMsb32::from_scalar(0);
	memset(pvd.unused_58, 0, 32);
	pvd.volume_set_size = IsoLsbMsb16::from_scalar(1);
	pvd.volume_sequence_number = IsoLsbMsb16::from_scalar(1);
	pvd.logical_block_size = IsoLsbMsb16::from_scalar(SECTOR_SIZE);
	pvd.path_table_size = {0, 0};
	pvd.l_path_table = 0;
	pvd.optional_l_path_table = 0;
	pvd.m_path_table = 0;
	pvd.optional_m_path_table = 0;
	pvd.root_directory.record_length = 0x22;
	pvd.root_directory.extended_attribute_record_length = 0;
	// Same disc-mastering date as write_directory_record() -- see the
	// comment there. This is a separate embedded copy of the root
	// directory's own record stored directly in the PVD.
	pvd.root_directory.recording_date_time.years_since_1900 = 102;
	pvd.root_directory.recording_date_time.month = 10;
	pvd.root_directory.recording_date_time.day = 7;
	pvd.root_directory.recording_date_time.hour = 20;
	pvd.root_directory.recording_date_time.minute = 6;
	pvd.root_directory.recording_date_time.second = 57;
	pvd.root_directory.recording_date_time.time_zone = 36;
	pvd.root_directory.file_flags = 2;
	pvd.root_directory.file_unit_size = 0;
	pvd.root_directory.interleave_gap_size = 0;
	pvd.root_directory.volume_sequence_number = IsoLsbMsb16::from_scalar(1);
	pvd.root_directory.identifier_length = 1;
	pvd.root_directory_pad = 0;
	copy_and_pad(pvd.volume_set_identifier,
		pvd_strings ? pvd_strings->volume_set_identifier.c_str() : "", sizeof(pvd.volume_set_identifier));
	copy_and_pad(pvd.publisher_identifier,
		pvd_strings ? pvd_strings->publisher_identifier.c_str() : "", sizeof(pvd.publisher_identifier));
	copy_and_pad(pvd.data_preparer_identifier,
		pvd_strings ? pvd_strings->data_preparer_identifier.c_str() : "", sizeof(pvd.data_preparer_identifier));
	copy_and_pad(pvd.application_identifier,
		pvd_strings ? pvd_strings->application_identifier.c_str() : "", sizeof(pvd.application_identifier));
	copy_and_pad(pvd.copyright_file_identifier,
		pvd_strings ? pvd_strings->copyright_file_identifier.c_str() : "", sizeof(pvd.copyright_file_identifier));
	copy_and_pad(pvd.abstract_file_identifier,
		pvd_strings ? pvd_strings->abstract_file_identifier.c_str() : "", sizeof(pvd.abstract_file_identifier));
	copy_and_pad(pvd.bibliographic_file_identifier,
		pvd_strings ? pvd_strings->bibliographic_file_identifier.c_str() : "", sizeof(pvd.bibliographic_file_identifier));
	// Same disc-mastering date as write_directory_record()/the embedded root
	// directory record above -- see the comment there. Only the creation
	// date is actually populated on the original disc; the other three are
	// left in the spec's "unspecified" (all-ASCII-zero) representation.
	pvd.volume_creation_date_time = make_pvd_date_time(2002, 10, 7, 20, 6, 57, 0, 36);
	pvd.volume_modification_date_time = make_pvd_date_time(0, 0, 0, 0, 0, 0, 0, 0);
	pvd.volume_expiration_date_time = make_pvd_date_time(0, 0, 0, 0, 0, 0, 0, 0);
	pvd.volume_effective_date_time = make_pvd_date_time(0, 0, 0, 0, 0, 0, 0, 0);
	pvd.file_structure_version = 1;
	pvd.unused_372 = 0;
	memset(pvd.application_use, 0, sizeof(pvd.application_use));
	memset(pvd.reserved, 0, sizeof(pvd.reserved));
	
	dest.pad(SECTOR_SIZE, 0);
	static const u8 volume_desc_set_terminator[] = {0xff, 'C', 'D', '0', '0', '1', 0x01};
	dest.write_n(volume_desc_set_terminator, sizeof(volume_desc_set_terminator));
	
	// UDF "bridge" marker sectors. Retail PS2 discs are mastered as
	// ISO9660+UDF bridge discs; these three sectors declare the UDF
	// extended area per the ECMA-167 bridge format convention.
	dest.pad(SECTOR_SIZE, 0);
	static const u8 bea01[] = {0x00, 'B', 'E', 'A', '0', '1', 0x01};
	dest.write_n(bea01, sizeof(bea01));
	dest.pad(SECTOR_SIZE, 0);
	static const u8 nsr02[] = {0x00, 'N', 'S', 'R', '0', '2', 0x01};
	dest.write_n(nsr02, sizeof(nsr02));
	dest.pad(SECTOR_SIZE, 0);
	static const u8 tea01[] = {0x00, 'T', 'E', 'A', '0', '1', 0x01};
	dest.write_n(tea01, sizeof(tea01));
	
	// Root cause #6: real UDF Volume Descriptor Sequence. The bridge markers
	// above are necessary but not sufficient -- the retail disc has an actual
	// (near-empty) UDF filesystem layered on top of the ISO9660 one. These
	// sectors are decoded byte-for-byte from the retail disc (ECMA-167):
	// sectors 32-37 = Main VDS (PVD, Implementation Use VD, Partition
	// Descriptor, Logical Volume Descriptor, Unallocated Space Descriptor,
	// Terminating Descriptor), sectors 48-53 = identical Reserve VDS copy,
	// sectors 64-65 = Logical Volume Integrity Descriptor + Terminating
	// Descriptor, sector 256 = Anchor Volume Descriptor Pointer (points back
	// at the Main/Reserve VDS extents at LBA 32/48, length 32768 bytes each).
	// Each array below holds only the nonzero prefix of its sector; the
	// remainder is zero padding, written explicitly below via dest.pad()
	// or the zeroed_sector loops.
	//
	// This UDF filesystem does not reference any packed file content itself
	// (root causes #4/#5 are unrelated) -- it is pure, disc-invariant
	// metadata, so unlike the ISO9660 tree below it is safe to hardcode
	// wholesale rather than derive procedurally.
#include "udf_vds_sectors.inl"
	
	// Pad from wherever we are (right after the TEA01 marker -- tea01 is only
	// 7 bytes and unaligned) up to sector 32 where the retail disc's real UDF
	// Main VDS begins. Must sector-align first, or the whole-sector loop
	// below overshoots the target instead of landing on it exactly.
	dest.pad(SECTOR_SIZE, 0);
	{
		static const u8 zeroed_sector_pre[SECTOR_SIZE] = {0};
		while (dest.tell() < 32 * SECTOR_SIZE) {
			dest.write_n(zeroed_sector_pre, sizeof(zeroed_sector_pre));
		}
	}
	verify(dest.tell() == 32 * SECTOR_SIZE, "UDF VDS alignment sanity check failed.");
	dest.write_n(udf_sector_32, sizeof(udf_sector_32)); dest.pad(SECTOR_SIZE, 0);
	dest.write_n(udf_sector_33, sizeof(udf_sector_33)); dest.pad(SECTOR_SIZE, 0);
	dest.write_n(udf_sector_34, sizeof(udf_sector_34)); dest.pad(SECTOR_SIZE, 0);
	dest.write_n(udf_sector_35, sizeof(udf_sector_35)); dest.pad(SECTOR_SIZE, 0);
	dest.write_n(udf_sector_36, sizeof(udf_sector_36)); dest.pad(SECTOR_SIZE, 0);
	dest.write_n(udf_sector_37, sizeof(udf_sector_37));
	dest.pad(SECTOR_SIZE, 0);
	{
		static const u8 zeroed_sector_a[SECTOR_SIZE] = {0};
		while (dest.tell() < 48 * SECTOR_SIZE) {
			dest.write_n(zeroed_sector_a, sizeof(zeroed_sector_a));
		}
	}
	dest.write_n(udf_sector_48, sizeof(udf_sector_48)); dest.pad(SECTOR_SIZE, 0);
	dest.write_n(udf_sector_49, sizeof(udf_sector_49)); dest.pad(SECTOR_SIZE, 0);
	dest.write_n(udf_sector_50, sizeof(udf_sector_50)); dest.pad(SECTOR_SIZE, 0);
	dest.write_n(udf_sector_51, sizeof(udf_sector_51)); dest.pad(SECTOR_SIZE, 0);
	dest.write_n(udf_sector_52, sizeof(udf_sector_52)); dest.pad(SECTOR_SIZE, 0);
	dest.write_n(udf_sector_53, sizeof(udf_sector_53));
	dest.pad(SECTOR_SIZE, 0);
	{
		static const u8 zeroed_sector_b[SECTOR_SIZE] = {0};
		while (dest.tell() < 64 * SECTOR_SIZE) {
			dest.write_n(zeroed_sector_b, sizeof(zeroed_sector_b));
		}
	}
	dest.write_n(udf_sector_64, sizeof(udf_sector_64)); dest.pad(SECTOR_SIZE, 0);
	dest.write_n(udf_sector_65, sizeof(udf_sector_65));
	dest.pad(SECTOR_SIZE, 0);
	{
		static const u8 zeroed_sector_c[SECTOR_SIZE] = {0};
		while (dest.tell() < 256 * SECTOR_SIZE) {
			dest.write_n(zeroed_sector_c, sizeof(zeroed_sector_c));
		}
	}
	dest.write_n(udf_sector_256, sizeof(udf_sector_256));
	
	// It seems like the path table is always expected to be at this LBA even if
	// we write a different one into the PVD. Maybe it's hardcoded?
	dest.pad(SECTOR_SIZE, 0);
	static const u8 zeroed_sector[SECTOR_SIZE] = {0};
	while (dest.tell() < 0x101 * SECTOR_SIZE) {
		dest.write_n(zeroed_sector, sizeof(zeroed_sector));
	}
	
	// Get a linear list of all the directories. This also sets the parent
	// pointers and indices.
	std::vector<IsoDirectory*> flat_dirs;
	flatten_subdirs(&flat_dirs, root_dir);
	
	// Fixup the file names.
	for (char& c : root_dir->name) {
		c = toupper(c);
	}
	for (IsoFileRecord& file : root_dir->files) {
		for (char& c : file.name) {
			c = toupper(c);
		}
		file.name += ";1";
	}
	for (IsoDirectory* dir : flat_dirs) {
		for (char& c : dir->name) {
			c = toupper(c);
		}
		for (IsoFileRecord& file : dir->files) {
			for (char& c : file.name) {
				c = toupper(c);
			}
			file.name += ";1";
		}
	}
	
	// Determine the LBAs of the path table and the root directory.
	dest.pad(SECTOR_SIZE, 0);
	pvd.l_path_table = dest.tell() / SECTOR_SIZE;
	pvd.optional_l_path_table = pvd.l_path_table + 1;
	pvd.m_path_table = byte_swap_32(pvd.l_path_table + 2);
	pvd.optional_m_path_table = byte_swap_32(pvd.l_path_table + 3);
	pvd.root_directory.lba = IsoLsbMsb32::from_scalar(pvd.l_path_table + 4);
	
	// Determine directory record LBAs and sizes.
	size_t next_dir_lba = pvd.root_directory.lba.lsb;
	root_dir->lba = {(s32) pvd.root_directory.lba.lsb};
	std::vector<u8> root_dummy_vec;
	MemoryOutputStream root_dummy(root_dummy_vec);
	write_directory_records(root_dummy, *root_dir);
	root_dir->size = root_dummy.size();
	pvd.root_directory.data_length = IsoLsbMsb32::from_scalar(root_dir->size);
	next_dir_lba += Sector32::size_from_bytes(root_dir->size).sectors;
	for (size_t i = 0; i < flat_dirs.size(); i++) {
		IsoDirectory* dir = flat_dirs[i];
		dir->lba = {(s32) next_dir_lba};
		std::vector<u8> dummy_vec;
		MemoryOutputStream dummy(dummy_vec);
		write_directory_records(dummy, *dir);
		dir->size = dummy.size();
		next_dir_lba += Sector32::size_from_bytes(dir->size).sectors;
	}
	
	// Write out little endian path table. The original retail discs write two
	// identical copies of each path table (a primary and an "optional"
	// redundant copy); reproduce that here so the on-disc layout matches,
	// even though only the primary copies are ever actually read back.
	auto write_le_path_table = [&]() {
		size_t start = dest.tell();
		IsoPathTableEntry root_pte_lsb;
		root_pte_lsb.identifier_length = 1;
		root_pte_lsb.record_length = 0;
		root_pte_lsb.lba = pvd.root_directory.lba.lsb;
		root_pte_lsb.parent = 1;
		dest.write(root_pte_lsb);
		dest.write<u8>(0); // identifier
		dest.write<u8>(0); // pad
		for (IsoDirectory* dir : flat_dirs) {
			IsoPathTableEntry root_pte;
			root_pte.identifier_length = dir->name.size();
			root_pte.record_length = 0;
			root_pte.lba = dir->lba.sectors;
			root_pte.parent = dir->parent_index;
			dest.write(root_pte);
			dest.write_n((u8*) dir->name.data(), dir->name.size());
			if (root_pte.identifier_length % 2 == 1) {
				dest.write<u8>(0); // pad
			}
		}
		return dest.tell() - start;
	};
	auto write_be_path_table = [&]() {
		// The M (big-endian) path table's "parent" field is a plain u16 in
		// the struct (native/little-endian in memory) but must be written
		// big-endian on disk here, same as the lba field just above it --
		// this was being left un-byte-swapped, corrupting every M-table
		// parent index whose value didn't happen to be byte-palindromic.
		IsoPathTableEntry root_pte_msb;
		root_pte_msb.identifier_length = 1;
		root_pte_msb.record_length = 0;
		root_pte_msb.lba = pvd.root_directory.lba.msb;
		root_pte_msb.parent = byte_swap_16(1);
		dest.write(root_pte_msb);
		dest.write<u8>(0); // identifier
		dest.write<u8>(0); // pad
		for (IsoDirectory* dir : flat_dirs) {
			IsoPathTableEntry root_pte;
			root_pte.identifier_length = dir->name.size();
			root_pte.record_length = 0;
			root_pte.lba = byte_swap_32(dir->lba.sectors);
			root_pte.parent = byte_swap_16(dir->parent_index);
			dest.write(root_pte);
			dest.write_n((u8*) dir->name.data(), dir->name.size());
			if (root_pte.identifier_length % 2 == 1) {
				dest.write<u8>(0); // pad
			}
		}
	};
	
	size_t path_table_size = write_le_path_table();
	pvd.path_table_size = IsoLsbMsb32::from_scalar(path_table_size);
	
	// Optional (duplicate) little endian path table.
	dest.pad(SECTOR_SIZE, 0);
	write_le_path_table();
	
	// Write out big endian path table.
	dest.pad(SECTOR_SIZE, 0);
	write_be_path_table();
	
	// Optional (duplicate) big endian path table.
	dest.pad(SECTOR_SIZE, 0);
	write_be_path_table();
	
	// Write out all the directories.
	dest.pad(SECTOR_SIZE, 0);
	verify_fatal(dest.tell() == pvd.root_directory.lba.lsb * SECTOR_SIZE);
	write_directory_records(dest, *root_dir);
	for (IsoDirectory* dir : flat_dirs) {
		dest.pad(SECTOR_SIZE, 0);
		write_directory_records(dest, *dir);
	}
	
	// Root cause #6, continued: the rest of the UDF filesystem. Sectors
	// 262-268 on the retail disc hold the UDF File Set Descriptor (262), a
	// Terminating Descriptor (263), File Identifier Descriptors for the UDF
	// root directory's entries (264), and 4 File Entries -- one for the UDF
	// root directory plus one each for SYSTEM.CNF, the boot ELF, and
	// IOPRP243.IMG (265-268). Decoded byte-for-byte from the retail disc,
	// same discipline as the sectors-32-65/256 UDF VDS content above: this
	// is pure filesystem metadata, safe to hardcode wholesale. The File
	// Entries reference the named files' real LBAs (289/290/965), which
	// this packer now also produces (see the RAC file/ToC ordering fix in
	// iso_packer.cpp), so the hardcoded bytes should already be correct
	// without further adjustment -- verified via cmp after this was written.
#include "udf_fsd_sectors.inl"
	
	dest.pad(SECTOR_SIZE, 0);
	verify_fatal(dest.tell() == 262 * SECTOR_SIZE);
	dest.write_n(udf_sector_262, sizeof(udf_sector_262)); dest.pad(SECTOR_SIZE, 0);
	dest.write_n(udf_sector_263, sizeof(udf_sector_263)); dest.pad(SECTOR_SIZE, 0);
	dest.write_n(udf_sector_264, sizeof(udf_sector_264)); dest.pad(SECTOR_SIZE, 0);
	dest.write_n(udf_sector_265, sizeof(udf_sector_265)); dest.pad(SECTOR_SIZE, 0);
	dest.write_n(udf_sector_266, sizeof(udf_sector_266)); dest.pad(SECTOR_SIZE, 0);
	dest.write_n(udf_sector_267, sizeof(udf_sector_267)); dest.pad(SECTOR_SIZE, 0);
	dest.write_n(udf_sector_268, sizeof(udf_sector_268)); dest.pad(SECTOR_SIZE, 0);
	// Do NOT pad/write anything further here -- sectors 269-288 are
	// genuinely zero on the retail disc, and real file content (SYSTEM.CNF
	// etc, starting at sector 289) is already written by the earlier,
	// separate pack_iso() content-writing pass. Padding past sector 268
	// would overwrite that content with zeros.
}

static void write_directory_records(OutputStream& dest, const IsoDirectory& dir)
{
	// Either this is being written out to a dummy stream to calculate the space
	// required for the directory record, or this should be being written out at
	// the correct LBA.
	verify_fatal(dest.tell() == 0 || dest.tell() == dir.lba.bytes());
	IsoFileRecord dot = {"", dir.lba, dir.size};
	write_directory_record(dest, dot, 2);
	IsoFileRecord dot_dot = {"\x01", dir.lba, dir.size};
	if (dir.parent != nullptr) {
		dot_dot.lba = dir.parent->lba;
		dot_dot.size = dir.parent->size;
	}
	write_directory_record(dest, dot_dot, 2);
	for (const IsoFileRecord& file : dir.files) {
		write_directory_record(dest, file, 0);
	}
	for (const IsoDirectory& dir : dir.subdirs) {
		IsoFileRecord record = {dir.name, dir.lba, dir.size};
		record.modified_time = fs::file_time_type::clock::now();
		write_directory_record(dest, record, 2);
	}
}

static void write_directory_record(OutputStream& dest, const IsoFileRecord& file, u8 flags)
{
	// Retail PS2 discs append a fixed 14-byte zero-filled "system use" field
	// to every directory record (root, ".", "..", and each file). Wrench's
	// own records need to match that length exactly to reproduce the
	// original's directory/path table layout byte-for-byte.
	static const size_t SYSTEM_USE_SIZE = 14;
	
	IsoDirectoryRecord record = {0};
	record.record_length =
		sizeof(IsoDirectoryRecord) +
		file.name.size() +
		(file.name.size() % 2 == 0) +
		SYSTEM_USE_SIZE;
	record.extended_attribute_record_length = 0;
	record.lba = IsoLsbMsb32::from_scalar(file.lba.sectors);
	record.data_length = IsoLsbMsb32::from_scalar(file.size);
	
	// Retail PS2 discs stamp the root/"."/".." records with a single
	// disc-mastering timestamp (2002-10-07 20:06:57 GMT+9) -- but each of
	// the 3 named files (SYSTEM.CNF, the boot ELF, IOPRP243.IMG) carries
	// its own distinct, real per-file modification date, not that same
	// mastering date. All 4 dates below were decoded directly from the
	// retail SCES_509.16 (EU v1.00) disc's own directory records.
	// TODO: if this packer is ever used to faithfully reproduce a
	// *different* original disc, this will need to become configurable
	// rather than hardcoded.
	IsoDirectoryDateTime& dt = record.recording_date_time;
	if (flags == 0 && file.name.find("SYSTEM.CNF") != std::string::npos) {
		// 2002-08-24 08:13:00 GMT+9
		dt.years_since_1900 = 102; dt.month = 8; dt.day = 24;
		dt.hour = 8; dt.minute = 13; dt.second = 0; dt.time_zone = 36;
	} else if (flags == 0 && file.name.find("IOPRP243.IMG") != std::string::npos) {
		// 2002-07-02 00:47:48 GMT+9
		dt.years_since_1900 = 102; dt.month = 7; dt.day = 2;
		dt.hour = 0; dt.minute = 47; dt.second = 48; dt.time_zone = 36;
	} else if (flags == 0) {
		// The boot ELF (e.g. "SCES_509.16", no ";1" suffix -- it's the only
		// named file besides SYSTEM.CNF/IOPRP243.IMG, so this is a safe
		// catch-all rather than a third explicit name match).
		// 2002-10-06 22:04:46 GMT+9
		dt.years_since_1900 = 102; dt.month = 10; dt.day = 6;
		dt.hour = 22; dt.minute = 4; dt.second = 46; dt.time_zone = 36;
	} else {
		// Root / "." / ".." -- the disc-mastering date.
		dt.years_since_1900 = 102; dt.month = 10; dt.day = 7;
		dt.hour = 20; dt.minute = 6; dt.second = 57; dt.time_zone = 36;
	}
	
	record.file_flags = flags;
	record.file_unit_size = 0;
	record.interleave_gap_size = 0;
	record.volume_sequence_number = IsoLsbMsb16::from_scalar(1);
	record.identifier_length = file.name.size() + (file.name.size() == 0);
	if ((dest.tell() % SECTOR_SIZE) + record.record_length > SECTOR_SIZE) {
		// Directory records cannot cross sector boundaries.
		dest.pad(SECTOR_SIZE, 0);
	}
	dest.write(record);
	dest.write_n((u8*) file.name.data(), file.name.size());
	if (file.name.size() % 2 == 0) {
		dest.write<u8>(0);
	}
	static const u8 system_use[SYSTEM_USE_SIZE] = {0};
	dest.write_n(system_use, SYSTEM_USE_SIZE);
}

void print_file_record(const IsoFileRecord& record)
{
	printf("%-16ld%-16ld%s\n", (size_t) record.lba.sectors, (size_t) record.size, record.name.c_str());
}
