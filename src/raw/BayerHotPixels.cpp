#include "raw/BayerHotPixels.h"

#include <opencv2/imgproc.hpp>
#include <vector>

namespace raw {
namespace {

// Ventana (2r+1)x(2r+1) con el centro a cero. Para cv::dilate sobre datos no
// negativos un cero en el kernel es seguro (max(0, x) == x), y sirve para
// quedarse con el máximo de los vecinos del mismo color EXCLUYENDO el propio
// fotosito, que es justo lo que necesita el test de aislamiento. Con erode un
// cero sí sería peligroso, por eso aquí no se usa.
cv::Mat neighbourMaxKernel(int radius)
{
    const int k = 2 * radius + 1;
    cv::Mat ker(k, k, CV_8U, cv::Scalar(1));
    ker.at<uint8_t>(radius, radius) = 0;
    return ker;
}

// ¿Es este mosaico un Bayer 2x2? Lo que se comprueba es periodicidad, no el
// recuento de colores: el id que devuelve FC() no es "el color" sino el índice
// CFA, y los dos verdes llevan códigos DISTINTOS (1 y 3 en la codificación de
// libraw, igual que dcraw). Un RGGB puede aparecer como 0,1,3,2 y tener cuatro
// valores distintos, así que exigir "tres colores distintos" rechazaba RAW
// válidos y dejaba el filtro entero apagado sin ningún error visible. Lo que
// distingue un mosaico de verdad es que el patrón se repite cada 2 en fila y
// columna, cosa que ya se comprueba al muestrear FC(); aquí solo se descarta lo
// que no sea un mosaico 2x2 con al menos dos clases distintas.
bool isUsablePhase(const int phase[2][2])
{
    const int ids[4] = {phase[0][0], phase[0][1], phase[1][0], phase[1][1]};
    for (int i = 0; i < 4; ++i) {
        if (ids[i] < 0 || ids[i] > 3)
            return false;
    }
    // Un mosaico degenerado (las cuatro paridades iguales) no es un Bayer: no
    // habría separación de canales y el filtro no significaría nada.
    bool allSame = true;
    for (int i = 1; i < 4 && allSame; ++i)
        allSame = allSame && (ids[i] == ids[0]);
    return !allSame;
}

// Copia densa de los fotositos de una clase de paridad. A partir de aquí todas
// las operaciones son contiguas y vectorizadas, que es lo que hace rápido el
// filtro; el apilado por原价 de los vecinos era el cuello de botella.
void gatherParity(const cv::Mat& mosaic, int py, int px, int rows, int cols,
                  cv::Mat& out)
{
    out.create(rows, cols, CV_16UC1);
    for (int r = 0; r < rows; ++r) {
        const uint16_t* src = mosaic.ptr<uint16_t>(py + 2 * r) + px;
        uint16_t* dst = out.ptr<uint16_t>(r);
        for (int c = 0; c < cols; ++c)
            dst[c] = src[2 * c];
    }
}

void scatterParity(cv::Mat& mosaic, int py, int px, int rows, int cols,
                   const cv::Mat& src)
{
    for (int r = 0; r < rows; ++r) {
        uint16_t* dst = mosaic.ptr<uint16_t>(py + 2 * r) + px;
        const uint16_t* s = src.ptr<uint16_t>(r);
        for (int c = 0; c < cols; ++c)
            dst[2 * c] = s[c];
    }
}

} // namespace

HotPixelResult removeHotPixels(cv::Mat& mosaic, const int phase[2][2],
                               const HotPixelParams& params)
{
    HotPixelResult result;
    if (!isUsablePhase(phase))
        return result;
    if (mosaic.empty() || mosaic.type() != CV_16UC1)
        return result;
    if (params.radius < 1 || params.k <= 0.0)
        return result;

    result.bayer = true;

    const int ksize = 2 * params.radius + 1;
    const cv::Mat maxKer = neighbourMaxKernel(params.radius);

    for (int py = 0; py < 2; ++py) {
        for (int px = 0; px < 2; ++px) {
            const int rows = (mosaic.rows - py + 1) / 2;
            const int cols = (mosaic.cols - px + 1) / 2;
            if (rows < ksize || cols < ksize)
                continue;

            cv::Mat sub;
            gatherParity(mosaic, py, px, rows, cols, sub);

            // Mediana local. El kernel incluye el centro: con un único outlier
            // la mediana de la ventana no se mueve, así que el estimador sigue
            // siendo robusto y además sirve para corregir.
            cv::Mat med;
            cv::medianBlur(sub, med, ksize);

            // MAD local: mediana de las desviaciones absolutas respecto a la
            // mediana. Se obtiene con un segundo medianBlur sobre |sub - med| en
            // vez de apilando los 24 vecinos, que en un sensor de 18 Mpx serían
            // 200 MB por clase.
            cv::Mat resid;
            cv::absdiff(sub, med, resid);
            cv::Mat mad;
            cv::medianBlur(resid, mad, ksize);

            // Umbral med + k * max(1.4826 * MAD, suelo), en aritmética de punto
            // flotante y que al final se satura a 16 bits: comparar un CV_16U
            // contra un CV_32F no está soportado, y saturar es seguro porque un
            // umbral por encima de 65535 simplemente no puede marcar nada.
            cv::Mat thrF;
            mad.convertTo(thrF, CV_32F, 1.4826);
            cv::max(thrF, cv::Scalar(params.sigmaFloor), thrF);
            thrF *= params.k;

            cv::Mat medF;
            med.convertTo(medF, CV_32F);
            cv::add(thrF, medF, thrF);

            cv::Mat thr;
            thrF.convertTo(thr, CV_16U);

            cv::Mat hot;
            cv::compare(sub, thr, hot, cv::CMP_GT);

            // Aislamiento: un defecto es un punto raro, no una región rareada.
            // Se descarta el candidato si algún vecino del mismo color también
            // supera el umbral.
            cv::Mat nbrMax, nbrBelow;
            cv::dilate(sub, nbrMax, maxKer);
            cv::compare(nbrMax, thr, nbrBelow, cv::CMP_LE);
            cv::bitwise_and(hot, nbrBelow, hot);

            const int hits = cv::countNonZero(hot);
            result.detected += hits;

            if (params.correct && hits > 0) {
                // `fixed` tiene que salir de una copia completa de la clase:
                // copyTo con máscara solo escribe los pixels marcados, y
                // scatterParity escribe TODA la sub-imagen, así que sin esto
                // los fotositos sanos recibirían memoria sin inicializar.
                cv::Mat fixed = sub.clone();
                med.copyTo(fixed, hot);
                scatterParity(mosaic, py, px, rows, cols, fixed);
                result.corrected += hits;
            }
        }
    }

    return result;
}

} // namespace raw