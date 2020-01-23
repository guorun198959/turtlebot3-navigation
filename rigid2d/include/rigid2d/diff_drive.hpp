#ifndef DIFFDRIVE_INCLUDE_GUARD_HPP
#define DIFFDRIVE_INCLUDE_GUARD_HPP
/// \file
/// \brief Library for tracking the state of a diff drive robot.

#include <iosfwd> // contains forward definitions for iostream objects
#include <cmath> // standard math functions
#include "rigid2d/rigid2d.hpp"

using std::fabs;

namespace rigid2d
{

  /// \brief Wheel velocities for a diff drive robot.
  struct WheelVelocities
  {
      double ul = 0;
      double ur = 0;
  };

  class DiffDrive
  {
  public:
      /// \brief the default constructor creates a robot at (0,0,0), with a fixed wheel base and wheel radius
      DiffDrive();

      /// \brief create a DiffDrive model by specifying the pose, and geometry
      ///
      /// \param pose - the current position of the robot
      /// \param wheel_base - the distance between the wheel centers
      /// \param wheel_radius - the raidus of the wheels
      DiffDrive(Pose2D pose, double wheel_base, double wheel_radius);

      /// \brief determine the wheel velocities required to make the robot
      ///        move with the desired linear and angular velocities
      /// \param twist - the desired twist in the body frame of the robot
      /// \returns - the wheel velocities to use
      /// \throws std::exception
      WheelVelocities twistToWheels(Twist2D twist);

      /// \brief determine the body twist of the robot from its wheel velocities
      /// \param vel - the velocities of the wheels, assumed to be held constant
      ///  for one time unit
      /// \returns twist in the original body frame of the
      // UNIFINISHED--------------------------------
      Twist2D wheelsToTwist(WheelVelocities vel);

      /// \brief Update the robot's odometry based on the current encoder readings
      /// \param left - the left encoder angle (in radians)
      /// \param right - the right encoder angle (in radians)
      /// \return the velocities of each wheel, assuming that they have been
      /// constant since the last call to updateOdometry
      // UNIFINISHED--------------------------------
      // WheelVelocities updateOdometry

      /// \brief update the odometry of the diff drive robot, assuming that
      /// it follows the given body twist for one time  unit
      /// \param cmd - the twist command to send to the robot
      // UNIFINISHED--------------------------------
      void feedforward(Twist2D cmd);

      /// \brief get the current pose of the robot
      Pose2D pose() const;

      /// \brief get the wheel speeds, based on the last encoder update
      WheelVelocities wheelVelocities() const;

      /// \brief reset the robot to the given position/orientation
      /// \param ps - the desired position/orientation to place the robot
      void reset(Pose2D ps);

  private:
      Pose2D pos; // position of the robot
      double r; // wheel radius of the robot
      double base; // distance between the wheel centers
      WheelVelocities w_vels; // velocities of the two wheels
      Transform2D T_wb, T_bl, T_br; // Transforms to the base and wheels

  };
}
#endif
