#include "RdPch.h"
#include "Render/Settings/RenderSettings.h"
#include "Render/GpuData.h"

namespace rd
{
	void RenderSettings::Reflect(PropertyVisitor& v)
	{
		if (v.BeginGroup("Shadow"))
		{
			v.Property("Enabled", shadows);
			v.Property("Map Size", shadowMapSize, { .speed = 64.0f, .min = 256.0f, .max = 4096.0f, .tooltip = "Resolution of one cascade" });
			v.Property("Cascades", shadowCascades, { .speed = 0.05f, .min = 1.0f, .max = float(kMaxShadowCascades) });
			v.Property("Distance", shadowDistance, { .speed = 0.5f, .min = 1.0f, .max = 1000.0f });
			v.Property("Split Lambda", cascadeSplitLambda, { .speed = 0.01f, .min = 0.0f, .max = 1.0f,
				.tooltip = "0 = uniform splits, 1 = logarithmic (more detail near the camera)" });
			v.Property("Bias", shadowBias, { .speed = 0.00005f, .min = 0.0f, .max = 0.05f });
			v.Property("Strength", shadowStrength, { .speed = 0.01f, .min = 0.0f, .max = 1.0f });
			v.Property("Show Cascades", debugCascades);
			v.EndGroup();
		}
		if (v.BeginGroup("Post Process"))
		{
			v.Property("Bloom", bloom);
			v.Property("Bloom Threshold", bloomThreshold, { .speed = 0.01f, .min = 0.0f, .max = 20.0f });
			v.Property("Bloom Knee", bloomKnee, { .speed = 0.01f, .min = 0.0f, .max = 1.0f });
			v.Property("Bloom Intensity", bloomIntensity, { .speed = 0.01f, .min = 0.0f, .max = 10.0f });
			v.Property("Exposure", exposure, { .speed = 0.01f, .min = 0.01f, .max = 20.0f });
			v.Property("Tonemap (ACES)", tonemap);
			v.Property("Vignette", vignette, { .speed = 0.01f, .min = 0.0f, .max = 2.0f });
			v.EndGroup();
		}
		if (v.BeginGroup("Debug"))
		{
			v.Property("Frustum Culling", frustumCulling);
			v.Property("Wireframe", wireframe);
			v.EndGroup();
		}
	}
}
