#include <eigen3/Eigen/Dense>
#include <vector>
#include <iostream>
#include <limits>
#include <cmath>
#include <random>

#include "nuslam/TurtleMap.h"

#include "nuslam/ekf_slam.hpp"
#include "rigid2d/rigid2d.hpp"

namespace ekf_slam
{
  std::mt19937 & get_random()
  {
      // static variables inside a function are created once and persist for the remainder of the program
      static std::random_device rd{};
      static std::mt19937 mt{rd()};
      // we return a reference to the pseudo-random number genrator object. This is always the
      // same object every time get_random is called
      return mt;
  }

  double sampleNormalDistribution()
  {
    std::normal_distribution<> d(0, 1);
    return d(get_random());
  }

  /////////////// Slam CLASS /////////////////////////
  Slam::Slam(int num_landmarks, Eigen::Matrix3d q_var, Eigen::Matrix3d r_var)
  {
    Qnoise = q_var;
    Rnoise = r_var;

    state_size = 3 + 2*num_landmarks;

    prev_state.resize(state_size);
    prev_state.setZero();

    sigma.resize(state_size, state_size); // Resize init covarience

    sigma.topLeftCorner(3,3).setZero();
    sigma.topRightCorner(3, 2*num_landmarks).setZero();
    sigma.bottomLeftCorner(2*num_landmarks, 3).setZero();
    sigma.bottomRightCorner(2*num_landmarks, 2*num_landmarks) = Eigen::MatrixXd::Identity(2*num_landmarks, 2*num_landmarks) * 10000;
  }

  void Slam::MotionModelUpdate(rigid2d::Twist2D tw)
  {

    Eigen::Vector3d noise = Slam::getStateNoise();

    Eigen::Vector3d update;
    Eigen::Vector3d dupdate;

    double th = prev_state(0);

    if(tw.wz == 0)
    {
      update(0) = 0;
      update(1) = tw.vx * cos(th);
      update(2) = tw.vy * sin(th);

      dupdate(0) = 0;
      dupdate(1) = -tw.vx * sin(th);
      dupdate(2) = tw.vx * cos(th);
    }
    else
    {
      double vel_ratio = tw.vx/tw.wz;

      update(0) = tw.wz;
      update(1) = -vel_ratio * sin(th) + vel_ratio * sin(th + tw.wz);
      update(2) = vel_ratio * cos(th) - vel_ratio * cos(th + tw.wz);

      dupdate(0) = 0;
      dupdate(1) = -vel_ratio * cos(th) + vel_ratio * cos(th + tw.wz);
      dupdate(2) = -vel_ratio * sin(th) + vel_ratio * sin(th + tw.wz);
    }

    // Update -- Prediction
    prev_state(0) += update(0) + noise(0);
    prev_state(1) += update(1) + noise(1);
    prev_state(2) += update(2) + noise(2);

    // Landmarks do not move so no need to update that part of the state matrix

    Slam::updateCovarPrediction(dupdate);
  }

  Eigen::VectorXd Slam::getStateNoise()
  {
    // Cholesky Decomp
    Eigen::MatrixXd l(Qnoise.llt().matrixL());

    Eigen::VectorXd samples(3);

    samples(0) = sampleNormalDistribution();
    samples(1) = sampleNormalDistribution();
    samples(2) = sampleNormalDistribution();

    Eigen::VectorXd noise(3);

    noise(0) = l(0,0) * samples(0) + l(0,1) * samples(1) + l(0,2) * samples(2);
    noise(1) = l(1,0) * samples(0) + l(1,1) * samples(1) + l(1,2) * samples(2);
    noise(2) = l(2,0) * samples(0) + l(2,1) * samples(1) + l(2,2) * samples(2);

    return noise;
  }

  std::vector<double> Slam::getRobotState()
  {
    return {prev_state(0), prev_state(1), prev_state(2)};
  }

  Eigen::VectorXd Slam::getMeasurementNoise()
  {
    // Cholesky Decomp
    Eigen::MatrixXd l(Rnoise.llt().matrixL());

    Eigen::VectorXd samples(2);

    samples(0) = sampleNormalDistribution();
    samples(1) = sampleNormalDistribution();

    Eigen::VectorXd noise(3);

    noise(0) = l(0,0) * samples(0) + l(0,1) * samples(1);
    noise(1) = l(1,0) * samples(0) + l(1,1) * samples(1);

    return noise;
  }

  void Slam::updateCovarPrediction(Eigen::Vector3d dupdate)
  {
    Eigen::MatrixXd Gt = Eigen::MatrixXd::Zero(state_size,state_size);

    Gt(0, 0) = dupdate(0);
    Gt(1, 0) = dupdate(1);
    Gt(2, 0) = dupdate(2);

    Gt += Eigen::MatrixXd::Identity(state_size, state_size);

    Eigen::MatrixXd Qbar = Eigen::MatrixXd::Zero(state_size,state_size);

    Qbar.topLeftCorner(3, 3) = Qnoise;

    sigma = Gt * sigma * Gt.transpose() + Qbar;
  }

  void Slam::MeasurmentModelUpdate(nuslam::TurtleMap &map_data)
  {
    Eigen::Vector2d z_expected = Eigen::Vector2d::Zero();
    Eigen::Vector2d z_actual = Eigen::Vector2d::Zero();

    Eigen::MatrixXd Ki = Eigen::MatrixXd::Zero(state_size,state_size);
    Eigen::MatrixXd Ri = Eigen::MatrixXd::Zero(state_size,state_size);
    Eigen::MatrixXd Hi = Eigen::MatrixXd::Zero(2,state_size);

    Eigen::Vector3d noise = Eigen::Vector3d::Zero();

    double cur_x = 0, cur_y = 0, cur_r = 0;
    auto landmark_index = 0;
    double del_x = 0, del_y = 0, dist = 0;

    for(unsigned int i = 0; i < map_data.centers.size(); i++)
    {
      cur_x = map_data.centers.at(i).x;
      cur_y = map_data.centers.at(i).y;
      cur_r = map_data.radii.at(i);

      landmark_index = 3 + 2*i; // replace this with data association later

      // Compute the expected measurment
      noise = Slam::getMeasurementNoise();
      z_expected = sensorModel(prev_state(landmark_index), prev_state(landmark_index+1), noise);

      // Compute actual measurment
      noise = Eigen::Vector3d::Zero();
      z_actual = sensorModel(cur_x, cur_y, noise);

      // Assemble H Matrix
      del_x = prev_state(landmark_index) - prev_state(1);
      del_y = prev_state(landmark_index + 1) - prev_state(2);

      dist = del_x*del_x + del_y*del_y;

      Hi = getHMatrix(del_x, del_y, dist, i);

      // Compute the Kalman Gain
      Ki = sigma * Hi.transpose() * (Hi * sigma * Hi.transpose() + Rnoise).inverse();

      // Update the Posterior
      prev_state += Ki * (z_actual - z_expected);

      // Update the Covarience
      sigma = (Eigen::MatrixXd::Identity(state_size, state_size) -Ki * Hi) * sigma;
    }
  }

  Eigen::Vector2d Slam::sensorModel(double x, double y, Eigen::VectorXd noise)
  {
    Eigen::Vector2d output;

    double x_diff = x - prev_state(1);
    double y_diff = y - prev_state(2);

    // range calculation
    output(0) = sqrt(x_diff*x_diff + y_diff*y_diff) + noise(0);

    // bearing calculation
    output(1) = atan2(y_diff, x_diff) - prev_state(0) + noise(1);
    output(1) = rigid2d::normalize_angle(output(1));

    return output;
  }

  Eigen::MatrixXd Slam::getHMatrix(double x, double y, double d, int id)
  {
    Eigen::MatrixXd Hi(2,state_size);
    double sqd = sqrt(d);

    Eigen::MatrixXd pt1(2,3);
    pt1 << 0, -x/sqd, -y/sqd, -1, y/d, x/d;

    Eigen::MatrixXd pt2 = Eigen::MatrixXd::Zero(2, 2*id);

    Eigen::MatrixXd pt3(2,2);
    pt3 << x/sqd, y/sqd, -y/d, x/d;

    Eigen::MatrixXd pt4 = Eigen::MatrixXd::Zero(2, state_size - 3 - 2*id);

    Hi << pt1, pt2, pt3, pt4;

    return Hi;
  }
}