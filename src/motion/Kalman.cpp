#include "motion/Kalman.h"

Kalman::Kalman()
{
    F_ = {1, 0, 1, 0,
          0, 1, 0, 1,
          0, 0, 1, 0,
          0, 0, 0, 1};

    H_ = {1, 0, 0, 0,
          0, 1, 0, 0};

    R_ = cv::Matx22f::zeros();
    R_(0, 0) = 4.0f;
    R_(1, 1) = 4.0f;

    Q_ = cv::Matx44f::zeros();
    Q_(0, 0) = 0.5f;
    Q_(1, 1) = 0.5f;
    Q_(2, 2) = 0.25f;
    Q_(3, 3) = 0.25f;

    I_ = cv::Matx44f::eye();
    P_ = I_;
    P_(2, 2) = 100.0f;
    P_(3, 3) = 100.0f;
}

void Kalman::reset(float x, float y)
{
    s_ = cv::Vec4f(x, y, 0.f, 0.f);
    P_ = I_;
    P_(2, 2) = 100.0f;
    P_(3, 3) = 100.0f;
}

void Kalman::predict()
{
    s_ = F_ * s_;
    P_ = F_ * P_ * F_.t() + Q_;
}

void Kalman::correct(float mx, float my)
{
    const cv::Vec2f z(mx, my);
    const cv::Vec2f innov = z - H_ * s_;
    const cv::Matx22f S = H_ * P_ * H_.t() + R_;
    const cv::Matx<float,4,2> K = P_ * H_.t() * S.inv(cv::DECOMP_LU);
    s_ = s_ + K * innov;
    P_ = (I_ - K * H_) * P_;
    predict();
}