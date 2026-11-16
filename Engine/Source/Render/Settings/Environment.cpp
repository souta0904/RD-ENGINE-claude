#include "RdPch.h"
#include "Render/Settings/Environment.h"

namespace rd
{
	void Environment::Reflect(PropertyVisitor& v)
	{
		v.Property("Ambient Sky", ambientSky);
		v.Property("Ambient Ground", ambientGround);
		v.Property("Ambient Intensity", ambientIntensity, { .speed = 0.01f, .min = 0.0f, .max = 10.0f });
		v.Property("Draw Sky", drawSky);
		v.Property("Sky Zenith", skyZenith, { .flags = PropertyMeta::HDR });
		v.Property("Sky Horizon", skyHorizon, { .flags = PropertyMeta::HDR });
		v.Property("Fog Color", fogColor);
		v.Property("Fog Density", fogDensity, { .speed = 0.001f, .min = 0.0f, .max = 1.0f });
		v.Property("Fog Start", fogStart, { .speed = 0.1f, .min = 0.0f, .max = 10000.0f });
	}
}
