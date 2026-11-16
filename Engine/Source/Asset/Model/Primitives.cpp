#include "RdPch.h"
#include "Asset/Model/Primitives.h"

namespace rd::Primitives
{
	namespace
	{
		// 経度 segments・緯度 rings の格子の三角形を並べる
		void AddGridIndices(std::vector<uint32_t>& idx, uint32_t segments, uint32_t rings)
		{
			for (uint32_t r = 0; r < rings; ++r)
			{
				for (uint32_t s = 0; s < segments; ++s)
				{
					uint32_t a = r * (segments + 1) + s;
					uint32_t b = a + segments + 1;
					idx.insert(idx.end(), { a, b, a + 1, a + 1, b, b + 1 });
				}
			}
		}

		Vector3 SpherePoint(float phi, float theta)
		{
			return { std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta) };
		}
	}

	void Cube(std::vector<MeshVertex>& v, std::vector<uint32_t>& idx)
	{
		const Vector3 normals[6] = { Vector3::kUnitX, -Vector3::kUnitX, Vector3::kUnitY, -Vector3::kUnitY, Vector3::kUnitZ, -Vector3::kUnitZ };
		for (const auto& n : normals)
		{
			Vector3 up = std::fabs(n.y) > 0.5f ? Vector3::kUnitZ : Vector3::kUnitY;
			Vector3 right = Cross(up, n);
			uint32_t base = uint32_t(v.size());
			Vector3 c = n * 0.5f;
			v.push_back({ c - right * 0.5f - up * 0.5f, n, {}, { 0.0f, 1.0f } });
			v.push_back({ c - right * 0.5f + up * 0.5f, n, {}, { 0.0f, 0.0f } });
			v.push_back({ c + right * 0.5f + up * 0.5f, n, {}, { 1.0f, 0.0f } });
			v.push_back({ c + right * 0.5f - up * 0.5f, n, {}, { 1.0f, 1.0f } });
			idx.insert(idx.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
		}
	}

	void Sphere(std::vector<MeshVertex>& v, std::vector<uint32_t>& idx, uint32_t segments, uint32_t rings)
	{
		for (uint32_t r = 0; r <= rings; ++r)
		{
			float phi = math::kPi * float(r) / float(rings);
			for (uint32_t s = 0; s <= segments; ++s)
			{
				Vector3 n = SpherePoint(phi, math::k2Pi * float(s) / float(segments));
				v.push_back({ n * 0.5f, n, {}, { float(s) / float(segments), float(r) / float(rings) } });
			}
		}
		AddGridIndices(idx, segments, rings);
	}

	void Plane(std::vector<MeshVertex>& v, std::vector<uint32_t>& idx, float size)
	{
		float h = size * 0.5f;
		float uv = size;
		v.push_back({ { -h, 0.0f, -h }, Vector3::kUnitY, {}, { 0.0f, uv } });
		v.push_back({ { -h, 0.0f, h }, Vector3::kUnitY, {}, { 0.0f, 0.0f } });
		v.push_back({ { h, 0.0f, h }, Vector3::kUnitY, {}, { uv, 0.0f } });
		v.push_back({ { h, 0.0f, -h }, Vector3::kUnitY, {}, { uv, uv } });
		idx.insert(idx.end(), { 0, 1, 2, 0, 2, 3 });
	}

	// 上の半球 → 円柱 → 下の半球の順に輪を並べる（円柱の部分は上下の半球の境目の輪をつなぐだけ）
	void Capsule(std::vector<MeshVertex>& v, std::vector<uint32_t>& idx, float radius, float height)
	{
		const uint32_t segments = 32, halfRings = 8;
		float half = std::max(0.0f, height * 0.5f - radius);
		uint32_t rings = halfRings * 2 + 1;
		for (uint32_t r = 0; r <= rings; ++r)
		{
			float phi;
			float offset;
			if (r <= halfRings) { phi = math::kPiOver2 * float(r) / float(halfRings); offset = half; }
			else { phi = math::kPiOver2 + math::kPiOver2 * float(r - halfRings - 1) / float(halfRings); offset = -half; }
			for (uint32_t s = 0; s <= segments; ++s)
			{
				Vector3 n = SpherePoint(phi, math::k2Pi * float(s) / float(segments));
				v.push_back({ n * radius + Vector3(0.0f, offset, 0.0f), n, {}, { float(s) / float(segments), float(r) / float(rings) } });
			}
		}
		AddGridIndices(idx, segments, rings);
	}
}
