TEMPLATE = app
CONFIG += console c++14
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -std=c++0x -pthread
LIBS += -pthread

SOURCES += \
        main.cpp
