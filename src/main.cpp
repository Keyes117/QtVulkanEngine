#include "MyVulkanApp.h"
#include <QApplication>
#include <QWidget>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    MyVulkanApp myApp;

    myApp.run();
    return app.exec();

}