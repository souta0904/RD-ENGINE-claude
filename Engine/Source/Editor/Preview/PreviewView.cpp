#include "RdPch.h"
#include "Editor/Preview/PreviewView.h"
#include "Asset/Texture/Texture.h"

namespace rd
{
	namespace
	{
		constexpr float kFovDegrees = 30.0f;

		// 空は描かず、暗い灰色の背景にする
		Environment PreviewEnvironment()
		{
			Environment env;
			env.drawSky = false;
			env.skyHorizon = Color(0.13f, 0.14f, 0.16f, 1.0f);
			env.ambientIntensity = 0.8f;
			env.fogDensity = 0.0f;
			return env;
		}
	}

	bool PreviewView::IsSettled(const AssetRef<Texture>& texture)
	{
		return !texture || texture->IsReady() || texture->IsFailed();
	}

	// 包む球が画面に収まる距離から見て、カメラの左上から当てる光と、後ろからの弱い光で照らす
	void PreviewView::DrawList(const AABB& bounds, uint32_t width, uint32_t height)
	{
		Vector3 center = bounds.GetCenter();
		float radius = std::max(Length(bounds.GetExtents()), 1.0e-3f);
		float fov = math::ToRadians(kFovDegrees);
		float aspect = float(width) / float(height);
		// 横長でも縦長でも、狭い方の画角に球が収まるようにする
		float halfFov = aspect < 1.0f ? std::atan(std::tan(fov * 0.5f) * aspect) : fov * 0.5f;
		float distance = radius / std::sin(halfFov) * zoom;
		Quaternion rotation = Quaternion::FromEuler({ math::ToRadians(pitch), math::ToRadians(yaw), 0.0f });
		Vector3 forward = Vector3::kUnitZ * rotation;
		Vector3 right = Vector3::kUnitX * rotation;
		Vector3 up = Vector3::kUnitY * rotation;
		float nearZ = std::max(distance - radius * 1.5f, distance * 0.01f);
		RenderCamera camera = RenderCamera::Make(center - forward * distance, rotation, fov, aspect, nearZ, distance + radius * 2.0f);

		GpuDirLight key;
		key.direction = Normalize(forward - up * 0.9f + right * 0.6f);
		key.color = Vector3(1.0f, 0.97f, 0.92f);
		key.intensity = 1.6f;
		GpuDirLight rim;
		rim.direction = Normalize(-forward - up * 0.3f - right * 0.4f);
		rim.color = Vector3(0.75f, 0.85f, 1.0f);
		rim.intensity = 0.6f;
		mList.dirLights.push_back(key);
		mList.dirLights.push_back(rim);

		Renderer::Get().RenderOffscreen(mTargets, mList, camera, PreviewEnvironment(), width, height);
	}
}
