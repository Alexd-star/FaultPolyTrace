TEMPLATE = app
CONFIG += console release c++17
CONFIG -= app_bundle
QT += core
QT -= gui
QMAKE_CXXFLAGS += /utf-8

INCLUDEPATH += ../geoeast_plugin/qt_launcher

SOURCES += \
    test_geoeast_dat_exporter.cpp \
    ../geoeast_plugin/qt_launcher/GeoEastDatExporter.cpp

HEADERS += \
    ../geoeast_plugin/qt_launcher/GeoEastDatExporter.h

RESOURCES += ../geoeast_plugin/qt_launcher/FaultPolyTraceLauncher.qrc

DESTDIR = ./bin
OBJECTS_DIR = .obj_geoeast_dat
