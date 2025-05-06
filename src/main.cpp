#include "MyVulkanApp.h"
#include <QApplication>
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    MyVulkanApp myApp;
    myApp.run();
    return app.exec();

}