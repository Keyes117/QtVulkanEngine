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
    float projectionViewMartix[16];     //QMartix4x4 封装了一个flag进去，导致大小是68字节，不能直接传，改为float[16]
    QVector3D color = { 1,0,0 };
};

MyVulkanApp::MyVulkanApp() :
    m_window(m_widget.getVulkanWindowHandle()),
    m_cameraObject(Object::createObject()),
    m_previewObject(Object::createObject()),
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

    VkQueryPoolCreateInfo queryPoolInfo{};
    queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryPoolInfo.queryCount = 2;
    vkCreateQueryPool(m_device.device(), &queryPoolInfo, nullptr, &m_queryPool);

    m_pointRenderSystem = std::make_shared<RenderSystem>(m_device,
        m_renderer.getSwapChainRenderPass(),
        m_globalSetLayout->getDescriptorSetLayout(),
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

    m_lineRenderSystem = std::make_shared<RenderSystem>(m_device,
        m_renderer.getSwapChainRenderPass(),
        m_globalSetLayout->getDescriptorSetLayout(),
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST);

    m_polygonRenderSystem = std::make_shared<RenderSystem>(m_device,
        m_renderer.getSwapChainRenderPass(),
        m_globalSetLayout->getDescriptorSetLayout(),
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    connect(&m_window, &MyVulkanWindow::drawAddVertex, this, &MyVulkanApp::onAddDrawVertex);
    connect(&m_window, &MyVulkanWindow::drawEnd, this, &MyVulkanApp::onDrawEnd);
    connect(&m_window, &MyVulkanWindow::drawMove, this, &MyVulkanApp::onDrawMove);
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
                vkCmdResetQueryPool(commandBuffer, m_queryPool, 0, 2);
                vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_queryPool, 0);

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

                m_pointRenderSystem->renderObjects(frameInfo, m_pointObjects);
                m_lineRenderSystem->renderObjects(frameInfo, m_lineObjects);
                m_polygonRenderSystem->renderObjects(frameInfo, m_polygonObjects);

                m_renderer.endSwapChainRenderPass(commandBuffer);
                m_renderer.endFrame();


                vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPool, 1);
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

void MyVulkanApp::onAddDrawVertex(QVector3D vertexPos)
{
    DrawMode mode = m_window.getCurrentDrawMode();
    switch (mode)
    {
    case DrawMode::Point:
    Model::Builder modelBuilder;
    Model::Vertex vertex;
    vertex.position = vertexPos;
    vertex.color = { 1,0,0 };
    modelBuilder.vertices.push_back(vertex);
    }
}

void MyVulkanApp::onDrawEnd()
{
}

void MyVulkanApp::onDrawMove(QVector3D location)
{

}

//TODO: 设置可以手绘物体
void MyVulkanApp::loadObjects()
{

    std::string strPath = "E:\\云南测试数据\\00景洪市土地承包经营权\\532801景洪市_江北\\1.矢量空间数据\\统一坐标\\532801CBJYQZD_GD.shp";
    computeGeoBounds(strPath);
    loadShpObjects(strPath);

}

void MyVulkanApp::loadShpObjects(const std::string path)
{
    GDALAllRegister();
    std::unique_ptr<GDALDataset> ds(static_cast<GDALDataset*>(
        GDALOpenEx(path.c_str(),
            GDAL_OF_VECTOR,
            nullptr,
            nullptr,
            nullptr)));

    if (!ds)
    {
        std::runtime_error("failed to open shapefile");
    }

    auto layer = ds->GetLayer(0);
    layer->ResetReading();
    OGRFeature* feature;
    while ((feature = layer->GetNextFeature()) != nullptr)
    {
        OGRGeometry* geom = feature->GetGeometryRef();
        if (geom)
            parseFeature(geom);
        OGRFeature::DestroyFeature(feature);
    }
}

void MyVulkanApp::parseFeature(OGRGeometry* geom)
{
    auto geoType = geom->getGeometryType();
    switch (geoType)
    {
    case wkbPoint:
    {
        auto p = geom->toPoint();
        Model::Vertex vertex = geoToNDC(p->getX(), p->getY());
        Model::Builder builder;
        builder.vertices.push_back(vertex);

        auto model = std::make_shared<Model>(m_device, builder);

        auto point = Object::createObject();
        point.m_model = model;
        point.m_color = { 1,0,0 };
        m_pointObjects.push_back(std::move(point));
    }
    break;
    case wkbLineString:
    {
        Model::Builder builder;
        auto lineString = geom->toLineString();

        for (int i = 0; i < lineString->getNumPoints(); ++i)
        {
            builder.vertices.push_back(geoToNDC(lineString->getX(i), lineString->getY(i)));
            if (i > 0)
            {
                builder.indices.push_back(i - 1);
                builder.indices.push_back(i);
            }
        }

        auto model = std::make_shared<Model>(m_device, builder);
        auto line = Object::createObject();
        line.m_model = model;
        line.m_color = { 0,1,0 };
        m_lineObjects.push_back(std::move(line));
    }
    break;
    case wkbPolygon:
    {
        auto polygon = geom->toPolygon();
        auto ring = polygon->getExteriorRing();
        Model::Builder builder;

        for (int i = 0; i < ring->getNumPoints(); ++i)
        {
            builder.vertices.push_back(geoToNDC(ring->getX(i), ring->getY(i)));
        }

        for (uint32_t i = 1; i + 1 < builder.vertices.size(); ++i)
        {
            builder.indices.push_back(0);
            builder.indices.push_back(i);
            builder.indices.push_back(i + 1);
        }
        auto model = std::make_shared<Model>(m_device, builder);

        auto polyObj = Object::createObject();
        polyObj.m_model = model;
        polyObj.m_color = { 0,0,1 };
        m_polygonObjects.push_back(std::move(polyObj));
    }
    break;
    default:break;
    }
}

void MyVulkanApp::computeGeoBounds(const std::string& path)
{
    GDALAllRegister();
    std::unique_ptr<GDALDataset> ds(static_cast<GDALDataset*>(GDALOpenEx(path.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr)));
    if (!ds) throw std::runtime_error("failed to open shapefile");
    auto layer = ds->GetLayer(0);
    layer->ResetReading();
    OGRFeature* feat;
    while ((feat = layer->GetNextFeature()) != nullptr) {
        OGRGeometry* geom = feat->GetGeometryRef();
        if (geom) updateBounds(geom);
        OGRFeature::DestroyFeature(feat);
    }
}

void MyVulkanApp::updateBounds(OGRGeometry* geom)
{
    OGREnvelope env;
    geom->getEnvelope(&env);
    m_minX = m_minX < env.MinX ? m_minX : env.MinX;
    m_minY = m_minY < env.MinY ? m_minY : env.MinY;
    m_maxX = m_maxX > env.MaxX ? m_maxX : env.MaxX;
    m_maxY = m_maxY > env.MaxY ? m_maxY : env.MaxY;
}

Model::Vertex MyVulkanApp::geoToNDC(double x, double y)
{
    float ndcX = float((x - m_minX) / (m_maxX - m_minX) * 2.0 - 1.0);
    float ndcY = float((y - m_minY) / (m_maxY - m_minY) * 2.0 - 1.0);

    Model::Vertex vertex;
    vertex.position = QVector3D(ndcX, ndcY, 0);
    return vertex;
}

