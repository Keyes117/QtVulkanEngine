#include "Object.h"

Object::Object(ObjectID objID)
    :m_id(objID)
{
    markUpdate(static_cast<UpdateType>(
        static_cast<uint32_t>(UpdateType::Translation) |
        static_cast<uint32_t>(UpdateType::Scale) |
        static_cast<uint32_t>(UpdateType::Rotation)
        ));
}