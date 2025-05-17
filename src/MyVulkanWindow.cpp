#include "MyVulkanWindow.h"

#include <stdexcept>


MyVulkanWindow::MyVulkanWindow(QWindow* parent)
    :QWindow(parent)
{

}

MyVulkanWindow::~MyVulkanWindow()
{
}

void MyVulkanWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
{
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = ::GetModuleHandle(nullptr);
    createInfo.hwnd = reinterpret_cast<HWND>(winId());

    if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to crate Win32 Vulkan Surface");
    }
}
void MyVulkanWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton)
    {
        m_isDragging = false;
    }

    QWindow::mouseReleaseEvent(event);
}

void MyVulkanWindow::mousePressEvent(QMouseEvent* event)
{
    // 获取窗口的尺寸
    if (event->button() == Qt::MiddleButton)
    {
        // 获取上次中间按下鼠标位置
        if (!m_isDragging)
        {
            m_isDragging = true;
            m_lastMousePos = event->pos();
        }
    }
    else if (event->button() == Qt::LeftButton)
    {
        if (m_drawMode != DrawMode::None)
        {
            auto pos = event->pos();
            float normalizedX = normalizeX(pos.x());
            float normalizedY = normalizeX(pos.y());

            QVector3D vertex(normalizedX, normalizedY, 0);
            emit drawAddVertex(vertex);
        }

    }
    else if (event->button() == Qt::RightButton)
    {
        if (m_drawMode != DrawMode::None)
            emit drawEnd();
    }

    QWindow::mousePressEvent(event);

}

void MyVulkanWindow::resizeEvent(QResizeEvent* event)
{
    QWindow::resizeEvent(event);

    m_framebufferResized = true;
    m_width = event->size().width();
    m_width = event->size().height();

}

void MyVulkanWindow::exposeEvent(QExposeEvent* event)
{
    if (isExposed())
        requestUpdate();
}

bool MyVulkanWindow::event(QEvent* event)
{
    if (event->type() == QEvent::UpdateRequest && isExposed())
    {
        emit updateRequested();
        return true;
    }
    return QWindow::event(event);
}

void MyVulkanWindow::keyPressEvent(QKeyEvent* event)
{
    emit keyPressed(event->key(), event->modifiers());

    QWindow::keyPressEvent(event);
}

void MyVulkanWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isDragging)
    {
        QPointF currentPos = event->pos();
        QPointF delte = currentPos - m_lastMousePos;
        m_lastMousePos = currentPos;

        int windowWidth = this->width();
        int windowHeight = this->height();


        // 将屏幕坐标转换为归一化设备坐标 [-1, 1]
        float normalizedX = -(2.0f * delte.x()) / windowWidth;
        float normalizedY = -(2.0f * delte.y()) / windowHeight;

        // NDC 范围 [-1,1] 对应视锥 [left,right]，所以增量要乘 ½
        //float world_dx = normalizedX * (windowWidth * 0.5f);
        //float world_dy = normalizedY * (windowHeight * 0.5f);

        QVector3D delteVector = { normalizedX, normalizedY, 0.f };
        emit CameraMovement(delteVector);
    }
    if (m_drawMode != DrawMode::None)
    {
        auto pos = event->pos();
        float normalizedX = normalizeX(pos.x());
        float normalizedY = normalizeX(pos.y());

        QVector3D vertex(normalizedX, normalizedY, 0);

        emit drawMove(vertex);
    }

    QWindow::mouseMoveEvent(event);
}

void MyVulkanWindow::wheelEvent(QWheelEvent* event)
{
    int deltaY = event->angleDelta().y();
    qDebug() << deltaY;

    float numSteps = deltaY / 120.0f;

    QPointF pos = event->position();


    float normalizedX = 2.0f * pos.x() / m_width - 1.0f;
    float normalizedY = 2.0f * pos.y() / m_height - 1.0f;

    qDebug() << pos.x() << "," << pos.y();
    qDebug() << normalizedX << "," << normalizedY;

    QVector3D zVector{ normalizedX,normalizedY,numSteps };
    emit CameraZoom(zVector);
}
