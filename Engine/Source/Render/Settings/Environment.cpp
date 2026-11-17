#include "RdPch.h"
#include "Render/Settings/Environment.h"

namespace rd
{
	void Environment::Reflect(PropertyVisitor& v)
	{
		v.Property("Ambient Sky", ambientSky);
		v.Property("Ambient Ground", ambientGround);
		v.Property("Ambient Trilight", ambientTrilight);
		v.Property("Ambient Equator", ambientEquator);
		v.Property("Ambient Intensity", ambientIntensity, { .speed = 0.01f, .min = 0.0f, .max = 10.0f });
		v.Property("Draw Sky", drawSky);
		v.Property("Sky Zenith", skyZenith, { .flags = PropertyMeta::HDR });
		v.Property("Sky Horizon", skyHorizon, { .flags = PropertyMeta::HDR });
		v.Property("Fog Color", fogColor);
		v.Property("Fog Density", fogDensity, { .speed = 0.001f, .min = 0.0f, .max = 1.0f });
		v.Property("Fog Start", fogStart, { .speed = 0.1f, .min = 0.0f, .max = 10000.0f });
		v.Property("Additional Lights Per Vertex", additionalLightsPerVertex);
		v.Property("Lights Per Object", lightsPerObject, { .speed = 0.1f, .min = 0.0f, .max = 8.0f });
		v.Property("Tonemapper", tonemapper);
		v.Property("Post Exposure", postExposure, { .speed = 0.01f, .min = -10.0f, .max = 10.0f });
		v.Property("Contrast", contrast, { .speed = 0.5f, .min = -100.0f, .max = 100.0f });
		v.Property("Saturation", saturation, { .speed = 0.5f, .min = -100.0f, .max = 100.0f });
		v.Property("Retro Height", retroHeight, { .speed = 1.0f, .min = 0.0f, .max = 4320.0f });
		v.Property("Retro Color Levels", retroColorLevels, { .speed = 0.2f, .min = 2.0f, .max = 256.0f });
	}
}
