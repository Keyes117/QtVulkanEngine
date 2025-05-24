#pragma once

#include "const.h"
#include "Descriptors.h"
#include "Device.h"
#include "Model.h"
#include "Movement_Controller.h"
#include "MyVulkanWidget.h"
#include "MyVulkanWindow.h"
#include "Object.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "RenderSystem.h"
#include "Scene.h"
#include "SwapChain.h"

#include "qobject.h"


#include <chrono>
#include <memory>
#include <vector>

#include <ogrsf_frmts.h>

class MyVulkanApp : public QObject
{
public:
    MyVulkanApp();
    ~MyVulkanApp();


    MyVulkanApp(const MyVulkanApp&) = delete;
    MyVulkanApp& operator=(const MyVulkanApp&) = delete;

    MyVulkanApp(MyVulkanApp&&) = delete;
    MyVulkanApp& operator=(MyVulkanApp&&) = delete;

    void run();

    static constexpr size_t             MAX_VERTICES = 20000000;

    //std::shared_ptr<RenderSystem> getRenderSystem() { return m_renderSystem; }
private slots:
    void onAddDrawVertex(QVector3D vertexPos);
    void onDrawEnd();
    void onDrawMove(QVector3D location);

private:
    void loadObjects();
    void loadShpObjects(const std::string path);
    void parseFeature(OGRGeometry* geom);
    void computeGeoBounds(const std::string& path);
    void updateBounds(OGRGeometry* geom);
    Model::Vertex geoToNDC(double lon, double lat);

    void createQueryPools();
    void destroyQueryPools();
    void readbackQueryResults();

private:

    float                                                   m_minX = std::numeric_limits<float>::infinity();
    float                                                   m_maxX = -std::numeric_limits<float>::infinity();
    float                                                   m_minY = std::numeric_limits<float>::infinity();
    float                                                   m_maxY = -std::numeric_limits<float>::infinity();


    //VkQueryPool                                             m_queryPool;

    std::chrono::high_resolution_clock::time_point          m_lastFrameTime;
    MyVulkanWidget                                          m_widget;               //窗口类，后续考虑跟window一样改成引用
    MyVulkanWindow& m_window;                                                       //控制窗口中的vulkan 窗口
    Keyboard_Movement_Controller                            m_keyBoardController;
    Mouse_Movement_Controller                               m_mouseController;

    Object                                                  m_cameraObject;
    Object                                                  m_previewObject;
    Camera                                                  m_camera;               //相机类
    Device                                                  m_device;               //负责初始化Vulkan基础组件
    Renderer                                                m_renderer;
    //swapchain 和 commandbuffer
    std::shared_ptr<RenderSystem>                           m_pointRenderSystem;         //pipeline 和 pipelineLayout
    std::shared_ptr<RenderSystem>                           m_lineRenderSystem;
    std::shared_ptr<RenderSystem>                           m_polygonRenderSystem;

    Scene                                                   m_scene;

    uint32_t                                                m_offset;
    //Model::Builder                                          m_builder;
    //std::vector<Model::Builder>                             m_builders;


    std::unique_ptr<DescriptorPool>                         m_globalPool{};
    std::unique_ptr<DescriptorSetLayout>                    m_globalSetLayout;
    std::vector<VkDescriptorSet>                            m_globalDescriptSets;
    std::vector<std::unique_ptr<Buffer>>                    m_spGlobalUboBuffers;


    // GPU 性能计时与统计
    VkQueryPool m_timestampQueryPool = VK_NULL_HANDLE;
    VkQueryPool m_statsQueryPool = VK_NULL_HANDLE;

    // 每帧我们用两个 timestamp 和一个 pipeline-statistics
    static constexpr uint32_t TIMESTAMP_QUERIES = 2;
    static constexpr uint32_t STATS_QUERIES = 1;



};

