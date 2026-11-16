#include "RdPch.h"
#include "Scene/Serialization/RefFixupVisitor.h"
#include "Scene/Reference/ObjectRef.h"

namespace rd
{
	// 参照のプロパティはすべて ObjectRef の派生
	bool RefFixupVisitor::Property(const char*, ObjectRefBase& base, const PropertyMeta&)
	{
		auto& value = static_cast<ObjectRef&>(base);
		value.SetScene(mScene);
		if (value.GetId() == 0 || !mRemap) { return false; }
		if (auto it = mIdMap.find(value.GetId()); it != mIdMap.end())
		{
			value.SetId(it->second->GetId());
		}
		else if (!mKeepExternal)
		{
			value.Clear();
		}
		return false;
	}
}
