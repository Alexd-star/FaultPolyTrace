#pragma once

#include <QString>
#include <QStringList>

class QObject;
class QComboBox;

namespace fpt_ui {

QString normalizeLanguageCode(const QString& code);
QString translate(const QString& zh, const QString& languageCode);
QString canonical(const QString& text);
void captureTranslatableText(QObject* root);
void applyTranslatableText(QObject* root, const QString& languageCode);
void setComboItems(QComboBox* combo, const QStringList& keys, const QString& languageCode);
void refreshComboItems(QComboBox* combo, const QString& languageCode);
QString comboCurrentKey(const QComboBox* combo);
void setComboByKey(QComboBox* combo, const QString& key);

} // namespace fpt_ui
