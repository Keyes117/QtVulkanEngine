#pragma once
#include "Model.h"

#include <memory>
#include "const.h"

struct Transform2dComponent
{
    QVector2D translation{}; // position offset
    QVector2D scale{ 1.f,1.f };
    float rotation;
    Mat2F mat2f() {

        const float s = sin(rotation);
        const float c = cos(rotation);
        const float rotationMartixData[4] = { c, s, -s, c };
        Mat2F rotationMartix(rotationMartixData);

        const float data[4] = { scale.x(), .0f, .0f, scale.y() };
        Mat2F scaleMat(data);
        return rotationMartix * scaleMat;
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
    Transform2dComponent m_transform2d;
private:
    Object(id_t objID) :m_id(objID) {}


    id_t m_id;


};

