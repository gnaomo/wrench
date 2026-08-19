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

#ifndef CORE_GLTF_H
#define CORE_GLTF_H

#include <core/buffer.h>
#include <core/mesh.h>
#include <core/material.h>

namespace GLTF {

using SceneIndex = s32;
using NodeIndex = s32;
using AnimationIndex = s32;
using MeshIndex = s32;
using MaterialIndex = s32;
using TextureIndex = s32;
using ImageIndex = s32;
using SamplerIndex = s32;
using SkinIndex = s32;

struct Asset
{
	Opt<std::string> copyright;
	Opt<std::string> generator;
	std::string version;
	Opt<std::string> min_version;
};

struct Scene
{
	std::vector<NodeIndex> nodes;
	Opt<std::string> name;
};

struct Node
{
	// unimplemented: camera
	std::vector<NodeIndex> children;
	Opt<SkinIndex> skin;
	Opt<glm::mat4> matrix;
	Opt<MeshIndex> mesh;
	Opt<glm::vec4> rotation;
	Opt<glm::vec3> scale;
	Opt<glm::vec3> translation;
	// unimplemented: weights
	Opt<std::string> name;
};

struct AnimationAttributes
{
	glm::vec3 translation = {0.f, 0.f, 0.f};
	glm::vec4 rotation = {0.f, 0.f, 0.f, 0.f};
	glm::vec3 scale = {0.f, 0.f, 0.f};
};

struct AnimationChannelGroup
{
	s32 node;
	std::vector<AnimationAttributes> frames;
};

struct Animation
{
	Opt<std::string> name;
	std::vector<AnimationChannelGroup> channel_groups;
	std::vector<SamplerIndex> sampler_input;
};

struct TextureInfo
{
	TextureIndex index;
	Opt<s32> tex_coord;
};

struct MaterialPbrMetallicRoughness
{
	Opt<glm::vec4> base_color_factor;
	Opt<TextureInfo> base_color_texture;
	// unimplemented: metallicFactor
	// unimplemented: roughnessFactor
	// unimplemented: metallicRoughnessTexture
};

enum MaterialAlphaMode
{
	OPAQUE,
	MASK,
	BLEND
};

struct Material
{
	Opt<std::string> name;
	Opt<MaterialPbrMetallicRoughness> pbr_metallic_roughness;
	// unimplemented: normalTexture
	// unimplemented: occlusionTexture
	// unimplemented: emissiveTexture
	// unimplemented: emissiveFactor
	Opt<MaterialAlphaMode> alpha_mode;
	// unimplemented: alphaCutoff
	Opt<bool> double_sided;
};

enum MeshPrimitiveAttribute
{
	POSITION = 1 << 0,
	TEXCOORD_0 = 1 << 1,
	NORMAL = 1 << 2,
	COLOR_0 = 1 << 3,
	JOINTS_0 = 1 << 4,
	WEIGHTS_0 = 1 << 5
};

enum MeshPrimitiveMode
{
	POINTS = 0,
	LINES = 1,
	LINE_LOOP = 2,
	LINE_STRIP = 3,
	TRIANGLES = 4,
	TRIANGLE_STRIP = 5,
	TRIANGLE_FAN = 6
};

struct MeshPrimitive
{
	u32 attributes_bitfield = 0;
	std::vector<s32> indices;
	Opt<MaterialIndex> material;
	Opt<MeshPrimitiveMode> mode;
	// unimplemented: targets
};

struct Mesh
{
	Opt<std::string> name;
	std::vector<MeshPrimitive> primitives;
	std::vector<Vertex> vertices;
	// unimplemented: weights
};

struct Texture
{
	Opt<SamplerIndex> sampler;
	Opt<s32> source;
	Opt<std::string> name;
};

struct Image
{
	Opt<std::string> uri;
	Opt<std::string> mime_type;
	Opt<s32> buffer_view;
	Opt<std::string> name;
};

struct Skin
{
	std::vector<glm::mat4> inverse_bind_matrices;
	Opt<s32> skeleton;
	std::vector<s32> joints;
	Opt<std::string> name;
};

struct Sampler
{
	Opt<s32> mag_filter;
	Opt<s32> min_filter;
	Opt<s32> wrap_s;
	Opt<s32> wrap_t;
	Opt<std::string> name;
};

struct ModelFile
{
	Asset asset;
	std::vector<std::string> extensions_used;
	std::vector<std::string> extensions_required;
	// unimplemented: extensions
	// unimplemented: extras
	Opt<s32> scene;
	std::vector<Scene> scenes;
	std::vector<Node> nodes;
	// unimplemented: cameras
	std::vector<Animation> animations;
	std::vector<Mesh> meshes;
	std::vector<Material> materials;
	std::vector<Texture> textures;
	std::vector<Image> images;
	std::vector<Sampler> samplers;
	std::vector<Skin> skins;
};

struct DefaultScene
{
	ModelFile gltf;
	Scene* scene;
};

// Parse a .glb file from memory.
ModelFile read_glb(Buffer src);

// Create a .glb file in memory.
std::vector<u8> write_glb(const ModelFile& gltf);

// Create a model file with a single scene in it.
DefaultScene create_default_scene(const char* generator);

// Lookup glTF objects by their name.
Node* lookup_node(ModelFile& gltf, const char* name);
Mesh* lookup_mesh(ModelFile& gltf, const char* name);
Material* lookup_material(ModelFile& gltf, const char* name);

// Deduplicate identical vertices and update the index buffer accordingly. This
// is done automatically when meshes are imported.
void deduplicate_vertices(Mesh& mesh);

// Clean up meshes that have just been converted from triangle strips to lists.
void remove_zero_area_triangles(Mesh& mesh);
void fix_winding_orders_of_triangles_based_on_normals(Mesh& mesh);

// Rewrite material indices so they point into the provided materials array.
void map_gltf_materials_to_wrench_materials(ModelFile& gltf, const std::vector<::Material>& materials);

// Converts a plain triangulated GLTF::Mesh (e.g. a hero collision group) into
// a plain (native) Mesh, without any material remapping. This was originally
// collision-specific (collision_mesh.h/.cpp) but the conversion itself has no
// collision-specific logic, so it's shared here for reuse by other asset
// types that migrated off COLLADA (ties, tfrags).
::Mesh gltf_mesh_to_native_mesh(const Mesh& mesh);

// Converts a plain (native) Mesh -- which may contain quads -- into a
// GLTF::Mesh for serialisation to a .glb file, triangulating any quads and
// adding/reusing materials in gltf as required.
//
// glTF has no native quad primitive mode, so quads are split into two
// triangles here. This is a one-way simplification: once a mesh has been
// round-tripped through glTF, faces that used to be packed as quads will
// always be rebuilt as pairs of triangles instead. For collision meshes that
// only costs a little storage efficiency (see reduce_quads_to_tris/
// optimise_collision in engine/collision.cpp) and doesn't change in-game
// collision behaviour, since the octree accepts either representation --
// callers for other asset types should check whether an analogous packed
// format assumption holds before assuming the same is true for them.
//
// The output primitives always carry POSITION data, and additionally carry
// NORMAL/COLOR_0/TEXCOORD_0 data whenever the corresponding MESH_HAS_NORMALS/
// MESH_HAS_VERTEX_COLOURS/MESH_HAS_TEX_COORDS flag is set on the source mesh
// -- mirroring which <source> elements the old COLLADA writer used to emit
// (core/collada.cpp's write_geometries(), before that file was deleted once
// nothing called into it any more -- see wrench-roadmap.md Phase 1 item 5).
// Collision meshes don't set any of those flags so this is a no-op for them,
// but callers of other asset types (e.g. ties, tfrags) that do carry texture
// coordinates or normals need this to avoid silently dropping that data on
// the round trip through the .glb file.
Mesh native_mesh_to_gltf_mesh(
	ModelFile& gltf, const ::Mesh& mesh, const std::vector<::Material>& materials);

// When splitting a mesh up into packets, this is used to generate a new vertex
// buffer for each output mesh, and optionally to rewrite the index buffers of
// the mesh primitives appropriately.
void filter_vertices(Mesh& mesh, const std::vector<Vertex>& input_vertices, bool rewrite_indices);

// Check that various pairs of glTF objects are equal, used for testing.
void verify_meshes_equal(Mesh& lhs, Mesh& rhs, bool check_vertices, bool check_indices, const char* context);
void verify_mesh_primitives_equal(
	MeshPrimitive& lhs, MeshPrimitive& rhs, bool check_indices, const char* context);

}

#endif
