#pragma once

namespace rd
{
	class Scene;

	// シーンごとに 1 つずつあるシステム（物理ワールドなど）
	//  Scene::GetSystem<T>() で初めて使ったときに作られ、シーンと一緒に消える。
	//  Scene はシステムの具体的な型を知らないので、上のモジュール（Physics など）が自由に足せる。
	//  各関数はプレイ中、すべてのコンポーネントの同じ段階の更新が終わった後に呼ばれる。
	class SceneSystem
	{
	public:
		virtual ~SceneSystem() = default;
		virtual void Update(Scene& /*scene*/, float /*dt*/) {}
		virtual void FixedUpdate(Scene& /*scene*/, float /*dt*/) {}
		virtual void LateUpdate(Scene& /*scene*/, float /*dt*/) {}
	};
}
