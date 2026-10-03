#pragma once

#include <QDateTime>
#include <QString>

namespace export_naming {

// Nombre por defecto para las exportaciones: timestamp + nombre del origen.
// Ej.: "20261003_2045_luna" (vídeo) o "20261003_2045_luna_0001.jpg" (fotos).
// El timestamp evita sobrescribir exportaciones anteriores del mismo archivo,
// y el nombre del origen deja claro qué se exportó cuando hay varias sesiones.
QString timestampPrefix(const QDateTime& when = QDateTime::currentDateTime());

// Carpeta que contiene el archivo de origen (vídeo o primera foto de la
// secuencia). Vacía si no se puede determinar.
QString sourceFolder(const QString& sourcePathOrFolder, bool isFolder);

// Base saneado a partir del nombre de origen: se quitan la extensión y los
// caracteres no válidos en Windows, para poder usarlo como nombre de archivo.
QString baseName(const QString& sourcePathOrFolder, bool isFolder);

// Prefijo completo (timestamp + base) que se antepone a los archivos.
QString prefix(const QString& sourcePathOrFolder, bool isFolder,
               const QDateTime& when = QDateTime::currentDateTime());

} // namespace export_naming