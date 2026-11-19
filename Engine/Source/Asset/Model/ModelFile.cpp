#include "RdPch.h"
#include "Asset/Model/ModelFile.h"
#include "Core/Util/StringUtil.h"
#include <compressapi.h>
#include <fstream>
#include <type_traits>

#pragma comment(lib, "Cabinet.lib")

namespace rd::ModelFile
{
	namespace
	{
		namespace fs = std::filesystem;
		using Bytes = std::vector<uint8_t>;

		constexpr char kMagic[4] = { 'R', 'D', 'M', 'D' };
		constexpr uint32_t kVersion = 1;
		constexpr uint32_t kFlagSkinning = 1;
		constexpr uint32_t kFlagSkeleton = 2;

		static_assert(sizeof(Keyframe<Vector3>) == 16 && sizeof(Keyframe<Quaternion>) == 20);

		// メモリ上のバッファへ順に書く
		class Writer
		{
		public:
			Bytes data;

			void Raw(const void* source, size_t size)
			{
				auto bytes = static_cast<const uint8_t*>(source);
				if (size > 0) { data.insert(data.end(), bytes, bytes + size); }
			}
			template <class T>
			void Value(const T& value)
			{
				static_assert(std::is_trivially_copyable_v<T>);
				Raw(&value, sizeof(T));
			}
			void String(const std::string& text)
			{
				Value(uint32_t(text.size()));
				Raw(text.data(), text.size());
			}
			template <class T>
			void Items(const std::vector<T>& items)
			{
				static_assert(std::is_trivially_copyable_v<T>);
				Raw(items.data(), items.size() * sizeof(T));
			}
			template <class T>
			void Array(const std::vector<T>& items)
			{
				Value(uint32_t(items.size()));
				Items(items);
			}
		};

		// メモリ上のバッファから順に読む（壊れたファイルでも落ちないよう、範囲を超える読み取りはすべて失敗にする）
		class Reader
		{
		public:
			explicit Reader(const Bytes& data) : mData(data) {}
			bool IsOk() const { return mOk; }
			void Fail() { mOk = false; }

			void Raw(void* target, uint64_t size)
			{
				if (!mOk || size > Remaining()) { mOk = false; return; }
				if (size > 0) { std::memcpy(target, mData.data() + mPos, size_t(size)); }
				mPos += size_t(size);
			}
			template <class T>
			void Value(T& value)
			{
				static_assert(std::is_trivially_copyable_v<T>);
				Raw(&value, sizeof(T));
			}
			template <class T>
			T Get()
			{
				T value{};
				Value(value);
				return value;
			}
			// 個数を読む（1 個につき最低 minSize バイトあるはずなので、残りに収まらない個数は壊れていると判断する）
			uint32_t Count(uint64_t minSize)
			{
				uint32_t count = Get<uint32_t>();
				if (mOk && uint64_t(count) * minSize > Remaining()) { mOk = false; }
				return mOk ? count : 0;
			}
			void String(std::string& text)
			{
				text.resize(Count(1));
				Raw(text.data(), text.size());
			}
			template <class T>
			void Items(std::vector<T>& items, uint32_t count)
			{
				static_assert(std::is_trivially_copyable_v<T>);
				if (uint64_t(count) * sizeof(T) > Remaining()) { mOk = false; return; }
				items.resize(count);
				Raw(items.data(), uint64_t(count) * sizeof(T));
			}
			template <class T>
			void Array(std::vector<T>& items) { Items(items, Count(sizeof(T))); }

		private:
			uint64_t Remaining() const { return mData.size() - mPos; }

			const Bytes& mData;
			size_t mPos = 0;
			bool mOk = true;
		};

		// ---- 圧縮（Windows に入っている Compression API を使う。XPRESS は展開が速いので読み込みが遅くならない） ----

		// 圧縮できたら true。小さくならなければ false（そのときは圧縮せずに保存する）
		bool Compress(const Bytes& source, Bytes& out)
		{
			if (source.empty()) { return false; }
			COMPRESSOR_HANDLE handle = nullptr;
			if (!CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &handle)) { return false; }
			out.resize(source.size());
			SIZE_T size = 0;
			bool ok = ::Compress(handle, source.data(), source.size(), out.data(), out.size(), &size) && size < source.size();
			CloseCompressor(handle);
			if (ok) { out.resize(size); }
			return ok;
		}

		bool Decompress(const Bytes& source, Bytes& out)
		{
			DECOMPRESSOR_HANDLE handle = nullptr;
			if (!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &handle)) { return false; }
			SIZE_T size = 0;
			bool ok = ::Decompress(handle, source.data(), source.size(), out.data(), out.size(), &size) && size == out.size();
			CloseDecompressor(handle);
			return ok;
		}

		// ブロック = 元の大きさ u32 + 保存した大きさ u32 + 中身（2 つの大きさが同じなら圧縮なし）
		void WriteBlock(std::ofstream& file, const Bytes& block)
		{
			Bytes compressed;
			const Bytes& stored = Compress(block, compressed) ? compressed : block;
			uint32_t sizes[2] = { uint32_t(block.size()), uint32_t(stored.size()) };
			file.write(reinterpret_cast<const char*>(sizes), sizeof(sizes));
			file.write(reinterpret_cast<const char*>(stored.data()), std::streamsize(stored.size()));
		}

		bool ReadBlock(std::ifstream& file, uint64_t& remaining, Bytes& out)
		{
			uint32_t sizes[2] = {};
			if (remaining < sizeof(sizes)) { return false; }
			file.read(reinterpret_cast<char*>(sizes), sizeof(sizes));
			remaining -= sizeof(sizes);
			if (!file.good() || sizes[1] > remaining || sizes[1] > sizes[0]) { return false; }

			Bytes stored(sizes[1]);
			file.read(reinterpret_cast<char*>(stored.data()), std::streamsize(stored.size()));
			remaining -= sizes[1];
			if (!file.good()) { return false; }
			if (sizes[0] == sizes[1])
			{
				out = std::move(stored);
				return true;
			}
			out.resize(sizes[0]);
			return Decompress(stored, out);
		}

		// ---- 頂点を小さくする ----

		int16_t ToSnorm16(float value) { return int16_t(std::lround(std::clamp(value, -1.0f, 1.0f) * 32767.0f)); }
		float FromSnorm16(int16_t value) { return std::max(float(value) / 32767.0f, -1.0f); }
		float SignNotZero(float value) { return value >= 0.0f ? 1.0f : -1.0f; }

		// 長さ 1 の向きは 2 つの数で表せる（八面体に投影して平面へ開く方法）。16 ビットなら誤差は見えない
		struct Oct16
		{
			int16_t x = 0;
			int16_t y = 0;
		};

		Oct16 EncodeDirection(const Vector3& v)
		{
			float sum = std::fabs(v.x) + std::fabs(v.y) + std::fabs(v.z);
			if (sum <= 0.0f) { return {}; }
			float x = v.x / sum;
			float y = v.y / sum;
			if (v.z < 0.0f)
			{
				float folded = (1.0f - std::fabs(y)) * SignNotZero(x);
				y = (1.0f - std::fabs(x)) * SignNotZero(y);
				x = folded;
			}
			return { ToSnorm16(x), ToSnorm16(y) };
		}

		Vector3 DecodeDirection(Oct16 encoded)
		{
			float x = FromSnorm16(encoded.x);
			float y = FromSnorm16(encoded.y);
			float z = 1.0f - std::fabs(x) - std::fabs(y);
			if (z < 0.0f)
			{
				float folded = (1.0f - std::fabs(y)) * SignNotZero(x);
				y = (1.0f - std::fabs(x)) * SignNotZero(y);
				x = folded;
			}
			return Normalize(Vector3(x, y, z));
		}

		// インデックスは近い番号が続くので、前との差にすると小さな数が並んでよく縮む
		//  差は負にもなるので、0, -1, 1, -2, 2 … を 0, 1, 2, 3, 4 … へ写して正の数にする（ジグザグ符号化）
		uint32_t ZigZag(int32_t value) { return (uint32_t(value) << 1) ^ uint32_t(value >> 31); }
		int32_t UnZigZag(uint32_t value) { return int32_t(value >> 1) ^ -int32_t(value & 1); }

		void WriteVertices(Writer& w, const ModelData& data)
		{
			// 同じ種類の値を並べたほうが圧縮が効くので、頂点ごとではなく要素ごとに書く
			for (const auto& v : data.vertices) { w.Value(v.position); }
			for (const auto& v : data.vertices) { w.Value(EncodeDirection(v.normal)); }
			for (const auto& v : data.vertices) { w.Value(EncodeDirection(v.tangent.XYZ())); }
			for (const auto& v : data.vertices) { w.Value(uint8_t(v.tangent.w < 0.0f ? 1 : 0)); }
			for (const auto& v : data.vertices) { w.Value(v.uv); }
		}

		void ReadVertices(Reader& r, uint32_t count, ModelData& out)
		{
			std::vector<Vector3> positions;
			std::vector<Oct16> normals, tangents;
			std::vector<uint8_t> signs;
			std::vector<Vector2> uvs;
			r.Items(positions, count);
			r.Items(normals, count);
			r.Items(tangents, count);
			r.Items(signs, count);
			r.Items(uvs, count);
			if (!r.IsOk()) { return; }
			out.vertices.resize(count);
			for (uint32_t i = 0; i < count; ++i)
			{
				MeshVertex& v = out.vertices[i];
				v.position = positions[i];
				v.normal = DecodeDirection(normals[i]);
				v.tangent = Vector4(DecodeDirection(tangents[i]), signs[i] ? -1.0f : 1.0f);
				v.uv = uvs[i];
			}
		}

		// 関節の番号は 16 ビット、ウェイトは 0～1 を 16 ビットで持つ
		void WriteSkins(Writer& w, const ModelData& data)
		{
			for (const auto& s : data.skins) { for (uint32_t joint : s.joints) { w.Value(uint16_t(joint)); } }
			for (const auto& s : data.skins)
			{
				for (float weight : s.weights) { w.Value(uint16_t(std::lround(std::clamp(weight, 0.0f, 1.0f) * 65535.0f))); }
			}
		}

		void ReadSkins(Reader& r, uint32_t count, ModelData& out)
		{
			std::vector<uint16_t> joints, weights;
			r.Items(joints, count * 4);
			r.Items(weights, count * 4);
			if (!r.IsOk()) { return; }
			out.skins.resize(count);
			for (uint32_t i = 0; i < count; ++i)
			{
				SkinVertex& s = out.skins[i];
				float sum = 0.0f;
				for (uint32_t k = 0; k < 4; ++k)
				{
					s.joints[k] = joints[i * 4 + k];
					s.weights[k] = float(weights[i * 4 + k]) / 65535.0f;
					sum += s.weights[k];
				}
				// 丸めで合計が 1 からずれるので直す
				if (sum > 0.0f) { for (float& weight : s.weights) { weight /= sum; } }
				else { s.weights[0] = 1.0f; }
			}
		}

		void WriteIndices(Writer& w, const ModelData& data)
		{
			std::vector<uint32_t> deltas(data.indices.size());
			uint32_t largest = 0;
			int64_t previous = 0;
			for (size_t i = 0; i < deltas.size(); ++i)
			{
				deltas[i] = ZigZag(int32_t(int64_t(data.indices[i]) - previous));
				previous = data.indices[i];
				largest = std::max(largest, deltas[i]);
			}
			bool use16Bit = largest <= UINT16_MAX;
			w.Value(uint32_t(use16Bit ? 2 : 4));
			if (use16Bit) { for (uint32_t delta : deltas) { w.Value(uint16_t(delta)); } }
			else { w.Items(deltas); }
		}

		void ReadIndices(Reader& r, uint32_t count, ModelData& out)
		{
			uint32_t indexSize = r.Get<uint32_t>();
			std::vector<uint32_t> deltas;
			if (indexSize == 2)
			{
				std::vector<uint16_t> narrow;
				r.Items(narrow, count);
				deltas.assign(narrow.begin(), narrow.end());
			}
			else if (indexSize == 4) { r.Items(deltas, count); }
			else { r.Fail(); } // 2 でも 4 でもないのは壊れたファイル
			if (!r.IsOk()) { return; }

			out.indices.resize(count);
			int64_t previous = 0;
			for (uint32_t i = 0; i < count; ++i)
			{
				previous += UnZigZag(deltas[i]);
				// 範囲の外は後の確認で弾けるよう、大きな値のままにする
				out.indices[i] = previous < 0 || previous > UINT32_MAX ? UINT32_MAX : uint32_t(previous);
			}
		}

		// ---- アニメーションのキーを減らす ----

		template <class T>
		bool SameValue(const T& a, const T& b) { return std::memcmp(&a, &b, sizeof(T)) == 0; }

		// 前後と同じ値のキーは補間の結果を変えないので省く（動かない関節が焼き込まれた fbx でよく縮む）
		//  全部同じ値なら 1 個だけ残す（AnimationClip はキーが 1 個ならその値を使い続ける）
		template <class T>
		std::vector<Keyframe<T>> RemoveRedundantKeys(const std::vector<Keyframe<T>>& keys)
		{
			if (keys.size() < 2) { return keys; }
			std::vector<Keyframe<T>> result;
			result.push_back(keys.front());
			for (size_t i = 1; i + 1 < keys.size(); ++i)
			{
				bool flat = SameValue(result.back().value, keys[i].value) && SameValue(keys[i].value, keys[i + 1].value);
				if (!flat) { result.push_back(keys[i]); }
			}
			result.push_back(keys.back());
			if (result.size() == 2 && SameValue(result[0].value, result[1].value)) { result.pop_back(); }
			return result;
		}

		// ---- 形状以外の情報 ----

		void WriteBounds(Writer& w, const AABB& bounds)
		{
			w.Value(bounds.min);
			w.Value(bounds.max);
		}

		void ReadBounds(Reader& r, AABB& bounds)
		{
			r.Value(bounds.min);
			r.Value(bounds.max);
		}

		void WriteMaterials(Writer& w, const ModelData& data)
		{
			w.Value(uint32_t(data.materials.size()));
			for (const auto& m : data.materials)
			{
				w.String(m.name);
				w.Value(m.baseColor);
				w.Value(m.metallic);
				w.Value(m.roughness);
				w.Value(m.emissive);
				w.Value(uint8_t(m.transparent ? 1 : 0));
				w.String(m.albedoPath);
				w.String(m.normalPath);
			}
		}

		// .rdmodel の場所を基準にしたパスを、エンジンが読むパスに直す（"../" もここで消える）
		std::string ResolveTexturePath(const std::string& dir, const std::string& relative)
		{
			if (relative.empty()) { return {}; }
			fs::path full = fs::path(ToWide(dir)) / fs::path(ToWide(relative));
			return ToUtf8(full.lexically_normal().generic_wstring());
		}

		void ReadMaterials(Reader& r, const std::string& dir, ModelData& out)
		{
			out.materials.resize(r.Count(1));
			for (auto& m : out.materials)
			{
				r.String(m.name);
				r.Value(m.baseColor);
				r.Value(m.metallic);
				r.Value(m.roughness);
				r.Value(m.emissive);
				m.transparent = r.Get<uint8_t>() != 0;
				std::string albedo, normal;
				r.String(albedo);
				r.String(normal);
				m.albedoPath = ResolveTexturePath(dir, albedo);
				m.normalPath = ResolveTexturePath(dir, normal);
				if (!m.normalPath.empty()) { m.normalPath += "#linear"; }
			}
			if (out.materials.empty()) { out.materials.emplace_back(); }
		}

		void WriteSubMeshes(Writer& w, const ModelData& data)
		{
			w.Value(uint32_t(data.subMeshes.size()));
			for (const auto& s : data.subMeshes)
			{
				w.String(s.name);
				w.Value(s.indexStart);
				w.Value(s.indexCount);
				w.Value(s.materialIndex);
				w.Value(uint8_t(s.skinned ? 1 : 0));
				WriteBounds(w, s.bounds);
			}
		}

		void ReadSubMeshes(Reader& r, ModelData& out)
		{
			out.subMeshes.resize(r.Count(1));
			for (auto& s : out.subMeshes)
			{
				r.String(s.name);
				r.Value(s.indexStart);
				r.Value(s.indexCount);
				r.Value(s.materialIndex);
				s.skinned = r.Get<uint8_t>() != 0;
				ReadBounds(r, s.bounds);
			}
		}

		void WriteSkeleton(Writer& w, const Skeleton& skeleton)
		{
			w.Value(uint32_t(skeleton.joints.size()));
			for (const auto& j : skeleton.joints)
			{
				w.String(j.name);
				w.Value(j.parent);
				w.Value(j.bindLocal);
				w.Value(j.inverseBind);
			}
		}

		void ReadSkeleton(Reader& r, Skeleton& skeleton)
		{
			uint32_t count = r.Count(1);
			skeleton.joints.resize(count);
			for (uint32_t i = 0; i < count && r.IsOk(); ++i)
			{
				Joint& j = skeleton.joints[i];
				r.String(j.name);
				r.Value(j.parent);
				r.Value(j.bindLocal);
				r.Value(j.inverseBind);
				skeleton.nameToIndex.emplace(j.name, int32_t(i));
			}
		}

		void WriteClips(Writer& w, const ModelData& data)
		{
			w.Value(uint32_t(data.clips.size()));
			for (const auto& clip : data.clips)
			{
				w.String(clip->name);
				w.Value(clip->duration);
				w.Value(uint32_t(clip->channels.size()));
				for (const auto& ch : clip->channels)
				{
					w.String(ch.jointName);
					w.Array(RemoveRedundantKeys(ch.positions));
					w.Array(RemoveRedundantKeys(ch.rotations));
					w.Array(RemoveRedundantKeys(ch.scales));
				}
			}
		}

		void ReadClips(Reader& r, ModelData& out)
		{
			uint32_t clipCount = r.Count(1);
			for (uint32_t c = 0; c < clipCount && r.IsOk(); ++c)
			{
				auto clip = std::make_shared<AnimationClip>();
				r.String(clip->name);
				r.Value(clip->duration);
				clip->channels.resize(r.Count(1));
				for (auto& ch : clip->channels)
				{
					r.String(ch.jointName);
					r.Array(ch.positions);
					r.Array(ch.rotations);
					r.Array(ch.scales);
				}
				out.clips.push_back(std::move(clip));
			}
		}

		// 番号が範囲の外を指していると描画やスキニングで不正なアクセスになるので、読み込みの時点で確かめる
		bool IsSkeletonValid(const Skeleton& skeleton)
		{
			for (size_t i = 0; i < skeleton.joints.size(); ++i)
			{
				if (skeleton.joints[i].parent >= int32_t(i) || skeleton.joints[i].parent < -1) { return false; }
			}
			return true;
		}

		bool IsGeometryValid(const ModelData& data)
		{
			uint32_t vertexCount = uint32_t(data.vertices.size());
			for (uint32_t index : data.indices)
			{
				if (index >= vertexCount) { return false; }
			}
			for (const auto& s : data.subMeshes)
			{
				if (uint64_t(s.indexStart) + s.indexCount > data.indices.size()) { return false; }
				if (s.materialIndex >= data.materials.size()) { return false; }
			}
			uint32_t jointCount = data.skeleton ? uint32_t(data.skeleton->joints.size()) : 0;
			for (const auto& skin : data.skins)
			{
				for (uint32_t joint : skin.joints)
				{
					if (joint >= jointCount) { return false; }
				}
			}
			return true;
		}
	}

	bool Write(const std::string& path, const ModelData& data, std::string& error)
	{
		if (data.skeleton && data.skeleton->joints.size() > 65536)
		{
			error = "too many joints";
			return false;
		}

		Writer info;
		WriteBounds(info, data.bounds);
		WriteMaterials(info, data);
		WriteSubMeshes(info, data);
		if (data.skeleton) { WriteSkeleton(info, *data.skeleton); }
		WriteClips(info, data);

		Writer geometry;
		geometry.Value(uint32_t(data.vertices.size()));
		geometry.Value(uint32_t(data.indices.size()));
		WriteVertices(geometry, data);
		if (data.hasSkinning) { WriteSkins(geometry, data); }
		WriteIndices(geometry, data);

		fs::path target = ToWide(path);
		fs::path temp = target;
		temp += L".tmp";
		std::error_code ec;
		{
			std::ofstream file(temp, std::ios::binary | std::ios::trunc);
			uint32_t flags = (data.hasSkinning ? kFlagSkinning : 0) | (data.skeleton ? kFlagSkeleton : 0);
			file.write(kMagic, sizeof(kMagic));
			file.write(reinterpret_cast<const char*>(&kVersion), sizeof(kVersion));
			file.write(reinterpret_cast<const char*>(&flags), sizeof(flags));
			WriteBlock(file, info.data);
			WriteBlock(file, geometry.data);
			if (!file.good())
			{
				error = "could not write the file";
				file.close();
				fs::remove(temp, ec);
				return false;
			}
		}
		fs::rename(temp, target, ec);
		if (ec)
		{
			error = ec.message();
			fs::remove(temp, ec);
			return false;
		}
		return true;
	}

	bool Read(const std::string& path, bool animOnly, ModelData& out, std::string& error)
	{
		fs::path file = ToWide(path);
		std::error_code ec;
		uint64_t remaining = fs::file_size(file, ec);
		std::ifstream stream(file, std::ios::binary);
		struct Header
		{
			char magic[4];
			uint32_t version;
			uint32_t flags;
		} header = {};
		if (ec || !stream.good() || remaining < sizeof(Header))
		{
			error = "not an .rdmodel file";
			return false;
		}
		stream.read(reinterpret_cast<char*>(&header), sizeof(header));
		remaining -= sizeof(header);
		if (!stream.good() || std::memcmp(header.magic, kMagic, sizeof(kMagic)) != 0)
		{
			error = "not an .rdmodel file";
			return false;
		}
		if (header.version != kVersion)
		{
			error = std::format("unsupported version {}", header.version);
			return false;
		}
		error = "the file is damaged";

		Bytes infoBytes;
		if (!ReadBlock(stream, remaining, infoBytes)) { return false; }
		Reader info(infoBytes);
		out.hasSkinning = (header.flags & kFlagSkinning) != 0;
		ReadBounds(info, out.bounds);
		ReadMaterials(info, GetDirectory(NormalizePath(path)), out);
		ReadSubMeshes(info, out);
		if (header.flags & kFlagSkeleton)
		{
			out.skeleton = std::make_shared<Skeleton>();
			ReadSkeleton(info, *out.skeleton);
		}
		ReadClips(info, out);
		if (!info.IsOk() || (out.skeleton && !IsSkeletonValid(*out.skeleton))) { return false; }

		// アニメーションだけなら、形状のブロックは読まずに終わる
		if (!animOnly)
		{
			Bytes geometryBytes;
			if (!ReadBlock(stream, remaining, geometryBytes)) { return false; }
			Reader geometry(geometryBytes);
			uint32_t vertexCount = geometry.Get<uint32_t>();
			uint32_t indexCount = geometry.Get<uint32_t>();
			// 1 頂点は最低でも 29 バイト。先に確かめて、壊れた個数で大きなメモリを取らないようにする
			if (uint64_t(vertexCount) * 29 > geometryBytes.size()) { return false; }
			ReadVertices(geometry, vertexCount, out);
			if (out.hasSkinning) { ReadSkins(geometry, vertexCount, out); }
			ReadIndices(geometry, indexCount, out);
			if (!geometry.IsOk() || !IsGeometryValid(out)) { return false; }
		}
		error.clear();
		return true;
	}
}
