#include "GlobalStaticOptimizationSolver.h"

#include "DeGroote2016Muscle.h"
#include "InverseMuscleSolverMotionData.h"

#include <mesh.h>

#include <algorithm>

using namespace OpenSim;

void GlobalStaticOptimizationSolver::Solution::write(const std::string& prefix)
        const {
    auto write = [&](const TimeSeriesTable& table, const std::string& suffix)
    {
        if (table.getNumRows()) {
            STOFileAdapter_<double>::write(table,
                                           prefix + "_" + suffix + ".sto");
        }
    };
    write(activation, "activation");
    write(other_controls, "other_controls");
    write(norm_fiber_length, "norm_fiber_length");
    write(norm_fiber_velocity, "norm_fiber_velocity");
    write(tendon_force, "tendon_force");
}

/// "Separate" denotes that the dynamics are not coming from OpenSim, but
/// rather are coded separately.
template<typename T>
class GSOProblemSeparate : public mesh::OptimalControlProblemNamed<T> {
public:
    GSOProblemSeparate(const GlobalStaticOptimizationSolver& mrs,
                       const Model& model,
                       const InverseMuscleSolverMotionData& motionData)
            : mesh::OptimalControlProblemNamed<T>("GSO"),
              _mrs(mrs), _model(model), _motionData(motionData) {
        SimTK::State state = _model.initSystem();

        // Set the time bounds.
        _initialTime = motionData.getInitialTime();
        _finalTime = motionData.getFinalTime();
        this->set_time({_initialTime}, {_finalTime});

        // States and controls for actuators.
        // ----------------------------------

        // TODO handle different actuator types more systematically.
        auto actuators = _model.getComponentList<ScalarActuator>();
        for (const auto& actuator : actuators) {
            if (actuator.get_appliesForce() &&
                    !dynamic_cast<const Muscle*>(&actuator) &&
                    !dynamic_cast<const CoordinateActuator*>(&actuator)) {
                throw std::runtime_error("[GSO] Only Muscles and "
                        "CoordinateActuators are currently supported but the"
                        " model contains an enabled "
                        + actuator.getConcreteClassName() + ". Either set "
                        "appliesForce=false for this actuator, or remove it "
                        "from the model.");
            }
        }

        // CoordinateActuators.
        // --------------------
        _numCoordActuators = 0;
        for (const auto& actuator :
                _model.getComponentList<CoordinateActuator>()) {
            if (!actuator.get_appliesForce()) continue;

            const auto& actuPath = actuator.getAbsolutePathName();
            this->add_control(actuPath + "_control",
                              {actuator.get_min_control(),
                               actuator.get_max_control()});

            _otherControlsLabels.push_back(actuPath);
            _numCoordActuators++;
        }
        const auto& coordPathsToActuate = motionData.getCoordinatesToActuate();
        _optimalForce.resize(_numCoordActuators);
        _coordActuatorDOFs.resize(_numCoordActuators);
        const ComponentPath modelPath = model.getAbsolutePathName();
        int i_act = 0;
        for (const auto& actuator :
                _model.getComponentList<CoordinateActuator>()) {
            if (!actuator.get_appliesForce()) continue;
            _optimalForce[i_act] = actuator.getOptimalForce();
            // Figure out which DOF each coordinate actuator is actuating.
            const auto* coord = actuator.getCoordinate();
            const auto coordPath =
                    ComponentPath(coord->getAbsolutePathName())
                            .formRelativePath(modelPath).toString();
            size_t i_coord = 0;
            while (i_coord < coordPathsToActuate.size() &&
                    coordPathsToActuate[i_coord] != coordPath) {
                ++i_coord;
            }
            // TODO move this into InverseMuscleSolver.
            if (i_coord == coordPathsToActuate.size()) {
                throw std::runtime_error("[GSO] Could not find Coordinate '" +
                        coord->getAbsolutePathName() + "' used in "
                        "CoordinateActuator '" + actuator.getAbsolutePathName()
                        + "'. Is the coordinate locked?");
            }
            _coordActuatorDOFs[i_act] = i_coord;
            ++i_act;
        }

        // Muscles.
        // --------
        _numMuscles = 0;
        const auto muscleList = _model.getComponentList<Muscle>();
        for (const auto& actuator : muscleList) {
            if (!actuator.get_appliesForce()) continue;

            const auto& actuPath = actuator.getAbsolutePathName();

            // TODO use activation bounds, not excitation bounds.
            this->add_control(actuPath + "_activation", {0, 1});
            // PR #1728 on opensim-core causes the min_control for muscles
            // to be the minimum activation, and using non-zero minimum
            // activation here causes issues (actually, only causes issues
            // with MuscleRedundancySolver, but for consistency, we also
            // ignore min_control for GSO).
            //                  {actuator.get_min_control(),
            //                   actuator.get_max_control()});

            _muscleLabels.push_back(actuPath);
            _numMuscles++;
        }

        // Create De Groote muscles.
        _muscles.resize(_numMuscles);
        int i_mus = 0;
        for (const auto& osimMus : muscleList) {
            if (!osimMus.get_appliesForce()) continue;

            _muscles[i_mus] = DeGroote2016Muscle<T>(
                    osimMus.get_max_isometric_force(),
                    osimMus.get_optimal_fiber_length(),
                    osimMus.get_tendon_slack_length(),
                    osimMus.get_pennation_angle_at_optimal(),
                    osimMus.get_max_contraction_velocity());

            i_mus++;
        }

        // Add a constraint for each coordinate we want to actuate.
        _numCoordsToActuate = coordPathsToActuate.size();
        for (const auto& coordPath : coordPathsToActuate) {
            this->add_path_constraint("net_gen_force_" + coordPath, 0);
        }
    }

    void initialize_on_mesh(const Eigen::VectorXd& mesh) const override {

        // For caching desired joint moments.
        auto* mutableThis = const_cast<GSOProblemSeparate<T>*>(this);

        const auto duration = _finalTime - _initialTime;
        // "array()" b/c Eigen matrix types do not support elementwise add.
        Eigen::VectorXd times = _initialTime + (duration * mesh).array();

        _motionData.interpolateNetGeneralizedForces(times,
                mutableThis->_desiredMoments);
        if (_numMuscles) {
            _motionData.interpolateMuscleTendonLengths(times,
                    mutableThis->_muscleTendonLengths);
            _motionData.interpolateMuscleTendonVelocities(times,
                    mutableThis->_muscleTendonVelocities);
            _motionData.interpolateMomentArms(times,
                    mutableThis->_momentArms);
        }

        // TODO precompute matrix A and vector b such that the muscle-generated
        // moments are A*a + b.
        // TODO
        // b(i_act, i_time) = calcRigidTendonNormFiberForceAlongTendon(0,
        //      musTenLength(i_act, i_mesh), musTenVelocity(i_act, i_mesh));
        // TODO diagonal matrix:
        // A[i_time](i_act, i_act) = calcRigidTendonNormFiberForceAlongTendon(1,
        //      musTenLength(i_act, i_mesh), musTenVelocity(i_act, i_mesh))
        //      - b(i_act, i_time);
        // Multiply A and b by the moment arm matrix.
    }

    void path_constraints(unsigned i_mesh,
                          const T& /*time*/,
                          const mesh::VectorX<T>& /*states*/,
                          const mesh::VectorX<T>& controls,
                          Eigen::Ref<mesh::VectorX<T>> constraints)
    const override {
        // Actuator equilibrium.
        // =====================
        // TODO in the future, we want this to be:
        // model.calcImplicitResidual(state); (would handle muscles AND moments)

        // Assemble generalized forces to apply to the joints.
        // TODO avoid reallocating this each time?
        mesh::VectorX<T> genForce(_numCoordsToActuate);
        genForce.setZero();

        // CoordinateActuators.
        // --------------------
        for (Eigen::Index i_act = 0; i_act < _numCoordActuators; ++i_act) {
            genForce[_coordActuatorDOFs[i_act]]
                    += _optimalForce[i_act] * controls[i_act];
        }

        // Muscles.
        // --------
        if (_numMuscles) {
            mesh::VectorX<T> muscleForces(_numMuscles);
            for (Eigen::Index i_act = 0; i_act < _numMuscles; ++i_act) {
                // Unpack variables.
                const T& activation = controls[_numCoordActuators + i_act];

                // Get the total muscle-tendon length and velocity from the
                // data.
                const T& musTenLen = _muscleTendonLengths(i_act, i_mesh);
                const T& musTenVel = _muscleTendonVelocities(i_act, i_mesh);

                muscleForces[i_act] =
                        _muscles[i_act].calcRigidTendonFiberForceAlongTendon(
                                activation, musTenLen, musTenVel);
            }

            // Compute generalized forces from muscles.
            const auto& momArms = _momentArms[i_mesh];
            genForce += momArms.template cast<adouble>() * muscleForces;
        }

        // Achieve the motion.
        // ===================
        constraints = _desiredMoments.col(i_mesh).template cast<adouble>()
                    - genForce;
    }
    void integral_cost(const T& /*time*/,
                       const mesh::VectorX<T>& /*states*/,
                       const mesh::VectorX<T>& controls,
                       T& integrand) const override {
        integrand = controls.squaredNorm();
    }
    GlobalStaticOptimizationSolver::Solution deconstruct_iterate(
            const mesh::OptimalControlIterate& ocpVars) const {

        GlobalStaticOptimizationSolver::Solution vars;
        if (_numCoordActuators) {
            vars.other_controls.setColumnLabels(_otherControlsLabels);
        }
        if (_numMuscles) {
            vars.activation.setColumnLabels(_muscleLabels);
            vars.norm_fiber_length.setColumnLabels(_muscleLabels);
            vars.norm_fiber_velocity.setColumnLabels(_muscleLabels);
            vars.tendon_force.setColumnLabels(_muscleLabels);
        }

        // TODO would it be faster to use vars.activation.updMatrix()?
        for (int i_time = 0; i_time < ocpVars.time.cols(); ++i_time) {
            const auto& time = ocpVars.time[i_time];
            const auto& controls = ocpVars.controls.col(i_time);

            // Other controls.
            // ---------------
            // The first _numCoordActuators rows of the controls matrix
            // are for the CoordinateActuators.
            if (_numCoordActuators) {
                SimTK::RowVector other_controls(_numCoordActuators,
                                                controls.data(),
                                                true /* <- this is a view */);
                vars.other_controls.appendRow(time, other_controls);
            }

            // Muscle-related quantities.
            // --------------------------
            if (_numMuscles == 0) continue;
            SimTK::RowVector activationRow(_numMuscles,
                                           controls.data() + _numCoordActuators,
                                           true /* makes this a view */);
            vars.activation.appendRow(time, activationRow);

            // Compute fiber length and velocity.
            // ----------------------------------
            // TODO this does not depend on the solution; this could be done in
            // initialize_on_mesh.
            SimTK::RowVector normFibLenRow(_numMuscles);
            SimTK::RowVector normFibVelRow(_numMuscles);
            SimTK::RowVector tenForceRow(_numMuscles);
            for (Eigen::Index i_act = 0; i_act < _numMuscles; ++i_act) {
                const auto& musTenLen = _muscleTendonLengths(i_act, i_time);
                const auto& musTenVel = _muscleTendonVelocities(i_act, i_time);
                double normFiberLength;
                double normFiberVelocity;
                const auto muscle = _muscles[i_act].convert_scalartype_double();
                muscle.calcRigidTendonFiberKinematics(musTenLen, musTenVel,
                        normFiberLength, normFiberVelocity);
                normFibLenRow[i_act] = normFiberLength;
                normFibVelRow[i_act] = normFiberVelocity;

                const auto& activation = activationRow.getElt(0, i_act);
                tenForceRow[i_act] =
                        muscle.calcRigidTendonFiberForceAlongTendon(
                                activation, musTenLen, musTenVel);
            }
            vars.norm_fiber_length.appendRow(time, normFibLenRow);
            vars.norm_fiber_velocity.appendRow(time, normFibVelRow);
            vars.tendon_force.appendRow(time, tenForceRow);
        }
        return vars;
    }
private:
    const GlobalStaticOptimizationSolver& _mrs;
    Model _model;
    const InverseMuscleSolverMotionData& _motionData;
    double _initialTime = SimTK::NaN;
    double _finalTime = SimTK::NaN;

    // Bookkeeping.
    int _numCoordsToActuate;
    int _numCoordActuators;
    int _numMuscles;
    std::vector<std::string> _muscleLabels;
    std::vector<std::string> _otherControlsLabels;
    // The index of the DOF that is actuated by each CoordinateActuator.
    std::vector<Eigen::Index> _coordActuatorDOFs;

    // Motion data to use during the optimization.
    Eigen::MatrixXd _desiredMoments;
    Eigen::MatrixXd _muscleTendonLengths;
    Eigen::MatrixXd _muscleTendonVelocities;
    // TODO use Eigen::SparseMatrixXd
    std::vector<Eigen::MatrixXd> _momentArms;

    // CoordinateActuator optimal forces.
    Eigen::VectorXd _optimalForce;

    // De Groote muscles.
    std::vector<DeGroote2016Muscle<T>> _muscles;
};

GlobalStaticOptimizationSolver::GlobalStaticOptimizationSolver(
        const std::string& setupFilePath) :
        InverseMuscleSolver(setupFilePath) {
    updateFromXMLDocument();
}

GlobalStaticOptimizationSolver::Solution
GlobalStaticOptimizationSolver::solve() {

    // Load model and kinematics files.
    // --------------------------------
    Model model;
    TimeSeriesTable kinematics;
    TimeSeriesTable netGeneralizedForces;
    loadModelAndData(model, kinematics, netGeneralizedForces);

    // Decide the coordinates for which net generalized forces will be achieved.
    // -------------------------------------------------------------------------
    SimTK::State state = model.initSystem();
    std::vector<const Coordinate*> coordsToActuate;
    const auto coordsInOrder = model.getCoordinatesInMultibodyTreeOrder();
    const ComponentPath modelPath = model.getAbsolutePathName();
    if (!getProperty_coordinates_to_include().empty()) {
        // Our goal is to create a list of Coordinate* in multibody tree order.
        // We will keep track of which requested coordinates we actually find.
        std::set<std::string> coordsToInclude;
        auto numCoordsToInclude = getProperty_coordinates_to_include().size();
        for (int iInclude = 0; iInclude < numCoordsToInclude; ++iInclude) {
            coordsToInclude.insert(get_coordinates_to_include(iInclude));
        }
        // Go through coordinates in order.
        for (auto& coord : coordsInOrder) {
            // Remove the model name from the coordinate path.
            const auto coordPath = ComponentPath(coord->getAbsolutePathName())
                            .formRelativePath(modelPath).toString();
            // Should this coordinate be included?
            const auto foundCoordPath = coordsToInclude.find(coordPath);
            if (foundCoordPath != coordsToInclude.end()) {
                OPENSIM_THROW_IF_FRMOBJ(coord->isConstrained(state), Exception,
                        "Coordinate '" + coordPath + "' is constrained and "
                        "thus cannot be listed under "
                        "'coordinates_to_include'.");
                coordsToActuate.push_back(coord.get());
                // No longer need to search for this coordinate.
                coordsToInclude.erase(foundCoordPath);
            }
        }
        // Any remaining "coordsToInclude" are not in the model.
        if (!coordsToInclude.empty()) {
            std::string msg = "Could not find the following coordinates "
                    "listed under 'coordinates_to_include' (make sure to "
                    "use the *path* to the coordinate):\n";
            for (const auto& coordPath : coordsToInclude) {
                msg += "  " + coordPath + "\n";
            }
            OPENSIM_THROW_FRMOBJ(Exception, msg);
        }
    } else {
        // User did not specify coords to include, so include all of them.
        for (auto& coord : model.getCoordinatesInMultibodyTreeOrder()) {
            if (!coord->isConstrained(state)) {
                coordsToActuate.push_back(coord.get());
            }
        }
    }
    std::cout << "The following Coordinates will be actuated:" << std::endl;
    for (const auto* coord : coordsToActuate) {
        std::cout << "  " << coord->getAbsolutePathName() << std::endl;
    }

    // Process which actuators are included.
    // -------------------------------------
    processActuatorsToInclude(model);

    // Create reserve actuators.
    // -------------------------
    if (get_create_reserve_actuators() != -1) {
        const auto& optimalForce = get_create_reserve_actuators();
        OPENSIM_THROW_IF(optimalForce <= 0, Exception,
                "Invalid value (" + std::to_string(optimalForce)
                + ") for create_reserve_actuators; should be -1 or positive.");

        std::cout << "Adding reserve actuators with an optimal force of "
                << optimalForce << "..." << std::endl;

        std::vector<std::string> coordPaths;
        // Borrowed from CoordinateActuator::CreateForceSetOfCoordinateAct...
        for (const auto* coord : coordsToActuate) {
            auto* actu = new CoordinateActuator();
            actu->setCoordinate(const_cast<Coordinate*>(coord));
            auto path = coord->getAbsolutePathName();
            coordPaths.push_back(path);
            // Get rid of model name.
            path = ComponentPath(path).formRelativePath(
                    model.getAbsolutePathName()).toString();
            // Get rid of slashes in the path; slashes not allowed in names.
            std::replace(path.begin(), path.end(), '/', '_');
            actu->setName("reserve_" + path);
            actu->setOptimalForce(optimalForce);
            model.addComponent(actu);
        }
        // Re-make the system, since there are new actuators.
        model.initSystem();
        std::cout << "Added " << coordPaths.size() << " reserve actuator(s), "
                "for each of the following coordinates:" << std::endl;
        for (const auto& name : coordPaths) {
            std::cout << "  " << name << std::endl;
        }
    }

    // Determine initial and final times.
    // ----------------------------------
    // TODO create tests for initial_time and final_time.
    // TODO check for errors in initial_time and final_time.
    double initialTime;
    double finalTime;
    determineInitialAndFinalTimes(kinematics, netGeneralizedForces,
            initialTime, finalTime);

    // Process experimental data.
    // --------------------------
    // TODO move to InverseMuscleSolver
    InverseMuscleSolverMotionData motionData;
    OPENSIM_THROW_IF(
            get_lowpass_cutoff_frequency_for_kinematics() <= 0 &&
            get_lowpass_cutoff_frequency_for_kinematics() != -1,
            Exception,
            "Invalid value for cutoff frequency for kinematics.");
    if (netGeneralizedForces.getNumRows()) {
        motionData = InverseMuscleSolverMotionData(model, coordsToActuate,
                initialTime, finalTime, kinematics,
                get_lowpass_cutoff_frequency_for_kinematics(),
                netGeneralizedForces);
    } else {
        // We must perform inverse dynamics.
        OPENSIM_THROW_IF(
                get_lowpass_cutoff_frequency_for_joint_moments() <= 0 &&
                get_lowpass_cutoff_frequency_for_joint_moments() != -1,
                Exception,
                "Invalid value for cutoff frequency for joint moments.");
        motionData = InverseMuscleSolverMotionData(model, coordsToActuate,
                initialTime, finalTime, kinematics,
                get_lowpass_cutoff_frequency_for_kinematics(),
                get_lowpass_cutoff_frequency_for_joint_moments());
    }

    // Solve the optimal control problem.
    // ----------------------------------
    auto ocp = std::make_shared<GSOProblemSeparate<adouble>>(*this, model,
            motionData);
    ocp->print_description();
    mesh::DirectCollocationSolver<adouble> dircol(ocp, "trapezoidal", "ipopt",
                                                  100);
    mesh::OptimalControlSolution ocp_solution = dircol.solve();

    // Return the solution.
    // --------------------
    // TODO remove
    ocp_solution.write("GlobalStaticOptimizationSolver_OCP_solution.csv");
    // dircol.print_constraint_values(ocp_solution);
    return ocp->deconstruct_iterate(ocp_solution);
}