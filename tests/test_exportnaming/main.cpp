#include "common/ExportNaming.h"

#include <QDateTime>
#include <cstdio>

namespace {

int failures = 0;

void expect(bool cond, const char* msg)
{
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        ++failures;
    }
}

} // namespace

int main()
{
    const QDateTime when(QDate(2026, 10, 3), QTime(20, 45));

    // 1. Timestamp: ordenable y con separador legible.
    expect(export_naming::timestampPrefix(when) == QStringLiteral("20261003_2045_"),
           "timestamp con formato yyyyMMdd_HHmm_");

    // 2. Carpeta del origen: de un archivo y de una carpeta.
    expect(export_naming::sourceFolder(QStringLiteral("C:/fotos/luna.mp4"), false)
               == QStringLiteral("C:/fotos"),
           "sourceFolder de archivo = directorio");
    expect(export_naming::sourceFolder(QStringLiteral("C:/fotos/sesion"), true)
               == QStringLiteral("C:/fotos/sesion"),
           "sourceFolder de carpeta = la propia carpeta");

    // 3. Base: se quita la extensión y se sanean los caracteres inválidos.
    expect(export_naming::baseName(QStringLiteral("C:/fotos/luna.mp4"), false)
               == QStringLiteral("luna"),
           "base de archivo sin extensión");
    expect(export_naming::baseName(QStringLiteral("C:/fotos/sesion luna"), true)
               == QStringLiteral("sesion_luna"),
           "base de carpeta con espacios");
    // Solo se usa el último componente: "sesion/eclipse 02" -> "eclipse_02".
    expect(export_naming::baseName(QStringLiteral("C:/fotos/sesion/eclipse 02"), true)
               == QStringLiteral("eclipse_02"),
           "base de carpeta usa solo el nombre, no toda la ruta");
    expect(export_naming::baseName(QStringLiteral("C:/fotos/luna.NEF"), false)
               == QStringLiteral("luna"),
           "base ignora mayúsculas de la extensión");

    // 4. Casos límite: nunca devolver un nombre vacío o solo guiones.
    expect(export_naming::baseName(QStringLiteral(""), false)
               == QStringLiteral("astrotracker"),
           "origen vacío -> nombre por defecto");
    expect(export_naming::baseName(QStringLiteral("C:/fotos/***"), false)
               == QStringLiteral("astrotracker"),
           "nombre sin caracteres válidos -> nombre por defecto");

    // 5. Prefijo completo: timestamp + base, con el mismo timestamp para todos
    //    los archivos de una misma exportación.
    expect(export_naming::prefix(QStringLiteral("C:/fotos/luna.mp4"), false, when)
               == QStringLiteral("20261003_2045_luna"),
           "prefijo = timestamp + base del origen");
    // Nombre muy largo: se recorta para no pasarse del límite de rutas.
    const QString huge = QStringLiteral("C:/fotos/") + QString(200, QLatin1Char('a'));
    expect(export_naming::baseName(huge, false).size() <= 60,
           "base muy largo se recorta");

    if (failures == 0)
        std::printf("test_exportnaming: OK\n");
    else
        std::printf("test_exportnaming: %d fallos\n", failures);
    return failures == 0 ? 0 : 1;
}