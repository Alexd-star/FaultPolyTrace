TEMPLATE = app
CONFIG += qt release warn_off c++17

QT += core gui widgets

QMAKE_CXXFLAGS += /utf-8

win32 {
    DEFINES += NOMINMAX
    DEFINES += _HAS_STD_BYTE=0
}

DESTDIR = ./
TARGET = FaultPolyTrace

SOURCES += \
    main.cpp \
    FaultPolyTraceWindow.cpp

HEADERS += \
    FaultPolyTraceWindow.h

OBJECTS_DIR = .obj
MOC_DIR = .moc
