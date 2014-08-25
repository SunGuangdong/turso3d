// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "Resource.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

bool Resource::Load(Deserializer&)
{
    return false;
}

bool Resource::Save(Serializer&) const
{
    LOGERROR("Save not supported for " + TypeName());
    return false;
}

void Resource::SetName(const String& newName)
{
    name = newName;
    nameHash = StringHash(newName);
}

}