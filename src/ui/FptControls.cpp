#include "FptControls.h"

#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QStyle>
#include <QStyleOption>
#include <QtMath>

namespace fpt_ui {

LayerToggle::LayerToggle(QWidget* parent) : QAbstractButton(parent) {
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(28);

    animation_ = new QPropertyAnimation(this, "knobPosition", this);
    animation_->setDuration(150);
    animation_->setEasingCurve(QEasingCurve::OutCubic);
    connect(this, &QAbstractButton::toggled, this, [this](bool checked) {
        const bool wasAnimating = animation_->state() == QAbstractAnimation::Running;
        animation_->stop();
        // A programmatic state change may use QSignalBlocker, which also blocks
        // this internal slot. Start from the visible endpoint in that case.
        animation_->setStartValue(wasAnimating ? knobPosition_ : (checked ? 0.0 : 1.0));
        animation_->setEndValue(checked ? 1.0 : 0.0);
        if (QApplication::isEffectEnabled(Qt::UI_AnimateCombo)) {
            animation_->start();
        } else {
            setKnobPosition(checked ? 1.0 : 0.0);
        }
    });
}

qreal LayerToggle::knobPosition() const {
    return knobPosition_;
}

void LayerToggle::setKnobPosition(qreal position) {
    knobPosition_ = qBound<qreal>(0.0, position, 1.0);
    update();
}

void LayerToggle::setAccentColor(const QColor& color) {
    if (color.isValid()) {
        accentColor_ = color;
        update();
    }
}

QSize LayerToggle::sizeHint() const {
    const QFontMetrics fm(font());
    return QSize(qMax(150, fm.horizontalAdvance(text()) + 70), 30);
}

QSize LayerToggle::minimumSizeHint() const {
    return QSize(120, 28);
}

void LayerToggle::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor textColor = isEnabled() ? QColor(QStringLiteral("#222222"))
                                          : QColor(QStringLiteral("#8A8A8A"));
    const QColor rowFill = hovered_ && isEnabled() ? QColor(QStringLiteral("#E5F1FB"))
                                                    : QColor(Qt::transparent);
    if (rowFill.alpha() > 0) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(rowFill);
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 2, 2);
    }

    const QRect swatchRect(7, (height() - 10) / 2, 10, 10);
    painter.setPen(QPen(isEnabled() ? accentColor_.darker(125) : QColor(QStringLiteral("#A0A0A0")), 1));
    painter.setBrush(isEnabled() ? accentColor_ : QColor(QStringLiteral("#C8C8C8")));
    painter.drawRect(swatchRect);

    const int switchWidth = 36;
    const int switchHeight = 18;
    const QRectF switchRect(width() - switchWidth - 7, (height() - switchHeight) / 2.0,
                            switchWidth, switchHeight);
    QColor track = isChecked() ? accentColor_ : QColor(QStringLiteral("#8A8A8A"));
    if (!isEnabled()) {
        track = QColor(QStringLiteral("#C5C5C5"));
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(track);
    painter.drawRoundedRect(switchRect, switchHeight / 2.0, switchHeight / 2.0);

    const qreal knobSize = 14.0;
    const qreal visualKnobPosition = animation_ && animation_->state() == QAbstractAnimation::Running
        ? knobPosition_
        : (isChecked() ? 1.0 : 0.0);
    const qreal knobX = switchRect.left() + 2.0 + visualKnobPosition * (switchWidth - knobSize - 4.0);
    painter.setBrush(QColor(QStringLiteral("#FFFFFF")));
    painter.drawEllipse(QRectF(knobX, switchRect.top() + 2.0, knobSize, knobSize));

    painter.setPen(textColor);
    const QRect textRect(24, 0, qMax(0, width() - switchWidth - 38), height());
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text());

    if (hasFocus()) {
        QStyleOptionFocusRect option;
        option.initFrom(this);
        option.rect = rect().adjusted(1, 1, -2, -2);
        option.backgroundColor = palette().window().color();
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &painter, this);
    }
}

void LayerToggle::enterEvent(QEvent* event) {
    hovered_ = true;
    update();
    QAbstractButton::enterEvent(event);
}

void LayerToggle::leaveEvent(QEvent* event) {
    hovered_ = false;
    update();
    QAbstractButton::leaveEvent(event);
}

}  // namespace fpt_ui
