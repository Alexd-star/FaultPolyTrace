TEMPLATE = app
CONFIG += qt release warn_off c++17

QT += core gui widgets

QMAKE_CXXFLAGS += /utf-8
INCLUDEPATH += ../../src/ui

win32 {
    DEFINES += NOMINMAX
    DEFINES += _HAS_STD_BYTE=0
    RC_ICONS = ../../assets/icons/faultpolytrace.ico
}

DESTDIR = ./
TARGET = FaultPolyTrace

SOURCES += \
    main.cpp \
    ../../src/ui/UiLocalization.cpp \
    ../../src/ui/FptTheme.cpp \
    ../../src/ui/TraceParameterModel.cpp \
    FaultPolyTraceWindow.cpp

HEADERS += \
    ../../src/ui/UiLocalization.h \
    ../../src/ui/FptTheme.h \
    ../../src/ui/TraceParameterModel.h \
    FaultPolyTraceWindow.h

OBJECTS_DIR = .obj
MOC_DIR = .moc
