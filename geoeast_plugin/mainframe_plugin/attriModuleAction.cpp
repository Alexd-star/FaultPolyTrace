#include "GtModulePlugin.h"

ngp::gsm::NGPLicense* g_pLicense = new ngp::gsm::NGPLicense();

ATTRI_MODULE_PLUGIN_DEFINE_WITH_TOOLTIP_BEGIN

ATTRI_MODULE_PLUGIN_DEFINE_WITH_TOOLTIP(
    QObject::tr("断层多边形追踪"),
    QObject::tr("基于平面断层属性自动追踪闭合断层多边形"),
    "eps.png",
    faultpolytrace_launcher,
    "",
    0)

ATTRI_MODULE_PLUGIN_DEFINE_WHIT_TOOLTIP_END
