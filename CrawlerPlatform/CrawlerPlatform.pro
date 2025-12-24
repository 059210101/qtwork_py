QT += core gui widgets sql network charts

# 64位Windows推荐C++17
CONFIG += c++17
CONFIG -= debug_and_release_target
CONFIG += x86_64  # 显式声明64位架构

# 64位MinGW链接QtCharts的核心配置
win32-g++: {
    # 64位库路径（适配Qt默认安装路径）
    LIBS += -L$$[QT_INSTALL_LIBS] -lQt6Charts
}

# 64位头文件路径（确保Charts头文件可找到）
INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtCharts
DEPENDPATH += $$[QT_INSTALL_HEADERS]/QtCharts

# 64位Windows兼容定义
DEFINES += QT_DEPRECATED_WARNINGS _WIN64

SOURCES += main.cpp \
           mainwindow.cpp \
           crawlerthread.cpp \
           databasemanager.cpp

HEADERS += mainwindow.h \
           crawlerthread.h \
           databasemanager.h
