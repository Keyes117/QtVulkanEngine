#include "Keyboard_Movement_Controller.h"

//#include <qevent.h>
#include <algorithm>

float Keyboard_Movement_Controller::lookSpeed = 1.5f;
float Keyboard_Movement_Controller::moveSpeed = 0.3f;

//TODO:优化该问题
void Keyboard_Movement_Controller::moveInPlane(int QtKey, float dt, Object& object)
{
    float d = moveSpeed * dt;
    QVector3D delta{ 0,0,0 };

    switch (QtKey) {
    case Qt::Key_W:
    case Qt::Key_Up:
    delta.setY(delta.y() - d);
    break;
    case Qt::Key_S:
    case Qt::Key_Down:
    delta.setY(delta.y() + d);
    break;
    case Qt::Key_A:
    case Qt::Key_Left:
    delta.setX(delta.x() - d);
    break;
    case Qt::Key_D:
    case Qt::Key_Right:
    delta.setX(delta.x() + d);
    break;
    default:
    return; // 其它键不处理
    }

    auto t = object.m_transform.translation;
    t.setX(t.x() + delta.x());
    t.setY(t.y() + delta.y());
    object.m_transform.translation = t;
}
