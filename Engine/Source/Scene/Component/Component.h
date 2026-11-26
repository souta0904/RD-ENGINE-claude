#pragma once
#include "Core/Reflection/PropertyVisitor.h"
#include "Scene/Component/ComponentTypeInfo.h"
#include <type_traits>

namespace rd
{
	class Actor;
	class Collider;
	class Component;
	class DebugDraw;
	class RenderList;
	class Scene;
	class Transform;
	struct Collision;

	namespace detail
	{
		template <class T> constexpr uint32_t ComputeCallbacks();
	}
	template <class T> const ComponentTypeInfo& GetComponentTypeInfo();

	// コンポーネントの型名を宣言する（クラスの定義の先頭に書く）
#define RD_COMPONENT(Type)                                                                          \
public:                                                                                             \
	static constexpr const char* kTypeName = #Type;                                                 \
	const char* GetTypeName() const override { return kTypeName; }                                  \
	const ::rd::ComponentTypeInfo& GetTypeInfo() const override { return ::rd::GetComponentTypeInfo<Type>(); } \
	template <class> friend constexpr uint32_t (::rd::detail::ComputeCallbacks)();                  \
private:

	// コンポーネントの基底
	//  Awake      : シーンに追加され、プロパティが読み込まれた直後（編集中も呼ばれる）
	//  Start      : プレイ開始後、最初の Update の直前
	//  Update / FixedUpdate / LateUpdate : プレイ中だけ
	//  OnDestroy  : 破棄される直前（編集中も呼ばれる）
	//  OnRender   : 編集中も毎フレーム呼ばれ、描きたいものを RenderList に積む
	//  OnEditorUpdate : 編集中だけ（プレビュー用）、OnInspectorGUI : Reflect では表せない追加の UI
	//  OnValidate : インスペクタや Undo で値が変わった後
	class RD_API Component
	{
	public:
		virtual ~Component() = default;
		virtual const char* GetTypeName() const = 0;
		virtual const ComponentTypeInfo& GetTypeInfo() const = 0;

		virtual void Reflect(PropertyVisitor& /*v*/) {}
		virtual void OnValidate() {}

		virtual void Awake() {}
		virtual void Start() {}
		virtual void Update(float /*dt*/) {}
		virtual void FixedUpdate(float /*dt*/) {}
		virtual void LateUpdate(float /*dt*/) {}
		virtual void OnDestroy() {}
		virtual void OnEditorUpdate(float /*dt*/) {}

		virtual void OnRender(RenderList& /*list*/) {}
#if RD_EDITOR
		virtual void OnDrawGizmos(DebugDraw& /*draw*/, bool /*selected*/) {}
		// 保存する値を変えたら true を返す（Undo に積み、シーンを「変更あり」にする）
		virtual bool OnInspectorGUI() { return false; }
#endif

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

		// 同じアクターの別のコンポーネント
		template <class T> T* GetComponent() const;

		bool IsPendingDestroy() const { return mPendingDestroy; }

	private:
		friend class Actor;
		friend class Scene;
		Actor* mActor = nullptr;
		bool mEnabled = true;
		bool mStarted = false;
		bool mPendingDestroy = false;
	};

	namespace detail
	{
		// メンバ関数ポインタの型が Component のままなら、その関数はどこでも上書きされていない
		template <class T>
		constexpr uint32_t ComputeCallbacks()
		{
			uint32_t flags = 0;
			if (!std::is_same_v<decltype(&T::Update), decltype(&Component::Update)>) { flags |= kCallbackUpdate; }
			if (!std::is_same_v<decltype(&T::FixedUpdate), decltype(&Component::FixedUpdate)>) { flags |= kCallbackFixedUpdate; }
			if (!std::is_same_v<decltype(&T::LateUpdate), decltype(&Component::LateUpdate)>) { flags |= kCallbackLateUpdate; }
			if (!std::is_same_v<decltype(&T::OnRender), decltype(&Component::OnRender)>) { flags |= kCallbackRender; }
			if (!std::is_same_v<decltype(&T::OnEditorUpdate), decltype(&Component::OnEditorUpdate)>) { flags |= kCallbackEditorUpdate; }
#if RD_EDITOR
			if (!std::is_same_v<decltype(&T::OnDrawGizmos), decltype(&Component::OnDrawGizmos)>) { flags |= kCallbackGizmos; }
#endif
			return flags;
		}
	}

	template <class T>
	const ComponentTypeInfo& GetComponentTypeInfo()
	{
		static const ComponentTypeInfo info{ T::kTypeName, typeid(T), detail::ComputeCallbacks<T>() };
		return info;
	}
}
