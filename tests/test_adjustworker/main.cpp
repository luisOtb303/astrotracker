#include "processing/AdjustWorker.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <opencv2/core.hpp>
#include <cstdio>

// Test del worker de ajustes: debe procesar fuera del hilo principal y
// coalescer las peticiones para que arrastrar un deslizador no encole trabajo.

namespace {

int failures = 0;

void expect(bool cond, const char* msg)
{
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        ++failures;
    }
}

cv::Mat makeImage()
{
    return cv::Mat(64, 64, CV_8UC3, cv::Scalar(128, 128, 128));
}

// Recolector de resultados: la señal del worker llega en cola, así que hay que
// dar vueltas al bucle de eventos para recibirla.
class Collector : public QObject
{
public:
    QList<cv::Mat> results;
    QList<quint64> seqs;

    int count() const { return results.size(); }

    void clear()
    {
        results.clear();
        seqs.clear();
    }

    // Da vueltas al bucle un máximo de `msecs` esperando al menos `want`
    // resultados.
    void spin(int want, int msecs = 5000)
    {
        QElapsedTimer t;
        t.start();
        while (results.size() < want && t.elapsed() < msecs)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
};

// Deja correr el bucle un tiempo fijo, para recoger señales pendientes.
void pump(int msecs)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < msecs)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // 1. Un ajuste no identidad produce un frame modificado.
    {
        AdjustWorker w;
        w.start();
        Collector c;
        QObject::connect(&w, &AdjustWorker::adjusted,
                         &c, [&c](const cv::Mat& out, quint64 seq) {
                             c.results.push_back(out);
                             c.seqs.push_back(seq);
                         });
        w.setSource(makeImage());
        ImageAdjust adj;
        adj.brightness = 40;
        expect(w.request(adj), "ajuste no identidad: request() devuelve true");
        expect(w.waitIdle(5000), "el worker termina el trabajo");
        c.spin(1);
        expect(c.count() >= 1, "se recibe el frame ajustado");
        if (c.count() >= 1) {
            expect(!c.results.first().empty(), "el frame ajustado no está vacío");
            expect(cv::mean(c.results.first())[0] > 128.0,
                   "el brillo se aplica de verdad");
        }
        w.shutdown();
    }

    // 2. Ajuste identidad: no se encola trabajo (no hay nada que aplicar).
    {
        AdjustWorker w;
        w.start();
        w.setSource(makeImage());
        expect(!w.request(ImageAdjust{}),
               "ajuste identidad: request() devuelve false");
        expect(w.waitIdle(1000), "ajuste identidad: el worker queda libre");
        w.shutdown();
    }

    // 3. Identidad después de un ajuste real: request() vuelve a ser falso, y
    //    el llamante ya no espera resultado del hilo.
    {
        AdjustWorker w;
        w.start();
        w.setSource(makeImage());
        ImageAdjust adj;
        adj.contrast = 20;
        w.request(adj);
        w.waitIdle(5000);
        expect(!w.request(ImageAdjust{}), "tras un ajuste, identidad ya no encola");
        w.shutdown();
    }

    // 4. Coalescing: muchas peticiones seguidas producen pocos resultados, y la
    //    última es la que llega.
    {
        AdjustWorker w;
        w.start();
        Collector c;
        QObject::connect(&w, &AdjustWorker::adjusted,
                         &c, [&c](const cv::Mat& out, quint64 seq) {
                             c.results.push_back(out);
                             c.seqs.push_back(seq);
                         });
        w.setSource(makeImage());
        for (int i = 1; i <= 40; ++i) {
            ImageAdjust adj;
            adj.brightness = i;
            w.request(adj);
        }
        expect(w.waitIdle(10000), "ráfaga de peticiones: el worker termina");
        c.spin(1);
        expect(c.count() >= 1, "ráfaga: llega al menos el último resultado");
        // Sin coalescing serían ~40; con él, muy pocos.
        expect(c.count() < 40, "ráfaga: las peticiones intermedias se descartan");
        if (!c.results.isEmpty()) {
            // El último valor pedido es brillo = 40 -> 128 + 40.
            expect(cv::mean(c.results.last())[0] >= 160.0,
                   "ráfaga: el resultado corresponde al último ajuste");
        }
        w.shutdown();
    }

    // 5. reset() descarta lo pendiente: no debe llegar ningún resultado.
    {
        AdjustWorker w;
        w.start();
        Collector c;
        QObject::connect(&w, &AdjustWorker::adjusted,
                         &c, [&c](const cv::Mat& out, quint64 seq) {
                             c.results.push_back(out);
                             c.seqs.push_back(seq);
                         });
        w.setSource(makeImage());
        ImageAdjust adj;
        adj.denoise = 80;
        w.request(adj);
        w.shutdown();
        pump(200);
        expect(c.count() == 0, "reset(): no llega resultado de lo cancelado");
    }

    // 6. Sin frame no hay resultado, pero el worker no se queda colgado.
    {
        AdjustWorker w;
        w.start();
        Collector c;
        QObject::connect(&w, &AdjustWorker::adjusted,
                         &c, [&c](const cv::Mat& out, quint64 seq) {
                             c.results.push_back(out);
                             c.seqs.push_back(seq);
                         });
        ImageAdjust adj;
        adj.brightness = 10;
        w.request(adj);
        expect(w.waitIdle(5000), "sin frame: el worker termina igualmente");
        pump(200);
        expect(c.count() == 0, "sin frame: no se emite nada");
        w.shutdown();
    }

    // 7. El hilo principal no se bloquea: request() debe devolver enseguida,
    //    aunque el denoise grande tarde (esto evita el congelamiento de la
    //    ventana al mover el deslizador de ruido).
    {
        AdjustWorker w;
        w.start();
        cv::Mat big(900, 900, CV_8UC3);
        cv::randu(big, cv::Scalar::all(0), cv::Scalar::all(255));
        w.setSource(big);
        ImageAdjust adj;
        adj.denoise = 100;
        QElapsedTimer timer;
        timer.start();
        w.request(adj);
        const qint64 elapsed = timer.elapsed();
        expect(elapsed < 500, "request() no bloquea el hilo principal");
        expect(w.waitIdle(60000), "denoise grande: el worker termina");
        w.shutdown();
    }

    // 8. El bucle de eventos sigue respondiendo mientras el worker trabaja.
    {
        AdjustWorker w;
        w.start();
        cv::Mat big(700, 700, CV_8UC3);
        cv::randu(big, cv::Scalar::all(0), cv::Scalar::all(255));
        w.setSource(big);
        ImageAdjust adj;
        adj.denoise = 90;
        w.request(adj);

        int ticks = 0;
        QTimer ticker;
        ticker.setInterval(1);
        QObject::connect(&ticker, &QTimer::timeout, [&ticks] { ++ticks; });
        ticker.start();
        // Solo se procesan eventos: waitIdle() bloquearía el bucle, que es
        // justo lo que este test comprueba que no ocurre.
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 3000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        ticker.stop();
        expect(ticks > 0, "el bucle de eventos sigue vivo durante el denoise");
        expect(w.waitIdle(60000), "el denoise grande termina igualmente");
        w.shutdown();
    }

    if (failures == 0)
        std::printf("test_adjustworker: OK\n");
    else
        std::printf("test_adjustworker: %d fallos\n", failures);
    return failures == 0 ? 0 : 1;
}