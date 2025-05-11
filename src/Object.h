#pragma once
#include "Model.h"

#include "const.h"
#include <memory>
#include <qmatrix4x4.h>
struct TransformComponent
{
    QVector3D translation{}; // position offset
    QVector3D scale{ 1.f,1.f,1.f };
    QVector3D rotation;

    QMatrix4x4 mat4f() {
        QMatrix4x4 transform;
        transform.translate(translation);

        transform.rotate(rotation.x(), { 1.f,0.f,0.f });
        transform.rotate(rotation.y(), { 0.f,1.f,0.f });
        transform.rotate(rotation.z(), { 0.f,0.f,1.f });

        transform.scale(scale);
        return transform;
    }
};

class Object
{
public:
    using id_t = unsigned int;

    static Object createObject()
    {
        static id_t currentId = 0;
        return Object(currentId++);
    }

    id_t getId() { return m_id; }

    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;
    Object(Object&&) = default;
    Object& operator=(Object&&) = default;

    std::shared_ptr<Model>  m_model;
    QVector3D   m_color{};
    TransformComponent m_transform;
private:
    Object(id_t objID) :m_id(objID) {}


    id_t m_id;


};

