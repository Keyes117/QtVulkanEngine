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
        transform.setToIdentity();
        /*
              transform.translate(translation);
              transform.rotate(rotation.y(), { 0,1,0 });
              transform.rotate(rotation.z(), { 0,0,1 });
              transform.rotate(rotation.x(), { 1,0,0 });
              transform.scale(scale);*/

        const float c3 = std::cos(rotation.z());
        const float s3 = std::sin(rotation.z());
        const float c2 = std::cos(rotation.x());
        const float s2 = std::sin(rotation.x());
        const float c1 = std::cos(rotation.y());
        const float s1 = std::sin(rotation.y());

        transform(0, 0) = scale.x() * (c1 * c3 + s1 * s2 * s3);
        transform(1, 0) = scale.x() * (c2 * s3);
        transform(2, 0) = scale.x() * (c1 * s2 * s3 - c3 * s1);
        transform(3, 0) = 0.f;

        transform(0, 1) = scale.y() * (c3 * s1 * s2 - c1 * s3);
        transform(1, 1) = scale.y() * (c2 * c3);
        transform(2, 1) = scale.y() * (c1 * c3 * s2 + s1 * s3);
        transform(3, 1) = 0.f;

        transform(0, 2) = scale.z() * (c2 * s1);
        transform(1, 2) = scale.z() * (-s2);
        transform(2, 2) = scale.z() * (c1 * c3 * s2 + s1 * s3);
        transform(3, 2) = 0.f;

        transform(0, 3) = translation.x();
        transform(1, 3) = translation.y();
        transform(2, 3) = translation.z();
        transform(3, 3) = (1.0f);

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

    Object(const Object&) = default;
    Object& operator=(const Object&) = default;
    Object(Object&&) = default;
    Object& operator=(Object&&) = default;

    AABB getBoundingBox() { return m_model->getBoundingBox(); }


    std::shared_ptr<Model>  m_model;
    QVector3D   m_color{};
    TransformComponent m_transform;
private:
    Object(id_t objID) :m_id(objID) {}

    id_t m_id;
};

