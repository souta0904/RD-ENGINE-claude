#include "TestFramework.h"
#include "Components/Rendering/MeshRenderer.h"
#include "Core/Serialization/JsonReader.h"
#include "Core/Serialization/JsonWriter.h"
#include "Core/Util/StringUtil.h"
#include "Render/Material/MaterialAsset.h"
#include "Scene/Scene.h"
#include <Windows.h>
#include <filesystem>

#if RD_EDITOR
#include "Editor/Commands/ActorStateCommand.h"
#include "Editor/Commands/CommandHistory.h"
#endif

using namespace rd;
namespace fs = std::filesystem;

// マテリアルのアセット（.rdmat）の保存と読み込み、MeshRenderer の枠、Undo との関係を確かめる
//  AssetManager は GPU が要るのでテストでは作らない。アセットを入れた枠の描画はエディタで確かめる

namespace
{
	struct TempMaterialFile
	{
		fs::path path;

		explicit TempMaterialFile(const char* name)
		{
			path = fs::temp_directory_path() / std::format("rd_material_{}_{}.rdmat", GetCurrentProcessId(), name);
			Remove();
		}
		~TempMaterialFile() { Remove(); }
		void Remove()
		{
			std::error_code ec;
			fs::remove(path, ec);
		}
		std::string Get() const { return ToUtf8(path.generic_wstring()); }
	};
}

// 作ったファイルを読み戻すと、同じ値になる。同じ名前ではもう作れない
RD_TEST(MaterialAsset, CreateAndReadBack)
{
	TempMaterialFile file("roundtrip");
	Material m;
	m.baseColor = Color(0.2f, 0.4f, 0.8f, 1.0f);
	m.shading = ShadingModel::Flat;
	m.metallic = 0.75f;
	m.roughness = 0.25f;
	m.blend = BlendMode::Alpha;
	m.doubleSided = true;
	m.albedoPath = "Assets/Textures/Wood.png";
	RD_REQUIRE(MaterialAsset::CreateFile(file.Get(), m));
	RD_CHECK(!MaterialAsset::CreateFile(file.Get(), Material()));

	Json json;
	RD_REQUIRE(LoadJsonFile(file.Get(), json));
	RD_CHECK_EQ(json.value("Format", std::string()), std::string("RDMaterial"));
	MaterialAsset loaded;
	loaded.FromJson(json);
	RD_CHECK_NEAR(loaded.material.baseColor.b, 0.8f, 1.0e-5f);
	RD_CHECK(loaded.material.shading == ShadingModel::Flat);
	RD_CHECK_NEAR(loaded.material.metallic, 0.75f, 1.0e-5f);
	RD_CHECK_NEAR(loaded.material.roughness, 0.25f, 1.0e-5f);
	RD_CHECK(loaded.material.blend == BlendMode::Alpha);
	RD_CHECK(loaded.material.doubleSided);
	RD_CHECK_EQ(loaded.material.albedoPath, std::string("Assets/Textures/Wood.png"));
}

// ファイルにない項目は既定値になる（前の値は残らない）。読むたびに版が進む
RD_TEST(MaterialAsset, MissingKeysUseDefaults)
{
	MaterialAsset asset;
	asset.material.baseColor = Color::kRed;
	asset.material.roughness = 0.1f;
	uint32_t version = asset.GetVersion();
	asset.FromJson(Json::parse(R"({ "Material": { "Metallic": 0.5 } })"));
	RD_CHECK_NEAR(asset.material.metallic, 0.5f, 1.0e-5f);
	RD_CHECK_NEAR(asset.material.baseColor.g, 1.0f, 1.0e-5f);
	RD_CHECK_NEAR(asset.material.roughness, Material().roughness, 1.0e-5f);
	RD_CHECK(asset.GetVersion() != version);
}

RD_TEST(MaterialAsset, ExtensionCheck)
{
	RD_CHECK(MaterialAsset::IsMaterialPath("Assets/Materials/Wood.rdmat"));
	RD_CHECK(MaterialAsset::IsMaterialPath("Assets/Materials/Wood.RDMAT"));
	// Unity の .mat（YAML）は別物
	RD_CHECK(!MaterialAsset::IsMaterialPath("Assets/Materials/Wood.mat"));
	RD_CHECK(!MaterialAsset::IsMaterialPath("Assets/Materials/rdmat"));
}

// 前からのシーン（"Asset" のない形）も読める。書き出すと枠ごとに空の "Asset" が付く
RD_TEST(MaterialAsset, OldRendererMaterialsStillLoad)
{
	Scene scene;
	Actor* a = scene.CreateActor("A");
	auto* mr = a->AddComponent<MeshRenderer>();
	Json old = Json::parse(R"({
		"Material Count": 2,
		"Material 0": { "Base Color": [1, 0, 0, 1], "Metallic": 0.3 },
		"Material 1": { "Base Color": [0, 1, 0, 1] }
	})");
	JsonReader reader(old);
	mr->Reflect(reader);
	RD_REQUIRE(mr->GetMaterialCount() == 2);
	RD_CHECK(mr->GetSharedMaterialPath(0).empty());
	RD_CHECK(mr->GetSharedMaterial(1) == nullptr);
	RD_CHECK_NEAR(mr->GetRenderMaterial(0).baseColor.r, 1.0f, 1.0e-5f);
	RD_CHECK_NEAR(mr->GetRenderMaterial(0).metallic, 0.3f, 1.0e-5f);
	RD_CHECK_NEAR(mr->GetRenderMaterial(1).baseColor.g, 1.0f, 1.0e-5f);

	Json saved = Json::object();
	JsonWriter writer(saved);
	mr->Reflect(writer);
	RD_CHECK(saved["Material 0"].contains("Asset"));
	RD_CHECK_EQ(saved["Material 0"]["Asset"].get<std::string>(), std::string());
	RD_CHECK(saved["Material 0"].contains("Base Color"));
}

// ゲームのコードが GetMaterials() の配列ごと入れ替えても、枠の数がそろう
RD_TEST(MaterialAsset, ReplacingMaterialsKeepsSlotsConsistent)
{
	Scene scene;
	Actor* a = scene.CreateActor("A");
	auto* mr = a->AddComponent<MeshRenderer>();
	Material m;
	m.baseColor = Color::kBlue;
	mr->GetMaterials() = { m, m, m };
	RD_CHECK_EQ(mr->GetMaterialCount(), size_t(3));
	RD_CHECK(mr->GetSharedMaterialPath(2).empty());
	RD_CHECK(!mr->SetSharedMaterial(5, ""));
	Json saved = Json::object();
	JsonWriter writer(saved);
	mr->Reflect(writer);
	RD_CHECK(saved.contains("Material 2"));
	RD_CHECK_NEAR(mr->GetRenderMaterial(2).baseColor.b, 1.0f, 1.0e-5f);
}

#if RD_EDITOR
namespace
{
	// シーンを書き換えない操作（マテリアルのファイルの編集にあたる）
	class FileOnlyCommand final : public EditorCommand
	{
	public:
		FileOnlyCommand() : EditorCommand("Edit File") {}
		void Undo(Scene&) override {}
		void Redo(Scene&) override {}
		bool ChangesScene() const override { return false; }
	};
}

// マテリアルの編集だけなら、シーンは「保存していない」にならない
RD_TEST(MaterialAsset, FileEditsDoNotDirtyScene)
{
	Scene scene;
	CommandHistory history;
	Actor* a = scene.CreateActor("A");

	history.Push(std::make_unique<FileOnlyCommand>());
	RD_CHECK(!history.IsDirty());

	Json before = scene.SerializeActorState(a);
	a->SetName("B");
	history.Push(std::make_unique<ActorStateCommand>("Rename", a->GetId(), std::move(before), scene.SerializeActorState(a)));
	RD_CHECK(history.IsDirty());

	history.MarkSaved();
	history.Push(std::make_unique<FileOnlyCommand>());
	RD_CHECK(!history.IsDirty());
	history.Undo(scene);
	RD_CHECK(!history.IsDirty());
	// 保存した時点より前のシーンの変更まで戻すと「保存していない」になる
	history.Undo(scene);
	RD_CHECK(history.IsDirty());
	history.Redo(scene);
	RD_CHECK(!history.IsDirty());
}
#endif
