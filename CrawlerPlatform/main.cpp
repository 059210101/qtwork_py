#include <QApplication>
#include "mainwindow.h"
#include "databasemanager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 初始化数据库（主线程）
    if (!DatabaseManager::initDatabaseSchema()) {
        qCritical() << "数据库初始化失败，程序退出";
        return -1;
    }

    MainWindow w;
    w.show();

    return a.exec();
}
