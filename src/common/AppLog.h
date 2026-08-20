#pragma once

#include <QObject>
#include <QString>

// Log global de la aplicación: un pequeño singleton QObject que permite a
// cualquier hilo (UI o workers) registrar líneas de salida ("procesando X",
// "centrando", avisos, errores, debug). Las conexiones entre hilos se entregan
// de forma segura con queued connections al panel de "Salida".
class AppLog : public QObject
{
    Q_OBJECT

public:
    enum Level {
        Info = 0,
        Warn = 1,
        Error = 2,
        Debug = 3,
    };

    static AppLog& instance()
    {
        static AppLog s;
        return s;
    }

    static void info(const QString& msg) { instance().post(Info, msg); }
    static void warn(const QString& msg) { instance().post(Warn, msg); }
    static void error(const QString& msg) { instance().post(Error, msg); }
    static void debug(const QString& msg) { instance().post(Debug, msg); }

signals:
    // level usa los valores de AppLog::Level (int para conexiones queued sin
    // registrar metatipo).
    void message(int level, const QString& text);

private:
    AppLog() = default;
    ~AppLog() override = default;

    void post(Level level, const QString& msg) { emit message(static_cast<int>(level), msg); }
};