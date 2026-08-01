TEMPLATE = app
CONFIG += qt release warn_off console c++17

QMAKE_CXXFLAGS += /utf-8

win32 {
    DEFINES += NOMINMAX
    DEFINES += _HAS_STD_BYTE=0
}

DESTDIR = ./
TARGET = faultpolytrace_geoeast_runner

INCLUDEPATH += ./
INCLUDEPATH += ../../src
INCLUDEPATH += ../../src/core
INCLUDEPATH += ../../src/io

INCLUDEPATH += $(IECO)/include/ndp/core
INCLUDEPATH += $(IECO)/include/ndp/bdo
INCLUDEPATH += $(IECO)/include/ndp/bo
INCLUDEPATH += $(IECO)/include/common/ggeometry

win32 {
    INCLUDEPATH += $(NGP)/support/libxml2-2.9.9/include
}

LIBS += -L$(IECO)/libso/ndp
LIBS += -L$(IECO)/libso/common

win32 {
    LIBS += -lbowell -lboref -lbointp -lboseis -lbomgr -lboprocess
    LIBS += -lbdowell -lbdoseis -lbdoprocess -lsqlcore -lcpool
    LIBS += -ldpplug -ldpbase -lbdointp -lbdomgr -ldpconf
    LIBS += -ldpthreadlck -ldpfiles -ldpuomsys -ldpcrs -lggeometry5
    include($(IECO)/support/support_config.pri)
    include($(IECO)/support/pg_config.pri)
}

SOURCES += \
    StagedCommit.cpp \
    main.cpp \
    GeoEastBoAdapter.cpp \
    ../../src/core/AdaptiveBoundary.cpp \
    ../../src/core/BinaryMask.cpp \
    ../../src/core/BranchAttribution.cpp \
    ../../src/core/FaultTracer.cpp \
    ../../src/core/LocalPolygonQuality.cpp \
    ../../src/core/MultiScaleEvidence.cpp \
    ../../src/core/WeakPathSupport.cpp \
    ../../src/core/WeakContinuityConnector.cpp \
    ../../src/core/WeakExtensionGate.cpp \
    ../../src/core/WeakTailRestorer.cpp \
    ../../src/core/Grid2D.cpp \
    ../../src/core/GridMetric.cpp \
    ../../src/core/PhysicalMorphology.cpp \
    ../../src/io/AsciiGridIO.cpp \
    ../../src/io/DiagnosticsIO.cpp \
    ../../src/core/Polygon.cpp \
    ../../src/core/Segmentation.cpp \
    ../../src/core/SkeletonGraph.cpp \
    ../../src/core/SkeletonGraphV2.cpp \
    ../../src/core/SkeletonGraphV2Attribution.cpp \
    ../../src/core/Topology.cpp

HEADERS += \
    StagedCommit.h \
    GeoEastBoAdapter.h \
    ../../src/core/AdaptiveBoundary.h \
    ../../src/core/BinaryMask.h \
    ../../src/core/BranchAttribution.h \
    ../../src/core/FaultTracer.h \
    ../../src/core/LocalPolygonQuality.h \
    ../../src/core/MultiScaleEvidence.h \
    ../../src/core/WeakPathSupport.h \
    ../../src/core/WeakContinuityConnector.h \
    ../../src/core/WeakExtensionGate.h \
    ../../src/core/WeakTailRestorer.h \
    ../../src/core/Grid2D.h \
    ../../src/core/GridMetric.h \
    ../../src/core/PhysicalMorphology.h \
    ../../src/io/AsciiGridIO.h \
    ../../src/io/DiagnosticsIO.h \
    ../../src/core/Polygon.h \
    ../../src/core/Segmentation.h \
    ../../src/core/SkeletonGraph.h \
    ../../src/core/SkeletonGraphV2.h \
    ../../src/core/SkeletonGraphV2Attribution.h \
    ../../src/core/Topology.h

OBJECTS_DIR = .obj
MOC_DIR = .moc
