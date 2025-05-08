#include "MyVulkanApp.h"

#include <array>
#include <QApplication>
#include <qtimer.h>



MyVulkanApp::MyVulkanApp() :
    m_window(m_widget.getVulkanWindowHandle()),
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

    QObject::connect(&m_window, &MyVulkanWindow::updateRequested, [this]() {

        if (!m_window.isExposed())
            return;

        if (auto commandBuffer = m_renderer.beginFrame())
        {
            m_renderer.beginSwapChainRenderPass(commandBuffer);
            this->getRenderSystem()->renderObjects(commandBuffer, m_objects);
            m_renderer.endSwapChainRenderPass(commandBuffer);
            m_renderer.endFrame();
        }
        m_window.requestUpdate();
        });
    return;
}

//TODO：将渲染对象的收集 变为窗口控制
void MyVulkanApp::loadObjects()
{
    std::vector<Model::Vertex> vertices
    {
        {{0.5f, -0.5f},{1.0f,0.0f,0.0f}},
        {{0.5f, 0.5f},{0.0f,1.0f,0.0f}},
        {{-0.5f,0.5f},{0.0f,0.0f,1.0f}}
    };

    auto model = std::make_shared<Model>(m_device, vertices);

    for (int i = 0; i < 10; i++)
    {
        auto triangle = Object::createObject();
        triangle.m_model = model;
        triangle.m_color = { .1f, .8f , .1f };
        triangle.m_transform2d.translation.setX(.2f);
        triangle.m_transform2d.scale = { .2f ,.5f };
        triangle.m_transform2d.rotation = .25f * M_PI;

        m_objects.push_back(std::move(triangle));
    }

}

