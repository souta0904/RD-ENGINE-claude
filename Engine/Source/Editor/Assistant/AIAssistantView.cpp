#include "RdPch.h"
#include "Editor/Assistant/AIAssistant.h"
#include "Editor/Style/EditorGui.h"
#include "Editor/Style/EditorStyle.h"
#include <ImGui/imgui_stdlib.h>

namespace rd
{
	namespace
	{
		constexpr const char* kProviderLabels[] = { "Local (Ollama)", "Local (OpenAI compatible)", "Claude API" };
	}

	void AIAssistant::OnGui()
	{
		if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen)) { DrawSettings(); }
		if (ImGui::Button("New Chat")) { NewChat(); }
		ImGui::SameLine();
		if (mSettings.IsLocal())
		{
			ImGui::TextDisabled("Tokens: in %llu / out %llu", (unsigned long long)mInputTokens, (unsigned long long)mOutputTokens);
		}
		else
		{
			ImGui::TextDisabled("Tokens: in %llu (cached %llu) / out %llu", (unsigned long long)mInputTokens,
				(unsigned long long)mCacheReadTokens, (unsigned long long)mOutputTokens);
		}
		float inputHeight = ImGui::GetTextLineHeightWithSpacing() * 4.0f + 10.0f;
		DrawChat(inputHeight);
		DrawInput(inputHeight);
	}

	void AIAssistant::DrawSettings()
	{
		bool changed = false;
		int provider = int(mSettings.provider);
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::Combo("##provider", &provider, kProviderLabels, int(std::size(kProviderLabels))))
		{
			mSettings.provider = AssistantProviderKind(provider);
			mModels.clear();
			mModelListError.clear();
			mModelsRequested = false;
			changed = true;
		}
		if (mSettings.IsLocal()) { DrawLocalSettings(changed); }
		else { DrawClaudeSettings(changed); }
		if (changed) { SaveSettings(); }
	}

	// 開いたら一度だけ、サーバーにあるモデルを聞きに行く。つながらない・モデルがないときは始め方を出す
	void AIAssistant::DrawLocalSettings(bool& changed)
	{
		if (!mModelsRequested) { mModelsRequested = true; RefreshModels(); }

		std::string& url = mSettings.provider == AssistantProviderKind::Ollama ? mSettings.ollamaUrl : mSettings.openAIUrl;
		// 名前の欄の幅は一番長い "Context" に合わせる
		const float labelWidth = ImGui::CalcTextSize("Context").x + ImGui::GetStyle().ItemInnerSpacing.x * 2.0f;
		ImGui::SetNextItemWidth(-labelWidth);
		if (ImGui::InputText("URL", &url, ImGuiInputTextFlags_EnterReturnsTrue)) { changed = true; RefreshModels(); }
		if (ImGui::IsItemDeactivatedAfterEdit()) { changed = true; }

		const float refreshWidth = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x;
		ImGui::SetNextItemWidth(-(labelWidth + refreshWidth));
		const char* preview = mSettings.localModel.empty() ? "(none)" : mSettings.localModel.c_str();
		if (ImGui::BeginCombo("##model", preview))
		{
			for (const auto& m : mModels)
			{
				if (ImGui::Selectable(m.c_str(), m == mSettings.localModel)) { mSettings.localModel = m; changed = true; }
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		if (EditorGui::IconButton("##refresh", Icon::Rotate, "Refresh the model list", false, mModelList == nullptr)) { RefreshModels(); }
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::TextUnformatted("Model");
		if (mSettings.provider == AssistantProviderKind::Ollama)
		{
			ImGui::SetNextItemWidth(-labelWidth);
			if (ImGui::InputInt("Context", &mSettings.contextSize, 1024, 4096)) { mSettings.contextSize = std::clamp(mSettings.contextSize, 2048, 262144); }
			if (ImGui::IsItemDeactivatedAfterEdit()) { changed = true; }
			if (ImGui::IsItemHovered()) { ImGui::SetTooltip("num_ctx: how many tokens the model can see. Larger uses more memory"); }
		}

		if (mModelList) { ImGui::TextDisabled("Connecting..."); return; }
		if (mModelListError.empty() && !mModels.empty()) { return; }
		ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::kWarning);
		if (!mModelListError.empty()) { ImGui::TextWrapped("%s", mModelListError.c_str()); }
		else { ImGui::TextWrapped("サーバーにモデルがありません。"); }
		ImGui::PopStyleColor();
		if (mSettings.provider == AssistantProviderKind::Ollama)
		{
			ImGui::TextWrapped("使い方: Ollama（ollama.com、無料）をインストールし、コマンドプロンプトで次を実行してから更新ボタンを押します。ツールを使えるモデルが必要です。");
			static std::string kCommand = "ollama pull qwen3:8b";
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputText("##cmd", &kCommand, ImGuiInputTextFlags_ReadOnly);
		}
		else
		{
			ImGui::TextWrapped("LM Studio なら Developer タブでサーバーを開始し、ツール呼び出しに対応したモデルを読み込んでから更新ボタンを押します。llama.cpp なら llama-server --jinja で起動します。");
		}
	}

	void AIAssistant::DrawClaudeSettings(bool& changed)
	{
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
		if (ImGui::BeginCombo("##claude", kClaudeModels[mSettings.claudeModel].label))
		{
			for (int i = 0; i < int(std::size(kClaudeModels)); ++i)
			{
				if (ImGui::Selectable(kClaudeModels[i].label, i == mSettings.claudeModel)) { mSettings.claudeModel = i; changed = true; }
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5.0f);
		changed |= ImGui::Combo("Effort", &mSettings.claudeEffort, kClaudeEfforts, int(std::size(kClaudeEfforts)));
		ImGui::TextDisabled("Needs ANTHROPIC_API_KEY (paid)");
	}

	void AIAssistant::DrawChat(float inputHeight)
	{
		ImGui::BeginChild("##chat", ImVec2(0, -inputHeight), ImGuiChildFlags_Border);
		for (const auto& e : mChat)
		{
			switch (e.kind)
			{
			case ChatEntry::Kind::User:
				ImGui::TextColored(EditorStyle::kAccent, "You");
				break;
			case ChatEntry::Kind::Assistant:
				ImGui::TextColored(EditorStyle::kIconAudio, "Assistant");
				break;
			case ChatEntry::Kind::Tool:
				ImGui::TextColored(EditorStyle::kSuccess, "  > %s", e.text.c_str());
				continue;
			case ChatEntry::Kind::Error:
				ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::kError);
				ImGui::TextWrapped("%s", e.text.c_str());
				ImGui::PopStyleColor();
				continue;
			case ChatEntry::Kind::Info:
				ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::kTextDim);
				ImGui::TextWrapped("%s", e.text.c_str());
				ImGui::PopStyleColor();
				continue;
			}
			ImGui::TextWrapped("%s", e.text.c_str());
			ImGui::Spacing();
		}
		if (mRequest)
		{
			static const char* kSpin = "|/-\\";
			ImGui::TextDisabled("考え中 %c", kSpin[int(ImGui::GetTime() * 8.0) % 4]);
		}
		if (mScrollToBottom) { ImGui::SetScrollHereY(1.0f); mScrollToBottom = false; }
		ImGui::EndChild();
	}

	// Ctrl+Enter でも送れる
	void AIAssistant::DrawInput(float inputHeight)
	{
		bool send = false;
		ImGui::BeginDisabled(mRequest != nullptr);
		ImGui::InputTextMultiline("##input", &mInput, ImVec2(-80.0f, inputHeight - 8.0f));
		if (ImGui::IsItemFocused() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Enter, false)) { send = true; }
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (mRequest)
		{
			if (ImGui::Button("Stop", ImVec2(-1, 0))) { Cancel(); }
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::kAccentDim);
			if (ImGui::Button("Send", ImVec2(-1, 0))) { send = true; }
			ImGui::PopStyleColor();
		}
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Ctrl+Enter"); }
		if (send && !mInput.empty() && !mRequest)
		{
			Send(mInput);
			mInput.clear();
			mScrollToBottom = true;
		}
	}
}
