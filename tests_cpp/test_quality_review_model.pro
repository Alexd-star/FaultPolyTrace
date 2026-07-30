QT += core
QT -= gui

CONFIG += console c++17
CONFIG -= app_bundle
QMAKE_CXXFLAGS += /utf-8
TEMPLATE = app
TARGET = test_quality_review_model

SOURCES += \
    test_quality_review_model.cpp \
    ../src/ui/QualityReviewModel.cpp

HEADERS += \
    ../src/ui/QualityReviewModel.h
