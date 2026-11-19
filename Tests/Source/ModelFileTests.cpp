#include "TestFramework.h"
#include "Asset/Model/ModelFile.h"
#include "Asset/Model/ModelImporter.h"
#include "Core/Util/StringUtil.h"
#if RD_EDITOR
#include "Editor/Core/AssetImporter.h"
#endif
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iterator>

using namespace rd;
namespace fs = std::filesystem;

namespace
{
	// テストごとに空のフォルダを作り、終わったら消す
	struct TempFolder
	{
		fs::path path;

		explicit TempFolder(const char* name)
		{
			path = fs::temp_directory_path() / std::format("rd_modelfile_{}_{}", GetCurrentProcessId(), name);
			fs::remove_all(path);
			fs::create_directories(path);
		}
		~TempFolder()
		{
			std::error_code ec;
			fs::remove_all(path, ec);
		}
		std::string File(const char* name) const { return ToUtf8((path / name).generic_wstring()); }
	};

	void WriteText(const fs::path& path, const std::string& text)
	{
		std::ofstream file(path, std::ios::binary);
		file << text;
	}

	// スキニングとアニメーションを含む小さなモデル
	ModelData MakeSkinnedModel()
	{
		ModelData data;
		MaterialDesc material;
		material.name = "Body";
		material.baseColor = Color(0.5f, 0.25f, 0.125f, 1.0f);
		material.roughness = 0.3f;
		material.albedoPath = "body.png";
		material.normalPath = "../shared/body_n.png";
		data.materials.push_back(material);

		data.vertices.resize(3);
		for (auto& v : data.vertices)
		{
			v.normal = Normalize(Vector3(0.2f, -0.7f, -0.4f));
			v.tangent = Vector4(Normalize(Vector3(-0.6f, 0.1f, 0.3f)), -1.0f);
		}
		data.vertices[1].position = Vector3(1.0f, 2.0f, 3.0f);
		data.vertices[2].uv = Vector2(0.25f, 0.75f);
		data.skins.resize(3);
		for (auto& s : data.skins) { s.weights[0] = 1.0f; }
		data.skins[1].joints[0] = 1;
		data.skins[1].joints[1] = 0;
		data.skins[1].weights[0] = 0.75f;
		data.skins[1].weights[1] = 0.25f;
		data.indices = { 0, 1, 2 };
		data.hasSkinning = true;
		data.bounds.Expand(Vector3(1.0f, 2.0f, 3.0f));

		SubMesh sub;
		sub.name = "Mesh";
		sub.indexCount = 3;
		sub.skinned = true;
		data.subMeshes.push_back(sub);

		data.skeleton = std::make_shared<Skeleton>();
		Joint root;
		root.name = "Root";
		Joint child;
		child.name = "Child";
		child.parent = 0;
		child.bindLocal.translation = Vector3(0.0f, 1.0f, 0.0f);
		data.skeleton->joints = { root, child };
		data.skeleton->nameToIndex = { { "Root", 0 }, { "Child", 1 } };

		auto clip = std::make_shared<AnimationClip>();
		clip->name = "Walk";
		clip->duration = 3.0f;
		AnimationChannel channel;
		channel.jointName = "Child";
		// 1 秒目のキーは前後と同じ値なので省かれ、スケールは全部同じなので 1 個になるはず
		channel.positions = { { 0.0f, Vector3(1, 0, 0) }, { 1.0f, Vector3(1, 0, 0) }, { 2.0f, Vector3(1, 0, 0) }, { 3.0f, Vector3(2, 0, 0) } };
		channel.scales = { { 0.0f, Vector3::kOne }, { 1.0f, Vector3::kOne }, { 3.0f, Vector3::kOne } };
		clip->channels.push_back(channel);
		data.clips.push_back(clip);
		return data;
	}
}

RD_TEST(ModelFile, RoundTripKeepsEverything)
{
	TempFolder folder("roundtrip");
	std::string path = folder.File("a.rdmodel");
	ModelData source = MakeSkinnedModel();
	std::string error;
	RD_REQUIRE(ModelFile::Write(path, source, error));

	ModelData loaded;
	RD_REQUIRE(ModelFile::Read(path, false, loaded, error));
	RD_CHECK(loaded.hasSkinning);
	RD_REQUIRE(loaded.vertices.size() == 3 && loaded.skins.size() == 3);
	for (size_t i = 0; i < 3; ++i)
	{
		const MeshVertex& a = source.vertices[i];
		const MeshVertex& b = loaded.vertices[i];
		// 位置と UV はそのまま、向きとウェイトは 16 ビットに丸めた分だけずれる
		RD_CHECK(std::memcmp(&a.position, &b.position, sizeof(Vector3)) == 0);
		RD_CHECK(std::memcmp(&a.uv, &b.uv, sizeof(Vector2)) == 0);
		RD_CHECK_NEAR(Dot(a.normal, b.normal), 1.0f, 1.0e-4f);
		RD_CHECK_NEAR(Dot(a.tangent.XYZ(), b.tangent.XYZ()), 1.0f, 1.0e-4f);
		RD_CHECK_NEAR(b.tangent.w, -1.0f, 0.0f);
		for (int k = 0; k < 4; ++k)
		{
			RD_CHECK_EQ(loaded.skins[i].joints[k], source.skins[i].joints[k]);
			RD_CHECK_NEAR(loaded.skins[i].weights[k], source.skins[i].weights[k], 1.0e-4f);
		}
	}
	RD_CHECK(loaded.indices == source.indices);
	RD_REQUIRE(loaded.subMeshes.size() == 1 && loaded.materials.size() == 1);
	RD_CHECK_EQ(loaded.subMeshes[0].name, std::string("Mesh"));
	RD_CHECK(loaded.subMeshes[0].skinned);
	RD_CHECK_NEAR(loaded.materials[0].roughness, 0.3f, 0.0f);
	RD_CHECK_EQ(loaded.materials[0].albedoPath, folder.File("body.png"));
	// 相対パスの ".." は解決され、法線マップには "#linear" が付く
	RD_CHECK_EQ(loaded.materials[0].normalPath, ToUtf8((folder.path.parent_path() / "shared/body_n.png").generic_wstring()) + "#linear");
	RD_REQUIRE(loaded.skeleton != nullptr);
	RD_CHECK_EQ(loaded.skeleton->Find("Child"), 1);
	RD_CHECK_EQ(loaded.skeleton->joints[1].parent, 0);
}

RD_TEST(ModelFile, RedundantKeysAreRemovedWithoutChangingThePose)
{
	TempFolder folder("keys");
	std::string path = folder.File("a.rdmodel");
	ModelData source = MakeSkinnedModel();
	std::string error;
	RD_REQUIRE(ModelFile::Write(path, source, error));
	ModelData loaded;
	RD_REQUIRE(ModelFile::Read(path, true, loaded, error));
	RD_REQUIRE(loaded.clips.size() == 1);
	const AnimationChannel& channel = loaded.clips[0]->channels[0];
	RD_CHECK_EQ(channel.positions.size(), size_t(3));
	RD_CHECK_EQ(channel.scales.size(), size_t(1));

	std::vector<int32_t> bindingA = source.clips[0]->BuildBinding(*source.skeleton);
	std::vector<int32_t> bindingB = loaded.clips[0]->BuildBinding(*loaded.skeleton);
	for (float time : { 0.0f, 0.5f, 1.0f, 1.9f, 2.5f, 3.0f })
	{
		std::vector<TransformTRS> a(2), b(2);
		source.clips[0]->Sample(time, bindingA, a);
		loaded.clips[0]->Sample(time, bindingB, b);
		RD_CHECK_NEAR(a[1].translation.x, b[1].translation.x, 1.0e-6f);
		RD_CHECK_NEAR(a[1].scale.y, b[1].scale.y, 1.0e-6f);
	}
}

RD_TEST(ModelFile, AnimationOnlySkipsGeometry)
{
	TempFolder folder("anim");
	std::string path = folder.File("a.rdmodel");
	std::string error;
	RD_REQUIRE(ModelFile::Write(path, MakeSkinnedModel(), error));
	ModelData loaded;
	RD_REQUIRE(ModelFile::Read(path, true, loaded, error));
	RD_CHECK(loaded.vertices.empty());
	RD_CHECK(loaded.indices.empty());
	RD_CHECK_EQ(loaded.clips.size(), size_t(1));
}

RD_TEST(ModelFile, DamagedFilesAreRejected)
{
	TempFolder folder("damaged");
	std::string path = folder.File("a.rdmodel");
	std::string error;
	RD_REQUIRE(ModelFile::Write(path, MakeSkinnedModel(), error));

	// どこで切れていても、落ちずに失敗を返す
	uintmax_t size = fs::file_size(ToWide(path));
	for (uintmax_t length = 0; length < size; length += 7)
	{
		fs::resize_file(ToWide(path), length);
		ModelData loaded;
		RD_CHECK(!ModelFile::Read(path, false, loaded, error));
	}

	WriteText(ToWide(path), "this is not a model file");
	ModelData loaded;
	RD_CHECK(!ModelFile::Read(path, false, loaded, error));
}

RD_TEST(ModelFile, FlippedBytesDoNotCrash)
{
	TempFolder folder("flipped");
	std::string path = folder.File("a.rdmodel");
	std::string error;
	RD_REQUIRE(ModelFile::Write(path, MakeSkinnedModel(), error));
	std::ifstream in(ToWide(path), std::ios::binary);
	std::string original((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	in.close();

	// 読めるかどうかは問わず、どのバイトが壊れていても安全に戻ってくること
	for (size_t i = 0; i < original.size(); ++i)
	{
		std::string damaged = original;
		damaged[i] = char(damaged[i] ^ 0x5A);
		WriteText(ToWide(path), damaged);
		ModelData loaded;
		ModelFile::Read(path, false, loaded, error);
	}
	RD_CHECK(true);
}

RD_TEST(ModelFile, LargeIndexJumpsSurvive)
{
	TempFolder folder("indices");
	std::string path = folder.File("a.rdmodel");
	ModelData source;
	source.materials.emplace_back();
	source.vertices.resize(70000);
	source.skins.resize(70000);
	source.indices = { 0, 69999, 1, 69998, 35000, 2 };
	SubMesh sub;
	sub.indexCount = 6;
	source.subMeshes.push_back(sub);
	std::string error;
	RD_REQUIRE(ModelFile::Write(path, source, error));
	ModelData loaded;
	RD_REQUIRE(ModelFile::Read(path, false, loaded, error));
	RD_CHECK(loaded.indices == source.indices);
	// 同じ値の並びは圧縮で大きく縮むはず
	RD_CHECK(fs::file_size(ToWide(path)) < 70000u * 29u / 10u);
}

RD_TEST(ModelFile, OutOfRangeIndicesAreRejected)
{
	TempFolder folder("range");
	std::string path = folder.File("a.rdmodel");
	ModelData source = MakeSkinnedModel();
	source.indices[2] = 99;
	std::string error;
	RD_REQUIRE(ModelFile::Write(path, source, error));
	ModelData loaded;
	RD_CHECK(!ModelFile::Read(path, false, loaded, error));
}

#if RD_EDITOR
namespace
{
	const char* const kTriangleObj =
		"mtllib tri.mtl\n"
		"v 0 0 0\nv 1 0 0\nv 0 1 0\n"
		"vt 0 0\nvt 1 0\nvt 0 1\n"
		"usemtl Red\n"
		"f 1/1 2/2 3/3\n";
	const char* const kTriangleMtl = "newmtl Red\nKd 1 0 0\nmap_Kd tri.png\n";

	void WriteTriangle(const fs::path& folder)
	{
		fs::create_directories(folder);
		WriteText(folder / "tri.obj", kTriangleObj);
		WriteText(folder / "tri.mtl", kTriangleMtl);
		WriteText(folder / "tri.png", "fake image");
	}
}

RD_TEST(AssetImporter, ModelBecomesRdmodelWithItsTexture)
{
	TempFolder source("import_src"), target("import_dst");
	WriteTriangle(source.path);
	RD_CHECK_EQ(AssetImporter::Import({ source.path / "tri.obj" }, ToUtf8(target.path.generic_wstring())), 1u);

	RD_CHECK(fs::exists(target.path / "tri.rdmodel"));
	RD_CHECK(fs::exists(target.path / "tri.png"));
	RD_CHECK(!fs::exists(target.path / "tri.obj"));
	RD_CHECK(!fs::exists(target.path / "tri.mtl"));
	RD_CHECK(fs::exists(source.path / "tri.obj"));

	ModelData original, converted;
	std::string error;
	RD_REQUIRE(ModelImporter::Import(ToUtf8((source.path / "tri.obj").generic_wstring()), false, original, error));
	RD_REQUIRE(ModelFile::Read(target.File("tri.rdmodel"), false, converted, error));
	RD_CHECK_EQ(converted.vertices.size(), original.vertices.size());
	RD_CHECK(converted.indices == original.indices);
	RD_REQUIRE(converted.subMeshes.size() == 1);
	// obj では assimp が既定のマテリアルを先頭に足すことがあるので、メッシュが使うものを見る
	RD_CHECK_EQ(converted.materials[converted.subMeshes[0].materialIndex].albedoPath, target.File("tri.png"));
}

RD_TEST(AssetImporter, FolderKeepsStructureAndDoesNotDuplicateTextures)
{
	TempFolder source("folder_src"), target("folder_dst");
	WriteTriangle(source.path / "Pack" / "Models");
	WriteText(source.path / "Pack" / "readme.txt", "hello");
	AssetImporter::Import({ source.path / "Pack" }, ToUtf8(target.path.generic_wstring()));

	fs::path models = target.path / "Pack" / "Models";
	RD_CHECK(fs::exists(target.path / "Pack" / "readme.txt"));
	RD_CHECK(fs::exists(models / "tri.rdmodel"));
	RD_CHECK(fs::exists(models / "tri.png"));
	RD_CHECK(!fs::exists(models / "tri 1.png"));
	RD_CHECK(!fs::exists(models / "tri.mtl"));
}

RD_TEST(AssetImporter, ExistingFilesAreNotOverwritten)
{
	TempFolder source("unique_src"), target("unique_dst");
	WriteTriangle(source.path);
	WriteText(target.path / "tri.png", "another image");
	WriteText(target.path / "tri.rdmodel", "keep me");
	AssetImporter::Import({ source.path / "tri.obj" }, ToUtf8(target.path.generic_wstring()));

	RD_CHECK_EQ(fs::file_size(target.path / "tri.rdmodel"), uintmax_t(7));
	RD_CHECK(fs::exists(target.path / "tri 1.rdmodel"));
	RD_CHECK(fs::exists(target.path / "tri 1.png"));
	ModelData converted;
	std::string error;
	RD_REQUIRE(ModelFile::Read(target.File("tri 1.rdmodel"), false, converted, error));
	RD_REQUIRE(converted.subMeshes.size() == 1);
	RD_CHECK_EQ(converted.materials[converted.subMeshes[0].materialIndex].albedoPath, target.File("tri 1.png"));
}
#endif
