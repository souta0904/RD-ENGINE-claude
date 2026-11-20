#include "RdPch.h"
#include "Scene/Component/MissingComponent.h"

namespace rd
{
	MissingComponent::MissingComponent(std::string typeName, Json data)
		: mTypeName(std::move(typeName)), mData(std::move(data))
	{
	}
}
