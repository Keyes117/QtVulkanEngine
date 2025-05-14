#include "MyVulkanApp.h"

#include <array>
#include <QApplication>
#include <qtimer.h>

#include "Movement_Controller.h"

constexpr float MAX_FRAME_TIME = 0.001;

float degressToRadians(float degress)
{
    return degress * static_cast<float>(M_PI) / 180.0f;
}

MyVulkanApp::MyVulkanApp() :
    m_window(m_widget.getVulkanWindowHandle()),
    m_cameraObject(Object::createObject()),
    m_keyBoardController(m_cameraObject, m_camera),
    m_mouseController(m_cameraObject, m_camera),
    m_device(m_window),
    m_renderer(m_window, m_device),
    m_renderSystem(m_device, m_renderer.getSwapChainRenderPass())
{

}

MyVulkanApp::~MyVulkanApp()
{
}

void MyVulkanApp::run()
{

    m_widget.resize(800, 600);
    m_widget.show();

    loadObjects();

    //TODO: ����Camera����ͨ�������߷�������е���

    //m_camera.setViewDirection(QVector3D(.0f, 0.0f, 0.f), QVector3D(.5f, 0.5f, 1.f));
    m_camera.setViewDirection(QVector3D(0.f, 0.f, 0.f), QVector3D(0.f, 0.f, 1.f));

    m_lastFrameTime = std::chrono::high_resolution_clock::now();



    QObject::connect(&m_window, &MyVulkanWindow::updateRequested, [this]()
        {
            if (!m_window.isExposed())
                return;

            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - m_lastFrameTime).count();
            m_lastFrameTime = newTime;

            frameTime = (frameTime < MAX_FRAME_TIME) ? frameTime : MAX_FRAME_TIME;


            QObject::connect(&m_window, &MyVulkanWindow::keyPressed, [=](int key, Qt::KeyboardModifiers mods) {
                m_keyBoardController.moveInPlane(key, frameTime);
                });

            QObject::connect(&m_window, &MyVulkanWindow::CameraMovement, [=](QVector3D delte) {
                m_mouseController.moveInPlane(delte, frameTime);
                });

            QObject::connect(&m_window, &MyVulkanWindow::CameraZoom, [=](QVector3D ndcAndSteps) {
                m_mouseController.zoom(ndcAndSteps, frameTime);
                });


            //
            float aspect = m_renderer.getAspectRatio();
            //m_camera.setOrthographciProjection(-aspect, aspect, -1, 1, -1.f, 10.f);
            m_camera.setPrespectiveProjection(degressToRadians(50.f), aspect, 0.1f, 200.f);

            if (auto commandBuffer = m_renderer.beginFrame())
            {
                m_renderer.beginSwapChainRenderPass(commandBuffer);
                this->getRenderSystem()->renderObjects(commandBuffer, m_objects, m_camera);
                m_renderer.endSwapChainRenderPass(commandBuffer);
                m_renderer.endFrame();
            }
            m_window.requestUpdate();
        });
    return;
}

std::shared_ptr<Model> createCubeMode(Device& device, QVector3D offset)
{
    Model::Builder modelBuilder;

    modelBuilder.vertices = {
        // left face (white)
             { { -.5f, -.5f, -.5f }, { .9f, .9f, .9f } },
             { {-.5f, .5f, .5f}, {.9f, .9f, .9f} },
             { {-.5f, -.5f, .5f}, {.9f, .9f, .9f} },
             { {-.5f, -.5f, -.5f}, {.9f, .9f, .9f} },
             { {-.5f, .5f, -.5f}, {.9f, .9f, .9f} },
             { {-.5f, .5f, .5f}, {.9f, .9f, .9f} },

             // right face (yellow)
         { {.5f, -.5f, -.5f}, {.8f, .8f, .1f} },
         { {.5f, .5f, .5f}, {.8f, .8f, .1f} },
         { {.5f, -.5f, .5f}, {.8f, .8f, .1f} },
         { {.5f, -.5f, -.5f}, {.8f, .8f, .1f} },
         { {.5f, .5f, -.5f}, {.8f, .8f, .1f} },
         { {.5f, .5f, .5f}, {.8f, .8f, .1f} },

         // top face (orange, remember y axis points down)
     { {-.5f, -.5f, -.5f}, {.9f, .6f, .1f} },
     { {.5f, -.5f, .5f}, {.9f, .6f, .1f} },
     { {-.5f, -.5f, .5f}, {.9f, .6f, .1f} },
     { {-.5f, -.5f, -.5f}, {.9f, .6f, .1f} },
     { {.5f, -.5f, -.5f}, {.9f, .6f, .1f} },
     { {.5f, -.5f, .5f}, {.9f, .6f, .1f} },

     // bottom face (red)
 { {-.5f, .5f, -.5f}, {.8f, .1f, .1f} },
 { {.5f, .5f, .5f}, {.8f, .1f, .1f} },
 { {-.5f, .5f, .5f}, {.8f, .1f, .1f} },
 { {-.5f, .5f, -.5f}, {.8f, .1f, .1f} },
 { {.5f, .5f, -.5f}, {.8f, .1f, .1f} },
 { {.5f, .5f, .5f}, {.8f, .1f, .1f} },

 // nose face (blue)
{ {-.5f, -.5f, 0.5f}, {.1f, .1f, .8f} },
{ {.5f, .5f, 0.5f}, {.1f, .1f, .8f} },
{ {-.5f, .5f, 0.5f}, {.1f, .1f, .8f} },
{ {-.5f, -.5f, 0.5f}, {.1f, .1f, .8f} },
{ {.5f, -.5f, 0.5f}, {.1f, .1f, .8f} },
{ {.5f, .5f, 0.5f}, {.1f, .1f, .8f} },

// tail face (green)
{ {-.5f, -.5f, -0.5f}, {.1f, .8f, .1f} },
{ {.5f, .5f, -0.5f}, {.1f, .8f, .1f} },
{ {-.5f, .5f, -0.5f}, {.1f, .8f, .1f} },
{ {-.5f, -.5f, -0.5f}, {.1f, .8f, .1f} },
{ {.5f, -.5f, -0.5f}, {.1f, .8f, .1f} },
{ {.5f, .5f, -0.5f}, {.1f, .8f, .1f} },

    };
    for (auto& v : modelBuilder.vertices) {
        v.position += offset;
    }
    return std::make_shared<Model>(device, modelBuilder);
}

//TODO: ���ÿ����ֻ�����
void MyVulkanApp::loadObjects()
{
    Model::Builder modelBuidler;
    modelBuidler.vertices = std::vector<Model::Vertex>{
        {{0.0f, -0.5f,  0.f}, {1.0f, 0.0f, 0.0f}}, // Triangle 1, vertex 1
        {{0.5f, 0.5f,   0.f}, {0.0f, 1.0f, 0.0f}},  // Triangle 1, vertex 2
        {{-0.5f, 0.5f,  0.f}, {0.0f, 0.0f, 1.0f}}, // Triangle 1, vertex 3
        //{{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 0.0f}}, // Triangle 2, vertex 1 (shared with triangle 1)
    };


    //modelBuidler.indices = { 0,1,2 };
    auto model = std::make_shared<Model>(m_device, modelBuidler);

    auto triangle = Object::createObject();
    triangle.m_model = model;
    triangle.m_color = { .1f, .8f , .1f };
    ///
    //triangle.m_transform.scale = { .2f ,1.f ,.0f };
    //triangle.m_transform.rotation = { .0f,.0f,.25f * M_PI };
    triangle.m_transform.translation = { 0.f, .0f , 2.5f };

    m_objects.push_back(std::move(triangle));

    //auto model = createCubeMode(m_device, QVector3D{ 0.f,0.f,0.f });

    //auto cube = Object::createObject();
    //cube.m_model = model;
    //cube.m_transform.translation = { .0f,0.0f,2.5f };
    //cube.m_transform.scale = { .5f,.5f,.5f };

    //m_objects.push_back(std::move(cube));

}

