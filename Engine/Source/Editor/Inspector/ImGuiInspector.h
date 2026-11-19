#pragma once
#include "Core/Reflection/PropertyVisitor.h"

namespace rd
{
	class Editor;

	// Reflect で並べたプロパティを ImGui の部品として表示・編集するビジター
	//  Undo のために、このフレームで値が変わったか（AnyChanged）と編集が確定したか（AnyCommitted）を記録する
	//  editor は参照の欄の候補の一覧と、ダブルクリックでの選択に使う
	class ImGuiInspector final : public PropertyVisitor
	{
	public:
		explicit ImGuiInspector(Editor* editor = nullptr) : mEditor(editor) {}

		bool Property(const char* name, bool& value, const PropertyMeta& meta = {}) override;
		bool Property(const char* name, int32_t& value, const PropertyMeta& meta = {}) override;
		bool Property(const char* name, uint32_t& value, const PropertyMeta& meta = {}) override;
		bool Property(const char* name, float& value, const PropertyMeta& meta = {}) override;
		bool Property(const char* name, Vector2& value, const PropertyMeta& meta = {}) override;
		bool Property(const char* name, Vector3& value, const PropertyMeta& meta = {}) override;
		bool Property(const char* name, Vector4& value, const PropertyMeta& meta = {}) override;
		bool Property(const char* name, Quaternion& value, const PropertyMeta& meta = {}) override;
		bool Property(const char* name, Color& value, const PropertyMeta& meta = {}) override;
		bool Property(const char* name, std::string& value, const PropertyMeta& meta = {}) override;
		bool Property(const char* name, ObjectRefBase& value, const PropertyMeta& meta = {}) override;
		using PropertyVisitor::Property;

		bool BeginGroup(const char* name) override;
		void SetNextGroupOpen() override;
		void EndGroup() override;

		bool AnyChanged() const { return mChanged; }
		bool AnyCommitted() const { return mCommitted; }
		void ResetFrameState() { mChanged = mCommitted = false; }
		static float LabelWidth();

	protected:
		bool EnumProperty(const char* name, int64_t& value, std::span<const std::string_view> names,
			std::span<const int64_t> values, const PropertyMeta& meta) override;

	private:
		void Label(const char* name, const PropertyMeta& meta);
		bool Track(bool changed, bool discrete = false);
		// マテリアル（.rdmat）・シェーダー（.hlsl）のアセットの欄（Unity のオブジェクトの欄と同じ見た目と操作）
		bool AssetObjectField(std::string& value, const PropertyMeta& meta);

		Editor* mEditor = nullptr;
		bool mChanged = false;
		bool mCommitted = false;
	};
}
