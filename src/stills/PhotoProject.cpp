#include "stills/PhotoProject.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <cmath>

namespace {

constexpr const char* kKeyApp = "app";
constexpr const char* kKeyFormat = "formato";
constexpr const char* kKeySavedAt = "guardado";
constexpr const char* kKeyPhotos = "fotos";
constexpr const char* kKeyVideo = "video";

constexpr const char* kAppId = "AstroTracker";

} // namespace

int PhotoProjectPhotos::indexOfResult(const QString& name) const
{
    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].file.compare(name, Qt::CaseInsensitive) == 0)
            return static_cast<int>(i);
    }
    return -1;
}

namespace photo_project {

namespace {

// Lee un objeto "ajuste" del JSON. El WB se guarda como desplazamiento relativo
// ("wbOffset", -100..100); los proyectos v0.5.0 usaban "wbK" (Kelvin absoluto
// con 11250 como neutro) y se migran aproximando el mismo efecto.
ImageAdjust decodeAdjust(const QJsonObject& aj)
{
    ImageAdjust a;
    if (aj.contains(QLatin1String("wbOffset"))) {
        a.wbWarmth = qBound(ImageAdjustLimits::kMinWarmth,
                            static_cast<int>(aj.value(QLatin1String("wbOffset")).toInt(0)),
                            ImageAdjustLimits::kMaxWarmth);
    } else if (aj.contains(QLatin1String("wbK"))) {
        constexpr int kLegacyNeutral = 11250;
        const int kelvin = static_cast<int>(aj.value(QLatin1String("wbK")).toInt(0));
        const int migrated = qRound(static_cast<double>(kelvin - kLegacyNeutral) / 40.0);
        a.wbWarmth = qBound(ImageAdjustLimits::kMinWarmth, migrated,
                            ImageAdjustLimits::kMaxWarmth);
    }
    a.lpSodium = static_cast<int>(aj.value(QLatin1String("lpSodio")).toInt(0));
    a.lpMercury = static_cast<int>(aj.value(QLatin1String("lpMercurio")).toInt(0));
    a.exposureEv = static_cast<int>(aj.value(QLatin1String("ev")).toInt(0));
    a.brightness = static_cast<int>(aj.value(QLatin1String("brillo")).toInt(0));
    a.contrast = static_cast<int>(aj.value(QLatin1String("contraste")).toInt(0));
    a.denoise = static_cast<int>(aj.value(QLatin1String("ruido")).toInt(0));
    return a;
}

QJsonObject encodePhotos(const PhotoProjectPhotos& p)
{
    QJsonObject o;
    o.insert(QStringLiteral("activo"), p.active);
    if (!p.active)
        return o;

    o.insert(QStringLiteral("origenTipo"), p.originType);
    if (p.originType == 1) {
        QJsonArray files;
        for (const QString& f : p.originFiles)
            files.append(f);
        o.insert(QStringLiteral("origenArchivos"), files);
    } else {
        o.insert(QStringLiteral("origenCarpeta"), p.originFolder);
    }
    o.insert(QStringLiteral("maxDim"), p.analysisMaxDim);
    o.insert(QStringLiteral("actual"), p.currentIndex);
    o.insert(QStringLiteral("perfil"), QLatin1String(objectProfileKey(p.profile)));
    if (!p.overrides.empty()) {
        QJsonArray overrides;
        for (const PhotoProjectOverride& ov : p.overrides) {
            QJsonObject e;
            e.insert(QStringLiteral("indice"), ov.index);
            e.insert(QStringLiteral("metodo"), QLatin1String(discMethodKey(ov.method)));
            overrides.append(e);
        }
        o.insert(QStringLiteral("overrides"), overrides);
    }

    if (p.hasSeed) {
        QJsonObject s;
        s.insert(QStringLiteral("indice"), p.seedIndex);
        s.insert(QStringLiteral("x"), static_cast<double>(p.seedX));
        s.insert(QStringLiteral("y"), static_cast<double>(p.seedY));
        s.insert(QStringLiteral("radio"), static_cast<double>(p.seedRadius));
        o.insert(QStringLiteral("semilla"), s);
    }
    if (!p.adjust.isDefault()) {
        QJsonObject aj;
        aj.insert(QStringLiteral("wbOffset"), p.adjust.wbWarmth);
        aj.insert(QStringLiteral("lpSodio"), p.adjust.lpSodium);
        aj.insert(QStringLiteral("lpMercurio"), p.adjust.lpMercury);
        aj.insert(QStringLiteral("ev"), p.adjust.exposureEv);
        aj.insert(QStringLiteral("brillo"), p.adjust.brightness);
        aj.insert(QStringLiteral("contraste"), p.adjust.contrast);
        aj.insert(QStringLiteral("ruido"), p.adjust.denoise);
        o.insert(QStringLiteral("ajuste"), aj);
    }
    if (!p.adjustOverrides.empty()) {
        QJsonArray overrides;
        for (const auto& [idx, adj] : p.adjustOverrides) {
            QJsonObject e;
            e.insert(QStringLiteral("indice"), idx);
            QJsonObject aj;
            aj.insert(QStringLiteral("wbOffset"), adj.wbWarmth);
            aj.insert(QStringLiteral("lpSodio"), adj.lpSodium);
            aj.insert(QStringLiteral("lpMercurio"), adj.lpMercury);
            aj.insert(QStringLiteral("ev"), adj.exposureEv);
            aj.insert(QStringLiteral("brillo"), adj.brightness);
            aj.insert(QStringLiteral("contraste"), adj.contrast);
            aj.insert(QStringLiteral("ruido"), adj.denoise);
            e.insert(QStringLiteral("ajuste"), aj);
            overrides.append(e);
        }
        o.insert(QStringLiteral("ajustesFotos"), overrides);
    }

    QJsonArray results;
    for (const PhotoProjectPhotoResult& r : p.results) {
        QJsonObject e;
        e.insert(QStringLiteral("archivo"), r.file);
        e.insert(QStringLiteral("x"), static_cast<double>(r.x));
        e.insert(QStringLiteral("y"), static_cast<double>(r.y));
        e.insert(QStringLiteral("radio"), static_cast<double>(r.radius));
        e.insert(QStringLiteral("estado"), r.status);
        e.insert(QStringLiteral("supuesta"), r.predicted);
        e.insert(QStringLiteral("bloqueada"), r.locked);
        e.insert(QStringLiteral("fijada"), r.manualFixed);
        e.insert(QStringLiteral("exportar"), r.exportSelected);
        e.insert(QStringLiteral("confianza"), static_cast<double>(r.confidence));
        e.insert(QStringLiteral("metodo"), QLatin1String(discMethodKey(r.method)));
        results.append(e);
    }
    o.insert(QStringLiteral("resultados"), results);
    return o;
}

QJsonObject encodeVideo(const PhotoProjectVideo& v)
{
    QJsonObject o;
    o.insert(QStringLiteral("activo"), v.active);
    if (!v.active)
        return o;

    o.insert(QStringLiteral("ruta"), v.path);
    if (v.hasRoi) {
        QJsonArray roi;
        roi.append(v.roiX);
        roi.append(v.roiY);
        roi.append(v.roiW);
        roi.append(v.roiH);
        o.insert(QStringLiteral("roi"), roi);
    }
    o.insert(QStringLiteral("inicioUs"), QString::number(v.startUs));
    o.insert(QStringLiteral("posicionUs"), QString::number(v.positionUs));
    o.insert(QStringLiteral("tracker"), v.tracker);
    o.insert(QStringLiteral("suavizado"), v.smoothingAlpha);
    o.insert(QStringLiteral("borde"), v.borderMode);
    if (!v.adjust.isDefault()) {
        QJsonObject aj;
        aj.insert(QStringLiteral("wbOffset"), v.adjust.wbWarmth);
        aj.insert(QStringLiteral("lpSodio"), v.adjust.lpSodium);
        aj.insert(QStringLiteral("lpMercurio"), v.adjust.lpMercury);
        aj.insert(QStringLiteral("ev"), v.adjust.exposureEv);
        aj.insert(QStringLiteral("brillo"), v.adjust.brightness);
        aj.insert(QStringLiteral("contraste"), v.adjust.contrast);
        aj.insert(QStringLiteral("ruido"), v.adjust.denoise);
        o.insert(QStringLiteral("ajuste"), aj);
    }
    return o;
}

int readInt(const QJsonValue& v, int def = 0)
{
    return v.isDouble() ? static_cast<int>(v.toDouble()) : def;
}

double readDouble(const QJsonValue& v, double def = 0.0)
{
    return v.isDouble() ? v.toDouble() : def;
}

bool readBool(const QJsonValue& v, bool def = false)
{
    return v.isBool() ? v.toBool() : def;
}

QString readString(const QJsonValue& v, const QString& def = {})
{
    return v.isString() ? v.toString() : def;
}

void decodePhotos(const QJsonObject& o, PhotoProjectPhotos& p)
{
    p = PhotoProjectPhotos{};
    p.active = readBool(o.value(QLatin1String("activo")));
    if (!p.active)
        return;

    p.originType = readInt(o.value(QLatin1String("origenTipo")), 0);
    p.originFolder = readString(o.value(QLatin1String("origenCarpeta")));
    const auto files = o.value(QLatin1String("origenArchivos")).toArray();
    for (const auto& f : files)
        p.originFiles.push_back(f.toString());
    p.analysisMaxDim = readInt(o.value(QLatin1String("maxDim")), 1600);
    p.currentIndex = readInt(o.value(QLatin1String("actual")), 0);
    if (!objectProfileFromKey(
            o.value(QLatin1String("perfil")).toString().toLatin1().constData(),
            p.profile))
        p.profile = ObjectProfile::Auto;
    const auto overrides = o.value(QLatin1String("overrides")).toArray();
    for (const auto& item : overrides) {
        const QJsonObject e = item.toObject();
        PhotoProjectOverride ov;
        ov.index = readInt(e.value(QLatin1String("indice")), -1);
        if (!discMethodFromKey(
                e.value(QLatin1String("metodo")).toString().toLatin1().constData(),
                ov.method))
            continue;
        p.overrides.push_back(ov);
    }

    const QJsonObject seed = o.value(QLatin1String("semilla")).toObject();
    p.hasSeed = !seed.isEmpty();
    if (p.hasSeed) {
        p.seedIndex = readInt(seed.value(QLatin1String("indice")), 0);
        p.seedX = static_cast<float>(readDouble(seed.value(QLatin1String("x")), 0.0));
        p.seedY = static_cast<float>(readDouble(seed.value(QLatin1String("y")), 0.0));
        p.seedRadius = static_cast<float>(readDouble(seed.value(QLatin1String("radio")), 0.0));
    }

    // Ajuste de imagen (v0.5.0+): "ajuste" objeto. Si no existe, el
    // "wbCalor" pre-0.5.0 se ignora (valor obsoleto): ajuste neutro.
    const QJsonObject aj = o.value(QLatin1String("ajuste")).toObject();
    if (!aj.isEmpty())
        p.adjust = decodeAdjust(aj);

    // Overrides por foto: "ajustesFotos"
    const auto ajOverrides = o.value(QLatin1String("ajustesFotos")).toArray();
    for (const auto& item : ajOverrides) {
        const QJsonObject e = item.toObject();
        const int idx = readInt(e.value(QLatin1String("indice")), -1);
        const QJsonObject adjObj = e.value(QLatin1String("ajuste")).toObject();
        if (idx < 0 || adjObj.isEmpty())
            continue;
        p.adjustOverrides[idx] = decodeAdjust(adjObj);
    }

    const auto results = o.value(QLatin1String("resultados")).toArray();
    p.results.reserve(static_cast<size_t>(results.size()));
    for (const auto& item : results) {
        const QJsonObject e = item.toObject();
        PhotoProjectPhotoResult r;
        r.file = readString(e.value(QLatin1String("archivo")));
        r.x = static_cast<float>(readDouble(e.value(QLatin1String("x"))));
        r.y = static_cast<float>(readDouble(e.value(QLatin1String("y"))));
        r.radius = static_cast<float>(readDouble(e.value(QLatin1String("radio"))));
        r.status = readInt(e.value(QLatin1String("estado")), 2);
        r.predicted = readBool(e.value(QLatin1String("supuesta")));
        r.locked = readBool(e.value(QLatin1String("bloqueada")));
        r.manualFixed = readBool(e.value(QLatin1String("fijada")));
        r.exportSelected = readBool(e.value(QLatin1String("exportar")), true);
        r.confidence = static_cast<float>(
            readDouble(e.value(QLatin1String("confianza")), 0.0));
        if (!discMethodFromKey(
                e.value(QLatin1String("metodo")).toString().toLatin1().constData(),
                r.method))
            r.method = DiscMethod::Prediction;
        p.results.push_back(r);
    }
}

void decodeVideo(const QJsonObject& o, PhotoProjectVideo& v)
{
    v = PhotoProjectVideo{};
    v.active = readBool(o.value(QLatin1String("activo")));
    if (!v.active)
        return;

    v.path = readString(o.value(QLatin1String("ruta")));
    const auto roi = o.value(QLatin1String("roi")).toArray();
    if (roi.size() == 4) {
        v.hasRoi = true;
        v.roiX = readInt(roi.at(0));
        v.roiY = readInt(roi.at(1));
        v.roiW = readInt(roi.at(2));
        v.roiH = readInt(roi.at(3));
    }
    v.startUs = o.value(QLatin1String("inicioUs")).toString().toLongLong();
    v.positionUs = o.value(QLatin1String("posicionUs")).toString().toLongLong();
    v.tracker = readInt(o.value(QLatin1String("tracker")));
    v.smoothingAlpha = readDouble(o.value(QLatin1String("suavizado")), 0.3);
    v.borderMode = readInt(o.value(QLatin1String("borde")));

    // Ajuste de imagen (v0.5.0+): "ajuste" objeto.
    const QJsonObject aj = o.value(QLatin1String("ajuste")).toObject();
    if (!aj.isEmpty())
        v.adjust = decodeAdjust(aj);
}

} // namespace

QByteArray encode(const PhotoProject& project)
{
    QJsonObject root;
    root.insert(QLatin1String(kKeyApp), QLatin1String(kAppId));
    root.insert(QLatin1String(kKeyFormat), project.format);
    root.insert(QLatin1String(kKeySavedAt), project.savedAt);
    root.insert(QLatin1String(kKeyPhotos), encodePhotos(project.photos));
    root.insert(QLatin1String(kKeyVideo), encodeVideo(project.video));

    const QJsonDocument doc(root);
    return doc.toJson(QJsonDocument::Indented);
}

bool decode(const QByteArray& json, PhotoProject& out)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject())
        return false;

    const QJsonObject root = doc.object();
    if (!root.contains(QLatin1String(kKeyFormat)) ||
        !root.value(QLatin1String(kKeyFormat)).isDouble())
        return false;

    const int format = static_cast<int>(root.value(QLatin1String(kKeyFormat)).toDouble());
    if (format <= 0 || format > PhotoProject::kFormat)
        return false;

    out = PhotoProject{};
    out.format = format;
    out.savedAt = readString(root.value(QLatin1String(kKeySavedAt)));
    decodePhotos(root.value(QLatin1String(kKeyPhotos)).toObject(), out.photos);
    decodeVideo(root.value(QLatin1String(kKeyVideo)).toObject(), out.video);
    return true;
}

} // namespace photo_project
