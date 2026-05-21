TARGET_PLUGDIR = GeoAttribute

NGP_PROJECT_TYPE = plugin
NGP_PROJECT_CONFIG_FILE = manifest.xml
NGP_MODULES += framework ggeometry graphics2d ribbon xml2 ndp plugincore

NGP_ROOTDIR = $$(NGP)
NGP_CONFIG_PRI_FILE = $$NGP_ROOTDIR/resource/templatepri/ngp_config.pri
!include($$NGP_CONFIG_PRI_FILE) : include($$NGP_CONFIG_PRI_FILE)

NGP_TARGET_PLUGDIR = $$GGI_BIN_PATH/plugins/$$TARGET_PLUGDIR/$$NGP_PROJECT_NAME
NGP_TARGET_BINDIR = $$NGP_TARGET_PLUGDIR
DESTDIR = $$NGP_TARGET_BINDIR

QMAKE_EXTRA_COMPILERS -= xmltar
outf = $$NGP_TARGET_BINDIR/$$NGP_PROJECT_CONFIG_FILE
inf = $$FILE_FULL_PATH

xmltar.output = $$outf
xmltar.input = inf

win32 {
    inf = $$NGP_PROJECT_CONFIG_FILE
    outf = $$(GEOEAST)\bin\plugins\\$$TARGET_PLUGDIR\\$$NGP_PROJECT_NAME\\$$NGP_PROJECT_CONFIG_FILE
    xmltar.commands = $${QMAKE_COPY} $${inf} $${outf}
}

xmltar.name = xmltarget
xmltar.CONFIG = no_link
xmltar.variable_out = PRE_TARGETDEPS
QMAKE_EXTRA_COMPILERS += xmltar
