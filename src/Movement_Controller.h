#pragma once

#include "Object.h"
#include "Camera.h"
#include "MyVulkanWindow.h"

class Keyboard_Movement_Controller
{
public:
    Keyboard_Movement_Controller(Object& object, Camera& camera);

    /* static moveInPlaneXY(QWindow* window,)*/
    void moveInPlane(int QtKey, float dt);

private:
    float moveSpeed{ 0.05f };
    Object& m_object;
    Camera& m_camera;
};

class Mouse_Movement_Controller
{
public:
    Mouse_Movement_Controller(Object& object, Camera& camera);

    void moveInPlane(const QVector3D& delteVector, float dt);
    void zoom(QVector3D ndcAndSteps, float dt);

private:

    float moveSpeed{ 0.05f };
    Object& m_object;
    Camera& m_camera;
};