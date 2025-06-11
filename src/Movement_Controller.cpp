#include "Movement_Controller.h"

#include <algorithm>

Keyboard_Movement_Controller::Keyboard_Movement_Controller(const std::shared_ptr<Object>& object, Camera& camera) :
    m_object(object),
    m_camera(camera)
{
}

//TODO:优化该问题
void Keyboard_Movement_Controller::moveInPlane(int QtKey, float dt)
{
    float d = moveSpeed * dt;
    QVector3D delta{ 0,0,0 };

    switch (QtKey) {
    case Qt::Key_Up:
    delta.setY(delta.y() - d);
    break;
    case Qt::Key_Down:
    delta.setY(delta.y() + d);
    break;
    case Qt::Key_Left:
    delta.setX(delta.x() - d);
    break;
    case Qt::Key_Right:
    delta.setX(delta.x() + d);
    break;
    default:
    return;
    }

    auto t = m_object->getTranslation();
    t.setX(t.x() + delta.x());
    t.setY(t.y() + delta.y());
    m_object->setTranslation(t);

    m_camera.setViewLocation(m_object->getTranslation(), m_object->getRotation());
}

Mouse_Movement_Controller::Mouse_Movement_Controller(const std::shared_ptr<Object>& object, Camera& camera) :
    m_object(object),
    m_camera(camera)
{
}

void Mouse_Movement_Controller::moveInPlane(const QVector3D& delteVector, float dt)
{

    m_object->setTranslation(m_object->getTranslation() += delteVector * dt * Mouse_Movement_Controller::moveSpeed);
    m_camera.setViewLocation(m_object->getTranslation(), m_object->getRotation());
}

void Mouse_Movement_Controller::zoom(QVector3D ndcAndSteps, float dt)
{
    m_object->setTranslation(m_object->getTranslation() += ndcAndSteps * dt * moveSpeed);
    m_camera.setViewLocation(m_object->getTranslation(), m_object->getRotation());
}
