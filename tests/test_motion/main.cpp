#include "motion/Kalman.h"
#include "motion/MotionModel.h"

#include <cmath>
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
    // Kalman: sigue una trayectoria lineal con ruido y converge a la velocidad.
    Kalman kf;
    kf.reset(100.f, 200.f);
    const float vx = 2.0f, vy = -1.5f;
    float px = 100.f, py = 200.f;
    for (int i = 0; i < 50; ++i) {
        px += vx;
        py += vy;
        kf.correct(px, py);
    }
    expect(std::abs(kf.vx() - vx) < 0.5f, "Kalman vx no converge");
    expect(std::abs(kf.vy() - vy) < 0.5f, "Kalman vy no converge");
    // correct() predice un paso tras la medición: el estado anticipa la posición
    // del siguiente frame (x + vx), que debe coincidir con la verdad + velocidad.
    expect(std::abs(kf.x() - (px + vx)) < 2.0f, "Kalman x no converge");
    expect(std::abs(kf.y() - (py + vy)) < 2.0f, "Kalman y no converge");

    // MotionModel: sigue una trayectoria lineal; estado VALID con mediciones,
    // UNCERTAIN/LOST sin ellas, y la predicción no se detiene estando LOST.
    MotionModel mm(3);
    mm.reset({100.f, 100.f});
    expect(mm.status() == TrackStatus::VALID, "reset debe quedar VALID");

    cv::Point2f p(100.f, 100.f);
    for (int i = 0; i < 30; ++i) {
        p.x += 1.0f;
        p.y += 0.5f;
        p = mm.update(true, p);
    }
    expect(mm.status() == TrackStatus::VALID, "mediciones mantienen VALID");
    expect(mm.consecutiveMisses() == 0, "misses debe resetearse");

    p = mm.update(false, p);
    expect(mm.status() == TrackStatus::UNCERTAIN, "1 miss debe ser UNCERTAIN");
    p = mm.update(false, p);
    p = mm.update(false, p);
    expect(mm.status() == TrackStatus::LOST, "3 misses debe ser LOST");
    expect(mm.consecutiveMisses() == 3, "consecutiveMisses == 3");

    // La predicción continúa el movimiento mientras LOST (vx ≈ 1 px/frame).
    const cv::Point2f before = mm.position();
    p = mm.update(false, p);
    expect(mm.position().x - before.x > 0.3f, "LOST sigue prediciendo (vx > 0)");

    // Recuperación: una medición vuelve a VALID y resetea misses.
    p = mm.update(true, mm.position());
    expect(mm.status() == TrackStatus::VALID, "medición tras LOST recupera VALID");
    expect(mm.consecutiveMisses() == 0, "misses reseteado tras recuperación");

    if (failures == 0) {
        std::printf("OK\n");
        return 0;
    }
    return 1;
}