#pragma once
#include "Core/Reflection.h"

namespace rd
{
	// Reflect の内容を ImGui のウィジェットとして表示・編集するビジター
	//  Undo のために「編集の開始/終了」を記録する
	class Editor;

	class ImGuiInspector final : public PropertyVisitor
	{
	public:
		// editor は参照フィールドの候補一覧と、ダブルクリックでの選択に使う
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
		void EndGroup() override;

		// このフレームで値が変わったか / 編集が確定したか
		bool AnyChanged() const { return mChanged; }
		bool AnyCommitted() const { return mCommitted; }
		void ResetFrameState() { mChanged = mCommitted = false; }
		// 共通のラベル幅
		static float LabelWidth();

	protected:
		bool EnumProperty(const char* name, int64_t& value, std::span<const std::string_view> names,
			std::span<const int64_t> values, const PropertyMeta& meta) override;

	private:
		void Label(const char* name, const PropertyMeta& meta);
		bool Track(bool changed, bool discrete = false);

		Editor* mEditor = nullptr;
		bool mChanged = false;
		bool mCommitted = false;
	};
}
