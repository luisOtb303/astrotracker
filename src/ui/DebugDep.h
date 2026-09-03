#pragma once

// Depuración temporal: escribe líneas a %TEMP%/at_dep.log para poder revisar
// el flujo aunque el tema del dock "Salida" haga ilegible el texto.
#include <QString>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QMutex>

inline void depLog(const QString& line)
{
    static QMutex mtx;
    QMutexLocker lock(&mtx);
    const QString p = QDir::temp().filePath(QStringLiteral("at_dep.log"));
    QFile f(p);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"))
           << QStringLiteral("  ") << line << QChar::LineFeed;
    }
}
