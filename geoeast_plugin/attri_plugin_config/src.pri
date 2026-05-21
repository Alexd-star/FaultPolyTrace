include (./makep)

QT += widgets core gui xml svg

MOC_DIR += ./.moc
OBJECTS_DIR += ./.obj
UI_DIR += ./.ui

INCLUDEPATH += $$join(GGI_MAKE_PATH,,,/include/ggi/seisrd/getoolkit)
INCLUDEPATH += $$join(GGI_MAKE_PATH,,,/include/ggi/seisrd/public)
INCLUDEPATH += $$join(BASE_MAKE_PATH,,,/include/appwidget/ggicomm)
INCLUDEPATH += $$join(BASE_MAKE_PATH,,,/include/appwidget5/ggicomm)
INCLUDEPATH += $$join(BASE_MAKE_PATH,,,/include/appwidget/dataset)
INCLUDEPATH += $$join(BASE_MAKE_PATH,,,/include/appwidget5/dataset)
INCLUDEPATH += $$join(BASE_MAKE_PATH,,,/include/appwidget/runctrldbus)
INCLUDEPATH += $$join(BASE_MAKE_PATH,,,/include/appwidget5/fwappwidget)
INCLUDEPATH += $$join(BASE_MAKE_PATH,,,/include/appwidget/surveyUI)
INCLUDEPATH += $$join(BASE_MAKE_PATH,,,/include/appwidget5/surveyUI)
INCLUDEPATH += $$join(BASE_MAKE_PATH,,,/include/appwidget/ggimath)
INCLUDEPATH += $$join(BASE_MAKE_PATH,,,/include/appwidget/wavelet)
INCLUDEPATH += $$join(BASE_MAKE_PATH,,,/include/appwidget5/wavelet)
INCLUDEPATH += $$join(BASE_MAKE_PATH,,,/include/appwidget/plot2d)
INCLUDEPATH += $$join(BASE_MAKE_PATH,,,/include/appwidget5/plot2d)
INCLUDEPATH += $(NGP)/include/nap/thirdpart/ribbon4/include
INCLUDEPATH += $(NGP)/include/nap/ap5/runctrl
INCLUDEPATH += $(NGP)/include/nap/ap5/runctrlnofw
INCLUDEPATH += $(NGP)/include/nap/ap5/fwappwidget
INCLUDEPATH += $(NGP)/include/nap/ap5/qt4support

win32 {
    INCLUDEPATH += $(NGP)/support/libxml2-2.9.9/include
    INCLUDEPATH += $(NGP)/support/iconv/include
    INCLUDEPATH += $(NGP)/support/thread/include
    INCLUDEPATH += $(NGP)/support/syslog/include
    INCLUDEPATH += $(NGP)/support/strptime/include
    INCLUDEPATH += $(NGP)/support/gdal/include

    LIBS += $$join(BASE_MAKE_PATH,,,/lib/ggi/getoolkit.lib)
    LIBS += -L$(NGP)/libso/nap -lframework5 -lfwappwidget5
    LIBS += -L$(NGP)/libso/nap -lnaptree -lngpbasetree
    LIBS += -L$(GEOEAST)/libso/appwidget -lgeocomrunctrldbus5
}
