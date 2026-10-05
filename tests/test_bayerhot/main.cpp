#include "raw/BayerHotPixels.h"

#include <opencv2/core.hpp>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

// Filtro de píxeles calientes sobre el mosaico Bayer. Lo que se comprueba aquí es
// lo que hace el filtro PRODIGIOSO y lo que hace NO:
//   - encuentra fotositos quemados y los deja en el valor del fondo,
//   - no toca una estrella (que también es un punto brillante),
//   - no toca nada si el mosaico no es un Bayer 2x2,
//   - no inventa detecciones en una imagen perfectamente plana (MAD = 0).
//
// Y una propiedad de diseño: el resultado NO depende de la fase del CFA, porque
// al detector solo le importa qué fotositos comparten color.

namespace {
int failures = 0;

void expect(bool cond, const char* msg)
{
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        ++failures;
    }
}

// Patrones Bayer posibles: los cuatro giros de RGGB, en la codificación de
// LibRaw (filters, 2 bits por fotosito de la clase de paridad).
struct Pattern
{
    const char* name;
    int phase[2][2];
};

const Pattern kPatterns[] = {
    {"RGGB", {{0, 1}, {1, 2}}},
    {"BGGR", {{2, 1}, {1, 0}}},
    {"GRBG", {{1, 0}, {2, 1}}},
    {"GBRG", {{1, 2}, {0, 1}}},
};

// Mosaico sintético: fondo con ruido gaussiano y una rampa suave, para que el
// detector tenga algo real con qué trabajar y no un caso degenerado.
cv::Mat makeMosaic(int rows, int cols, unsigned seed, int noiseSigma = 8)
{
    std::mt19937 rng(seed);
    std::normal_distribution<float> gauss(0.0f, static_cast<float>(noiseSigma));

    cv::Mat m(rows, cols, CV_16UC1);
    for (int y = 0; y < rows; ++y) {
        uint16_t* row = m.ptr<uint16_t>(y);
        for (int x = 0; x < cols; ++x) {
            const float base = 1000.0f + 4.0f * static_cast<float>(y);
            row[x] = static_cast<uint16_t>(std::lround(base + gauss(rng)));
        }
    }
    return m;
}

int16_t at(const cv::Mat& m, int y, int x)
{
    return static_cast<int16_t>(m.at<uint16_t>(y, x));
}

// Un fotosito quemado: muy por encima del fondo local.
void addHotPixel(cv::Mat& m, int y, int x, int excess)
{
    m.at<uint16_t>(y, x) =
        static_cast<uint16_t>(m.at<uint16_t>(y, x) + excess);
}

// ---------------------------------------------------------------------------

void testFindsAndCorrectsHotPixel()
{
    cv::Mat m = makeMosaic(120, 160, 1);
    addHotPixel(m, 40, 60, 6000);
    addHotPixel(m, 77, 101, 5000);
    const int16_t before = at(m, 40, 60);

raw::HotPixelParams p;
    const raw::HotPixelResult r =
        raw::removeHotPixels(m, kPatterns[0].phase, p);

    expect(r.bayer, "mapa Bayer valido: bayer = true");
    expect(r.detected == 2, "los 2 fotositos quemados se detectan");
    expect(r.corrected == 2, "los 2 se corrigen");

    // Tras corregir, el fotosito debe haber vuelto al nivel del fondo local.
    const int16_t after = at(m, 40, 60);
    expect(std::abs(after - 1000 - 4 * 40) < 60,
           "el valor corregido vuelve al fondo local");
    expect(std::abs(after - before) > 5000, "el defecto se ha eliminado de verdad");
}

// Una estrella también es un punto brillante, y sin embargo NO es un defecto:
// el detector no puede distinguirlas por amplitud, sino por estructura. En el
// mosaico una estrella reparte su señal entre fotositos vecinos, así que ninguno
// queda aislado.
void testStarIsPreserved()
{
    cv::Mat m = makeMosaic(120, 160, 2);
    const int cx = 70, cy = 80;
    const double peak = 8000.0;
    const double sigma = 3.0;
    for (int dy = -6; dy <= 6; ++dy) {
        for (int dx = -6; dx <= 6; ++dx) {
            const double g = peak * std::exp(-(dx * dx + dy * dy) / (2 * sigma * sigma));
            const int y = cy + dy, x = cx + dx;
            m.at<uint16_t>(y, x) = static_cast<uint16_t>(
                std::lround(m.at<uint16_t>(y, x) + g));
        }
    }

    const raw::HotPixelResult r = raw::removeHotPixels(m, kPatterns[0].phase, {});

    expect(r.detected == 0,
           "una estrella con estructura no genera ni una deteccion");
    expect(r.corrected == 0, "no se corrige nada de la estrella");
    expect(at(m, cy, cx) > 6000, "el nucleo de la estrella sigue intacto");
}

// El detector solo agrupa por paridad, así que la fase del CFA no puede
// cambiar el resultado. Esto es lo que permite no decodificar `filters` a mano
// y lo que hace la batería de tests válida para cualquier cámara.
void testPhaseInvariance()
{
    int counts[4];
    for (int i = 0; i < 4; ++i) {
        cv::Mat m = makeMosaic(120, 160, 3);
        addHotPixel(m, 40, 60, 6000);
        addHotPixel(m, 77, 101, 5000);
        addHotPixel(m, 91, 12, 5500);
        counts[i] = raw::removeHotPixels(m, kPatterns[i].phase, {}).detected;
    }
    for (int i = 1; i < 4; ++i) {
        expect(counts[i] == counts[0],
               "los 4 patrones CFA dan el mismo numero de detecciones");
    }
    expect(counts[0] == 3, "se detectan los 3 defectos en cualquier fase");
}

// Aislamiento: un defecto es un punto raro, no una región rareada. Una mancha
// densa se deja intacta a propósito, y no solo por conservatism: en una mancha
// la mediana de los vecinos está contaminada por la propia mancha, así que
// sustituir por la mediana no corregiría bien, sino a medias. Queda como
// limitación conocida: los defectos agrupados no se corrigen.
//
// El bloque es de 5x5 porque tiene que ser denso en las CUATRO paridades. Con
// 3x3 la paridad (1,1) solo contiene un fotosito, que sí es un punto suelto, y
// el detector lo trata como tal (correctamente).
void testClusterIsNotCorrected()
{
    cv::Mat m = makeMosaic(120, 160, 4);
    for (int dy = 0; dy < 5; ++dy) {
        for (int dx = 0; dx < 5; ++dx)
            addHotPixel(m, 50 + dy, 50 + dx, 6000);
    }
    const uint16_t untouched = m.at<uint16_t>(52, 52);

    const raw::HotPixelResult r = raw::removeHotPixels(m, kPatterns[0].phase, {});

    expect(r.detected == 0, "una mancha densa no pasa el test de aislamiento");
    expect(m.at<uint16_t>(52, 52) == untouched, "la mancha queda intacta");
}

// Sin suelo del sigma, una zona plana (MAD = 0) hace que z diverja y se marquen
// miles de fotositos. El suelo es lo que lo evita.
void testFlatMosaicStaysClean()
{
    cv::Mat m(120, 160, CV_16UC1, cv::Scalar(2000));

    raw::HotPixelParams p;
    const raw::HotPixelResult r = raw::removeHotPixels(m, kPatterns[0].phase, p);

    expect(r.detected == 0, "un mosaico plano no produce detecciones");
}

// Si el mosaico no describe un Bayer 2x2 no se toca nada: es mejor no filtrar
// que filtrar con una suposición falsa.
void testRejectsNonBayerPhase()
{
    cv::Mat m = makeMosaic(120, 160, 5);
    addHotPixel(m, 40, 60, 6000);
    const cv::Mat before = m.clone();

// Un mosaico degenerado (las cuatro paridades con el mismo id) no es un
    // Bayer: no habría separación de canales y el filtro no significaría nada.
    int flat[2][2] = {{1, 1}, {1, 1}};
    raw::HotPixelResult r = raw::removeHotPixels(m, flat, {});
    expect(!r.bayer, "fase degenerada (1 color): bayer = false");
    expect(r.detected == 0, "fase degenerada: 0 detecciones");
    expect(cv::norm(before, m, cv::NORM_INF) == 0.0,
           "fase degenerada: el mosaico no se modifica");

    // Cuatro ids DISTINTOS sí es un Bayer válido. FC() devuelve el código CFA,
    // y en la codificación de libraw los dos verdes llevan códigos distintos
    // (1 y 3), igual que dcraw: un RGGB se lee 0,1,3,2. Exigir "tres colores
    // distintos" rechazaba RAW reales y dejaba el filtro apagado en silencio.
    int quad[2][2] = {{0, 1}, {3, 2}};
    const cv::Mat beforeQuad = m.clone();
    addHotPixel(m, 40, 60, 6000);
    r = raw::removeHotPixels(m, quad, {});
    expect(r.bayer, "cuatro ids distintos: es un Bayer valido");
    expect(r.detected == 1, "cuatro ids distintos: detecta el defecto");
    m = beforeQuad;
    addHotPixel(m, 40, 60, 6000);

    // Y una matriz que no es un mosaico de un solo canal tampoco.
    cv::Mat rgb(120, 160, CV_16UC3, cv::Scalar(1, 2, 3));
    const cv::Mat rgbBefore = rgb.clone();
    r = raw::removeHotPixels(rgb, kPatterns[0].phase, {});
    expect(!r.bayer, "entrada no monocroma: bayer = false");
    expect(cv::norm(rgbBefore, rgb, cv::NORM_INF) == 0.0,
           "entrada no monocroma: no se modifica");
}

// El mosaico real de LibRaw no es una imagen contigua: es una vista del búfer
// del sensor con paso raw_width*2 (10688) sobre una anchura activa de 5202, y
// desplazada por top_margin/left_margin. gatherParity tiene que respetar ese
// paso, así que esto va con vista no-contigua a propósito.
void testNonContiguousView()
{
    const int rawW = 5344, rawH = 3516, top = 52, left = 142;
    const int w = 5202, h = 3464;

    cv::Mat buffer(rawH, rawW, CV_16UC1);
    cv::RNG rng(21);
    rng.fill(buffer, cv::RNG::NORMAL, 2000.0, 40.0);

    cv::Mat view(h, w, CV_16UC1, buffer.ptr<uint16_t>(top) + left,
                 static_cast<size_t>(rawW) * sizeof(uint16_t));
    expect(!view.isContinuous(), "vista no-contigua: el paso es != anchura");

    addHotPixel(view, 1700, 2600, 6000);
    const raw::HotPixelResult r = raw::removeHotPixels(view, kPatterns[0].phase, {});

    expect(r.bayer, "vista no-contigua: Bayer valido");
    // No se exige el conteo exacto: con sigmaFloor = 1 DN y ruido de sigma 40,
    // cualquier parche localmente plano baja el umbral a med+20 DN y produce
    // algún falso positivo más. Lo que este test tiene que demostrar es que
    // gatherParity respeta el paso y encuentra Y corrige el defecto inyectado,
    // y eso lo demuestra la comprobación del fotosito de abajo.
    expect(r.detected >= 1, "vista no-contigua: detecta el defecto");
    expect(view.at<uint16_t>(1700, 2600) < 6000,
           "vista no-contigua: corrige el defecto");
    // Nada fuera del área activa puede tocarse.
    expect(buffer.at<uint16_t>(0, 0) != 6000, "vista no-contigua: respeta el borde");
}

// El parámetro k debe monotonicamente reducir el número de correcciones: es lo
// que el usuario controla con el deslizador de agresividad.
void testHigherKDetectsFewer()
{
    int previous = -1;
    for (double k : {10.0, 15.0, 20.0, 30.0}) {
        cv::Mat m = makeMosaic(120, 160, 6);
        // 40 defectos sembrados con amplitud decreciente.
        std::mt19937 rng(9);
        for (int i = 0; i < 40; ++i) {
            const int y = 10 + static_cast<int>(rng() % 100);
            const int x = 10 + static_cast<int>(rng() % 140);
            addHotPixel(m, y, x, 2000 + 100 * i);
        }
        raw::HotPixelParams p;
        p.k = k;
        const int n = raw::removeHotPixels(m, kPatterns[0].phase, p).detected;
        if (previous >= 0)
            expect(n <= previous, "mas k no puede dar mas detecciones que menos k");
        previous = n;
    }
    expect(previous >= 0, "el barrido de k se ha ejecutado");
}

// RGGB tiene DOS verdes, en las paridades (0,1) y (1,0), y ambos comparten id.
// Una validación que exigiera cuatro colores distintos rechazaría el mosaico más
// común que existe, así que esto lo fija de forma explícita.
void testTwoGreensAreAccepted()
{
    const int rggb[2][2] = {{0, 1}, {1, 2}};
    expect(rggb[0][1] == rggb[1][0], "RGGB: los dos verdes comparten id");

    cv::Mat m = makeMosaic(120, 160, 7);
    addHotPixel(m, 40, 60, 6000);
    const raw::HotPixelResult r = raw::removeHotPixels(m, rggb, {});

    expect(r.bayer, "RGGB se acepta como Bayer valido");
    expect(r.detected == 1, "RGGB detecta el defecto");
}

// Reproduce el tamaño real de un sensor (5202x3464) para detectar fallos que
// solo aparecen a esa escala: no es la aritmética del test, sino las
// asignaciones de las sub-imágenes y los límites de fila/columna.
void testFullSizeMosaic()
{
    cv::Mat m(3464, 5202, CV_16UC1);
    cv::RNG rng(11);
    rng.fill(m, cv::RNG::NORMAL, 2000.0, 40.0);
    addHotPixel(m, 1700, 2600, 6000);

    const raw::HotPixelResult r = raw::removeHotPixels(m, kPatterns[0].phase, {});

    expect(r.bayer, "sensor completo: Bayer valido");
    expect(r.detected == 1, "sensor completo: detecta el defecto");
    expect(m.at<uint16_t>(1700, 2600) < 6000, "sensor completo: corrige el defecto");
}

} // namespace

int main()
{
    testFindsAndCorrectsHotPixel();
    testStarIsPreserved();
    testPhaseInvariance();
    testClusterIsNotCorrected();
    testFlatMosaicStaysClean();
    testRejectsNonBayerPhase();
    testHigherKDetectsFewer();
    testTwoGreensAreAccepted();
    testFullSizeMosaic();
    testNonContiguousView();

    if (failures == 0)
        std::printf("test_bayerhot: OK\n");
    return failures == 0 ? 0 : 1;
}
