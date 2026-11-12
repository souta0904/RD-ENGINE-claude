#pragma once
#include "Core/Reflection.h"

namespace rd
{
	class Actor;
	class Collider;
	class DebugDraw;
	class RenderList;
	class Scene;
	class Transform;
	struct Collision;

	// コンポーネントの型名を宣言するマクロ（クラス定義の先頭に書く）
#define RD_COMPONENT(Type)                                              \
public:                                                                 \
	static constexpr const char* kTypeName = #Type;                     \
	const char* GetTypeName() const override { return kTypeName; }      \
private:

	// コンポーネント基底
	//  ライフサイクル:
	//    Awake      : シーンに追加され、プロパティが読み込まれた直後（編集中も呼ばれる）
	//    Start      : プレイ開始後、最初の Update の直前
	//    Update / FixedUpdate / LateUpdate : プレイ中のみ
	//    OnDestroy  : 破棄される直前（編集中も呼ばれる）
	//  OnRender は編集中も毎フレーム呼ばれ、描画したいものを RenderList に積む
	class Component
	{
	public:
		virtual ~Component() = default;
		virtual const char* GetTypeName() const = 0;

		// プロパティ（保存・インスペクタ・Undo に使われる）
		virtual void Reflect(PropertyVisitor& /*v*/) {}
		// インスペクタやUndoで値が変わった後に呼ばれる
		virtual void OnValidate() {}

		virtual void Awake() {}
		virtual void Start() {}
		virtual void Update(float /*dt*/) {}
		virtual void FixedUpdate(float /*dt*/) {}
		virtual void LateUpdate(float /*dt*/) {}
		virtual void OnDestroy() {}
		virtual void OnEditorUpdate(float /*dt*/) {}// 編集中のみ（プレビュー用）

		virtual void OnRender(RenderList& /*list*/) {}
		virtual void OnDrawGizmos(DebugDraw& /*draw*/, bool /*selected*/) {}
		virtual void OnInspectorGUI() {}// Reflect では表現できない追加 UI

		// 衝突
		virtual void OnCollisionEnter(const Collision& /*c*/) {}
		virtual void OnCollisionStay(const Collision& /*c*/) {}
		virtual void OnCollisionExit(const Collision& /*c*/) {}
		virtual void OnTriggerEnter(Collider* /*other*/) {}
		virtual void OnTriggerStay(Collider* /*other*/) {}
		virtual void OnTriggerExit(Collider* /*other*/) {}

		Actor* GetActor() const { return mActor; }
		Scene* GetScene() const;
		Transform& GetTransform() const;
		bool IsEnabled() const { return mEnabled; }
		void SetEnabled(bool enabled) { mEnabled = enabled; }
		bool IsActiveAndEnabled() const;
		bool HasStarted() const { return mStarted; }

		// 同じアクターの別コンポーネント
		template <class T> T* GetComponent() const;

	private:
		friend class Actor;
		friend class Scene;
		Actor* mActor = nullptr;
		bool mEnabled = true;
		bool mStarted = false;
		bool mPendingDestroy = false;
	};
}
