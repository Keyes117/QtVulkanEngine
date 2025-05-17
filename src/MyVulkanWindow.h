#pragma once

#include <qevent.h>
#include <QWindow>
#include <QKeyEvent>
#define VK_USE_PLATFORM_WIN32_KHR
#include <Windows.h>


#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

enum class DrawMode
{
    None,
    Point,
    Line,
    Polygon
};


class MyVulkanWindow : public QWindow
{
    Q_OBJECT
public:
    explicit MyVulkanWindow(QWindow* parent = nullptr);
    ~MyVulkanWindow();

    void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);

    VkExtent2D getExtent() {
        return { static_cast<uint32_t>(m_width),static_cast<uint32_t>(m_height) };
    }

    bool isWidthResized() { return m_framebufferResized; }
    void resetWindowResizedFlag() { m_framebufferResized = false; }
    DrawMode getCurrentDrawMode() { return m_drawMode; }

    int width() { return m_width; }
    int height() { return m_height; }

signals:
    void updateRequested();
    void keyPressed(int key, Qt::KeyboardModifiers mods);
    void CameraMovement(QVector3D vector);
    void CameraZoom(QVector3D ndcAndSteps);
    void drawAddVertex(QVector3D vertex);
    void drawEnd();
    void drawMove(QVector3D location);

public slots:
    void setDrawNone() { m_drawMode = DrawMode::None; }
    void setDrawPoint() { m_drawMode = DrawMode::Point; }
    void setDrawLine() { m_drawMode = DrawMode::Line; }
    void setDrawPolygon() { m_drawMode = DrawMode::Polygon; }

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void exposeEvent(QExposeEvent* event) override;
    bool event(QEvent* event) override;

private:
    float normalizeX(float x) { return 2.0f * x / m_width - 1.0f; }
    float normalizeY(float y) { return 2.0f * y / m_width - 1.0f; }


private:
    int m_width{ 800 };
    int m_height{ 600 };

    bool      m_isDragging;
    QPointF   m_lastMousePos;


    DrawMode m_drawMode{ DrawMode::None };
    bool m_framebufferResized{ false };


};

