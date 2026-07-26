#include "FptIcons.h"

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QRectF>

namespace fpt_ui {

namespace {

const QColor kOutline("#4A4A4A");
const QColor kBlue("#1D7FC1");
const QColor kBlueDeep("#005A9E");
const QColor kBlueLight("#EAF6FF");
const QColor kGreen("#2E8B57");
const QColor kGreenLight("#E6F5EC");
const QColor kYellow("#F4B844");
const QColor kYellowLight("#FFD36A");
const QColor kOrange("#E08A2E");
const QColor kRed("#C94040");
const QColor kPurple("#6A5ACD");
const QColor kPurpleLight("#EEEAFE");
const QColor kTeal("#2A9D8F");
const QColor kBrown("#A9743B");

// 打开状态的文件夹：夹层 + 前盖，Open 与 Folder 共用轮廓，仅色调不同。
void paintFolder(QPainter& p, const QColor& body, const QColor& flap) {
    p.setPen(QPen(QColor("#C58A1D"), 1.2));
    p.setBrush(body);
    p.drawRoundedRect(QRectF(3, 8, 18, 11), 2, 2);
    p.setBrush(flap);
    p.drawRoundedRect(QRectF(3, 6, 8, 5), 1.5, 1.5);
}

void paintMagnifier(QPainter& p, bool plus, bool minus) {
    p.setPen(QPen(kBlue, 2.0));
    p.setBrush(kBlueLight);
    p.drawEllipse(QRectF(4, 4, 11, 11));
    p.drawLine(QPointF(13, 13), QPointF(20, 20));
    p.setPen(QPen(kBlueDeep, 1.7, Qt::SolidLine, Qt::RoundCap));
    if (plus || minus) {
        p.drawLine(QPointF(7, 9.5), QPointF(12, 9.5));
    }
    if (plus) {
        p.drawLine(QPointF(9.5, 7), QPointF(9.5, 12));
    }
}

// 回转箭头：重置视图与恢复默认共用形状，用色调区分语义。
void paintCircularArrow(QPainter& p, const QColor& color) {
    p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(5, 5, 14, 14), 35 * 16, 285 * 16);
    p.drawLine(QPointF(6, 8), QPointF(5, 4));
    p.drawLine(QPointF(6, 8), QPointF(10, 7));
}

// 以 24×24 逻辑坐标绘制，按目标像素尺寸整体缩放，供多分辨率图标复用。
QPixmap renderPixmap(IconKind kind, int px) {
    QPixmap pixmap(px, px);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(px / 24.0, px / 24.0);
    painter.setPen(QPen(kOutline, 1.4));

    switch (kind) {
    case IconKind::Open:
        paintFolder(painter, kYellow, kYellowLight);
        break;
    case IconKind::Folder:
        paintFolder(painter, QColor("#F0C070"), QColor("#FFE0A0"));
        painter.setPen(QPen(QColor("#C58A1D"), 1.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(QPointF(6, 12.5), QPointF(18, 12.5));
        painter.drawLine(QPointF(6, 15.5), QPointF(15, 15.5));
        break;
    case IconKind::Save:
        painter.setPen(QPen(kBlueDeep, 1.2));
        painter.setBrush(QColor("#3F8ED0"));
        painter.drawRoundedRect(QRectF(5, 4, 14, 16), 1.5, 1.5);
        painter.setBrush(QColor("#EAF4FF"));
        painter.drawRect(QRectF(8, 5.5, 7, 4.5));
        painter.drawRect(QRectF(8, 14, 8, 5));
        break;
    case IconKind::Load:
        painter.setPen(QPen(kBlueDeep, 1.2));
        painter.setBrush(QColor("#7FB6E4"));
        painter.drawRoundedRect(QRectF(3, 9, 18, 10), 2, 2);
        painter.setPen(QPen(kBlueDeep, 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(12, 3), QPointF(12, 10));
        painter.drawLine(QPointF(8.6, 6.4), QPointF(12, 3));
        painter.drawLine(QPointF(15.4, 6.4), QPointF(12, 3));
        break;
    case IconKind::Preset:
        painter.setPen(QPen(kPurple, 1.4));
        painter.setBrush(kPurpleLight);
        painter.drawRoundedRect(QRectF(5, 4, 14, 16), 2, 2);
        painter.setPen(QPen(kPurple, 1.6, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(8, 9), QPointF(16, 9));
        painter.drawLine(QPointF(8, 13), QPointF(16, 13));
        painter.setPen(QPen(kGreen, 2.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(8, 17), QPointF(10.5, 19));
        painter.drawLine(QPointF(10.5, 19), QPointF(17, 12));
        break;
    case IconKind::Pan:
        painter.setPen(QPen(kGreen, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(QPointF(8, 12), QPointF(8, 7));
        painter.drawLine(QPointF(11, 12), QPointF(11, 5));
        painter.drawLine(QPointF(14, 13), QPointF(14, 7));
        painter.drawLine(QPointF(17, 14), QPointF(17, 9));
        painter.drawLine(QPointF(7, 13), QPointF(11, 19));
        painter.drawLine(QPointF(11, 19), QPointF(18, 18));
        painter.drawLine(QPointF(18, 18), QPointF(19, 13));
        break;
    case IconKind::ZoomIn:
        paintMagnifier(painter, true, false);
        break;
    case IconKind::ZoomOut:
        paintMagnifier(painter, false, true);
        break;
    case IconKind::Reset:
        paintCircularArrow(painter, kBlue);
        break;
    case IconKind::Restore:
        paintCircularArrow(painter, kOrange);
        break;
    case IconKind::Run:
        painter.setPen(Qt::NoPen);
        painter.setBrush(kBlue);
        painter.drawPolygon(QPolygonF() << QPointF(7, 5) << QPointF(19, 12) << QPointF(7, 19));
        break;
    case IconKind::Stop:
        painter.setPen(Qt::NoPen);
        painter.setBrush(kRed);
        painter.drawRect(QRectF(7, 7, 10, 10));
        break;
    case IconKind::Image:
        painter.setPen(QPen(kGreen, 1.4));
        painter.setBrush(kGreenLight);
        painter.drawRoundedRect(QRectF(3.5, 5, 17, 14), 2, 2);
        painter.setPen(QPen(kGreen, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(QColor("#8FCFAA"));
        painter.drawPolygon(QPolygonF() << QPointF(5.5, 17) << QPointF(10.5, 10.5)
                                        << QPointF(14, 14.5) << QPointF(16, 12)
                                        << QPointF(18.5, 17));
        painter.setPen(Qt::NoPen);
        painter.setBrush(kYellow);
        painter.drawEllipse(QPointF(8.6, 8.8), 1.5, 1.5);
        break;
    case IconKind::Database:
        painter.setPen(QPen(kBlueDeep, 1.3));
        painter.setBrush(QColor("#DFF0FF"));
        painter.drawEllipse(QRectF(5, 4, 14, 5));
        painter.drawRect(QRectF(5, 6.5, 14, 11));
        painter.drawEllipse(QRectF(5, 15, 14, 5));
        painter.drawLine(QPointF(5, 9.5), QPointF(19, 9.5));
        painter.drawLine(QPointF(5, 13), QPointF(19, 13));
        break;
    case IconKind::Refresh:
        painter.setPen(QPen(kTeal, 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawArc(QRectF(4.5, 4.5, 15, 15), 20 * 16, 150 * 16);
        painter.drawArc(QRectF(4.5, 4.5, 15, 15), 200 * 16, 150 * 16);
        painter.setPen(Qt::NoPen);
        painter.setBrush(kTeal);
        painter.drawPolygon(QPolygonF() << QPointF(4, 8) << QPointF(9, 8) << QPointF(6, 12));
        painter.drawPolygon(QPolygonF() << QPointF(20, 16) << QPointF(15, 16) << QPointF(18, 12));
        break;
    case IconKind::Basemap:
        painter.setPen(QPen(kBrown, 1.3));
        painter.setBrush(QColor("#F2E3CE"));
        painter.drawPolygon(QPolygonF() << QPointF(3.5, 7) << QPointF(9, 5) << QPointF(15, 8)
                                        << QPointF(20.5, 5.5) << QPointF(20.5, 17)
                                        << QPointF(15, 19.5) << QPointF(9, 16.5)
                                        << QPointF(3.5, 19));
        painter.setPen(QPen(kBrown, 1.1, Qt::DashLine));
        painter.drawLine(QPointF(9, 5), QPointF(9, 16.5));
        painter.drawLine(QPointF(15, 8), QPointF(15, 19.5));
        break;
    case IconKind::Help:
        painter.setPen(QPen(QColor("#DC143C"), 1.5));
        painter.setBrush(QColor("#FFF5F7"));
        painter.drawEllipse(QRectF(4, 4, 16, 16));
        painter.setPen(QPen(QColor("#DC143C"), 2.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawText(QRectF(4, 4, 16, 16), Qt::AlignCenter, QStringLiteral("?"));
        break;
    }

    painter.end();
    return pixmap;
}

}  // namespace

QIcon toolIcon(IconKind kind) {
    // 装入多档位图，窗口在不同缩放比的显示器之间移动时由 QIcon 自行挑选，
    // 不把某一块屏幕的设备像素比固化进图标。
    QIcon icon;
    for (int px : {16, 24, 32, 48, 64}) {
        icon.addPixmap(renderPixmap(kind, px));
    }
    return icon;
}

}  // namespace fpt_ui
