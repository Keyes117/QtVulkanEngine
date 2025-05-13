#pragma once

#include "Object.h"
#include "MyVulkanWindow.h"

class Keyboard_Movement_Controller
{
public:
    /* static moveInPlaneXY(QWindow* window,)*/
    static void moveInPlane(int QtKey, float dt, Object& object);

private:
    static float moveSpeed;
    static float lookSpeed;

    Keyboard_Movement_Controller() = delete;

};

