#pragma once
#include "Model.h"

#include <memory>
#include <cstdint>
#include <functional>

#include <qmatrix4x4.h>

#include "const.h"



struct TransformComponent
{
    QVector3D translation{}; // position offset
    QVector3D scale{ 1.f,1.f,1.f };
    QVector3D rotation{ 0.f,0.f,0.f };

    QMatrix4x4 mat4f()  const {

        QMatrix4x4 transform;
        transform.setToIdentity();
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

class Object : public std::enable_shared_from_this<Object>
{
public:
    using ObjectID = uint64_t;
    using UpdateCallback = std::function<void(const std::shared_ptr<Object>&)>;

    struct Builder
    {
        std::vector<Model::Vertex> vertices{};
        std::vector<uint32_t> indices{};
        ModelType           type{ ModelType::None };
        AABB bounds{ 0.0,0.0,0.0,0.0 };
        TransformComponent transform{};
        QVector3D color{ 0.f, 0.f,0.f };
    };

    enum class UpdateType
    {
        None = 0,
        Translation = 1 << 0,
        Scale = 1 << 1,
        Rotation = 1 << 2,
        Color = 1 << 3,
        Model = 1 << 4,
        Visibility = 1 << 5,
        All = Translation | Scale | Rotation | Color | Model | Visibility
    };

    static std::shared_ptr<Object> createObject()
    {
        static ObjectID currentId = 0;
        struct make_shared_enabler : public Object {
            make_shared_enabler(ObjectID id) : Object(id) {}
        };
        return std::make_shared<make_shared_enabler>(currentId++);
    }

    ObjectID getId() { return m_id; }

    Object(const Object&) = default;
    Object& operator=(const Object&) = default;
    Object(Object&&) = default;
    Object& operator=(Object&&) = default;

    AABB getBoundingBox() { return m_model->getBoundingBox(); }
    bool needsUpdate() const { return m_updateFlags != 0; }

    void clearUpdateFlags() { m_updateFlags = 0; }

    std::shared_ptr<Model> getModel() const { return m_model; }

    uint32_t  getChunkID() const { return m_chunkId; }
    QVector3D getColor() const { return m_color; }
    QVector3D getTranslation() const { return m_transform.translation; }
    QVector3D getScale() const { return m_transform.scale; }
    QVector3D getRotation() const { return m_transform.rotation; }
    TransformComponent getTransform() const { return m_transform; }

    void setChunkId(uint32_t chunkId) { m_chunkId = chunkId; }

    void setPosition(const QVector3D& position) { setTranslation(position); }

    void setTranslation(const QVector3D& translation)
    {
        if (m_transform.translation != translation)
        {
            m_transform.translation = translation;
            markUpdate(UpdateType::Translation);
        }
    }

    void setScale(const QVector3D& scale)
    {
        if (m_transform.scale != scale)
        {
            m_transform.scale = scale;
            markUpdate(UpdateType::Scale);
        }
    }

    void setRotation(const QVector3D& rotation)
    {
        if (m_transform.rotation != rotation)
        {
            m_transform.rotation = rotation;
            markUpdate(UpdateType::Rotation);
        }
    }

    void setTransform(const TransformComponent transform) {
        m_transform = transform;
    }

    void setColor(const QVector3D& color)
    {
        if (m_color != color)
        {
            m_color = color;
            markUpdate(UpdateType::Color);
        }
    }

    void setModel(std::shared_ptr<Model>& model)
    {
        m_model = model;
    }

    void setUpdateCallback(UpdateCallback&& callback)
    {
        m_updateCallback = std::move(callback);
    }

private:

    Object(ObjectID objID) :m_id(objID) {}
    QVector3D   m_color{};
    TransformComponent m_transform{};
    uint32_t    m_chunkId{ 0 };
    uint32_t    m_updateFlags{ 0 };
    ObjectID    m_id{ 0 };
    std::shared_ptr<Model>  m_model{ nullptr };


    UpdateCallback m_updateCallback{ nullptr };
    void markUpdate(UpdateType type)
    {
        m_updateFlags |= static_cast<uint32_t>(type);
        if (m_updateCallback)
            m_updateCallback(shared_from_this());
    }

};

inline Object::UpdateType operator|(Object::UpdateType a, Object::UpdateType b)
{
    return static_cast<Object::UpdateType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline Object::UpdateType operator&(Object::UpdateType a, Object::UpdateType b)
{
    return static_cast<Object::UpdateType>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}