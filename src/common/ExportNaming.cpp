#include "common/ExportNaming.h"

#include <QFileInfo>

namespace export_naming {
namespace {

// Sustituye por guion cualquier carácter no válido en nombres de archivo de
// Windows, y recorta guiones sobrantes en los extremos.
QString sanitize(const QString& in)
{
    QString out;
    out.reserve(in.size());
    for (const QChar& c : in) {
        const bool ok = c.isLetterOrNumber() || c == QLatin1Char('_')
                        || c == QLatin1Char('-');
        out.append(ok ? c : QLatin1Char('_'));
    }
    while (out.startsWith(QLatin1Char('_')))
        out.remove(0, 1);
    while (out.endsWith(QLatin1Char('_')))
        out.chop(1);
    if (out.size() > 60)
        out.truncate(60);
    return out;
}

} // namespace

QString timestampPrefix(const QDateTime& when)
{
    return when.toString(QStringLiteral("yyyyMMdd_HHmm_"));
}

QString sourceFolder(const QString& sourcePathOrFolder, bool isFolder)
{
    if (sourcePathOrFolder.isEmpty())
        return QString();
    const QFileInfo fi(sourcePathOrFolder);
    if (isFolder)
        return fi.absoluteFilePath();
    return fi.absolutePath();
}

QString baseName(const QString& sourcePathOrFolder, bool isFolder)
{
    if (sourcePathOrFolder.isEmpty())
        return QStringLiteral("astrotracker");
    QFileInfo fi(sourcePathOrFolder);
    const QString raw = isFolder ? fi.fileName() : fi.completeBaseName();
    const QString clean = sanitize(raw);
    return clean.isEmpty() ? QStringLiteral("astrotracker") : clean;
}

QString prefix(const QString& sourcePathOrFolder, bool isFolder, const QDateTime& when)
{
    return timestampPrefix(when) + baseName(sourcePathOrFolder, isFolder);
}

} // namespace export_naming