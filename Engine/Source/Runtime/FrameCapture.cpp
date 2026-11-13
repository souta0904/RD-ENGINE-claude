#include "RdPch.h"
#include "FrameCapture.h"
#include "Render/Renderer.h"

namespace rd
{
	void FrameCapture::BeforeRender(Renderer& renderer, uint64_t frame) const
	{
		if (IsTarget(frame)) { renderer.RequestCapture(mPath); }
	}

	bool FrameCapture::AfterPresent(Renderer& renderer, uint64_t frame) const
	{
		renderer.FinishCapture();
		return IsTarget(frame);
	}
}
