#include "MyVulkanApp.h"

#include <array>
#include <QApplication>
#include <qtimer.h>
#include <random>
#include "Buffer.h"
#include "Movement_Controller.h"


#ifdef max
#undef max
#endif
constexpr float MAX_FRAME_TIME = 0.01667;

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
    m_renderer(m_window, m_device),
    m_scene(m_device)
{

    createQueryPools();

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
    destroyQueryPools();
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
            m_camera.setPrespectiveProjection(degressToRadians(50.f), aspect, 0.001f, 200.f);

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
                vkCmdResetQueryPool(commandBuffer, m_timestampQueryPool, 0, TIMESTAMP_QUERIES);
                vkCmdResetQueryPool(commandBuffer, m_statsQueryPool, 0, STATS_QUERIES);


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


                vkCmdWriteTimestamp(commandBuffer,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    m_timestampQueryPool, 0);
                //render
                m_renderer.beginSwapChainRenderPass(commandBuffer);

                // 同时开启管线统计
                vkCmdBeginQuery(commandBuffer, m_statsQueryPool, 0, 0);

                m_pointRenderSystem->renderScene(frameInfo, m_scene);
                m_lineRenderSystem->renderScene(frameInfo, m_scene);
                m_polygonRenderSystem->renderScene(frameInfo, m_scene);

                vkCmdEndQuery(commandBuffer, m_statsQueryPool, 0);

                m_renderer.endSwapChainRenderPass(commandBuffer);

                vkCmdWriteTimestamp(commandBuffer,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    m_timestampQueryPool, 1);

                m_renderer.endFrame();

                readbackQueryResults();
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

    /*  Model::Builder builder;
      builder.vertices = {
          {{-0.5f, -0.5f, 0.0f}, {1.0f , 0.f , 0.f}},
          {{-0.5f, 0.5f, 0.0f}, {1.0f , 0.f , 0.f}},
          {{0.5f, -0.5f, 0.0f}, {1.0f , 0.f , 0.f}},
          {{0.5f, 0.5f, 0.0f}, {1.0f , 0.f , 0.f}}
      };

      builder.indices = { 0,1,1,2,2,3 };
      auto model = std::make_shared<Model>(m_device, builder);
      auto obj = Object::createObject();
      obj.m_model = model;
      obj.m_color = { 1,0,0 };
      obj.m_transform.translation = { 0.f,0.f,2.5f };

      m_lineObjects.push_back(std::move(obj));*/
    std::string strPath = "E:\\Kontur_prj.gdb";
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
    int count = 0;
    auto layer = ds->GetLayer(0);
    layer->ResetReading();
    OGRFeature* feature;
    while ((feature = layer->GetNextFeature()) != nullptr)
    {
        OGRGeometry* geom = feature->GetGeometryRef();
        if (geom)
            parseFeature(geom);
        count++;
        OGRFeature::DestroyFeature(feature);
        if (count == 100000)
            break;

    }

    /*  for (int i = 0; i < m_builders.size(); i++)
      {
          auto geoModel = std::make_shared<Model>(m_device, m_builders[i]);

          auto object = Object::createObject();
          object.m_color = { 1,0,0 };
          object.m_model = geoModel;
          object.m_transform.translation = { 0.f,0.f,0.f };

          m_lineObjects.push_back(std::move(object));

      }*/
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
        builder.type = ModelType::Point;
        builder.vertices.push_back(vertex);

        auto model = std::make_shared<Model>(m_device, builder);

        auto point = Object::createObject();
        point.m_model = model;
        point.m_color = { 1,0,0 };
        m_scene.addObject(std::move(point));
    }
    break;
    case wkbLineString:
    {
        Model::Builder builder;
        builder.type = ModelType::Line;
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
        m_scene.addObject(std::move(line));
    }
    break;
    case wkbMultiLineString25D:
    {
        auto mls = geom->toMultiLineString();

        // 对于每一条 LineString
        for (int iLine = 0; iLine < mls->getNumGeometries(); ++iLine) {
            auto ls = mls->getGeometryRef(iLine)->toLineString();
            int nPts = ls->getNumPoints();
            if (nPts < 2)
                continue;

            Model::Builder builder;
            builder.type = ModelType::Line;
            for (int i = 0; i < nPts; i++)
            {
                double x = ls->getX(i);
                double y = ls->getY(i);
                builder.vertices.push_back(geoToNDC(x, y));
            }

            for (uint32_t i = 0; i + 1 < static_cast<uint32_t>(nPts); ++i)
            {
                builder.indices.push_back(i);
                builder.indices.push_back(i + 1);
            }
            builder.indices.push_back(std::numeric_limits<uint32_t>::max());

            auto geoModel = std::make_shared<Model>(m_device, builder);

            auto object = Object::createObject();
            object.m_color = { 1,0,0 };
            object.m_model = geoModel;
            object.m_transform.translation = { 0.f,0.f,0.f };

            m_scene.addObject(std::move(object));
            //m_lineObjects.push_back(std::move(object));
        }
    }
    break;
    case wkbPolygon:
    {
        auto polygon = geom->toPolygon();
        auto ring = polygon->getExteriorRing();
        auto nPts = ring->getNumPoints();

        //m_offset = static_cast<uint32_t>(m_builder.vertices.size());

   /*     for (int i = 0; i < nPts; ++i)
        {
            m_builder.vertices.push_back(geoToNDC(ring->getX(i), ring->getY(i)));
        }


        for (uint32_t i = 0; i + 1 < nPts; ++i)
        {
            m_builder.indices.push_back(m_offset + 0);
            m_builder.indices.push_back(m_offset + i);
            m_builder.indices.push_back(m_offset + i + 1);

        }*/

    }
    break;
    default:
    {
        qDebug() << " has other type ";
    }
    break;
    }
}

void MyVulkanApp::computeGeoBounds(const std::string& path)
{
    GDALAllRegister();
    std::unique_ptr<GDALDataset> ds(static_cast<GDALDataset*>(GDALOpenEx(path.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr)));
    if (!ds)
        throw std::runtime_error("failed to open shapefile");
    auto layerCount = ds->GetLayerCount();
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
    float ndcX = 100 * float((x - m_minX) / (m_maxX - m_minX) * 2.0 - 1.0);
    float ndcY = -100 * float((y - m_minY) / (m_maxY - m_minY) * 2.0 - 1.0);

    static std::random_device rd;
    static std::mt19937       gen(rd());
    static std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    Model::Vertex vertex;
    vertex.position = QVector3D(ndcX, ndcY, 0);
    //vertex.color = QVector3D(dist01(gen), dist01(gen), dist01(gen));
    return vertex;
}

void MyVulkanApp::createQueryPools()
{

    VkQueryPoolCreateInfo tsInfo{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
    tsInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    tsInfo.queryCount = TIMESTAMP_QUERIES;

    vkCreateQueryPool(m_device.device(), &tsInfo, nullptr, &m_timestampQueryPool);


    VkQueryPoolCreateInfo statsInfo{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
    statsInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
    statsInfo.queryCount = STATS_QUERIES;
    statsInfo.pipelineStatistics =
        VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
        VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT;

    vkCreateQueryPool(m_device.device(), &statsInfo, nullptr, &m_statsQueryPool);

}

void MyVulkanApp::destroyQueryPools()
{
    if (m_timestampQueryPool != VK_NULL_HANDLE)
        vkDestroyQueryPool(m_device.device(), m_timestampQueryPool, nullptr);

    if (m_statsQueryPool != VK_NULL_HANDLE)
        vkDestroyQueryPool(m_device.device(), m_statsQueryPool, nullptr);
}

void MyVulkanApp::readbackQueryResults()
{
    uint64_t timestamps[2];
    vkGetQueryPoolResults(
        m_device.device(),
        m_timestampQueryPool,
        0, TIMESTAMP_QUERIES,
        sizeof(timestamps), timestamps,
        sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

    // 2) 转换为毫秒
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_device.physicalDevice(), &props);
    double period = double(props.limits.timestampPeriod); // 纳秒／tick
    double ms = (timestamps[1] - timestamps[0]) * period * 1e-6;

    qDebug() << QString().asprintf("[GPU] RenderPass time: %.3f ms\n", ms);

    // 3) 读管线统计
    struct Stats { uint64_t vsInvocs, fsInvocs; } stats;
    vkGetQueryPoolResults(
        m_device.device(),
        m_statsQueryPool,
        0, STATS_QUERIES,
        sizeof(stats), &stats,
        sizeof(uint64_t) * 2,
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

    qDebug() << QString().asprintf("[GPU] VS invocations: %llu, FS invocations: %llu\n",
        stats.vsInvocs, stats.fsInvocs);
}

