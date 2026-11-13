#pragma once
#include <cmath>
#include <format>
#include <functional>
#include <string>
#include <vector>

// 小さな自前のテストフレームワーク
//  RD_TEST(Suite, Name) { RD_CHECK(a == b); }
//  外部ライブラリを増やさないため、必要な分だけ用意している。
//  RD_CHECK は失敗しても続ける。RD_REQUIRE は失敗したらそのテストをやめる。
namespace rdtest
{
	struct TestCase
	{
		const char* suite;
		const char* name;
		void (*func)();
	};

	inline std::vector<TestCase>& Registry()
	{
		static std::vector<TestCase> tests;
		return tests;
	}

	struct Registrar
	{
		Registrar(const char* suite, const char* name, void (*func)()) { Registry().push_back({ suite, name, func }); }
	};

	// 実行中のテストの失敗を記録する
	struct Context
	{
		int checks = 0;
		std::vector<std::string> failures;
	};
	inline Context*& Current()
	{
		static Context* ctx = nullptr;
		return ctx;
	}

	inline bool Report(bool ok, const char* expr, const char* file, int line, const std::string& detail = {})
	{
		Context* ctx = Current();
		++ctx->checks;
		if (!ok)
		{
			std::string msg = std::format("{}({}): {}", file, line, expr);
			if (!detail.empty()) { msg += "  [" + detail + "]"; }
			ctx->failures.push_back(std::move(msg));
		}
		return ok;
	}

	// RD_REQUIRE が失敗したときに投げる（テスト関数を抜けるため）
	struct RequireFailed {};

	inline bool Near(float a, float b, float eps) { return std::fabs(a - b) <= eps; }

	// 引数を 1 回だけ評価する（副作用のある式を渡してもよいように）
	template <class A, class B>
	bool CheckEq(const A& a, const B& b, const char* expr, const char* file, int line)
	{
		bool ok = a == b;
		return Report(ok, expr, file, line, ok ? std::string() : std::format("{} vs {}", a, b));
	}
	inline bool CheckNear(float a, float b, float eps, const char* expr, const char* file, int line)
	{
		bool ok = Near(a, b, eps);
		return Report(ok, expr, file, line, ok ? std::string() : std::format("{} vs {}", a, b));
	}
}

#define RD_TEST_CONCAT_(a, b) a##b
#define RD_TEST_CONCAT(a, b) RD_TEST_CONCAT_(a, b)

#define RD_TEST(Suite, Name)                                                                   \
	static void RD_TEST_CONCAT(Test_##Suite##_, Name)();                                       \
	static const ::rdtest::Registrar RD_TEST_CONCAT(sReg_##Suite##_, Name)(#Suite, #Name, &RD_TEST_CONCAT(Test_##Suite##_, Name)); \
	static void RD_TEST_CONCAT(Test_##Suite##_, Name)()

#define RD_CHECK(expr) ::rdtest::Report(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
#define RD_CHECK_EQ(a, b) ::rdtest::CheckEq((a), (b), #a " == " #b, __FILE__, __LINE__)
#define RD_CHECK_NEAR(a, b, eps) ::rdtest::CheckNear((a), (b), (eps), #a " ~= " #b, __FILE__, __LINE__)
#define RD_REQUIRE(expr) do { if (!RD_CHECK(expr)) { throw ::rdtest::RequireFailed{}; } } while (0)
