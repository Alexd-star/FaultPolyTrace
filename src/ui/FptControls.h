#pragma once

#include <QAbstractButton>
#include <QColor>

class QPropertyAnimation;

namespace fpt_ui {

class LayerToggle final : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(qreal knobPosition READ knobPosition WRITE setKnobPosition)

public:
    explicit LayerToggle(QWidget* parent = nullptr);

    qreal knobPosition() const;
    void setKnobPosition(qreal position);
    void setAccentColor(const QColor& color);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    qreal knobPosition_ = 0.0;
    QColor accentColor_ = QColor(QStringLiteral("#0078D7"));
    QPropertyAnimation* animation_ = nullptr;
    bool hovered_ = false;
};

}  // namespace fpt_ui
