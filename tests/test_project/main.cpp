#include "stills/PhotoProject.h"

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

bool sameFloat(float a, float b)
{
    const float eps = 1e-4f;
    return (a - b) < eps && (b - a) < eps;
}

PhotoProject makeSample()
{
    PhotoProject p;
    p.savedAt = QStringLiteral("2026-08-21T10:30:00");

    p.photos.active = true;
    p.photos.originType = 1;
    p.photos.originFiles = {QStringLiteral("D:/fotos/IMG_0001.CR2"),
                            QStringLiteral("D:/fotos/IMG_0002.CR2"),
                            QStringLiteral("D:/fotos/IMG_0003.CR2")};
    p.photos.analysisMaxDim = 1600;
    p.photos.currentIndex = 1;
    p.photos.hasSeed = true;
    p.photos.seedIndex = 2;
    p.photos.seedX = 800.5f;
    p.photos.seedY = 601.25f;
    p.photos.seedRadius = 120.f;

    // Resultado disperso: solo las fotos con círculo o alguna marca.
    PhotoProjectPhotoResult r1;
    r1.file = QStringLiteral("IMG_0002.CR2");
    r1.x = 795.f;
    r1.y = 600.f;
    r1.radius = 119.5f;
    r1.status = 0;
    r1.predicted = false;
    PhotoProjectPhotoResult r2;
    r2.file = QStringLiteral("IMG_0003.CR2");
    r2.x = 810.75f;
    r2.y = 598.25f;
    r2.radius = 120.25f;
    r2.status = 1;
    r2.predicted = true;
    r2.locked = true;
    r2.exportSelected = false;
    PhotoProjectPhotoResult r3;
    r3.file = QStringLiteral("IMG_0009.CR2"); // ya no existe en la secuencia
    r3.x = 900.f;
    r3.y = 700.f;
    r3.radius = 100.f;
    r3.manualFixed = true;
    p.photos.results = {r1, r2, r3};

    p.video.active = true;
    p.video.path = QStringLiteral("D:/videos/luna.mp4");
    p.video.hasRoi = true;
    p.video.roiX = 10;
    p.video.roiY = 20;
    p.video.roiW = 300;
    p.video.roiH = 200;
    p.video.startUs = 1234567;
    p.video.positionUs = 987654321;
    p.video.tracker = 1;
    p.video.smoothingAlpha = 0.42;
    p.video.borderMode = 1;
    return p;
}

} // namespace

int main()
{
    // 1. Ida y vuelta completa.
    const PhotoProject src = makeSample();
    const QByteArray json = photo_project::encode(src);
    PhotoProject dst;
    expect(photo_project::decode(json, dst), "decode del proyecto de muestra");

    expect(dst.format == PhotoProject::kFormat, "formato preservado");
    expect(dst.savedAt == src.savedAt, "fecha preservada");

    expect(dst.photos.active, "seccion fotos activa");
    expect(dst.photos.originType == 1, "origen tipo archivos");
    expect(dst.photos.originFiles.size() == 3 &&
               dst.photos.originFiles[1] == QStringLiteral("D:/fotos/IMG_0002.CR2"),
           "lista de archivos preservada");
    expect(dst.photos.analysisMaxDim == 1600, "maxDim preservado");
    expect(dst.photos.currentIndex == 1, "indice actual preservado");
    expect(dst.photos.hasSeed && dst.photos.seedIndex == 2, "semilla preservada");
    expect(sameFloat(dst.photos.seedX, 800.5f) &&
               sameFloat(dst.photos.seedY, 601.25f) &&
               sameFloat(dst.photos.seedRadius, 120.f),
           "circulo de la semilla preservado");
    expect(dst.photos.results.size() == 3, "resultados dispersos preservados");

    const int i2 = dst.photos.indexOfResult(QStringLiteral("IMG_0002.CR2"));
    expect(i2 >= 0, "resultado IMG_0002 encontrado");
    if (i2 >= 0) {
        const PhotoProjectPhotoResult& r = dst.photos.results[size_t(i2)];
        expect(sameFloat(r.x, 795.f) && sameFloat(r.y, 600.f) &&
                   sameFloat(r.radius, 119.5f) && r.status == 0 && !r.predicted,
               "valores del resultado IMG_0002");
    }
    // Coincidencia por nombre insensible a mayusculas.
    expect(dst.photos.indexOfResult(QStringLiteral("img_0003.cr2")) >= 0,
           "coincidencia sin mayusculas");
    expect(dst.photos.indexOfResult(QStringLiteral("NO_EXISTE.CR2")) < 0,
           "archivo ausente no encontrado");

    const int i3 = dst.photos.indexOfResult(QStringLiteral("IMG_0003.CR2"));
    if (i3 >= 0) {
        const PhotoProjectPhotoResult& r = dst.photos.results[size_t(i3)];
        expect(r.predicted && r.locked && !r.exportSelected,
               "marcas de la foto bloqueada y excluida");
    } else {
        expect(false, "resultado IMG_0003 encontrado");
    }

    expect(dst.video.active, "seccion video activa");
    expect(dst.video.path == QStringLiteral("D:/videos/luna.mp4"), "ruta de video");
    expect(dst.video.hasRoi && dst.video.roiX == 10 && dst.video.roiW == 300,
           "roi del video");
    expect(dst.video.startUs == 1234567 && dst.video.positionUs == 987654321,
           "posiciones temporales del video");
    expect(dst.video.tracker == 1 && dst.video.borderMode == 1, "ajustes del pipeline");
    expect(dst.video.smoothingAlpha > 0.41 && dst.video.smoothingAlpha < 0.43,
           "suavizado preservado");

    // 2. Proyecto con secciones inactivas.
    PhotoProject empty;
    const QByteArray emptyJson = photo_project::encode(empty);
    PhotoProject emptyOut;
    expect(photo_project::decode(emptyJson, emptyOut), "decode de proyecto vacio");
    expect(!emptyOut.photos.active && !emptyOut.video.active,
           "proyecto vacio sin secciones activas");

    // 3. Rechazo de documentos invalidos.
    PhotoProject junk;
    expect(!photo_project::decode(QByteArray("{ no es json"), junk),
           "json corrupto rechazado");
    expect(!photo_project::decode(QByteArray("[]"), junk), "raiz no objeto rechazada");
    expect(!photo_project::decode(
               QByteArray("{\"app\":\"AstroTracker\",\"guardado\":\"x\"}"), junk),
           "documento sin formato rechazado");
    expect(!photo_project::decode(
               QByteArray("{\"app\":\"AstroTracker\",\"formato\":99}"), junk),
           "formato posterior rechazado");

    if (failures == 0)
        std::printf("test_project: OK\n");
    else
        std::printf("test_project: %d fallos\n", failures);
    return failures == 0 ? 0 : 1;
}
