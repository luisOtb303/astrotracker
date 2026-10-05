// Test del detector temporal de píxeles calientes.
//
// La idea del detector es que un fotosito quemado está SIEMPRE en el mismo sitio,
// mientras que una estrella se mueve de un fotograma a otro. Aquí se comprueba
// justo esa distinción con un IVideoReader falso: más rápido y determinista que
// codificar un MP4 real.

#include "processing/HotPixelMap.h"
#include "video/IVideoReader.h"

#include <QCoreApplication>

#include <opencv2/core.hpp>
#include <cstdio>
#include <memory>

namespace {
int failures = 0;

void expect(bool cond, const char* msg)
{
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        ++failures;
    }
}

// Lector sintético: fondo ruidoso, dos píxeles quemados fijos y una "estrella"
// que se mueve de fotograma en fotograma.
class FakeReader : public IVideoReader
{
public:
    FakeReader(int frames, int w = 160, int h = 120)
        : frames_(frames)
        , w_(w)
        , h_(h)
    {
    }

    bool open(const std::string&) override
    {
        open_ = true;
        return true;
    }
    void close() override { open_ = false; }
    bool isOpen() const override { return open_; }

    bool readNext(Frame& out) override
    {
        if (!open_ || pos_ >= frames_)
            return false;
        out.image = build(pos_);
        out.index = pos_;
        out.ptsUs = pos_ * kFrameUs;
        ++pos_;
        return true;
    }

    // El detector reparte las muestras por toda la duración, así que esto tiene
    // que posicionar de verdad y no devolver siempre el primer fotograma: si no,
    // la "estrella" sería fija y el test no probaría nada.
    bool seekToUs(int64_t us) override
    {
        if (frames_ <= 0)
            return false;
        int64_t idx = us / kFrameUs;
        if (idx < 0)
            idx = 0;
        if (idx >= frames_)
            idx = frames_ - 1; // el ultimo frame con pts >= us
        pos_ = static_cast<int>(idx);
        return true;
    }

    int64_t durationUs() const override { return frames_ * kFrameUs; }
    int64_t frameCount() const override { return frames_; }
    double fps() const override { return 25.0; }
    int width() const override { return w_; }
    int height() const override { return h_; }

// Reproduce el frame i. Se regenera en cada llamada porque la estrella se
    // mueve: si se cacheara, todos los fotogramas serian el primero y el test
    // de "la estrella no se marca" pasaria por el motivo equivocado.
    cv::Mat build(int i)
    {
        cv::RNG rng(11);
        cv::Mat f(h_, w_, CV_8UC3);
        rng.fill(f, cv::RNG::NORMAL, 60.0, 2.0);

        // Dos fotositos quemados: misma posición en todos los fotogramas.
        f.at<cv::Vec3b>(30, 40) = cv::Vec3b(250, 250, 250);
        f.at<cv::Vec3b>(90, 120) = cv::Vec3b(250, 250, 250);

        // Una estrella: se mueve, como una de verdad.
        const int x = 20 + (i * 3) % 100;
        const int y = 60 + (i * 2) % 20;
        f.at<cv::Vec3b>(y, x) = cv::Vec3b(252, 252, 252);
        return f;
    }

    // protected, no private: las subclases que adulteran un fotograma (FlakyReader)
    // necesitan leer pos_ para saber en que indice estan.
protected:
    static constexpr int64_t kFrameUs = 40000;
    bool open_ = false;
    int frames_;
    int w_;
    int h_;
    int pos_ = 0;
};

void testFindsFixedPixelsIgnoresMovingStar()
{
    FakeReader reader(12);
    reader.open("sintetico.mp4");
    HotPixelMap::Options opts;
    opts.sampleCount = 12;
    opts.dilate = 0; // sin dilatar para poder comprobar las coordenadas exactas

    const HotPixelMap::Result res = HotPixelMap::fromVideo(reader, opts);
    expect(res.ok, "temporal: analysis succeeds");
    expect(res.sampled == 12, "temporal: all frames read");
    expect(res.hotCount == 2, "temporal: exactly the two fixed pixels found");
    expect(!res.mask.empty(), "temporal: mask produced");

    if (!res.mask.empty()) {
        expect(res.mask.at<uchar>(30, 40) == 255, "temporal: first hot pixel flagged");
        expect(res.mask.at<uchar>(90, 120) == 255, "temporal: second hot pixel flagged");
    }
}

void testStarPositionsStayClean()
{
    FakeReader reader(12);
    reader.open("sintetico.mp4");
    HotPixelMap::Options opts;
    opts.sampleCount = 12;
    opts.dilate = 0;

    const HotPixelMap::Result res = HotPixelMap::fromVideo(reader, opts);
    if (res.mask.empty())
        return;

    // Ninguna posición por la que pasó la estrella debe marcarse: es justo el
    // caso que el filtro espacial no puede descartar.
    int touched = 0;
    for (int i = 0; i < 12; ++i) {
        const int x = 20 + (i * 3) % 100;
        const int y = 60 + (i * 2) % 20;
        if (res.mask.at<uchar>(y, x) != 0)
            ++touched;
    }
    expect(touched == 0, "temporal: moving star never flagged");
}

void testDilateExpandsNeighbourhood()
{
    FakeReader reader(12);
    reader.open("sintetico.mp4");
    HotPixelMap::Options opts;
    opts.sampleCount = 12;
    opts.dilate = 1;

    const HotPixelMap::Result res = HotPixelMap::fromVideo(reader, opts);
    expect(res.ok, "temporal dilated: analysis succeeds");
    expect(res.hotCount == 2, "temporal dilated: hit count is before dilation");

    if (!res.mask.empty()) {
        // Con dilatación, los vecinos inmediatos entran en la máscara para que la
        // corrección no deje un borde de un solo píxel.
        expect(res.mask.at<uchar>(30, 41) != 0, "temporal dilated: neighbour covered");
        expect(cv::countNonZero(res.mask) > res.hotCount,
               "temporal dilated: mask is larger than hit count");
    }
}

void testMinHitRatioFiltersTransients()
{
    // Un píxel que solo está quemado a ratos no es un defecto del sensor: con un
    // ratio mínimo alto debe descartarse.
    class FlakyReader : public FakeReader
    {
    public:
        using FakeReader::FakeReader;
        bool readNext(Frame& out) override
        {
            const int before = pos_;
            if (!FakeReader::readNext(out))
                return false;
            if (!out.image.empty() && before % 2 == 0)
                out.image.at<cv::Vec3b>(30, 40) = cv::Vec3b(60, 60, 60);
            return true;
        }
    };

    FlakyReader reader(12);
    reader.open("sintetico.mp4");
    HotPixelMap::Options opts;
    opts.sampleCount = 12;
    opts.dilate = 0;
    opts.minHitRatio = 0.9; // solo casi todos los fotogramas

    const HotPixelMap::Result res = HotPixelMap::fromVideo(reader, opts);
    expect(res.ok, "temporal flaky: analysis succeeds");
    expect(res.hotCount == 1, "temporal flaky: intermittent pixel dropped, stable one kept");
}

void testClosedReaderFails()
{
    FakeReader reader(12); // nunca abierto
    HotPixelMap::Options opts;
    const HotPixelMap::Result res = HotPixelMap::fromVideo(reader, opts);
    expect(!res.ok, "temporal: closed reader fails cleanly");
    expect(!res.error.isEmpty(), "temporal: closed reader reports an error");
}

void testTooFewFramesFails()
{
    FakeReader reader(1);
    reader.open("sintetico.mp4");
    HotPixelMap::Options opts;
    opts.sampleCount = 8;
    const HotPixelMap::Result res = HotPixelMap::fromVideo(reader, opts);
    expect(!res.ok, "temporal: single-frame video is rejected");
}

void testCancellation()
{
    FakeReader reader(40);
    reader.open("sintetico.mp4");
    HotPixelMap::Options opts;
    opts.sampleCount = 40;
    opts.dilate = 0;

    std::atomic<bool> cancel{false};
    opts.cancel = &cancel;
    // Se aborta desde el propio callback de progreso, que es como lo haría el
    // botón de cancelar de la UI.
    opts.progress = [&cancel](int sampled, int) {
        if (sampled >= 4)
            cancel.store(true);
    };

    const HotPixelMap::Result res = HotPixelMap::fromVideo(reader, opts);
    expect(res.cancelled, "temporal: cancel flag honoured");
    expect(res.sampled < 40, "temporal: stops early on cancel");
    expect(!res.ok, "temporal: cancelled run yields no mask");
}

void testProgressCallback()
{
    FakeReader reader(10);
    reader.open("sintetico.mp4");
    HotPixelMap::Options opts;
    opts.sampleCount = 10;

    int lastSeen = 0;
    int planned = 0;
    opts.progress = [&lastSeen, &planned](int sampled, int total) {
        lastSeen = sampled;
        planned = total;
    };

    const HotPixelMap::Result res = HotPixelMap::fromVideo(reader, opts);
    expect(res.ok, "temporal progress: analysis succeeds");
    expect(lastSeen == 10, "temporal progress: reports every frame");
    expect(planned == 10, "temporal progress: reports the plan");
}

void testCorrectFillsMaskedPixels()
{
    cv::Mat frame(40, 40, CV_8UC3, cv::Scalar(60, 60, 60));
    frame.at<cv::Vec3b>(20, 20) = cv::Vec3b(255, 255, 255);

    cv::Mat mask = cv::Mat::zeros(frame.size(), CV_8U);
    mask.at<uchar>(20, 20) = 255;

    const cv::Mat out = HotPixelMap::correct(frame, mask);
    expect(out.at<cv::Vec3b>(20, 20) != cv::Vec3b(255, 255, 255),
           "correct: masked pixel replaced");
    expect(out.at<cv::Vec3b>(5, 5) == frame.at<cv::Vec3b>(5, 5),
           "correct: rest of frame untouched");
}

void testEmptyMaskIsNoop()
{
    cv::Mat frame(40, 40, CV_8UC3, cv::Scalar(60, 60, 60));
    const cv::Mat mask = cv::Mat::zeros(frame.size(), CV_8U);
    const cv::Mat out = HotPixelMap::correct(frame, mask);
    expect(cv::norm(out, frame, cv::NORM_INF) == 0, "correct: empty mask changes nothing");
}

} // namespace

int main(int argc, char** argv)
{
    // fromVideo() cede el hilo con processEvents, que necesita una instancia.
    QCoreApplication app(argc, argv);

    testFindsFixedPixelsIgnoresMovingStar();
    testStarPositionsStayClean();
    testDilateExpandsNeighbourhood();
    testMinHitRatioFiltersTransients();
    testClosedReaderFails();
    testTooFewFramesFails();
    testCancellation();
    testProgressCallback();
    testCorrectFillsMaskedPixels();
    testEmptyMaskIsNoop();

    if (failures == 0)
        std::printf("ALL TESTS PASSED\n");
    else
        std::printf("%d TEST(S) FAILED\n", failures);
    return failures;
}