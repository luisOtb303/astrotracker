#pragma once

#include <QObject>
#include <QString>

enum class AppTheme { Dark, Light, System };

class ThemeManager : public QObject
{
    Q_OBJECT
public:
    static ThemeManager& instance();

    void setTheme(AppTheme theme);
    AppTheme currentTheme() const { return currentTheme_; }
    bool isDark() const;

    static QString themeName(AppTheme t);

signals:
    void themeChanged();

private:
    ThemeManager() = default;
    void applyTheme();
    QString loadQss(const QString& resourcePath) const;

    AppTheme currentTheme_ = AppTheme::Dark;
    bool firstApply_ = true;
};
