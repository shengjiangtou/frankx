#include <frankx/robot.hpp>

#include <ruckig/ruckig.hpp>
#include <ruckig/alternative/smoothie.hpp>


namespace frankx {

Robot::Robot(std::string fci_ip, double dynamic_rel, bool repeat_on_error, bool stop_at_python_signal): franka::Robot(fci_ip), fci_ip(fci_ip), velocity_rel(dynamic_rel), acceleration_rel(dynamic_rel), jerk_rel(dynamic_rel), repeat_on_error(repeat_on_error), stop_at_python_signal(stop_at_python_signal) { }

void Robot::setDefaultBehavior() {
    setCollisionBehavior(
        {{20.0, 20.0, 20.0, 20.0, 20.0, 20.0, 20.0}},
        {{20.0, 20.0, 20.0, 20.0, 20.0, 20.0, 20.0}},
        {{20.0, 20.0, 20.0, 20.0, 20.0, 20.0, 20.0}},
        {{20.0, 20.0, 20.0, 20.0, 20.0, 20.0, 20.0}},
        {{30.0, 30.0, 30.0, 30.0, 30.0, 30.0}},
        {{30.0, 30.0, 30.0, 30.0, 30.0, 30.0}},
        {{30.0, 30.0, 30.0, 30.0, 30.0, 30.0}},
        {{30.0, 30.0, 30.0, 30.0, 30.0, 30.0}}
    );

    // setJointImpedance({{3000, 3000, 3000, 2500, 2500, 2000, 2000}});
    // setCartesianImpedance({{3000, 3000, 3000, 300, 300, 300}});
    setEE({0.7071, 0.7071, 0.0, 0.0, 0.7071, -0.7071, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 1.0});
}

void Robot::setDynamicRel(double dynamic_rel) {
    velocity_rel = dynamic_rel;
    acceleration_rel = dynamic_rel;
    jerk_rel = dynamic_rel;
}

bool Robot::hasErrors() {
    return bool(readOnce().current_errors);
}

bool Robot::recoverFromErrors() {
    automaticErrorRecovery();
    return !hasErrors();
}

Affine Robot::currentPose(const Affine& frame) {
    auto state = readOnce();
    return Affine(state.O_T_EE) * frame;
}

// Affine Robot::forwardKinematics(const std::array<double, 7>& q) {
//     Affine result;

//     control([&](const franka::RobotState& robot_state, franka::Duration period) -> franka::Torques {
//         franka::CartesianPose cartesian_target(robot_state.O_T_EE_c, robot_state.elbow_c);
//         result = Affine(cartesian_target.O_T_EE);
//         return franka::MotionFinished(franka::Torques({0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}));

//     }, [&](const franka::RobotState& robot_state, franka::Duration period) -> franka::JointPositions {

//         return franka::JointPositions(q);
//     });

//     return result;
// }

// std::array<double, 7> Robot::inverseKinematics(const Affine& target, const Affine& frame) {
//     std::array<double, 7> result;
//     return result;
// }



bool Robot::move(ImpedanceMotion& motion) {
    return move(Affine(), motion);
}

bool Robot::move(ImpedanceMotion& motion, MotionData& data) {
    return move(Affine(), motion, data);
}

bool Robot::move(const Affine& frame, ImpedanceMotion& motion) {
    auto data = MotionData();
    return move(frame, motion, data);
}

bool Robot::move(const Affine& frame, ImpedanceMotion& motion, MotionData& data) {
    ImpedanceMotionGenerator<Robot> mg {this, frame, motion, data};

    try {
        control(mg);
        motion.is_active = false;

    } catch (const franka::Exception& exception) {
        std::cout << exception.what() << std::endl;
        motion.is_active = false;
        return false;
    }
    return true;
}


bool Robot::move(JointMotion motion) {
    auto data = MotionData();
    return move(motion, data);
}

bool Robot::move(JointMotion motion, MotionData& data) {
    JointMotionGenerator<Robot> mg {this, motion, data};

    try {
        control(mg);

    } catch (franka::Exception exception) {
        std::cout << exception.what() << std::endl;
        return false;
    }
    return true;
}


bool Robot::move(PathMotion motion) {
    return move(Affine(), motion);
}

bool Robot::move(PathMotion motion, MotionData& data) {
    return move(Affine(), motion, data);
}

bool Robot::move(const Affine& frame, PathMotion motion) {
    auto data = MotionData();
    return move(frame, motion, data);
}

bool Robot::move(const Affine& frame, PathMotion motion, MotionData& data) {
    PathMotionGenerator<Robot> mg {this, frame, motion, data};

    try {
        control(mg, controller_mode);

    } catch (franka::Exception exception) {
        std::cout << exception.what() << std::endl;
        return false;
    }
    return true;
}


bool Robot::move(WaypointMotion& motion) {
    return move(Affine(), motion);
}

bool Robot::move(WaypointMotion& motion, MotionData& data) {
    return move(Affine(), motion, data);
}

bool Robot::move(const Affine& frame, WaypointMotion& motion) {
    auto data = MotionData();
    return move(frame, motion, data);
}

bool Robot::move(const Affine& frame, WaypointMotion& motion, MotionData& data) {
    WaypointMotionGenerator<Robot> mg {this, frame, motion, data};

    try {
        control(mg, controller_mode);

    } catch (franka::Exception exception) {
        auto errors = readOnce().last_motion_errors;
        if (repeat_on_error && errors.cartesian_motion_generator_joint_acceleration_discontinuity || errors.cartesian_motion_generator_joint_velocity_discontinuity || errors.cartesian_motion_generator_velocity_discontinuity || errors.cartesian_motion_generator_acceleration_discontinuity) {
            std::cout << "[frankx robot] continue motion" << std::endl;
            automaticErrorRecovery();

            data.velocity_rel *= 0.4;
            data.acceleration_rel *= 0.4;
            data.jerk_rel *= 0.4;
            mg.reset();

            try {
                control(mg, controller_mode);
            } catch (franka::Exception exception) {
                std::cout << exception.what() << std::endl;
                return false;
            }
            return true;
        }
        std::cout << exception.what() << std::endl;
        return false;
    }
    return true;
}

} // namepace frankx
