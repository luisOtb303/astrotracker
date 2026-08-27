#include "ui/theme/ThemeManager.h"

#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QStyleFactory>

ThemeManager& ThemeManager::instance()
{
    static ThemeManager mgr;
    return mgr;
}

void ThemeManager::setTheme(AppTheme theme)
{
    if (currentTheme_ == theme && !firstApply_)
        return;
    firstApply_ = false;
    currentTheme_ = theme;
    applyTheme();
    emit themeChanged();
}

bool ThemeManager::isDark() const
{
    if (currentTheme_ == AppTheme::System) {
        const QPalette pal = QApplication::palette();
        return pal.color(QPalette::Window).lightness() < 128;
    }
    return currentTheme_ == AppTheme::Dark;
}

QString ThemeManager::themeName(AppTheme t)
{
    switch (t) {
    case AppTheme::Dark:   return QStringLiteral("Dark");
    case AppTheme::Light:  return QStringLiteral("Light");
    case AppTheme::System: return QStringLiteral("System");
    }
    return {};
}

void ThemeManager::applyTheme()
{
    // Reset to Fusion base style for consistent theming across platforms.
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette pal;
    if (isDark()) {
        pal.setColor(QPalette::Window,          QColor(32, 32, 32));
        pal.setColor(QPalette::WindowText,      QColor(255, 255, 255));
        pal.setColor(QPalette::Base,            QColor(40, 40, 40));
        pal.setColor(QPalette::AlternateBase,   QColor(53, 53, 53));
        pal.setColor(QPalette::ToolTipBase,     QColor(53, 53, 53));
        pal.setColor(QPalette::ToolTipText,     QColor(255, 255, 255));
        pal.setColor(QPalette::Text,            QColor(255, 255, 255));
        pal.setColor(QPalette::Button,          QColor(53, 53, 53));
        pal.setColor(QPalette::ButtonText,      QColor(255, 255, 255));
        pal.setColor(QPalette::BrightText,      QColor(255, 0, 0));
        pal.setColor(QPalette::Link,            QColor(96, 205, 255));
        pal.setColor(QPalette::Highlight,       QColor(0, 120, 212));
        pal.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
        pal.setColor(QPalette::Disabled, QPalette::Text,       QColor(127, 127, 127));
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
    } else {
        pal.setColor(QPalette::Window,          QColor(243, 243, 243));
        pal.setColor(QPalette::WindowText,      QColor(26, 26, 26));
        pal.setColor(QPalette::Base,            QColor(255, 255, 255));
        pal.setColor(QPalette::AlternateBase,   QColor(233, 233, 233));
        pal.setColor(QPalette::ToolTipBase,     QColor(255, 255, 220));
        pal.setColor(QPalette::ToolTipText,     QColor(26, 26, 26));
        pal.setColor(QPalette::Text,            QColor(26, 26, 26));
        pal.setColor(QPalette::Button,          QColor(240, 240, 240));
        pal.setColor(QPalette::ButtonText,      QColor(26, 26, 26));
        pal.setColor(QPalette::BrightText,      QColor(255, 0, 0));
        pal.setColor(QPalette::Link,            QColor(0, 120, 212));
        pal.setColor(QPalette::Highlight,       QColor(0, 120, 212));
        pal.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
        pal.setColor(QPalette::Disabled, QPalette::Text,       QColor(160, 160, 160));
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(160, 160, 160));
    }

    QApplication::setPalette(pal);

    // Load supplemental QSS for widgets that need fine-tuning beyond palette.
    if (isDark()) {
        const QString qss = loadQss(QStringLiteral(":/styles/dark.qss"));
        if (!qss.isEmpty())
            qApp->setStyleSheet(qss);
    } else {
        qApp->setStyleSheet({});
    }
}

QString ThemeManager::loadQss(const QString& resourcePath) const
{
    QFile f(resourcePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readAll());
}
