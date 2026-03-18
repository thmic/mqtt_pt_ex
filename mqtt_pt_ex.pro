QT += core gui network charts

# 设置C++17标准
CONFIG += c++17

# 禁用Qt关键字以避免与STL冲突
DEFINES += QT_NO_KEYWORDS

# 解决Qt 6.10.2 + MinGW 13.1.0兼容性问题
DEFINES += QT_NO_COMPARISON_HELPERS

# 添加额外的编译器标志来解决兼容性问题
QMAKE_CXXFLAGS += -fpermissive
QMAKE_CXXFLAGS += -Wno-error=deprecated-declarations

# 目标名称
TARGET = mqtt_pt_ex

TEMPLATE = app

# ============================================
# MQTT 模块配置
# ============================================
# 由于Qt 6.10.2的MQTT模块可能需要单独安装或配置
# 如果编译时找不到MQTT模块，请使用以下手动配置：

# 手动添加MQTT头文件路径（根据你的实际安装路径修改）
INCLUDEPATH += D:/software/MyQtLib/6.10.2/mingw_64/include/QtMqtt
INCLUDEPATH += D:/software/MyQtLib/6.10.2/mingw_64/include

# 手动添加MQTT库路径
LIBS += -L$$[QT_INSTALL_LIBS] -lQt6Mqtt

# ============================================

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    mqttsender.cpp

HEADERS += \
    mainwindow.h \
    mqttsender.h

FORMS += \
    mainwindow.ui
