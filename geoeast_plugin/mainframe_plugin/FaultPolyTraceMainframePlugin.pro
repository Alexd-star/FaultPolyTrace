include (../attri_plugin_config/src.pri)

TARGET = FaultPolyTraceMainframePlugin
DEFINES += FW_QT4_SUPPORT
win32 {
    LIBS += -L$$(NGP)/support/libxml2-2.9.9/lib/release -llibxml2
    INCLUDEPATH += $$(NGP)/support/libxml2-2.9.9/include

    LIBS += -L$$(NGP)/libso/common -lngplicense
    LIBS += -L$$(NGP)/licmng/lib -llicclient -llicbase
    LIBS += -L$$(NGP)/support/boost/lib \
        -llibboost_thread-vc142-mt-x64-1_70 \
        -llibboost_chrono-vc142-mt-x64-1_70 \
        -llibboost_date_time-vc142-mt-x64-1_70 \
        -llibboost_regex-vc142-mt-x64-1_70
    LIBS += -L$$(GEOEAST)/lib/appwidget -lsurveyUI5 -ldataset5
}

SOURCES += attriModuleAction.cpp

include (../attri_plugin_config/attriPlugin.pri)
