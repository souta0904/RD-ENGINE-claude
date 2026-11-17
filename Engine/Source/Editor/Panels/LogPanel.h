#pragma once
#include "Editor/Panels/EditorPanel.h"
#include "Editor/Style/EditorIcons.h"
#include <string>

namespace rd
{
	// ログ（Unity の Console と同じく、右上のボタンで情報・警告・エラーごとに表示を切り替え、数も出す）
	//  Collapse で同じ文のログを 1 行にまとめ、Clear on Play ならプレイを始めたときに消す
	class LogPanel final : public EditorPanel
	{
	public:
		explicit LogPanel(Editor& editor);
		void Update(float dt) override;

	protected:
		void OnGui() override;
		ImVec2 GetDefaultSize() const override { return ImVec2(800.0f, 260.0f); }

	private:
		void DrawToolbar(int infoCount, int warnCount, int errorCount);
		bool LevelToggle(const char* id, Icon icon, int count, bool& show, const char* tooltip);

		std::string mFilter;
		bool mShowDebug = false;
		bool mShowInfo = true;
		bool mShowWarn = true;
		bool mShowError = true;
		bool mCollapse = false;
		bool mClearOnPlay = true;
		bool mWasPlaying = false;
		uint64_t mVersion = 0;
	};
}
