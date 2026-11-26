#include "RdPch.h"
#include "AI/Navigation/NavMeshBuilder.h"
#include "Core/Serialization/Json.h"
#include <execution>
#include <unordered_map>

namespace rd
{
	namespace
	{
		constexpr int kMaxHeight = 0x7fffff; // 上に何もない柱の「上の端」
		// 向き 0: -X, 1: +Z, 2: +X, 3: -Z（Recast と同じ並び。向き d の次の向きは (d + 1) & 3）
		constexpr int kDirX[4] = { -1, 0, 1, 0 };
		constexpr int kDirZ[4] = { 0, 1, 0, -1 };

		// 焼くときの作業用の配列（中身は数や構造体だけ）
		//  std::vector は Debug だと push_back などのたびにプロセス全体で 1 つのロックを取る（イテレーターの検査）ので、
		//  タイルを並べて焼くとスレッドどうしが待ち合って、1 本で焼くより遅くなる。それを避けるための簡単な配列
		template <class T>
		class Buffer
		{
		public:
			size_t size() const { return mSize; }
			void clear() { mSize = 0; }
			void reserve(size_t n)
			{
				if (n <= mCapacity) { return; }
				auto data = std::make_unique_for_overwrite<T[]>(n);
				if (mSize > 0) { std::memcpy(data.get(), mData.get(), mSize * sizeof(T)); }
				mData = std::move(data);
				mCapacity = n;
			}
			// 増えた分の中身は決まっていない
			void resize(size_t n)
			{
				reserve(n);
				mSize = n;
			}
			void assign(size_t n, const T& value)
			{
				resize(n);
				std::fill_n(mData.get(), n, value);
			}
			void CopyFrom(const Buffer& other)
			{
				resize(other.mSize);
				if (mSize > 0) { std::memcpy(mData.get(), other.mData.get(), mSize * sizeof(T)); }
			}
			void push_back(const T& value)
			{
				if (mSize == mCapacity) { reserve(std::max<size_t>(16, mCapacity * 2)); }
				mData[mSize++] = value;
			}
			void pop_back() { --mSize; }
			T& back() { return mData[mSize - 1]; }
			const T& back() const { return mData[mSize - 1]; }
			T& operator[](size_t i) { return mData[i]; }
			const T& operator[](size_t i) const { return mData[i]; }
			T* begin() { return mData.get(); }
			T* end() { return mData.get() + mSize; }
			const T* begin() const { return mData.get(); }
			const T* end() const { return mData.get() + mSize; }

		private:
			std::unique_ptr<T[]> mData;
			size_t mSize = 0;
			size_t mCapacity = 0;
		};

		// 柱の中の、ふさがった区間（ボクセルの高さの単位）。area は 0 なら歩けない、それ以外は区画の番号 + 1
		//  柱ごとに下から順の連結リスト（next は spans の番号、-1 で終わり）
		struct Span
		{
			int smin;
			int smax;
			int area;
			int next;
		};

		// 柱（マス）ごとの区間。柱が何百万本にもなるので、区間は 1 つの配列にまとめて置き、
		//  柱は先頭の番号だけを持つ（柱ごとに配列を持つとメモリの確保が多すぎて遅い）
		struct Heightfield
		{
			int width = 0;
			int depth = 0;
			Vector3 bmin;
			Vector3 bmax;
			float cs = 0.1f;
			float ch = 0.05f;
			Buffer<int> heads; // x + z * width → 最初の区間（-1 なら空）
			Buffer<Span> spans;
			int freeList = -1;      // まとめて消えた区間を使い回す

			int Allocate()
			{
				if (freeList >= 0)
				{
					int i = freeList;
					freeList = spans[size_t(i)].next;
					return i;
				}
				spans.push_back({});
				return int(spans.size()) - 1;
			}

			// 区間を足す。重なる区間とは 1 つにまとめ、上の端が近い（mergeThreshold 以内）なら歩ける方を残す
			void AddSpan(int x, int z, int smin, int smax, int area, int mergeThreshold)
			{
				int& head = heads[size_t(x + z * width)];
				int prev = -1;
				int cur = head;
				while (cur >= 0)
				{
					Span& c = spans[size_t(cur)];
					if (c.smin > smax) { break; }
					if (c.smax < smin)
					{
						prev = cur;
						cur = c.next;
						continue;
					}
					int merged = area;
					if (std::abs(smax - c.smax) <= mergeThreshold) { merged = std::max(area, c.area); }
					else if (c.smax > smax) { merged = c.area; }
					smin = std::min(smin, c.smin);
					smax = std::max(smax, c.smax);
					area = merged;
					// cur を外して使い回しへ
					int next = c.next;
					c.next = freeList;
					freeList = cur;
					if (prev >= 0) { spans[size_t(prev)].next = next; }
					else { head = next; }
					cur = next;
				}
				int s = Allocate();
				spans[size_t(s)] = { smin, smax, area, cur };
				if (prev >= 0) { spans[size_t(prev)].next = s; }
				else { head = s; }
			}
		};

		// 多角形を、axis（0: X, 2: Z）の座標 = line の直線で 2 つに分ける（out1 が小さい側）
		void DividePoly(const Vector3* in, int count, Vector3* out1, int& count1, Vector3* out2, int& count2, float line, int axis)
		{
			float d[12];
			for (int i = 0; i < count; ++i) { d[i] = line - (axis == 0 ? in[i].x : in[i].z); }
			count1 = 0;
			count2 = 0;
			for (int i = 0, j = count - 1; i < count; j = i, ++i)
			{
				bool ina = d[j] >= 0.0f;
				bool inb = d[i] >= 0.0f;
				if (ina != inb)
				{
					float s = d[j] / (d[j] - d[i]);
					Vector3 p = in[j] + (in[i] - in[j]) * s;
					out1[count1++] = p;
					out2[count2++] = p;
					if (d[i] > 0.0f) { out1[count1++] = in[i]; }
					else if (d[i] < 0.0f) { out2[count2++] = in[i]; }
				}
				else
				{
					if (d[i] >= 0.0f)
					{
						out1[count1++] = in[i];
						if (d[i] != 0.0f) { continue; }
					}
					out2[count2++] = in[i];
				}
			}
		}

		// 三角形を、通るマスごとに切り分けて、その中の高さの範囲を区間として塗る
		void RasterizeTriangle(Heightfield& hf, const Vector3& v0, const Vector3& v1, const Vector3& v2, int area, int mergeThreshold)
		{
			Vector3 tmin(std::min({ v0.x, v1.x, v2.x }), std::min({ v0.y, v1.y, v2.y }), std::min({ v0.z, v1.z, v2.z }));
			Vector3 tmax(std::max({ v0.x, v1.x, v2.x }), std::max({ v0.y, v1.y, v2.y }), std::max({ v0.z, v1.z, v2.z }));
			if (tmin.x > hf.bmax.x || tmax.x < hf.bmin.x || tmin.y > hf.bmax.y || tmax.y < hf.bmin.y ||
				tmin.z > hf.bmax.z || tmax.z < hf.bmin.z) { return; }

			const float ics = 1.0f / hf.cs;
			const float ich = 1.0f / hf.ch;
			const float height = hf.bmax.y - hf.bmin.y;
			int z0 = std::clamp(int((tmin.z - hf.bmin.z) * ics), -1, hf.depth - 1);
			int z1 = std::clamp(int((tmax.z - hf.bmin.z) * ics), 0, hf.depth - 1);

			Vector3 in[12], rest[12], row[12], cell[12], rowRest[12];
			int inCount = 3;
			in[0] = v0; in[1] = v1; in[2] = v2;
			for (int z = z0; z <= z1; ++z)
			{
				int rowCount = 0, restCount = 0;
				DividePoly(in, inCount, row, rowCount, rest, restCount, hf.bmin.z + float(z + 1) * hf.cs, 2);
				std::copy(rest, rest + restCount, in);
				inCount = restCount;
				if (rowCount < 3 || z < 0) { continue; }

				float minX = row[0].x, maxX = row[0].x;
				for (int i = 1; i < rowCount; ++i)
				{
					minX = std::min(minX, row[i].x);
					maxX = std::max(maxX, row[i].x);
				}
				int x0 = int((minX - hf.bmin.x) * ics);
				int x1 = int((maxX - hf.bmin.x) * ics);
				if (x1 < 0 || x0 >= hf.width) { continue; }
				x0 = std::clamp(x0, -1, hf.width - 1);
				x1 = std::clamp(x1, 0, hf.width - 1);

				for (int x = x0; x <= x1; ++x)
				{
					int cellCount = 0, rowRestCount = 0;
					DividePoly(row, rowCount, cell, cellCount, rowRest, rowRestCount, hf.bmin.x + float(x + 1) * hf.cs, 0);
					std::copy(rowRest, rowRest + rowRestCount, row);
					rowCount = rowRestCount;
					if (cellCount < 3 || x < 0) { continue; }

					float smin = cell[0].y, smax = cell[0].y;
					for (int i = 1; i < cellCount; ++i)
					{
						smin = std::min(smin, cell[i].y);
						smax = std::max(smax, cell[i].y);
					}
					smin -= hf.bmin.y;
					smax -= hf.bmin.y;
					if (smax < 0.0f || smin > height) { continue; }
					smin = std::max(smin, 0.0f);
					smax = std::min(smax, height);
					int ismin = std::clamp(int(std::floor(smin * ich)), 0, kMaxHeight);
					int ismax = std::clamp(int(std::ceil(smax * ich)), ismin + 1, kMaxHeight);
					hf.AddSpan(x, z, ismin, ismax, area, mergeThreshold);
				}
			}
		}

		// 上が空いた面（歩ける区間の上の端から、上の区間の下の端まで）
		struct OpenSpan
		{
			int y = 0;
			int h = 0;
			int con[4] = { -1, -1, -1, -1 };
			int area = 0;
		};

		struct OpenField
		{
			int width = 0;
			int depth = 0;
			Buffer<int> cellStart; // マスごとの、spans の最初の番号（cellStart[c] ～ cellStart[c + 1]）
			Buffer<OpenSpan> spans;
			Buffer<int> spanCell; // 面 → マスの番号（x + z * width）

			// 歩ける隣（つながっていないか、削られていれば -1）
			int Neighbor(int i, int dir) const
			{
				int n = spans[size_t(i)].con[dir];
				return n >= 0 && spans[size_t(n)].area != 0 ? n : -1;
			}

			// マスの角の高さ。角を囲む（つながった）面の中でいちばん高いもの（Recast の getCornerHeight）。
			//  角を共有するどのマスから求めても同じ値になるので、隣り合う四角形の頂点の高さがそろう
			int CornerHeight(int i, int dir) const
			{
				int h = spans[size_t(i)].y;
				int dirp = (dir + 1) & 3;
				if (int a = Neighbor(i, dir); a >= 0)
				{
					h = std::max(h, spans[size_t(a)].y);
					if (int aa = Neighbor(a, dirp); aa >= 0) { h = std::max(h, spans[size_t(aa)].y); }
				}
				if (int b = Neighbor(i, dirp); b >= 0)
				{
					h = std::max(h, spans[size_t(b)].y);
					if (int bb = Neighbor(b, dir); bb >= 0) { h = std::max(h, spans[size_t(bb)].y); }
				}
				return h;
			}
		};

		// 段差が climb 以内の、歩けない区間の上の端は歩けることにする（階段の段の縁など）
		void FilterLowHangingObstacles(Heightfield& hf, int climb)
		{
			for (int head : hf.heads)
			{
				bool prevWalkable = false;
				int prevArea = 0;
				int prevSmax = 0;
				for (int i = head; i >= 0; i = hf.spans[size_t(i)].next)
				{
					Span& s = hf.spans[size_t(i)];
					bool walkable = s.area != 0;
					if (!walkable && prevWalkable && std::abs(s.smax - prevSmax) <= climb) { s.area = prevArea; }
					prevWalkable = walkable;
					prevArea = s.area;
					prevSmax = s.smax;
				}
			}
		}

		// 上の区間までの空きがエージェントの背より低い面は歩けない
		void FilterLowHeightSpans(Heightfield& hf, int walkableHeight)
		{
			for (int head : hf.heads)
			{
				for (int i = head; i >= 0; i = hf.spans[size_t(i)].next)
				{
					Span& s = hf.spans[size_t(i)];
					int top = s.next >= 0 ? hf.spans[size_t(s.next)].smin : kMaxHeight;
					if (top - s.smax < walkableHeight) { s.area = 0; }
				}
			}
		}

		void BuildOpenField(const Heightfield& hf, int walkableHeight, int climb, OpenField& out)
		{
			out.width = hf.width;
			out.depth = hf.depth;
			out.cellStart.assign(size_t(hf.width * hf.depth) + 1, 0);
			out.spans.clear();
			out.spanCell.clear();
			for (int c = 0; c < hf.width * hf.depth; ++c)
			{
				out.cellStart[size_t(c)] = int(out.spans.size());
				for (int i = hf.heads[size_t(c)]; i >= 0; i = hf.spans[size_t(i)].next)
				{
					const Span& span = hf.spans[size_t(i)];
					if (span.area == 0) { continue; }
					OpenSpan s;
					s.y = span.smax;
					s.h = (span.next >= 0 ? hf.spans[size_t(span.next)].smin : kMaxHeight) - s.y;
					s.area = span.area;
					out.spans.push_back(s);
					out.spanCell.push_back(c);
				}
			}
			out.cellStart.back() = int(out.spans.size());

			// 隣のマスの面で、段差が climb 以内で、重なった空きに背が入るものとつなぐ
			for (int z = 0; z < hf.depth; ++z)
			{
				for (int x = 0; x < hf.width; ++x)
				{
					int c = x + z * hf.width;
					for (int i = out.cellStart[size_t(c)]; i < out.cellStart[size_t(c) + 1]; ++i)
					{
						OpenSpan& s = out.spans[size_t(i)];
						for (int dir = 0; dir < 4; ++dir)
						{
							int nx = x + kDirX[dir], nz = z + kDirZ[dir];
							if (nx < 0 || nz < 0 || nx >= hf.width || nz >= hf.depth) { continue; }
							int nc = nx + nz * hf.width;
							for (int j = out.cellStart[size_t(nc)]; j < out.cellStart[size_t(nc) + 1]; ++j)
							{
								const OpenSpan& n = out.spans[size_t(j)];
								int bottom = std::max(s.y, n.y);
								int top = std::min(s.y + s.h, n.y + n.h);
								if (top - bottom >= walkableHeight && std::abs(n.y - s.y) <= climb)
								{
									s.con[dir] = j;
									break;
								}
							}
						}
					}
				}
			}
		}

		// 縁から radius マスの内側までを歩けなくする（Recast の rcErodeWalkableArea。
		//  縦横は 2、斜めは 3 の重みで縁からの距離を測る）
		void ErodeWalkableArea(OpenField& f, int radius)
		{
			const size_t count = f.spans.size();
			thread_local Buffer<uint8_t> dist;
			dist.assign(count, 0xff);
			for (int c = 0; c < f.width * f.depth; ++c)
			{
				for (int i = f.cellStart[size_t(c)]; i < f.cellStart[size_t(c) + 1]; ++i)
				{
					int connected = 0;
					for (int dir = 0; dir < 4; ++dir)
					{
						if (f.Neighbor(i, dir) >= 0) { ++connected; }
					}
					if (connected != 4) { dist[size_t(i)] = 0; }
				}
			}

			auto relax = [&](int i, int a, int weight)
			{
				int d = std::min(int(dist[size_t(a)]) + weight, 255);
				if (d < dist[size_t(i)]) { dist[size_t(i)] = uint8_t(d); }
			};
			// 左下から右上へ（-X と -Z の側から）
			for (int z = 0; z < f.depth; ++z)
			{
				for (int x = 0; x < f.width; ++x)
				{
					int c = x + z * f.width;
					for (int i = f.cellStart[size_t(c)]; i < f.cellStart[size_t(c) + 1]; ++i)
					{
						if (int a = f.spans[size_t(i)].con[0]; a >= 0)
						{
							relax(i, a, 2);
							if (int aa = f.spans[size_t(a)].con[3]; aa >= 0) { relax(i, aa, 3); }
						}
						if (int a = f.spans[size_t(i)].con[3]; a >= 0)
						{
							relax(i, a, 2);
							if (int aa = f.spans[size_t(a)].con[2]; aa >= 0) { relax(i, aa, 3); }
						}
					}
				}
			}
			// 右上から左下へ（+X と +Z の側から）
			for (int z = f.depth - 1; z >= 0; --z)
			{
				for (int x = f.width - 1; x >= 0; --x)
				{
					int c = x + z * f.width;
					for (int i = f.cellStart[size_t(c)]; i < f.cellStart[size_t(c) + 1]; ++i)
					{
						if (int a = f.spans[size_t(i)].con[2]; a >= 0)
						{
							relax(i, a, 2);
							if (int aa = f.spans[size_t(a)].con[1]; aa >= 0) { relax(i, aa, 3); }
						}
						if (int a = f.spans[size_t(i)].con[1]; a >= 0)
						{
							relax(i, a, 2);
							if (int aa = f.spans[size_t(a)].con[0]; aa >= 0) { relax(i, aa, 3); }
						}
					}
				}
			}

			const int threshold = radius * 2;
			for (size_t i = 0; i < count; ++i)
			{
				if (dist[i] < threshold) { f.spans[i].area = 0; }
			}
		}

		// つながった面の集まりのうち、minCells より狭いものを捨てる。
		//  タイルの担当範囲（core）の外へはみ出す集まりは、隣のタイルへ続いていて本当の広さが分からないので残す
		void RemoveSmallRegions(OpenField& f, int minCells, int coreX0, int coreZ0, int coreX1, int coreZ1)
		{
			thread_local Buffer<char> visited;
			thread_local Buffer<int> region;
			visited.assign(f.spans.size(), 0);
			for (int start = 0; start < int(f.spans.size()); ++start)
			{
				if (visited[size_t(start)] || f.spans[size_t(start)].area == 0) { continue; }
				region.clear();
				region.push_back(start);
				visited[size_t(start)] = 1;
				for (size_t head = 0; head < region.size(); ++head)
				{
					for (int dir = 0; dir < 4; ++dir)
					{
						int n = f.Neighbor(region[head], dir);
						if (n < 0 || visited[size_t(n)]) { continue; }
						visited[size_t(n)] = 1;
						region.push_back(n);
					}
				}
				bool inside = true;
				for (int i : region)
				{
					int c = f.spanCell[size_t(i)];
					int x = c % f.width, z = c / f.width;
					inside = inside && x >= coreX0 && x < coreX1 && z >= coreZ0 && z < coreZ1;
				}
				if (inside && int(region.size()) < minCells)
				{
					for (int i : region) { f.spans[size_t(i)].area = 0; }
				}
			}
		}

		// まとめた四角形（タイルの中のマスの座標と、四隅の格子点の高さ）
		struct Quad
		{
			int x0, z0, x1, z1;
			int h00, h10, h01, h11;
			int area;
		};

		// 残った面を四角形にまとめて三角形にする
		//  四角形は、格子点（マスの角）の高さがすべて 1 枚の平面から tolerance 以内に収まる範囲まで広げる。
		//  平面は 左下の角 + X 方向の傾き（1 行目で決まる）+ Z 方向の傾き（左の辺で決まる）で表し、
		//  行ごとに「X 方向の傾きを引いた残り」の最小と最大を覚えておくと、行を足すたびの確かめが行の数だけで済む
		//  四角形はタイルの担当範囲（core）の中だけに作る（隣のタイルとの境目では辺の一部が重なる）
		void BuildRectangles(const OpenField& f, int coreX0, int coreZ0, int coreX1, int coreZ1, int maxCells, std::vector<Quad>& out)
		{
			constexpr float kTolerance = 2.0f; // ボクセルの高さの単位
			thread_local Buffer<char> used;
			used.assign(f.spans.size(), 0);
			auto usable = [&](int i, int area) { return i >= 0 && !used[size_t(i)] && f.spans[size_t(i)].area == area; };

			thread_local Buffer<int> cells;          // 四角形のマス（行ごとに cols 個ずつ）
			thread_local Buffer<float> rmin, rmax;   // 格子の行ごとの、X 方向の傾きを引いた残りの最小と最大
			thread_local Buffer<int> bottom, top, line, next; // 1 行目の下と上の格子点の高さ、足す行の上の格子点の高さとマス
			for (int z = coreZ0; z < coreZ1; ++z)
			{
				for (int x = coreX0; x < coreX1; ++x)
				{
					int c = x + z * f.width;
					for (int start = f.cellStart[size_t(c)]; start < f.cellStart[size_t(c) + 1]; ++start)
					{
						const int area = f.spans[size_t(start)].area;
						if (area == 0 || used[size_t(start)]) { continue; }

						// 1 行目を +X へ伸ばす。下の辺（b = 0）と上の辺（b = 1）の格子点が平面に乗る範囲まで
						cells.clear();
						cells.push_back(start);
						bottom.clear();
						bottom.push_back(f.CornerHeight(start, 3));
						bottom.push_back(f.CornerHeight(start, 2));
						top.clear();
						top.push_back(f.CornerHeight(start, 0));
						top.push_back(f.CornerHeight(start, 1));
						while (int(cells.size()) < maxCells && x + int(cells.size()) < coreX1)
						{
							int n = f.Neighbor(cells.back(), 2);
							if (!usable(n, area)) { break; }
							bottom.push_back(f.CornerHeight(n, 2));
							top.push_back(f.CornerHeight(n, 1));
							const int cols = int(cells.size()) + 1;
							const float gx = float(bottom[size_t(cols)] - bottom[0]) / float(cols);
							const float gz = float(top[0] - bottom[0]);
							bool fits = true;
							for (int a = 0; a <= cols && fits; ++a)
							{
								float planeBottom = float(bottom[0]) + gx * float(a);
								fits = std::fabs(float(bottom[size_t(a)]) - planeBottom) <= kTolerance &&
									std::fabs(float(top[size_t(a)]) - planeBottom - gz) <= kTolerance;
							}
							if (!fits)
							{
								bottom.pop_back();
								top.pop_back();
								break;
							}
							cells.push_back(n);
						}
						const int cols = int(cells.size());
						const float gx = float(bottom[size_t(cols)] - bottom[0]) / float(cols);
						auto residual = [&](const Buffer<int>& heights, float& lo, float& hi)
						{
							lo = FLT_MAX;
							hi = -FLT_MAX;
							for (int a = 0; a <= cols; ++a)
							{
								float r = float(heights[size_t(a)] - bottom[0]) - gx * float(a);
								lo = std::min(lo, r);
								hi = std::max(hi, r);
							}
						};
						int rowCount = 1;
						rmin.resize(2);
						rmax.resize(2);
						residual(bottom, rmin[0], rmax[0]);
						residual(top, rmin[1], rmax[1]);

						// 行を +Z へ足す。すべてのマスが下の行とつながり、横どうしもつながり、平面に乗る間
						while (rowCount < maxCells && z + rowCount < coreZ1)
						{
							const size_t prev = size_t(rowCount - 1) * size_t(cols);
							next.resize(size_t(cols));
							bool ok = true;
							for (int a = 0; a < cols && ok; ++a)
							{
								int n = f.Neighbor(cells[prev + size_t(a)], 1);
								ok = usable(n, area) && (a == 0 || f.Neighbor(next[size_t(a) - 1], 2) == n);
								next[size_t(a)] = n;
							}
							if (!ok) { break; }

							line.resize(size_t(cols) + 1);
							for (int a = 0; a < cols; ++a) { line[size_t(a)] = f.CornerHeight(next[size_t(a)], 0); }
							line[size_t(cols)] = f.CornerHeight(next[size_t(cols) - 1], 1);
							float lo = 0.0f, hi = 0.0f;
							residual(line, lo, hi);
							const int b = rowCount + 1;
							const float gz = float(line[0] - bottom[0]) / float(b);
							bool fits = true;
							for (int k = 0; k < b && fits; ++k)
							{
								fits = rmin[size_t(k)] >= gz * float(k) - kTolerance && rmax[size_t(k)] <= gz * float(k) + kTolerance;
							}
							fits = fits && lo >= gz * float(b) - kTolerance && hi <= gz * float(b) + kTolerance;
							if (!fits) { break; }
							for (int a = 0; a < cols; ++a) { cells.push_back(next[size_t(a)]); }
							rmin.push_back(lo);
							rmax.push_back(hi);
							top.CopyFrom(line);
							++rowCount;
						}

						for (int i : cells) { used[size_t(i)] = 1; }
						Quad q;
						q.x0 = x;
						q.z0 = z;
						q.x1 = x + cols;
						q.z1 = z + rowCount;
						q.h00 = bottom[0];
						q.h10 = bottom[size_t(cols)];
						q.h01 = top[0];
						q.h11 = top[size_t(cols)];
						q.area = area - 1;
						out.push_back(q);
					}
				}
			}
		}
	}

	void NavMeshBuilder::AddTriangles(const std::vector<Vector3>& positions, const std::vector<uint32_t>& indices, int area)
	{
		uint32_t base = uint32_t(mPositions.size());
		mPositions.insert(mPositions.end(), positions.begin(), positions.end());
		for (size_t i = 0; i + 2 < indices.size(); i += 3)
		{
			if (indices[i] >= positions.size() || indices[i + 1] >= positions.size() || indices[i + 2] >= positions.size()) { continue; }
			mIndices.push_back(base + indices[i]);
			mIndices.push_back(base + indices[i + 1]);
			mIndices.push_back(base + indices[i + 2]);
			mAreas.push_back(area);
		}
	}

	void NavMeshBuilder::Clear()
	{
		mPositions.clear();
		mIndices.clear();
		mAreas.clear();
	}

	bool NavMeshBuilder::Build(const NavMeshBuildSettings& settings, NavMeshBuildResult& out) const
	{
		out = {};
		if (mIndices.empty() || settings.cellSize <= 0.0f) { return false; }

		const float cs = settings.cellSize;
		const float ch = settings.cellHeight > 0.0f ? settings.cellHeight : settings.cellSize * 0.5f;
		Vector3 bmin(FLT_MAX, FLT_MAX, FLT_MAX), bmax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		for (uint32_t i : mIndices)
		{
			const Vector3& p = mPositions[i];
			bmin = Vector3(std::min(bmin.x, p.x), std::min(bmin.y, p.y), std::min(bmin.z, p.z));
			bmax = Vector3(std::max(bmax.x, p.x), std::max(bmax.y, p.y), std::max(bmax.z, p.z));
		}
		if (settings.useBounds)
		{
			bmin = Vector3(std::max(bmin.x, settings.boundsMin.x), std::max(bmin.y, settings.boundsMin.y), std::max(bmin.z, settings.boundsMin.z));
			bmax = Vector3(std::min(bmax.x, settings.boundsMax.x), std::min(bmax.y, settings.boundsMax.y), std::min(bmax.z, settings.boundsMax.z));
		}
		if (bmin.x >= bmax.x || bmin.y > bmax.y || bmin.z >= bmax.z) { return false; }
		const int width = std::max(1, int((bmax.x - bmin.x) / cs + 0.5f));
		const int depth = std::max(1, int((bmax.z - bmin.z) / cs + 0.5f));

		const int walkableHeight = std::max(1, int(std::ceil(settings.agentHeight / ch)));
		const int walkableClimb = std::max(0, int(std::floor(settings.agentClimb / ch)));
		const int walkableRadius = std::max(0, int(std::ceil(settings.agentRadius / cs)));
		const float walkableNormalY = std::cos(settings.agentMaxSlope * 3.14159265f / 180.0f);
		const int minRegionCells = int(std::ceil(settings.minRegionArea / (cs * cs)));
		const int maxCells = std::max(1, int(settings.maxRectSize / cs));

		// 三角形ごとの、上から見た範囲と区画（上を向いて坂が緩い面だけ歩ける。左手系で、上から見て時計回りの面が上向き）
		struct InputTri
		{
			float minX, maxX, minZ, maxZ;
			int area;
		};
		std::vector<InputTri> tris(mAreas.size());
		for (size_t t = 0; t < mAreas.size(); ++t)
		{
			const Vector3& v0 = mPositions[mIndices[t * 3]];
			const Vector3& v1 = mPositions[mIndices[t * 3 + 1]];
			const Vector3& v2 = mPositions[mIndices[t * 3 + 2]];
			Vector3 n = Cross(v1 - v0, v2 - v0);
			float len = Length(n);
			int area = 0;
			if (len > 1.0e-12f && mAreas[t] != 1 && n.y / len > walkableNormalY) { area = mAreas[t] + 1; }
			tris[t] = { std::min({ v0.x, v1.x, v2.x }), std::max({ v0.x, v1.x, v2.x }), std::min({ v0.z, v1.z, v2.z }), std::max({ v0.z, v1.z, v2.z }), area };
		}

		// タイルに分けて焼く（Unity・Recast と同じ。広い場所でもメモリが少なくて済み、タイルごとに並べて動かせる）
		//  タイルは担当範囲（kTileSize マス四方）の周りに、縁を削る幅と角の高さを求める分だけ余分に塗る
		constexpr int kTileSize = 256;
		const int border = walkableRadius + 3;
		const int tilesX = (width + kTileSize - 1) / kTileSize;
		const int tilesZ = (depth + kTileSize - 1) / kTileSize;
		std::vector<std::vector<Quad>> tileQuads(size_t(tilesX) * size_t(tilesZ));
		std::vector<int> tileIndices(tileQuads.size());
		for (size_t i = 0; i < tileIndices.size(); ++i) { tileIndices[i] = int(i); }

		auto tileJob = [&](int tileIndex)
		{
			const int tx = tileIndex % tilesX, tz = tileIndex / tilesX;
			const int coreX = tx * kTileSize, coreZ = tz * kTileSize;
			const int coreW = std::min(kTileSize, width - coreX), coreD = std::min(kTileSize, depth - coreZ);
			// 作業用の配列はスレッドごとに使い回す（タイルごとに大きな配列を取り直すと、メモリの確保とページの割り当てでスレッドどうしが待ち合う）
			thread_local Heightfield hf;
			thread_local OpenField field;
			hf.spans.clear();
			hf.freeList = -1;
			hf.cs = cs;
			hf.ch = ch;
			hf.width = coreW + border * 2;
			hf.depth = coreD + border * 2;
			hf.bmin = Vector3(bmin.x + float(coreX - border) * cs, bmin.y, bmin.z + float(coreZ - border) * cs);
			hf.bmax = Vector3(hf.bmin.x + float(hf.width) * cs, bmax.y, hf.bmin.z + float(hf.depth) * cs);
			hf.heads.assign(size_t(hf.width) * size_t(hf.depth), -1);

			// 1. 塗る
			bool any = false;
			for (size_t t = 0; t < tris.size(); ++t)
			{
				const InputTri& it = tris[t];
				if (it.minX > hf.bmax.x || it.maxX < hf.bmin.x || it.minZ > hf.bmax.z || it.maxZ < hf.bmin.z) { continue; }
				RasterizeTriangle(hf, mPositions[mIndices[t * 3]], mPositions[mIndices[t * 3 + 1]], mPositions[mIndices[t * 3 + 2]], it.area, walkableClimb);
				any = true;
			}
			if (!any) { return; }

			// 2. 段の縁を歩けるようにし、頭がつかえる面を外す
			FilterLowHangingObstacles(hf, walkableClimb);
			FilterLowHeightSpans(hf, walkableHeight);

			// 3～4. 隣とつなぎ、縁を削り、離れ小島を捨てる
			BuildOpenField(hf, walkableHeight, walkableClimb, field);
			ErodeWalkableArea(field, walkableRadius);
			RemoveSmallRegions(field, minRegionCells, border, border, border + coreW, border + coreD);

			// 5. 四角形にまとめる（座標は全体のマスに直す）
			std::vector<Quad>& quads = tileQuads[size_t(tileIndex)];
			BuildRectangles(field, border, border, border + coreW, border + coreD, maxCells, quads);
			for (Quad& q : quads)
			{
				q.x0 += coreX - border;
				q.x1 += coreX - border;
				q.z0 += coreZ - border;
				q.z1 += coreZ - border;
			}
		};
		std::for_each(std::execution::par, tileIndices.begin(), tileIndices.end(), tileJob);

		// 頂点は格子点と高さが同じなら 1 つにまとめる
		std::unordered_map<uint64_t, uint32_t> vertexIndex;
		auto addVertex = [&](int lx, int lz, int y) -> uint32_t
		{
			uint64_t key = (uint64_t(uint32_t(lx)) << 42) | (uint64_t(uint32_t(lz)) << 21) | uint64_t(uint32_t(y) & 0x1fffff);
			auto [it, inserted] = vertexIndex.emplace(key, uint32_t(out.vertices.size()));
			if (inserted) { out.vertices.emplace_back(bmin.x + float(lx) * cs, bmin.y + float(y) * ch, bmin.z + float(lz) * cs); }
			return it->second;
		};
		for (const auto& quads : tileQuads)
		{
			for (const Quad& q : quads)
			{
				// 四隅の格子点の高さで 2 枚の三角形にする（上から見て時計回り。Unity の三角形と同じ向き）
				uint32_t p00 = addVertex(q.x0, q.z0, q.h00);
				uint32_t p10 = addVertex(q.x1, q.z0, q.h10);
				uint32_t p01 = addVertex(q.x0, q.z1, q.h01);
				uint32_t p11 = addVertex(q.x1, q.z1, q.h11);
				out.indices.insert(out.indices.end(), { p00, p01, p11, p00, p11, p10 });
				out.areas.push_back(q.area);
				out.areas.push_back(q.area);
			}
		}
		return !out.indices.empty();
	}
	bool NavMeshBuildResult::SaveJson(const std::string& path) const
	{
		Json vertexArray = Json::array();
		for (const Vector3& v : vertices)
		{
			// 1/10000 m で丸める（ファイルを小さくするため）
			vertexArray.push_back(std::round(v.x * 10000.0f) / 10000.0f);
			vertexArray.push_back(std::round(v.y * 10000.0f) / 10000.0f);
			vertexArray.push_back(std::round(v.z * 10000.0f) / 10000.0f);
		}
		Json json;
		json["Vertices"] = std::move(vertexArray);
		json["Indices"] = indices;
		json["Areas"] = areas;
		std::filesystem::path dir = std::filesystem::path(path).parent_path();
		if (!dir.empty())
		{
			std::error_code ec;
			std::filesystem::create_directories(dir, ec);
		}
		return SaveJsonFile(path, json);
	}
}
