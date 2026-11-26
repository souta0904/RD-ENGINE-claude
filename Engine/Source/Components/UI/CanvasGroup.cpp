#include "RdPch.h"
#include "Components/UI/CanvasGroup.h"

namespace rd
{
	void CanvasGroup::Reflect(PropertyVisitor& v)
	{
		v.Property("Alpha", alpha, { .speed = 0.01f, .min = 0.0f, .max = 1.0f });
	}
}
