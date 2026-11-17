#include "RdPch.h"
#include "Math/Quaternion.h"
#include "Math/Matrix4.h"

namespace rd
{
	// 角度を半分にするのは、クォータニオンでベクトルを回すときに q とその逆を両側から掛け、角度が 2 倍効くため
	Quaternion Quaternion::AngleAxis(const Vector3& axis, float radians)
	{
		Vector3 n = Normalize(axis);
		float h = radians * 0.5f;
		float s = std::sin(h);
		return { std::cos(h), n.x * s, n.y * s, n.z * s };
	}

	// X・Y・Z 軸それぞれの AngleAxis を X→Y→Z の順に掛けたものを、1 つの式に展開したもの
	Quaternion Quaternion::FromEuler(const Vector3& e)
	{
		float cx = std::cos(e.x * 0.5f), sx = std::sin(e.x * 0.5f);
		float cy = std::cos(e.y * 0.5f), sy = std::sin(e.y * 0.5f);
		float cz = std::cos(e.z * 0.5f), sz = std::sin(e.z * 0.5f);
		return Normalize(Quaternion(
			cz * cy * cx + sz * sy * sx,
			cz * cy * sx - sz * sy * cx,
			sz * cy * sx + cz * sy * cx,
			sz * cy * cx - cz * sy * sx));
	}

	// 回転行列の成分にあたる値から、atan2 / asin で角度を取り出す
	Vector3 Quaternion::ToEuler() const
	{
		Vector3 e;
		e.x = std::atan2(2.0f * (w * x + y * z), 1.0f - 2.0f * (x * x + y * y));
		// Y は asin で求める。誤差で ±1 を少し超えると asin が NaN を返すので、そのときは ±90 度にする
		float sy = 2.0f * (w * y - z * x);
		e.y = std::fabs(sy) >= 1.0f ? std::copysign(math::kPiOver2, sy) : std::asin(sy);
		e.z = std::atan2(2.0f * (w * z + x * y), 1.0f - 2.0f * (y * y + z * z));
		return e;
	}

	// 対角成分のうち一番大きいものを基準にして、割り算の誤差を小さくする
	//   trace（対角の和）> 0 なら w が大きいので w から、そうでなければ x・y・z のうち一番大きいものから求める。
	//   小さな値を基準にすると、0 に近い数で割ることになり結果が大きくぶれるため
	Quaternion Quaternion::FromMatrix(const Matrix4& a)
	{
		const auto& m = a.m;
		Quaternion q;
		float trace = m[0][0] + m[1][1] + m[2][2];
		if (trace > 0.0f)
		{
			float s = std::sqrt(trace + 1.0f) * 2.0f;
			q.w = 0.25f * s;
			q.x = (m[1][2] - m[2][1]) / s;
			q.y = (m[2][0] - m[0][2]) / s;
			q.z = (m[0][1] - m[1][0]) / s;
		}
		else if (m[0][0] > m[1][1] && m[0][0] > m[2][2])
		{
			float s = std::sqrt(1.0f + m[0][0] - m[1][1] - m[2][2]) * 2.0f;
			q.w = (m[1][2] - m[2][1]) / s;
			q.x = 0.25f * s;
			q.y = (m[0][1] + m[1][0]) / s;
			q.z = (m[2][0] + m[0][2]) / s;
		}
		else if (m[1][1] > m[2][2])
		{
			float s = std::sqrt(1.0f + m[1][1] - m[0][0] - m[2][2]) * 2.0f;
			q.w = (m[2][0] - m[0][2]) / s;
			q.x = (m[0][1] + m[1][0]) / s;
			q.y = 0.25f * s;
			q.z = (m[1][2] + m[2][1]) / s;
		}
		else
		{
			float s = std::sqrt(1.0f + m[2][2] - m[0][0] - m[1][1]) * 2.0f;
			q.w = (m[0][1] - m[1][0]) / s;
			q.x = (m[2][0] + m[0][2]) / s;
			q.y = (m[1][2] + m[2][1]) / s;
			q.z = 0.25f * s;
		}
		return Normalize(q);
	}

	// 前（Z）・右（X）・上（Y）の 3 本の軸を作り、それを並べた回転行列からクォータニオンにする
	Quaternion Quaternion::LookRotation(const Vector3& forward, const Vector3& up)
	{
		Vector3 z = Normalize(forward);
		if (LengthSq(z) < math::kEpsilon) { return kIdentity; }
		// 右 = 上 × 前。up は大まかな上でよく、ここで前と直角な右が決まる
		Vector3 x = Cross(up, z);
		if (LengthSq(x) < math::kEpsilon)
		{
			// forward と up が平行なときは別の up を使う
			x = Cross(std::fabs(z.y) < 0.99f ? Vector3::kUnitY : Vector3::kUnitX, z);
		}
		x = Normalize(x);
		// 本当の上 = 前 × 右（up とは少しずれることがある）
		Vector3 y = Cross(z, x);
		// 行ベクトル規約の回転行列は、各行が回転後の X・Y・Z 軸になる
		Matrix4 m(
			x.x, x.y, x.z, 0.0f,
			y.x, y.y, y.z, 0.0f,
			z.x, z.y, z.z, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);
		return FromMatrix(m);
	}

	// 回転軸は from と to の両方に垂直な向き（外積）、角度は 2 つのなす角（内積の acos）
	Quaternion Quaternion::FromTo(const Vector3& from, const Vector3& to)
	{
		Vector3 a = Normalize(from), b = Normalize(to);
		float d = Dot(a, b);
		// 同じ向きなら回さない（外積が 0 になり軸が決まらないため、先に除く）
		if (d >= 1.0f - 1.0e-6f) { return kIdentity; }
		if (d <= -1.0f + 1.0e-6f)
		{
			// 真逆のときは回転軸が決まらないので、a に垂直な軸で半回転する
			Vector3 axis = Cross(Vector3::kUnitX, a);
			if (LengthSq(axis) < 1.0e-6f) { axis = Cross(Vector3::kUnitY, a); }
			return AngleAxis(Normalize(axis), math::kPi);
		}
		// 誤差で d が ±1 を少し超えると acos が NaN になるので Clamp する
		return AngleAxis(Normalize(Cross(a, b)), std::acos(math::Clamp(d, -1.0f, 1.0f)));
	}

	// 長さがほぼ 0 のときは回転として意味がないので「回転なし」を返す
	Quaternion Normalize(const Quaternion& q)
	{
		float len = std::sqrt(Dot(q, q));
		if (len < math::kEpsilon) { return Quaternion::kIdentity; }
		float inv = 1.0f / len;
		return { q.w * inv, q.x * inv, q.y * inv, q.z * inv };
	}

	// クォータニオンの掛け算を 2 回する代わりに、外積 2 回で同じ結果を出す速い式
	//   u = 虚部 (x, y, z)、t = 2 (u × v) として  v' = v + w t + u × t
	Vector3 operator*(const Vector3& v, const Quaternion& q)
	{
		Vector3 u(q.x, q.y, q.z);
		Vector3 t = 2.0f * Cross(u, v);
		return v + q.w * t + Cross(u, t);
	}

	// 遠回りしないよう、内積が負なら片方の符号を反転してから補間する
	//   q と -q は同じ回転を表すが、補間すると片方は近回り、もう片方は遠回り（360 度近く）になる
	Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t)
	{
		float cosOmega = Dot(a, b);
		Quaternion c = b;
		if (cosOmega < 0.0f)
		{
			c = { -b.w, -b.x, -b.y, -b.z };
			cosOmega = -cosOmega;
		}
		// a と c のなす角を omega として、sin の比で重み k0・k1 を決めると回る速さが一定になる
		float k0, k1;
		if (cosOmega > 0.9995f)
		{
			// ほぼ同じ向きだと sinOmega が 0 に近く割り算が不安定なので、ふつうの Lerp にする
			k0 = 1.0f - t;
			k1 = t;
		}
		else
		{
			float sinOmega = std::sqrt(1.0f - cosOmega * cosOmega);
			float omega = std::atan2(sinOmega, cosOmega);
			k0 = std::sin((1.0f - t) * omega) / sinOmega;
			k1 = std::sin(t * omega) / sinOmega;
		}
		return Normalize(Quaternion(
			k0 * a.w + k1 * c.w, k0 * a.x + k1 * c.x, k0 * a.y + k1 * c.y, k0 * a.z + k1 * c.z));
	}

	// Slerp と同じく、遠回りしないよう内積が負なら b の符号を反転する
	Quaternion Nlerp(const Quaternion& a, const Quaternion& b, float t)
	{
		float sign = Dot(a, b) < 0.0f ? -1.0f : 1.0f;
		return Normalize(Quaternion(
			math::Lerp(a.w, b.w * sign, t), math::Lerp(a.x, b.x * sign, t),
			math::Lerp(a.y, b.y * sign, t), math::Lerp(a.z, b.z * sign, t)));
	}
}
