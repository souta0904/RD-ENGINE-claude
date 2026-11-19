#pragma once
#include <ImGui/imgui.h>
#include <string>

namespace rd
{
	class Actor;
	class Component;

	// エディタで使うアイコン（フォントに頼らず図形で描くので、どのパネルでも同じ形・同じ色になる）
	enum class Icon
	{
		Actor, Mesh, Light, Camera, Particles, Physics, Audio, Animation, Component,
		Folder, File, Image, Model, Scene, Prefab, Shader, Material,
		Play, Pause, Step, Move, Rotate, Scale, Global, Local,
		Eye, EyeOff, Plus, Search, Menu, Info, Warning, Error,
	};

	class EditorIcons
	{
	public:
		// pos は左上、size は一辺の長さ
		static void Draw(ImDrawList* dl, ImVec2 pos, float size, Icon icon, ImU32 color);
		static void Draw(ImDrawList* dl, ImVec2 pos, float size, Icon icon) { Draw(dl, pos, size, icon, DefaultColor(icon)); }
		// 今の行に size の大きさで描いて、カーソルを進める
		static void Inline(Icon icon, float size = 0.0f);
		static ImU32 DefaultColor(Icon icon);

		// アクターやコンポーネント、ファイルの拡張子に合うアイコン
		static Icon ForActor(const Actor* actor);
		static Icon ForComponent(const Component* component);
		static Icon ForFile(const std::string& extension, bool directory);
	};
}
