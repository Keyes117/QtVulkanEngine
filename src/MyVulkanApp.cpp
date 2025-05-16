#include "MyVulkanApp.h"

#include <array>
#include <QApplication>
#include <qtimer.h>

#include "Buffer.h"
#include "Movement_Controller.h"

constexpr float MAX_FRAME_TIME = 0.001;

float degressToRadians(float degress)
{
    return degress * static_cast<float>(M_PI) / 180.0f;
}


struct GlobalUbo
{
    float projectionViewMartix[16];
    QVector3D color = { 1,0,0 };
};

MyVulkanApp::MyVulkanApp() :
    m_window(m_widget.getVulkanWindowHandle()),
    m_cameraObject(Object::createObject()),
    m_keyBoardController(m_cameraObject, m_camera),
    m_mouseController(m_cameraObject, m_camera),
    m_device(m_window),
    m_renderer(m_window, m_device)
{

    m_globalPool = DescriptorPool::Builder(m_device)
        .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT)
        .build();



    auto minOffsetAlignment = std::lcm(
        m_device.properties.limits.minUniformBufferOffsetAlignment,
        m_device.properties.limits.nonCoherentAtomSize
    );

    m_spGlobalUboBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

    for (int i = 0; i < m_spGlobalUboBuffers.size(); i++)
    {
        m_spGlobalUboBuffers[i] = std::make_unique<Buffer>(m_device,
            sizeof(GlobalUbo),
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            m_device.properties.limits.minUniformBufferOffsetAlignment);
        m_spGlobalUboBuffers[i]->map();
    }

    m_globalSetLayout = DescriptorSetLayout::Builder(m_device)
        .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
        .build();


    m_globalDescriptSets.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < m_globalDescriptSets.size(); i++)
    {
        auto bufferInfo = m_spGlobalUboBuffers[i]->descriptorInfo();
        DescriptorWriter(*m_globalSetLayout, *m_globalPool)
            .writeBuffer(0, &bufferInfo)
            .build(m_globalDescriptSets[i]);
    }

    m_renderSystem = std::make_unique<RenderSystem>(m_device,
        m_renderer.getSwapChainRenderPass(),
        m_globalSetLayout->getDescriptorSetLayout());

}

MyVulkanApp::~MyVulkanApp()
{

}

void MyVulkanApp::run()
{
    m_widget.resize(800, 600);
    m_widget.show();

    loadObjects();
    m_camera.setViewDirection(QVector3D(0.f, 0.f, 0.f), QVector3D(0.f, 0.f, 1.f));
    m_lastFrameTime = std::chrono::high_resolution_clock::now();
    QObject::connect(&m_window, &MyVulkanWindow::updateRequested, [this]()
        {
            if (!m_window.isExposed())
                return;

            float aspect = m_renderer.getAspectRatio();
            m_camera.setPrespectiveProjection(degressToRadians(50.f), aspect, 0.1f, 200.f);

            //TODO:检查这里的帧时间的问题，好像MAX_FRAME_TIME 会失效？
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



            //render
            if (auto commandBuffer = m_renderer.beginFrame())
            {
                //update
                int frameIndex = m_renderer.getFrameIndex();
                FrameInfo frameInfo{
                    frameIndex,
                    frameTime,
                    commandBuffer,
                    m_camera,
                    m_globalDescriptSets[frameIndex]
                };

                auto metaData = (m_camera.getProjection() * m_camera.getView()).constData();

                GlobalUbo ubo{};
                std::memcpy(&ubo.projectionViewMartix, metaData, 16 * sizeof(float));
                m_spGlobalUboBuffers[frameIndex]->writeToIndex(&ubo, frameIndex);
                m_spGlobalUboBuffers[frameIndex]->flushIndex(frameIndex);

                //render
                m_renderer.beginSwapChainRenderPass(commandBuffer);
                this->getRenderSystem()->renderObjects(frameInfo, m_objects);
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

//TODO: 设置可以手绘物体
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

