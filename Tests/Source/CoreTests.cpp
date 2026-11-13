#include "TestFramework.h"
#include "Core/JobSystem.h"
#include "Core/JsonArchive.h"
#include "Core/StringUtil.h"
#include <atomic>
#include <vector>

using namespace rd;

RD_TEST(Jobs, ParallelForCoversEveryIndexOnce)
{
	JobSystem::Initialize(4);
	std::vector<std::atomic<int>> hits(10000);
	JobHandle h = JobSystem::ParallelFor(uint32_t(hits.size()), 64, [&](uint32_t begin, uint32_t end)
		{
			for (uint32_t i = begin; i < end; ++i) { hits[i].fetch_add(1); }
		});
	JobSystem::Wait(h);
	RD_CHECK(h.IsDone());
	bool allOnce = true;
	for (auto& v : hits) { allOnce &= v.load() == 1; }
	RD_CHECK(allOnce);

	// ジョブの中からジョブを投げて待ってもデッドロックしない
	std::atomic<int> inner = 0;
	JobHandle outer = JobSystem::Schedule([&]
		{
			JobHandle child = JobSystem::ParallelFor(100, 1, [&](uint32_t, uint32_t) { inner.fetch_add(1); });
			JobSystem::Wait(child);
		});
	JobSystem::Wait(outer);
	RD_CHECK_EQ(inner.load(), 100);

	// ワーカーからメインスレッドへ戻す
	bool ranOnMain = false;
	JobSystem::Wait(JobSystem::Schedule([&] { JobSystem::RunOnMainThread([&] { ranOnMain = JobSystem::IsMainThread(); }); }));
	JobSystem::ExecuteMainThreadTasks();
	RD_CHECK(ranOnMain);
	JobSystem::Shutdown();
}

RD_TEST(Core, Utf8WideRoundTrip)
{
	std::string s = "アセット/テクスチャ.png";
	RD_CHECK_EQ(ToUtf8(ToWide(s)), s);
}

RD_TEST(Core, JsonMathTypes)
{
	Json j;
	j["v"] = Vector3(1, 2, 3);
	j["q"] = Quaternion(1, 0, 0, 0);
	j["c"] = Color(0.1f, 0.2f, 0.3f, 0.4f);
	RD_CHECK(j["v"].get<Vector3>() == Vector3(1, 2, 3));
	RD_CHECK(j["q"].get<Quaternion>() == Quaternion::kIdentity);
	RD_CHECK(j["c"].get<Color>() == Color(0.1f, 0.2f, 0.3f, 0.4f));
}
