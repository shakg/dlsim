// Small key/value store for UI state (panel open, sizes, ...), backed by
// QSettings under "ui/". Qt 6.4 has no QtCore.Settings QML type.
#pragma once

#include <QFontInfo>
#include <QGuiApplication>
#include <QObject>
#include <QSettings>
#include <QVariant>

class UiSettings : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE QVariant value(const QString &key, const QVariant &fallback = {}) const
    {
        return QSettings().value("ui/" + key, fallback);
    }
    Q_INVOKABLE bool flag(const QString &key, bool fallback = false) const
    {
        return QSettings().value("ui/" + key, fallback).toBool();
    }
    // The desktop's UI font size in pixels.
    Q_INVOKABLE int systemFontPx() const { return QFontInfo(QGuiApplication::font()).pixelSize(); }
    Q_INVOKABLE void setValue(const QString &key, const QVariant &v) { QSettings().setValue("ui/" + key, v); }
};
