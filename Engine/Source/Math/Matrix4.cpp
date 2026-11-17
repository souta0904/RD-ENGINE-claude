#include "RdPch.h"
#include "Math/Matrix4.h"

namespace rd
{
	Matrix4 Matrix4::CreateScale(const Vector3& s)
	{
		Matrix4 r;
		r.m[0][0] = s.x;
		r.m[1][1] = s.y;
		r.m[2][2] = s.z;
		return r;
	}

	// 各軸の回転行列。回す軸の行と列はそのままで、残りの 2 軸が cos / sin で回る
	// 左手座標系なので、軸の正の向きから見て時計回りが正の角度になる
	Matrix4 Matrix4::CreateRotateX(float radians)
	{
		float c = std::cos(radians), s = std::sin(radians);
		Matrix4 r;
		r.m[1][1] = c; r.m[1][2] = s;
		r.m[2][1] = -s; r.m[2][2] = c;
		return r;
	}

	Matrix4 Matrix4::CreateRotateY(float radians)
	{
		float c = std::cos(radians), s = std::sin(radians);
		Matrix4 r;
		r.m[0][0] = c; r.m[0][2] = -s;
		r.m[2][0] = s; r.m[2][2] = c;
		return r;
	}

	Matrix4 Matrix4::CreateRotateZ(float radians)
	{
		float c = std::cos(radians), s = std::sin(radians);
		Matrix4 r;
		r.m[0][0] = c; r.m[0][1] = s;
		r.m[1][0] = -s; r.m[1][1] = c;
		return r;
	}

	// クォータニオンを回転行列にする公式。2 倍した値（ww, xx, ...）を先に作って掛け算を減らしている
	Matrix4 Matrix4::CreateRotate(const Quaternion& q)
	{
		float ww = 2.0f * q.w, xx = 2.0f * q.x, yy = 2.0f * q.y, zz = 2.0f * q.z;
		Matrix4 r;
		r.m[0][0] = 1.0f - yy * q.y - zz * q.z;
		r.m[0][1] = xx * q.y + ww * q.z;
		r.m[0][2] = xx * q.z - ww * q.y;
		r.m[1][0] = xx * q.y - ww * q.z;
		r.m[1][1] = 1.0f - xx * q.x - zz * q.z;
		r.m[1][2] = yy * q.z + ww * q.x;
		r.m[2][0] = xx * q.z + ww * q.y;
		r.m[2][1] = yy * q.z - ww * q.x;
		r.m[2][2] = 1.0f - xx * q.x - yy * q.y;
		return r;
	}

	Matrix4 Matrix4::CreateTranslate(const Vector3& t)
	{
		Matrix4 r;
		r.m[3][0] = t.x;
		r.m[3][1] = t.y;
		r.m[3][2] = t.z;
		return r;
	}

	// 行列を 3 つ掛ける代わりに、回転行列の各行（各軸）を拡大率で伸ばし、4 行目に移動を入れる
	// Transform が毎フレーム呼ぶので、64 回の掛け算 2 回分を省いている
	Matrix4 Matrix4::CreateAffine(const Vector3& s, const Quaternion& q, const Vector3& t)
	{
		Matrix4 r = CreateRotate(q);
		for (int j = 0; j < 3; ++j)
		{
			r.m[0][j] *= s.x;
			r.m[1][j] *= s.y;
			r.m[2][j] *= s.z;
		}
		r.m[3][0] = t.x;
		r.m[3][1] = t.y;
		r.m[3][2] = t.z;
		return r;
	}

	// カメラの前（z）・右（x）・上（y）の軸を作り、「カメラのワールド行列の逆行列」を直接組み立てる
	//   回転部分 … 軸を列に並べる（回転行列の逆は転置なので、行ではなく列に入れる）
	//   移動部分 … 各軸と eye の内積をマイナスにしたもの（カメラの位置を原点へ戻す）
	Matrix4 Matrix4::CreateLookAt(const Vector3& eye, const Vector3& target, const Vector3& up)
	{
		Vector3 z = Normalize(target - eye);
		Vector3 x = Normalize(Cross(up, z));
		Vector3 y = Cross(z, x);
		return {
			x.x, y.x, z.x, 0.0f,
			x.y, y.y, z.y, 0.0f,
			x.z, y.z, z.z, 0.0f,
			-Dot(x, eye), -Dot(y, eye), -Dot(z, eye), 1.0f };
	}

	// 透視投影（DirectXMath の XMMatrixPerspectiveFovLH と同じ式）
	//   yScale … 視野角の端が画面の端（±1）に来るように縮める。視野角が狭いほど大きく写る
	//   xScale … 画面の縦横比の分だけ横を縮める
	//   range  … 奥行き z を nearZ で 0、farZ で 1 になるように変換する係数
	//   3 行 4 列目の 1 … 変換後の w に z（奥行き）を入れる。GPU が後で x, y を w で割るので遠くが小さくなる
	Matrix4 Matrix4::CreatePerspectiveFov(float fovY, float aspect, float nearZ, float farZ)
	{
		float yScale = 1.0f / std::tan(fovY * 0.5f);
		float xScale = yScale / aspect;
		float range = farZ / (farZ - nearZ);
		return {
			xScale, 0.0f, 0.0f, 0.0f,
			0.0f, yScale, 0.0f, 0.0f,
			0.0f, 0.0f, range, 1.0f,
			0.0f, 0.0f, -nearZ * range, 0.0f };
	}

	// 指定した箱（left～right, top～bottom, nearZ～farZ）を、画面の範囲（x, y は -1～1、z は 0～1）に
	// 拡大と移動だけで合わせる。top < bottom（下向きが +y のスクリーン座標）を渡すと上下が反転する
	Matrix4 Matrix4::CreateOrthographic(float left, float top, float right, float bottom, float nearZ, float farZ)
	{
		return {
			2.0f / (right - left), 0.0f, 0.0f, 0.0f,
			0.0f, 2.0f / (top - bottom), 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f / (farZ - nearZ), 0.0f,
			(left + right) / (left - right), (top + bottom) / (bottom - top), nearZ / (nearZ - farZ), 1.0f };
	}

	Matrix4 Matrix4::CreateOrthographicOffCenter(float left, float right, float bottom, float top, float nearZ, float farZ)
	{
		float range = 1.0f / (farZ - nearZ);
		return {
			2.0f / (right - left), 0.0f, 0.0f, 0.0f,
			0.0f, 2.0f / (top - bottom), 0.0f, 0.0f,
			0.0f, 0.0f, range, 0.0f,
			(left + right) / (left - right), (top + bottom) / (bottom - top), -nearZ * range, 1.0f };
	}

	// 各軸の長さが拡大率（回転では長さが変わらないので、残っているのは拡大の分だけ）
	Vector3 Matrix4::GetScale() const
	{
		return { Length(GetAxisX()), Length(GetAxisY()), Length(GetAxisZ()) };
	}

	Quaternion Matrix4::GetRotation() const
	{
		Vector3 s;
		Quaternion r;
		Vector3 t;
		Decompose(s, r, t);
		return r;
	}

	// 移動は 4 行目、拡大は各軸の長さ、回転は各軸を長さ 1 に戻した行列から求める
	void Matrix4::Decompose(Vector3& scale, Quaternion& rotation, Vector3& translation) const
	{
		translation = GetTranslation();
		scale = GetScale();
		// 各軸を拡大率で割って、回転だけの行列を作る（拡大率 0 の軸は 0 のまま）
		Matrix4 rot;
		Vector3 axes[3] = { GetAxisX(), GetAxisY(), GetAxisZ() };
		for (int i = 0; i < 3; ++i)
		{
			float s = scale[i] > math::kEpsilon ? 1.0f / scale[i] : 0.0f;
			rot.m[i][0] = axes[i].x * s;
			rot.m[i][1] = axes[i].y * s;
			rot.m[i][2] = axes[i].z * s;
		}
		// 鏡像（負のスケール）は X 軸に押し付ける
		//   長さは常に正なので、負の拡大率は回転行列の側に「裏返し」として残る（行列式が負になる）。
		//   裏返しは回転では表せないので、X の拡大率を負にして回転行列の X 軸を反転させ、ふつうの回転に戻す
		if (Determinant(rot) < 0.0f)
		{
			scale.x = -scale.x;
			for (int j = 0; j < 3; ++j) { rot.m[0][j] = -rot.m[0][j]; }
		}
		rotation = Quaternion::FromMatrix(rot);
	}

	// 例: 画面のクリック位置からワールドの位置を求めるとき、ビュー射影行列の逆行列と一緒に使う
	Vector3 TransformCoord(const Vector3& v, const Matrix4& m)
	{
		Vector3 p = TransformPoint(v, m);
		// TransformPoint は 4 列目を計算しないので、w だけ別に求める（w が 0 に近いときは割らない）
		float w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + m.m[3][3];
		return std::fabs(w) > math::kEpsilon ? p / w : p;
	}

	// 上 2 行と下 2 行の 2x2 小行列式（s, c）を組み合わせて求める（ラプラス展開）
	//   s0～s5 … 上 2 行（0 行と 1 行）から 2 列ずつ選んだ 6 通りの 2x2 行列式
	//   c0～c5 … 下 2 行（2 行と 3 行）から、残りの 2 列を選んだ 2x2 行列式
	//   4x4 を 1 行ずつ展開するより掛け算がずっと少なく、Inverse でも同じ値を使い回せる
	float Determinant(const Matrix4& a)
	{
		const auto& m = a.m;
		float s0 = m[0][0] * m[1][1] - m[1][0] * m[0][1];
		float s1 = m[0][0] * m[1][2] - m[1][0] * m[0][2];
		float s2 = m[0][0] * m[1][3] - m[1][0] * m[0][3];
		float s3 = m[0][1] * m[1][2] - m[1][1] * m[0][2];
		float s4 = m[0][1] * m[1][3] - m[1][1] * m[0][3];
		float s5 = m[0][2] * m[1][3] - m[1][2] * m[0][3];
		float c5 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
		float c4 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
		float c3 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
		float c2 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
		float c1 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
		float c0 = m[2][0] * m[3][1] - m[3][0] * m[2][1];
		return s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
	}

	// Determinant と同じ 2x2 小行列式から余因子を作って割る
	//   逆行列 = 余因子行列の転置 / 行列式。各成分の式は、この s と c の組み合わせで書ける
	//   旧版はアフィン変換専用だったが、これは射影行列を含むどんな 4x4 でも使える
	Matrix4 Inverse(const Matrix4& a)
	{
		const auto& m = a.m;
		float s0 = m[0][0] * m[1][1] - m[1][0] * m[0][1];
		float s1 = m[0][0] * m[1][2] - m[1][0] * m[0][2];
		float s2 = m[0][0] * m[1][3] - m[1][0] * m[0][3];
		float s3 = m[0][1] * m[1][2] - m[1][1] * m[0][2];
		float s4 = m[0][1] * m[1][3] - m[1][1] * m[0][3];
		float s5 = m[0][2] * m[1][3] - m[1][2] * m[0][3];
		float c5 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
		float c4 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
		float c3 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
		float c2 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
		float c1 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
		float c0 = m[2][0] * m[3][1] - m[3][0] * m[2][1];
		float det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
		RD_ASSERT(std::fabs(det) > 1.0e-12f, "Matrix4 is singular");
		float inv = 1.0f / det;

		Matrix4 r;
		r.m[0][0] = (m[1][1] * c5 - m[1][2] * c4 + m[1][3] * c3) * inv;
		r.m[0][1] = (-m[0][1] * c5 + m[0][2] * c4 - m[0][3] * c3) * inv;
		r.m[0][2] = (m[3][1] * s5 - m[3][2] * s4 + m[3][3] * s3) * inv;
		r.m[0][3] = (-m[2][1] * s5 + m[2][2] * s4 - m[2][3] * s3) * inv;
		r.m[1][0] = (-m[1][0] * c5 + m[1][2] * c2 - m[1][3] * c1) * inv;
		r.m[1][1] = (m[0][0] * c5 - m[0][2] * c2 + m[0][3] * c1) * inv;
		r.m[1][2] = (-m[3][0] * s5 + m[3][2] * s2 - m[3][3] * s1) * inv;
		r.m[1][3] = (m[2][0] * s5 - m[2][2] * s2 + m[2][3] * s1) * inv;
		r.m[2][0] = (m[1][0] * c4 - m[1][1] * c2 + m[1][3] * c0) * inv;
		r.m[2][1] = (-m[0][0] * c4 + m[0][1] * c2 - m[0][3] * c0) * inv;
		r.m[2][2] = (m[3][0] * s4 - m[3][1] * s2 + m[3][3] * s0) * inv;
		r.m[2][3] = (-m[2][0] * s4 + m[2][1] * s2 - m[2][3] * s0) * inv;
		r.m[3][0] = (-m[1][0] * c3 + m[1][1] * c1 - m[1][2] * c0) * inv;
		r.m[3][1] = (m[0][0] * c3 - m[0][1] * c1 + m[0][2] * c0) * inv;
		r.m[3][2] = (-m[3][0] * s3 + m[3][1] * s1 - m[3][2] * s0) * inv;
		r.m[3][3] = (m[2][0] * s3 - m[2][1] * s1 + m[2][2] * s0) * inv;
		return r;
	}

	Matrix4 Transpose(const Matrix4& m)
	{
		Matrix4 r;
		for (int i = 0; i < 4; ++i)
		{
			for (int j = 0; j < 4; ++j) { r.m[i][j] = m.m[j][i]; }
		}
		return r;
	}
}
