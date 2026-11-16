#include "RdPch.h"
#include "Asset/Model/MeshUtil.h"

namespace rd::MeshUtil
{
	void FixWinding(const std::vector<MeshVertex>& v, std::vector<uint32_t>& idx)
	{
		for (size_t i = 0; i + 2 < idx.size(); i += 3)
		{
			const auto& a = v[idx[i]];
			const auto& b = v[idx[i + 1]];
			const auto& c = v[idx[i + 2]];
			Vector3 n = Cross(b.position - a.position, c.position - a.position);
			if (Dot(n, a.normal + b.normal + c.normal) < 0.0f) { std::swap(idx[i + 1], idx[i + 2]); }
		}
	}

	// 三角形ごとに UV の変化から接線と従法線を求めて頂点に足し、最後に法線と直交させる
	void ComputeTangents(std::vector<MeshVertex>& v, const std::vector<uint32_t>& idx)
	{
		std::vector<Vector3> tan(v.size()), bitan(v.size());
		for (size_t i = 0; i + 2 < idx.size(); i += 3)
		{
			auto& a = v[idx[i]];
			auto& b = v[idx[i + 1]];
			auto& c = v[idx[i + 2]];
			Vector3 e1 = b.position - a.position, e2 = c.position - a.position;
			Vector2 d1 = b.uv - a.uv, d2 = c.uv - a.uv;
			float det = d1.x * d2.y - d2.x * d1.y;
			if (std::fabs(det) < 1.0e-8f) { continue; }
			float r = 1.0f / det;
			Vector3 t = (e1 * d2.y - e2 * d1.y) * r;
			Vector3 bt = (e2 * d1.x - e1 * d2.x) * r;
			for (int k = 0; k < 3; ++k)
			{
				tan[idx[i + k]] += t;
				bitan[idx[i + k]] += bt;
			}
		}
		for (size_t i = 0; i < v.size(); ++i)
		{
			Vector3 n = v[i].normal;
			Vector3 t = Normalize(ProjectOnPlane(tan[i], n));
			if (LengthSq(t) < 1.0e-6f) { t = Normalize(Cross(std::fabs(n.y) < 0.99f ? Vector3::kUnitY : Vector3::kUnitX, n)); }
			float w = Dot(Cross(n, t), bitan[i]) < 0.0f ? -1.0f : 1.0f;
			v[i].tangent = Vector4(t, w);
		}
	}
}
