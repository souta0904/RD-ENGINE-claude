#include "RdPch.h"
#include "Components/UI/UIText.h"
#include "Components/UI/UIFont.h"
#include "Core/Util/StringUtil.h"

namespace rd
{
	namespace
	{
		// リッチテキストを解いた 1 文字
		struct RichChar
		{
			char32_t c = 0;
			Color color;     // sRGB
			float size = 1.0f; // 大きさの倍率
		};

		struct Line
		{
			std::vector<RichChar> chars;
			float width = 0.0f;
			float maxSize = 1.0f;
		};

		char32_t DecodeUtf8(const std::string& s, size_t& i)
		{
			unsigned char c = static_cast<unsigned char>(s[i++]);
			if (c < 0x80) { return c; }
			int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
			char32_t cp = c & (0x3F >> extra);
			for (int k = 0; k < extra && i < s.size(); ++k) { cp = (cp << 6) | (static_cast<unsigned char>(s[i++]) & 0x3F); }
			return cp;
		}

		bool ParseHexColor(std::string_view hex, Color& out)
		{
			if (!hex.empty() && hex[0] == '#') { hex.remove_prefix(1); }
			if (hex.size() != 6 && hex.size() != 8) { return false; }
			auto byte = [&](size_t at) { return float(std::stoi(std::string(hex.substr(at, 2)), nullptr, 16)) / 255.0f; };
			out = Color(byte(0), byte(2), byte(4), hex.size() == 8 ? byte(6) : out.a);
			return true;
		}

		// <sprite name="x"> はボタンの絵の代わりに、ボタンの文字を色を付けて出す
		Color ButtonColor(char32_t c)
		{
			switch (c)
			{
			case U'A': return Color(0.30f, 0.69f, 0.31f);
			case U'B': return Color(0.85f, 0.29f, 0.25f);
			case U'X': return Color(0.20f, 0.46f, 0.73f);
			case U'Y': return Color(0.90f, 0.71f, 0.23f);
			default: return Color(0.4f, 0.4f, 0.4f);
			}
		}

		std::string TagValue(std::string_view tag)
		{
			size_t eq = tag.find('=');
			if (eq == std::string_view::npos) { return {}; }
			std::string v;
			for (char c : tag.substr(eq + 1))
			{
				if (c != '"' && c != '\'' && c != ' ') { v.push_back(c); }
			}
			return v;
		}

		std::vector<RichChar> ParseRichText(const std::string& text, const Color& baseColor)
		{
			std::vector<RichChar> out;
			std::vector<Color> colors{ baseColor };
			std::vector<float> sizes{ 1.0f };
			size_t i = 0;
			while (i < text.size())
			{
				if (text[i] == '<')
				{
					size_t close = text.find('>', i);
					if (close != std::string::npos)
					{
						std::string_view tag(text.data() + i + 1, close - i - 1);
						std::string lower = ToLower(std::string(tag));
						bool handled = true;
						if (lower.starts_with("color="))
						{
							Color c = colors.back();
							if (ParseHexColor(TagValue(tag), c)) { colors.push_back(c); }
						}
						else if (lower == "/color") { if (colors.size() > 1) { colors.pop_back(); } }
						else if (lower.starts_with("size="))
						{
							std::string v = TagValue(tag);
							float s = v.ends_with("%") ? std::stof(v.substr(0, v.size() - 1)) / 100.0f : sizes.back();
							sizes.push_back(s);
						}
						else if (lower == "/size") { if (sizes.size() > 1) { sizes.pop_back(); } }
						else if (lower.starts_with("sprite"))
						{
							for (char ch : TagValue(tag))
							{
								char32_t up = char32_t(std::toupper(static_cast<unsigned char>(ch)));
								out.push_back({ up, ButtonColor(up), sizes.back() });
							}
						}
						else if (lower == "b" || lower == "/b" || lower == "i" || lower == "/i" || lower.starts_with("font") || lower.starts_with("/font")) {}
						else { handled = false; }
						if (handled)
						{
							i = close + 1;
							continue;
						}
					}
				}
				char32_t c = DecodeUtf8(text, i);
				if (c == U'\r') { continue; }
				out.push_back({ c, colors.back(), sizes.back() });
			}
			return out;
		}

		bool IsBreakableSpace(char32_t c) { return c == U' ' || c == U'\t' || c == U'　'; }

		// 幅に収まるように行に分ける。英数字の単語の途中では、できれば空白で折り返す
		std::vector<Line> Layout(const std::vector<RichChar>& chars, UIFont& font, float fontSize, float maxWidth)
		{
			std::vector<Line> lines(1);
			float unit = fontSize / font.GetBakeEm();
			for (const RichChar& rc : chars)
			{
				if (rc.c == U'\n')
				{
					lines.emplace_back();
					continue;
				}
				float advance = font.GetGlyph(rc.c).advance * unit * rc.size;
				Line* line = &lines.back();
				if (!line->chars.empty() && line->width + advance > maxWidth + 0.01f)
				{
					if (IsBreakableSpace(rc.c))
					{
						lines.emplace_back();
						continue;
					}
					Line next;
					// 英数字が続いているなら、最後の空白の後ろから次の行へ送る
					if (rc.c < 0x2E80)
					{
						for (size_t k = line->chars.size(); k-- > 0;)
						{
							if (IsBreakableSpace(line->chars[k].c))
							{
								next.chars.assign(line->chars.begin() + k + 1, line->chars.end());
								line->chars.resize(k);
								break;
							}
							if (line->chars[k].c >= 0x2E80) { break; }
						}
					}
					lines.push_back(std::move(next));
					line = &lines.back();
				}
				line->chars.push_back(rc);
			}
			for (Line& line : lines)
			{
				line.width = 0.0f;
				line.maxSize = line.chars.empty() ? 1.0f : 0.0f;
				for (const RichChar& rc : line.chars)
				{
					line.width += font.GetGlyph(rc.c).advance * unit * rc.size;
					line.maxSize = std::max(line.maxSize, rc.size);
				}
				// 行末の空白は幅に入れない
				for (size_t k = line.chars.size(); k-- > 0 && IsBreakableSpace(line.chars[k].c);)
				{
					line.width -= font.GetGlyph(line.chars[k].c).advance * unit * line.chars[k].size;
				}
			}
			return lines;
		}

		float LinesHeight(const std::vector<Line>& lines, UIFont& font, float fontSize, float spacing)
		{
			float unit = fontSize / font.GetBakeEm();
			float h = 0.0f;
			for (size_t i = 0; i < lines.size(); ++i)
			{
				h += font.GetLineHeight() * unit * lines[i].maxSize;
				if (i + 1 < lines.size()) { h += spacing * fontSize * 0.01f; }
			}
			return h;
		}

		float MaxWidth(const std::vector<Line>& lines)
		{
			float w = 0.0f;
			for (const Line& l : lines) { w = std::max(w, l.width); }
			return w;
		}
	}

	void UIText::Reflect(PropertyVisitor& v)
	{
		v.Property("Text", text);
		v.Property("Font", font);
		v.Property("Font Size", fontSize, { .speed = 0.1f, .min = 1.0f, .max = 500.0f });
		v.Property("Color", color);
		v.Property("Alignment", alignment);
		v.Property("Auto Size", autoSize);
		v.Property("Line Spacing", lineSpacing, { .speed = 0.1f });
		v.Property("Outline Width", outlineWidth, { .speed = 0.01f, .min = 0.0f, .max = 1.0f });
		v.Property("Outline Color", outlineColor);
	}

	Vector2 UIText::GetPreferredSize() const
	{
		UIFont* f = UIFont::Get(font);
		if (text.empty() || !f || !f->IsValid()) { return {}; }
		std::vector<Line> lines = Layout(ParseRichText(text, color), *f, fontSize, 1e9f);
		return { MaxWidth(lines), LinesHeight(lines, *f, fontSize, lineSpacing) };
	}

	void UIText::Draw(const UIRect& rect, const UIDrawContext& ctx)
	{
		if (text.empty()) { return; }
		UIFont* f = UIFont::Get(font);
		if (!f || !f->IsValid()) { return; }

		std::vector<RichChar> chars = ParseRichText(text, color);
		float size = fontSize;
		std::vector<Line> lines = Layout(chars, *f, size, rect.Width());
		if (autoSize)
		{
			// 入りきるまで少しずつ小さくする（下限は指定の 1/4）
			float minSize = std::max(fontSize * 0.25f, 4.0f);
			while (size > minSize && (LinesHeight(lines, *f, size, lineSpacing) > rect.Height() + 0.01f || MaxWidth(lines) > rect.Width() + 0.01f))
			{
				size = std::max(size * 0.94f, minSize);
				lines = Layout(chars, *f, size, rect.Width());
			}
		}

		const Texture* atlas = f->GetTexture();
		if (!atlas) { return; }

		bool top = alignment.find("Top") != std::string::npos;
		bool bottom = alignment.find("Bottom") != std::string::npos;
		bool left = alignment.find("Left") != std::string::npos;
		bool right = alignment.find("Right") != std::string::npos;

		// 文字ごとの長方形を先に決める（縁取りを全部の文字の下に敷いてから文字を描くため）
		struct Placed
		{
			UIRect rect;
			Vector4 uv;
			Color color;
		};
		std::vector<Placed> placed;
		float unit = size / f->GetBakeEm();
		float total = LinesHeight(lines, *f, size, lineSpacing);
		float y = top ? rect.max.y : bottom ? rect.min.y + total : (rect.min.y + rect.max.y) * 0.5f + total * 0.5f;
		for (const Line& line : lines)
		{
			float lineUnit = unit * line.maxSize;
			float baseline = y - f->GetAscent() * lineUnit;
			float x = left ? rect.min.x : right ? rect.max.x - line.width : (rect.min.x + rect.max.x - line.width) * 0.5f;
			for (const RichChar& rc : line.chars)
			{
				const UIFont::Glyph& g = f->GetGlyph(rc.c);
				float u = unit * rc.size;
				if (!g.empty)
				{
					// 文字の上端はベースラインから offset.y（上が負）だけ離れた所
					float topY = baseline - g.offset.y * u;
					UIRect r;
					r.min = { x + g.offset.x * u, topY - g.size.y * u };
					r.max = { x + (g.offset.x + g.size.x) * u, topY };
					placed.push_back({ r, g.uv, rc.color });
				}
				x += g.advance * u;
			}
			y -= f->GetLineHeight() * lineUnit + lineSpacing * size * 0.01f;
		}

		// 縁取りは、文字を 8 方向へずらして縁の色で描いて作る（アトラスは距離場ではないため）
		//  太さは TextMeshPro の 0.3 が文字の大きさのおよそ 4% になるように合わせた
		if (outlineWidth > 0.0f && outlineColor.a > 0.0f)
		{
			float thickness = outlineWidth * size * 0.12f;
			Color oc = UIColorToLinear(outlineColor);
			oc.a *= ctx.alpha;
			for (int k = 0; k < 8; ++k)
			{
				float angle = math::kPi * 0.25f * float(k);
				Vector2 offset = { std::cos(angle) * thickness, std::sin(angle) * thickness };
				for (const Placed& p : placed)
				{
					ctx.DrawQuad({ p.rect.min + offset, p.rect.max + offset }, atlas, p.uv, oc);
				}
			}
		}
		for (const Placed& p : placed)
		{
			Color c = UIColorToLinear(p.color);
			c.a *= ctx.alpha;
			ctx.DrawQuad(p.rect, atlas, p.uv, c);
		}
	}
}
