// SimbodyEngine.cpp
// Authors: Frank C. Anderson
/*
 * Copyright (c) 2006, Stanford University. All rights reserved. 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including 
 * without limitation the rights to use, copy, modify, merge, publish, 
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included 
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//=============================================================================
// INCLUDES
//=============================================================================
#include <iostream>
#include <string>
#include <math.h>
#include <float.h>
#include <time.h>
#include <OpenSim/Common/rdMath.h>
#include <OpenSim/Common/Mtx.h>
#include <OpenSim/Common/LoadOpenSimLibrary.h>
#include "SimbodyEngine.h"
#include <OpenSim/Simulation/Model/BodySet.h>
#include <OpenSim/Simulation/Model/CoordinateSet.h>
#include <OpenSim/Simulation/Model/SpeedSet.h>
#include <OpenSim/Simulation/Model/JointSet.h>
#include <OpenSim/Simulation/Model/AbstractDof.h>
#include <OpenSim/Simulation/Model/Model.h>
#include <OpenSim/Simulation/Model/AbstractMuscle.h>


//=============================================================================
// STATICS
//=============================================================================
using namespace std;
using namespace OpenSim;
using namespace SimTK;

const SimTK::Transform SimbodyEngine::GroundFrame;
const int SimbodyEngine::GROUND = -1;

static char simbodyGroundName[] = "ground";


//=============================================================================
// CONSTRUCTOR(S) AND DESTRUCTOR
//=============================================================================
//_____________________________________________________________________________
/**
 * Destructor.
 */

SimbodyEngine::~SimbodyEngine()
{
	cout<<"Destroying SimbodyEngine."<<endl;
}
//_____________________________________________________________________________
/**
 * Default constructor.  This constructor constructs a dynamic model of a
 * simple pendulum.
 */
SimbodyEngine::SimbodyEngine() :
	AbstractDynamicsEngine()
{
	setNull();
	setupProperties();

	// CONSTRUCT SIMPLE PENDULUM
	constructPendulum();
}

//_____________________________________________________________________________
/**
 * Constructor from an XML Document
 */
SimbodyEngine::SimbodyEngine(const string &aFileName) :
	AbstractDynamicsEngine(aFileName)
{
	setNull();
	setupProperties();
	updateFromXMLNode();

	// BUILD THE SIMBODY MODEL
}


//_____________________________________________________________________________
/**
 * Copy constructor.
 */
SimbodyEngine::SimbodyEngine(const SimbodyEngine& aEngine) :
   AbstractDynamicsEngine(aEngine)
{
	setNull();
	setupProperties();
	copyData(aEngine);
}

//_____________________________________________________________________________
/**
 * Copy this engine and return a pointer to the copy.
 * The copy constructor for this class is used.
 *
 * @return Pointer to a copy of this SimbodyEngine.
 */
Object* SimbodyEngine::copy() const
{
	SimbodyEngine *object = new SimbodyEngine(*this);
	return object;
}


//=============================================================================
// CONSTRUCTION METHODS
//=============================================================================
//_____________________________________________________________________________
/**
 * Construct a dynamic model of a simple pendulum.
 *
 * @param aEngine SimbodyEngine to be copied.
 */
void SimbodyEngine::constructPendulum()
{
	// Parameters
	double length = 1.0;
	double mass = 1.0;
	double g[] = { 0.0, -9.8, 0.0 };

	// Add pendulum mass to matter subsystem
	const Vec3 massLocation(0, -length/2, 0);
	const Vec3 jointLocation(0, length/2, 0);
	MassProperties massProps(mass, massLocation, mass*Inertia::pointMassAt(massLocation));
	const BodyId bodyId = _matter.addRigidBody(massProps,jointLocation,GroundId,GroundFrame,Mobilizer::Pin());

	// Put subsystems into system
	_system.setMatterSubsystem(_matter);
	_system.addForceSubsystem(_gravity);

	// Realize the state
	_system.get
	_system.realize(_s);

	// Set gravity
	setGravity(g);

	// Realize the state
	//_system.realize(_s,Stage::Model);
	//_system.realize(_s,Stage::Position);
	//_system.realize(_s,Stage::Velocity);
	//_system.realize(_s,Stage::Dynamics);

	// CONSTRUCT CORRESPONDING OPENSIM OBJECTS
	// Body
	SimbodyBody *body = new SimbodyBody();
	body->_engine = this;
	body->_id = bodyId;
	body->setMass(mass);
	body->setMassCenter(&jointLocation[0]);
	Array<double> inertia(0.0,9);
	body->getInertia(inertia);
	body->setInertia(inertia);
	body->setup(this);
	_bodySet.append(body);

	// Coordinate
	SimbodyCoordinate *coordinate = new SimbodyCoordinate();
	coordinate->_engine = this;
	coordinate->_bodyId = bodyId;
	coordinate->_mobilityIndex = 0;
	coordinate->setup(this);
	_coordinateSet.append(coordinate);

	// Speed
	SimbodySpeed *speed = new SimbodySpeed();
	speed->_engine = this;
	speed->_bodyId = bodyId;
	speed->_mobilityIndex = 0;
	speed->setup(this);
	_speedSet.append(speed);

}


//_____________________________________________________________________________
/**
 * Copy data members from one SimbodyEngine to another.
 *
 * @param aEngine SimbodyEngine to be copied.
 */
void SimbodyEngine::copyData(const SimbodyEngine &aEngine)
{

}

//_____________________________________________________________________________
/**
 * Set NULL values for all the variable members of this class.
 */
void SimbodyEngine::setNull()
{
	setType("SimbodyEngine");
	_groundBody = NULL;
}


//_____________________________________________________________________________
/**
 * Perform some set up functions that happen after the
 * object has been deserialized or copied.
 *
 * @param aModel model containing this SimbodyEngine.
 */
void SimbodyEngine::setup(Model* aModel)
{
	AbstractDynamicsEngine::setup(aModel);
	_groundBody = identifyGroundBody();
}

//=============================================================================
// OPERATORS
//=============================================================================
//_____________________________________________________________________________
/**
 * Assignment operator.
 *
 * @return Reference to this object.
 */
SimbodyEngine& SimbodyEngine::operator=(const SimbodyEngine &aEngine)
{
	AbstractDynamicsEngine::operator=(aEngine);
	copyData(aEngine);
	//setup(aEngine._model);

	return(*this);
}

//=============================================================================
// TYPE REGISTRATION
//=============================================================================
//_____________________________________________________________________________
/**
 * Connect properties to local pointers.
 *
 */
void SimbodyEngine::setupProperties()
{
}

//_____________________________________________________________________________
/**
 * Register the types used by this class.
 */
void SimbodyEngine::registerTypes()
{
	Object::RegisterType(SimbodyBody());
	Object::RegisterType(SimbodyJoint());
	Object::RegisterType(SimbodyCoordinate());
	Object::RegisterType(SimbodySpeed());
}


//--------------------------------------------------------------------------
// ADDING COMPONENTS
//--------------------------------------------------------------------------
//_____________________________________________________________________________
/**
 * Add a body to the engine
 *
 * @param aBody pointer to the body to add
 */
void SimbodyEngine::addBody(SimbodyBody* aBody)
{
	// TODO:  Fill in Simbody stuff
	_bodySet.append(aBody);
}

//_____________________________________________________________________________
/**
 * Add a joint to the engine
 *
 * @param aJoint pointer to the joint to add
 */
void SimbodyEngine::addJoint(SimbodyJoint* aJoint)
{
	// TODO: Fill in Simbody stuff
	_jointSet.append(aJoint);
}

//_____________________________________________________________________________
/**
 * Add a coordinate to the engine
 *
 * @param aCoord pointer to the coordinate to add
 */
void SimbodyEngine::addCoordinate(SimbodyCoordinate* aCoord)
{
	// TODO: Fill in Simbody stuff
	_coordinateSet.append(aCoord);
}

//_____________________________________________________________________________
/**
 * Add a speed to the engine
 *
 * @param aSpeed pointer to the speed to add
 */
void SimbodyEngine::addSpeed(SimbodySpeed* aSpeed)
{
	// TODO: Fill in Simbody stuff
	_speedSet.append(aSpeed);
}

//--------------------------------------------------------------------------
// COORDINATES
//--------------------------------------------------------------------------
//_____________________________________________________________________________
/**
 * Update all coordinates in the model with the ones in the
 * passed-in coordinate set. If the coordinate does not exist
 * in the model, it is not added.
 *
 * @param aCoordinateSet set of coordinates to be updated/added
 */
void SimbodyEngine::updateCoordinateSet(CoordinateSet& aCoordinateSet)
{
	for (int i = 0; i < aCoordinateSet.getSize(); i++) {
		AbstractCoordinate* modelCoordinate = _coordinateSet.get(aCoordinateSet.get(i)->getName());
		if (modelCoordinate)
			modelCoordinate->updateFromCoordinate(*aCoordinateSet.get(i));
	}

	cout << "Updated coordinates in model " << _model->getName() << endl;
}

//_____________________________________________________________________________
/**
 * Get the set of coordinates that are not locked
 *
 * @param rUnlockedCoordinates set of unlocked coordinates is returned here
 */
void SimbodyEngine::getUnlockedCoordinates(CoordinateSet& rUnlockedCoordinates) const
{
	rUnlockedCoordinates.setSize(0);
	rUnlockedCoordinates.setMemoryOwner(false);

	for (int i = 0; i < _coordinateSet.getSize(); i++)
		if (!_coordinateSet.get(i)->getLocked())
			rUnlockedCoordinates.append(_coordinateSet.get(i));
}

//--------------------------------------------------------------------------
// CONFIGURATION
//--------------------------------------------------------------------------
//done_____________________________________________________________________________
/**
 * Set the configuration (cooridnates and speeds) of the model.
 *
 * @param aY Array of coordinates followed by the speeds.
 */
void SimbodyEngine::setConfiguration(const double aY[])
{
	int nq = getNumCoordinates();
	setConfiguration(aY,&aY[nq]);
}
//done_____________________________________________________________________________
/**
 * Get the configuration (cooridnates and speeds) of the model.
 *
 * @param rY Array of coordinates followed by the speeds.
 */
void SimbodyEngine::getConfiguration(double rY[]) const
{
	int nq = getNumCoordinates();
	getConfiguration(rY,&rY[nq]);
}

//done_____________________________________________________________________________
/**
 * Set the configuration (cooridnates and speeds) of the model.
 *
 * @param aQ Array of generalized coordinates.
 * @param aU Array of generalized speeds.
 */
void SimbodyEngine::setConfiguration(const double aQ[],const double aU[])
{
	// SET Qs
	int nq = getNumCoordinates();
	Vector q(nq,aQ,true);
	_matter.setQ(_s,q);

	// SET Us
	int nu = getNumSpeeds();
	Vector u(nu,aU,true);
	_matter.setU(_s,u);

	// MARK ACTUATOR PATHS AS INVALID
	// TODO: use Observer mechanism
	// TODO: dynamic cast is slow, make invalidate a general method
	ActuatorSet* act = getModel()->getActuatorSet();
	int size = act->getSize();
	for(int i=0; i<size; i++) {
		AbstractMuscle* m = dynamic_cast<AbstractMuscle*>(act->get(i));
		if(m) m->invalidatePath();
	}
}
//done_____________________________________________________________________________
/**
 * Get the configuration (cooridnates and speeds) of the model.
 *
 * @param rQ Array of generalized coordinates.
 * @param rU Array of generalized speeds.
 */
void SimbodyEngine::getConfiguration(double rQ[],double rU[]) const
{
	getCoordinates(rQ);
	getSpeeds(rU);
}

//done_____________________________________________________________________________
/**
 * Get the values of the generalized coordinates.
 *
 * @param rQ Array of coordinates.
 */
void SimbodyEngine::getCoordinates(double rQ[]) const
{
	int nq = getNumCoordinates();
	Vector q(nq,rQ,true);
	q = _matter.getQ(_s);
}

//done_____________________________________________________________________________
/**
 * Get the values of the generalized speeds.
 *
 * @param rU Array of speeds.
 */
void SimbodyEngine::getSpeeds(double rU[]) const
{
	int nu = getNumSpeeds();
	Vector u(nu,rU,true);
	u = _matter.getU(_s);
}

//done_____________________________________________________________________________
/**
 * Get the last-computed values of the accelerations of the generalized
 * coordinates.  For the values to be valid, the method
 * computeDerivatives() must have been called.
 *
 * @param rDUDT Array to be filled with values of the accelerations of the
 * generalized coordinates.  The length of rDUDT should be at least as large
 * as the value returned by getNumSpeeds().
 * @see computeDerivatives()
 * @see getAcceleration(int aIndex)
 * @see getAcceleration(const char* aName);
 */
void SimbodyEngine::getAccelerations(double rDUDT[]) const
{
	int nu = getNumSpeeds();
	Vector dudt(nu,rDUDT,true);
	dudt = _matter.getUDot(_s);
}
//done_____________________________________________________________________________
/**
 * Extract the generalized coordinates and speeds from a combined array of
 * the coordinates and speeds.  This is only a utility method.  The
 * configuration of the model is not changed.
 *
 * @param aY Array of coordinates followed by the speeds.
 * @param rQ Array of coordinates taken from aY. 
 * @param rU Array of speeds taken from aY.
 */
void SimbodyEngine::extractConfiguration(const double aY[],double rQ[],double rU[]) const
{
	int nq = getNumCoordinates();
	memcpy(rQ,aY,nq*sizeof(double));

	int nu = getNumSpeeds();
	memcpy(rU,&aY[nq],nu*sizeof(double));
}

//_____________________________________________________________________________
/**
 * applyDefaultConfiguration
 *
 */
void SimbodyEngine::applyDefaultConfiguration()
{
	throw Exception("SimbodyEngine::applyDefaultConfiguration() not yet implemented.");
}


//--------------------------------------------------------------------------
// GRAVITY
//--------------------------------------------------------------------------
//done_____________________________________________________________________________
/**
 * Set the gravity vector in the gloabl frame.
 *
 * @param aGrav the XYZ gravity vector
 * @return Whether or not the gravity vector was successfully set.
 */
bool SimbodyEngine::setGravity(double aGrav[3])
{
	_gravity.setGravity(_s,Vec3(aGrav[0],aGrav[1],aGrav[2]));
	return true;
}
//done_____________________________________________________________________________
/**
 * Get the gravity vector in the gloabl frame.
 *
 * @param aGrav the XYZ gravity vector
 * @return Whether or not the gravity vector was successfully set.
 */
void SimbodyEngine::getGravity(double aGrav[3]) const
{
	// TODO:  Is there a better way to do this?
	Vec3 g;
	g = _gravity.getGravity(_s);
	aGrav[0] = g[0];
	aGrav[1] = g[1];
	aGrav[2] = g[2];
}


//--------------------------------------------------------------------------
// BODY INFORMATION
//--------------------------------------------------------------------------
//_____________________________________________________________________________
/**
 * Get the body that is being used as ground.
 *
 * @return Pointer to the ground body.
 */
AbstractBody& SimbodyEngine::getGroundBody() const
{
	assert(_groundBody);
	return *_groundBody;
}
//_____________________________________________________________________________
/**
 * Get the SD/FAST index of the body that is being used as ground.
 *
 * @return the index
 */
int SimbodyEngine::getGroundBodyIndex() const
{
	// Should probably call getGroundBody(), dynamic cast it to an SimbodyBody,
	// and then get its index, but we know it should always be -1.
	return GROUND;
}
//_____________________________________________________________________________
/**
 * Determine which body should be treated as the ground body.
 *
 * @return Pointer to the ground body.
 */
AbstractBody* SimbodyEngine::identifyGroundBody()
{
	// The ground body is the one that is named simmGroundName.
	for(int i=0; i<_bodySet.getSize(); i++) {
		if(_bodySet.get(i)->getName() == simbodyGroundName)
			return _bodySet.get(i);
	}

	// If that name is not found, then the first body is selected as ground.
	if(_bodySet.getSize()>0) {
		int j = 0;
		return _bodySet.get(j);
	}

	return NULL;
}
//_____________________________________________________________________________
/**
 * Get tree joint whose child is the given body.
 *
 * @param aBody Pointer to the body.
 */
SimbodyJoint *SimbodyEngine::getInboardTreeJoint(SimbodyBody* aBody) const
{
	for(int i=0; i<_jointSet.getSize(); i++) {
		SimbodyJoint *joint = dynamic_cast<SimbodyJoint*>(_jointSet[i]);
		SimbodyBody *child = joint->getChildBody();
		if((child==aBody) && joint->isTreeJoint())
			return joint;
	}
	return 0;
} 

//_____________________________________________________________________________
/**
 * Adjust to body-to-joint and inboard-to-joint vectors to account for the
 * changed center of mass location of an SD/Fast body
 *
 * @param aBody Pointer to the body.
 * @param aNewMassCenter New mass center location in the SIMM body frame.
 */
bool SimbodyEngine::adjustJointVectorsForNewMassCenter(SimbodyBody* aBody)
{

	return true;
}


//--------------------------------------------------------------------------
// INERTIA
//--------------------------------------------------------------------------
//_____________________________________________________________________________
/**
 * Get the total mass of the model
 *
 * @return the mass of the model
 */
double SimbodyEngine::getMass() const
{
	double totalMass = 0.0;

	for (int i=0; i<_bodySet.getSize(); i++)
		totalMass += _bodySet.get(i)->getMass();

	return totalMass;
}

//_____________________________________________________________________________
/**
 * getSystemInertia
 *
 * @param rM
 * @param rCOM
 * @param rI
 */
void SimbodyEngine::getSystemInertia(double *rM, double rCOM[3], double rI[3][3]) const
{
	throw Exception("SimbodyEngine.getSystemInertia: not yet implemented.");
}

//_____________________________________________________________________________
/**
 * getSystemInertia
 *
 * @param rM
 * @param rCOM
 * @param rI
 */
void SimbodyEngine::getSystemInertia(double *rM, double *rCOM, double *rI) const
{
	throw Exception("SimbodyEngine.getSystemInertia: not yet implemented.");
}

//--------------------------------------------------------------------------
// KINEMATICS
//--------------------------------------------------------------------------
//_____________________________________________________________________________
/**
 * Get the inertial position of a point on a body.
 *
 * Note that the configuration of the model must be set before calling this
 * method.
 *
 * @param aBody Pointer to body.
 * @param aPoint Point on the body expressed in the body-local frame.
 * @param rPos Position of the point in the inertial frame.
 *
 * @see setConfiguration()
 */
void SimbodyEngine::getPosition(const AbstractBody &aBody, const double aPoint[3], double rPos[3]) const
{
	const SimbodyBody* b = dynamic_cast<const SimbodyBody*>(&aBody);

	if(b) {

	}
}

//_____________________________________________________________________________
/**
 * Get the inertial velocity of a point on a body.
 *
 * Note that the configuration of the model must be set before calling this
 * method.
 *
 * @param aBody Pointer to body.
 * @param aPoint Point on the body expressed in the body-local frame.
 * @param rVel Velocity of the point in the inertial frame.
 *
 * @see setConfiguration()
 */
void SimbodyEngine::getVelocity(const AbstractBody &aBody, const double aPoint[3], double rVel[3]) const
{
	const SimbodyBody* b = dynamic_cast<const SimbodyBody*>(&aBody);

	if(b) {

	}
}

//_____________________________________________________________________________
/**
 * Get the inertial acceleration of a point on a body.
 *
 * Note that the configuration of the model must be set and accelerations of
 * the generalized coordinates must be computed before calling this method.
 *
 * @param aBody Pointer to body.
 * @param aPoint Point on the body expressed in the body-local frame.
 * @param rAcc Acceleration of the point in the inertial frame.
 *
 * @see set()
 * @see computeAccelerations()
 */
void SimbodyEngine::getAcceleration(const AbstractBody &aBody, const double aPoint[3], double rAcc[3]) const
{
	const SimbodyBody* b = dynamic_cast<const SimbodyBody*>(&aBody);

	if(b) {

	}
}

//_____________________________________________________________________________
/**
 * Get the body orientation with respect to the ground.
 *
 * @param aBody Pointer to body.
 * @param rDirCos Orientation of the body with respect to the ground frame.
 */
void SimbodyEngine::getDirectionCosines(const AbstractBody &aBody, double rDirCos[3][3]) const
{
	const SimbodyBody* b = dynamic_cast<const SimbodyBody*>(&aBody);

	if(b) {

	}
}

//_____________________________________________________________________________
/**
 * Get the body orientation with respect to the ground.
 *
 * @param aBody Pointer to body.
 * @param rDirCos Orientation of the body with respect to the ground frame.
 */
void SimbodyEngine::getDirectionCosines(const AbstractBody &aBody, double *rDirCos) const
{
	if(!rDirCos) return;
	const SimbodyBody* b = dynamic_cast<const SimbodyBody*>(&aBody);

	if(b) {

	}
}

//_____________________________________________________________________________
/**
 * Get the inertial angular velocity of a body in the ground reference frame.
 *
 * @param aBody Pointer to body.
 * @param rAngVel Angular velocity of the body.
 */
void SimbodyEngine::getAngularVelocity(const AbstractBody &aBody, double rAngVel[3]) const
{
	const SimbodyBody *b = dynamic_cast<const SimbodyBody*>(&aBody);

	if(b) {

	}
}

//_____________________________________________________________________________
/**
 * Get the inertial angular velocity in the local body reference frame.
 *
 * @param aBody Pointer to body.
 * @param rAngVel Angular velocity of the body.
 */
void SimbodyEngine::getAngularVelocityBodyLocal(const AbstractBody &aBody, double rAngVel[3]) const
{
	const SimbodyBody *b = dynamic_cast<const SimbodyBody*>(&aBody);

	if(b) {

	}
}

//_____________________________________________________________________________
/**
 * Get the inertial angular acceleration of a body in the ground reference 
 * frame.
 *
 * @param aBody Pointer to body.
 * @param rAngAcc Angular acceleration of the body.
 */
void SimbodyEngine::getAngularAcceleration(const AbstractBody &aBody, double rAngAcc[3]) const
{
	const SimbodyBody *b = dynamic_cast<const SimbodyBody*>(&aBody);

	if(b) {

	}
}

//_____________________________________________________________________________
/**
 * Get the inertial angular acceleration in the local body reference frame.
 *
 * @param aBody Pointer to body.
 * @param rAngAcc Angular acceleration of the body.
 */
void SimbodyEngine::getAngularAccelerationBodyLocal(const AbstractBody &aBody, double rAngAcc[3]) const
{
	const SimbodyBody *b = dynamic_cast<const SimbodyBody*>(&aBody);

	if(b) {

	}
}

//_____________________________________________________________________________
/**
 * get a copy of the transform from the inertial frame to a body
 *
 * @param aBody
 * @return Transform from inertial frame to body
 */
OpenSim::Transform SimbodyEngine::getTransform(const AbstractBody &aBody)
{
	Transform t;
	const SimbodyBody* b = dynamic_cast<const SimbodyBody*>(&aBody);

	if(b) {

		// 1. Get the coordinates of aBody's origin in its SD/FAST body's frame.

		// 2. Convert the origin of aBody into the inertial frame.

		// 3. Get the orientation of aBody in the inertial frame.

		// 4. Fill in the transform with the translation and rotation.

	}

	return t;
}

//--------------------------------------------------------------------------
// LOAD APPLICATION
//--------------------------------------------------------------------------
// FORCES EXPRESSED IN INERTIAL FRAME
//_____________________________________________________________________________
/**
 * Apply a force to a body
 *
 * @param aBody Body to apply force to
 * @param aPoint Point on body at which force is applied
 * @param aForce Force vector, expressed in inertial frame
 */
void SimbodyEngine::applyForce(const AbstractBody &aBody, const double aPoint[3], const double aForce[3])
{
	const SimbodyBody* body = (const SimbodyBody*)&aBody;

	Vec3 point,force;
	point.getAs(aPoint);
	force.getAs(aForce);
	//_matter.addInStationForce(_s,body->_id,point,force);
}

//_____________________________________________________________________________
/**
 * Apply a set of forces to a set of bodies
 *
 * @param aN Number of forces to apply
 * @param aBodies Array of bodies
 * @param aPoints Array of points of application, expressed in body frames
 * @param aForces Array of forces to apply, expressed in the inertial frame
 */
void SimbodyEngine::applyForces(int aN, const AbstractBody *aBodies[], const double aPoints[][3], const double aForces[][3])
{
	for(int i=0; i<aN; i++) {
		applyForce(*aBodies[i], aPoints[i], aForces[i]);
	}
}

//_____________________________________________________________________________
/**
 * Apply a set of forces to a set of bodies
 *
 * @param aN Number of forces to apply
 * @param aBodies Array of bodies
 * @param aPoints Array of points of application, expressed in body frames
 * @param aForces Array of forces to apply, expressed in the inertial frame
 */
void SimbodyEngine::applyForces(int aN, const AbstractBody *aBodies[], const double *aPoints, const double *aForces)
{
	// Check that the input vectors have been defined.
	if (!aBodies || !aPoints || !aForces)  return;

	int I;
	for(int i=0; i<aN; i++) {
		I = Mtx::ComputeIndex(i, 3, 0);
		applyForce(*aBodies[i], &aPoints[I], &aForces[I]);
	}
}

//_____________________________________________________________________________
/**
 * Apply a force to a body
 *
 * @param aBody Body to apply force to
 * @param aPoint Point on body at which to apply force
 * @param aForce Force to apply, expressed in body frame
 */
void SimbodyEngine::applyForceBodyLocal(const AbstractBody &aBody, const double aPoint[3], const double aForce[3])
{
	const SimbodyBody* body = dynamic_cast<const SimbodyBody*>(&aBody);

	if(body) {

	}
}

//_____________________________________________________________________________
/**
 * Apply a set of forces to a set of bodies
 *
 * @param aN Number of forces to apply
 * @param aBodies Array of bodies
 * @param aPoints Array of points of application, expressed in body frames
 * @param aForces Array of forces to apply, expressed in the body frames
 */
void SimbodyEngine::applyForcesBodyLocal(int aN, const AbstractBody *aBodies[], const double aPoints[][3], const double aForces[][3])
{
	int i;

	for (i = 0; i < aN; i++)
		applyForce(*aBodies[i], aPoints[i], aForces[i]);
}

//_____________________________________________________________________________
/**
 * Apply a set of forces to a set of bodies
 *
 * @param aN Number of forces to apply
 * @param aBodies Array of bodies
 * @param aPoints Array of points of application, expressed in body frames
 * @param aForces Array of forces to apply, expressed in the body frames
 */
void SimbodyEngine::applyForcesBodyLocal(int aN, const AbstractBody *aBodies[], const double *aPoints, const double *aForces)
{
	// Check that the input vectors have been defined.
	if (!aBodies || !aPoints || !aForces)
		return;

	int i;
	double point[3], force[3];

	for (i = 0; i < aN; i++)
	{
		point[0] = *aPoints++;
		point[1] = *aPoints++;
		point[2] = *aPoints;

		force[0] = *aForces++;
		force[1] = *aForces++;
		force[2] = *aForces;

		applyForceBodyLocal(*aBodies[i], point, force);
	}
}

//_____________________________________________________________________________
/**
 * Apply a torque expressed in the inertial frame to a body.
 *
 * @param aBody Pointer to body.
 * @param aTorque Torque expressed in the inertial frame.
 */
void SimbodyEngine::applyTorque(const AbstractBody &aBody, const double aTorque[3])
{
	const SimbodyBody* b = dynamic_cast<const SimbodyBody*>(&aBody);

	if(b) {

	}
}

//_____________________________________________________________________________
/**
 * Apply a set of torques expressed in the inertial frame to a set of bodies.
 *
 * @param aN Number of Torques.
 * @param aBody Array of Body pointers.
 * @param aTorques Array of torques applied to the body expressed the inertial 
 * frame.
 */
void SimbodyEngine::applyTorques(int aN, const AbstractBody *aBodies[], const double aTorques[][3])
{
	for (int i=0; i<aN; i++) {
		applyTorque(*aBodies[i], aTorques[i]);
	}
}

//_____________________________________________________________________________
/**
 * Apply a set of torques expressed in the inertial frame to a set of bodies.
 *
 * @param aN Number of Torques.
 * @param aBody Array of Body pointers.
 * @param aTorques Array of torques applied to the body expressed the inertial 
 * frame.
 */
void SimbodyEngine::applyTorques(int aN, const AbstractBody *aBodies[], const double *aTorques)
{
	if(aTorques) {
		double torque[3];

		for (int i=0; i<aN; i++) {
			torque[0] = *(aTorques++);
			torque[1] = *(aTorques++);
			torque[2] = *(aTorques++);
			applyTorque(*aBodies[i], torque);
		}
	}
}

// TORQUES EXPRESSED IN BODY-LOCAL FRAME (sdbodyt())
//_____________________________________________________________________________
/**
 * Apply a torque expressed in the body-local frame to a body.
 *
 * @param aBody Pointer to body.
 * @param aTorque Torque expressed in the body-local frame.
 */
void SimbodyEngine::applyTorqueBodyLocal(const AbstractBody &aBody, const double aTorque[3])
{
	const SimbodyBody *b = dynamic_cast<const SimbodyBody*>(&aBody);

	if(b) {

	}
}

//_____________________________________________________________________________
/**
 * Apply a set of torques expressed in the body-local frame to a set of bodies.
 *
 * @param aN Number of Torques.
 * @param aBody Array of Body pointers.
 * @param aTorques Array of torques applied to the body expressed the 
 * body-local frame.
 */
void SimbodyEngine::applyTorquesBodyLocal(int aN, const AbstractBody *aBodies[], const double aTorques[][3])
{
	for(int i=0; i<aN; i++) {
		applyTorqueBodyLocal(*aBodies[i], aTorques[i]);
	}
}

//_____________________________________________________________________________
/**
 * Apply a set of torques expressed in the body-local frame to a set of bodies.
 *
 * @param aN Number of Torques.
 * @param aBody Array of Body pointers.
 * @param aTorques Array of torques applied to the body expressed the 
 * body-local frame.
 */
void SimbodyEngine::applyTorquesBodyLocal(int aN, const AbstractBody *aBodies[], const double *aTorques)
{
	if (aTorques)
	{
		int i;
		double torque[3];

		for (i = 0; i < aN; i++)
		{
			torque[0] = *(aTorques++);
			torque[1] = *(aTorques++);
			torque[2] = *(aTorques++);

			applyTorqueBodyLocal(*aBodies[i], torque);
		}
	}
}

// GENERALIZED FORCES
//_____________________________________________________________________________
/**
 * Apply a generalized force to a generalized coordinate.
 * Note that depending on the axis type the generalized force can be a
 * torque or a force.
 * @param aU Generalized coordinate.
 * @param aF Applied force.
 */
void SimbodyEngine::applyGeneralizedForce(const AbstractCoordinate &aU, double aF)
{
	const SimbodyCoordinate *c = dynamic_cast<const SimbodyCoordinate*>(&aU);

	if(c) {

	}
}

//_____________________________________________________________________________
/**
 * Apply generalized forces.
 * The dimension of aF is assumed to be the number of generalized coordinates.
 * @param aF Applied force.
 */
void SimbodyEngine::applyGeneralizedForces(const double aF[])
{
	for(int i=0; i<_coordinateSet.getSize(); i++) {
		applyGeneralizedForce(*_coordinateSet.get(i), aF[i]);
	}
}

//_____________________________________________________________________________
/**
 * Apply generalized forces.
 * @param aN Number of generalized forces.
 * @param aU Generalized coordinates.
 * @param aF Applied forces.
 */
void SimbodyEngine::applyGeneralizedForces(int aN, const AbstractCoordinate *aU[], const double aF[])
{
	for(int i=0; i<aN; i++) {
		applyGeneralizedForce(*aU[i], aF[i]);
	}
}

//--------------------------------------------------------------------------
// LOAD ACCESS AND COMPUTATION
//--------------------------------------------------------------------------
//_____________________________________________________________________________
/**
 * Get the net applied generalized force.  The returned force is the sum of
 * all applied forces plus any forces needed for any prescribed motions.
 * The methods setState() (or equivalent) and computeAccelerations() must
 * be called prior to calling this method for the returned result to be
 * valid.
 *
 * @param aU Generalized speed (degree of freedom).
 * @return Net applied force/torque at degree of freedom aU.
 */
double SimbodyEngine::getNetAppliedGeneralizedForce(const AbstractCoordinate &aU) const
{
	const SimbodyCoordinate *c = dynamic_cast<const SimbodyCoordinate*>(&aU);

	if(c) {

	}

	return 0.0;
}

//_____________________________________________________________________________
/**
 * Compute the generalized forces necessary to achieve a set of specified
 * accelerations.  If any forces have been applied to the model, the balance
 * of generalized forces needed to achieve the desired accelerations is
 * computed.  Note that constraints are not taken into account by this
 * method.
 *
 * @param aDUDT Array of desired accelerations of the generalized coordinates-
 * should be dimensioned to NU (see getNumSpeeds()).
 * @param rF Array of generalized forces that will achieve aDUDT without
 * enforcing any constraints- should be dimensioned to NU (see getNumSpeeds()).
 */
void SimbodyEngine::computeGeneralizedForces(double aDUDT[], double rF[]) const
{


}

//_____________________________________________________________________________
/**
 * Compute the reaction forces and torques at all the joints in the model.
 *
 * It is necessary to call computeAccelerations() before this method
 * to get valid results.  This method is expensive to call, beyond the
 * expense of computing the accelerations.  So, this method should be
 * called as infrequently as possible.
 *
 * @param rForces Matrix of reaction forces.  The size should be
 * at least NumberOfJoints x 3.
 * @param rTorques Matrix of reaction torques.  The size should be
 * at least NumberOfJoints x 3.
 */
void SimbodyEngine::computeReactions(double rForces[][3], double rTorques[][3]) const
{

}


//--------------------------------------------------------------------------
// DERIVATIVES
//--------------------------------------------------------------------------
//_____________________________________________________________________________
/**
 * Compute the derivatives of the generalized coordinates and speeds.
 *
 * @param dqdt Derivatives of generalized coordinates.
 * @param dudt Derivatives of generalized speeds.
 */
void SimbodyEngine::computeDerivatives(double *dqdt,double *dudt)
{
	//_s.updTime() = t;

	// COMPUTE ACCELERATIONS
	try {
		_system.realize(_s,Stage::Acceleration);
	} catch(...) {
		cout<<"SimbodyEngine.computeDerivatives: invalid derivatives.\n\n";
		return;
	}
	Vector qdot = _s.getQDot();
	Vector udot = _s.getUDot();

	// ASSIGN THEM (MAYBE SLOW BUT CORRECT
	int nq = _s.getNQ();
	for(int i=0;i<nq;i++) dqdt[i] = qdot[i];

	int nu = _s.getNU();
	for(int i=0;i<nu;i++) dudt[i] = udot[i];
}


//--------------------------------------------------------------------------
// UTILITY
//--------------------------------------------------------------------------
//_____________________________________________________________________________
/**
 * Transform a vector from one body to another
 *
 * @param aBodyFrom the body in which the vector is currently expressed
 * @param aPos the vector to be transformed
 * @param aBodyTo the body the vector will be transformed into
 * @param rPos the vector in the aBodyTo frame is returned here
 */
void SimbodyEngine::transform(const AbstractBody &aBodyFrom, const double aVec[3], const AbstractBody &aBodyTo, double rVec[3]) const
{
	for(int i=0; i<3; i++)  rVec[i] = aVec[i];

	if(&aBodyFrom == &aBodyTo) return;

	const SimbodyBody* b1 = dynamic_cast<const SimbodyBody*>(&aBodyFrom);
	const SimbodyBody* b2 = dynamic_cast<const SimbodyBody*>(&aBodyTo);

	if(b1 && b2) {

	}
}

//_____________________________________________________________________________
/**
 * Transform a vector from one body to another
 *
 * @param aBodyFrom the body in which the vector is currently expressed
 * @param aPos the vector to be transformed
 * @param aBodyTo the body the vector will be transformed into
 * @param rPos the vector in the aBodyTo frame is returned here
 */
void SimbodyEngine::transform(const AbstractBody &aBodyFrom, const OpenSim::Array<double>& aVec, const AbstractBody &aBodyTo, OpenSim::Array<double>& rVec) const
{
	for(int i=0; i<3; i++)  rVec[i] = aVec[i];

	if(&aBodyFrom == &aBodyTo) return;

	const SimbodyBody* b1 = dynamic_cast<const SimbodyBody*>(&aBodyFrom);
	const SimbodyBody* b2 = dynamic_cast<const SimbodyBody*>(&aBodyTo);

	if (b1 && b2) {

	}
}

//_____________________________________________________________________________
/**
 * Transform a point from one body to another
 *
 * @param aBodyFrom the body in which the point is currently expressed
 * @param aPos the XYZ coordinates of the point
 * @param aBodyTo the body the point will be transformed into
 * @param rPos the XYZ coordinates of the point in the aBodyTo frame are returned here
 */
void SimbodyEngine::
transformPosition(const AbstractBody &aBodyFrom, const double aPos[3],
	const AbstractBody &aBodyTo, double rPos[3]) const
{

}

//_____________________________________________________________________________
/**
 * Transform a point from one body to another
 *
 * @param aBodyFrom the body in which the point is currently expressed
 * @param aPos the XYZ coordinates of the point
 * @param aBodyTo the body the point will be transformed into
 * @param rPos the XYZ coordinates of the point in the aBodyTo frame are returned here
 */
void SimbodyEngine::
transformPosition(const AbstractBody &aBodyFrom, const OpenSim::Array<double>& aPos,
	const AbstractBody &aBodyTo, OpenSim::Array<double>& rPos) const
{
	double aPos2[3], rPos2[3];

	for(int i=0; i<3; i++)  aPos2[i] = aPos[i];

	transformPosition(aBodyFrom, aPos2, aBodyTo, rPos2);

	for(int i=0; i<3; i++) rPos[i] = rPos2[i];
}

//_____________________________________________________________________________
/**
 * Transform a point from one body to the ground body
 *
 * @param aBodyFrom the body in which the point is currently expressed
 * @param aPos the XYZ coordinates of the point
 * @param rPos the XYZ coordinates of the point in the ground frame are returned here
 */
void SimbodyEngine::transformPosition(const AbstractBody &aBodyFrom, const double aPos[3], double rPos[3]) const
{

}

//_____________________________________________________________________________
/**
 * Transform a point from one body to the ground body
 *
 * @param aBodyFrom the body in which the point is currently expressed
 * @param aPos the XYZ coordinates of the point
 * @param rPos the XYZ coordinates of the point in the ground frame are returned here
 */
void SimbodyEngine::
transformPosition(const AbstractBody &aBodyFrom,const OpenSim::Array<double>& aPos,
	OpenSim::Array<double>& rPos) const
{
	double aPos2[3], rPos2[3];
	for(int i = 0; i<3; i++)  aPos2[i] = aPos[i];

	transformPosition(aBodyFrom, aPos2, rPos2);

	for(int i=0; i<3; i++)  rPos[i] = rPos2[i];
}

//_____________________________________________________________________________
/**
 * Calculate the distance between a point on one body and a point on another body
 *
 * @param aBody1 the body that the first point is expressed in
 * @param aPoint1 the XYZ coordinates of the first point
 * @param aBody2 the body that the second point is expressed in
 * @param aPoint2 the XYZ coordinates of the second point
 * @return the distance between aPoint1 and aPoint2
 */
double SimbodyEngine::
calcDistance(const AbstractBody& aBody1, const OpenSim::Array<double>& aPoint1,
	const AbstractBody& aBody2, const OpenSim::Array<double>& aPoint2) const
{
	const SimbodyBody* b1 = dynamic_cast<const SimbodyBody*>(&aBody1);
	const SimbodyBody* b2 = dynamic_cast<const SimbodyBody*>(&aBody2);

	if(b1 && b2) {

	}

	return 0.0;
}

//_____________________________________________________________________________
/**
 * Calculate the distance between a point on one body and a point on another body
 *
 * @param aBody1 the body that the first point is expressed in
 * @param aPoint1 the XYZ coordinates of the first point
 * @param aBody2 the body that the second point is expressed in
 * @param aPoint2 the XYZ coordinates of the second point
 * @return the distance between aPoint1 and aPoint2
 */
double SimbodyEngine::calcDistance(const AbstractBody& aBody1, const double aPoint1[3], const AbstractBody& aBody2, const double aPoint2[3]) const
{
	const SimbodyBody* b1 = dynamic_cast<const SimbodyBody*>(&aBody1);
	const SimbodyBody* b2 = dynamic_cast<const SimbodyBody*>(&aBody2);

	if(b1 && b2) {

	}

	return 0.0;
}

//_____________________________________________________________________________
/**
 * Convert quaterions to angles.
 *
 * @param aQ Array of generalized coordinates, some of which may be
 * quaternions.  The length of aQ must be at least getNumCoordinates().
 * @param rQAng Array of equivalent angles.
 */
void SimbodyEngine::convertQuaternionsToAngles(double *aQ, double *rQAng) const
{

}

//_____________________________________________________________________________
/**
 * For all the generalized coordinates held in a storage object, convert the
 * generalized coordinates expressed in quaternions to Euler angles.
 *
 * @param rQStore Storage object of generalized coordinates, some of which
 * may be quaternions.  The length of each state-vector in rQStore must be
 * at least getNumCoordinates().
 */
void SimbodyEngine::convertQuaternionsToAngles(Storage *rQStore) const
{
	if(rQStore==NULL) return;

	// NUMBER OF Q'S
	int nq = getNumCoordinates();
	int nu = getNumSpeeds();
	int dn = nq - nu;
	if(nq<=0) {
		printf("rdSDFast.convertQuaternionsToAngles: ERROR- models has ");
		printf("no generalized coordinates.\n");
		return;
	}

	// LOOP THROUGH STATE-VECTORS
	int i;
	int size,newSize=0;
	double t,*data,*newData=NULL;
	StateVector *vec;
	for(i=0;i<rQStore->getSize();i++) {

		// GET STATE-VECTOR
		vec = rQStore->getStateVector(i);
		if(vec==NULL) continue;

		// CHECK SIZE
		size = vec->getSize();
		if(size<nq) {
			printf("rdSDFast.convertQuaternionsToAngles: WARN- the size of ");
			printf("a state-vector is less than nq(%d).\n",nq);
			continue;
		}

		// GET DATA
		t = vec->getTime();
		data = vec->getData().get();
		if(data==NULL) continue;

		// ALLOCATE NEW DATA IF NECESSARY
		if(newSize<(size-dn)) {
			if(newData!=NULL) delete[] newData;
			newSize = size-dn;
			newData = new double[newSize];
		}

		// CONVERT QUATERNIONS TO ANGLES
		convertQuaternionsToAngles(data,newData);

		// FILL IN THE REST OF THE DATA
		for(i=nu;i<(size-dn);i++) {
			newData[i] = data[i+dn];
		}

		// CHANGE THE STATE-VECTOR
		vec->setStates(t,newSize,newData);
	}

	// CHANGE THE COLUMN LABELS
	cout<<"rdSDFast.convertQuaternionsToAngles: NOTE- the column labels"<<
		" for "<<rQStore->getName()<<" were not changed."<<endl;

	// CLEANUP
	if(newData!=NULL) delete[] newData;
}

//_____________________________________________________________________________
/**
 * Convert angles to quaterions.
 *
 * @param aQAng Array of generalized coordinates expressed in Euler angles.
 * The length of aQAng must be at least getNumSpeeds().
 * @param rQ Vector of equivalent quaternions.
 */
void SimbodyEngine::convertAnglesToQuaternions(double *aQAng, double *rQ) const
{

}

//_____________________________________________________________________________
/**
 * For all the generalized coordinates held in a storage object, convert the
 * generalized coordinates expressed in Euler angles to quaternions when
 * appropriate.
 *
 * @param rQStore Storage object of generalized coordinates that has all
 * angles expressed as Euler angles in radians.  The length of each
 * state-vector in rQStore must be at least getNumSpeeds().
 */
void SimbodyEngine::convertAnglesToQuaternions(Storage *rQStore) const
{
	if(rQStore==NULL) return;

	// NUMBER OF Q'S
	int nq = getNumCoordinates();
	int nu = getNumSpeeds();
	int dn = nq - nu;
	if(nu<=0) {
		printf("rdSDFast.convertAnglesToQuaternions: ERROR- models has ");
		printf("no generalized coordinates.\n");
		return;
	}
	if(dn<=0) return;

	// LOOP THROUGH THE STATE-VECTORS
	int i,j;
	int size,newSize=0;
	double t,*data,*newData=NULL;
	StateVector *vec;
	for(i=0;i<rQStore->getSize();i++) {

		// GET STATE-VECTOR
		vec = rQStore->getStateVector(i);
		if(vec==NULL) continue;

		// CHECK SIZE
		size = vec->getSize();
		if(size<nu) {
			printf("rdSDFast.convertAnglesToQuaternions: WARN- the size of ");
			printf("a state-vector is less than nu(%d).\n",nu);
			continue;
		}

		// GET DATA
		t = vec->getTime();
		data = vec->getData().get();
		if(data==NULL) continue;

		// ALLOCATE NEW DATA IF NECESSARY
		if(newSize<(size+dn)) {
			if(newData!=NULL) delete[] newData;
			newSize = size+dn;
			newData = new double[newSize];
		}

		// CONVERT QUATERNIONS TO ANGLES
		convertAnglesToQuaternions(data,newData);

		// FILL IN THE REST OF THE DATA
		for(j=nu;j<size;j++) {
			newData[j+dn] = data[j];
		}

		// CHANGE THE STATE-VECTOR
		vec->setStates(t,newSize,newData);
	}

	// CHANGE THE COLUMN LABELS
	cout<<"rdSDFast.convertAnglesToQuaternions: NOTE- the column labels"<<
		" for "<<rQStore->getName()<<" were not changed."<<endl;

	// CLEANUP
	if(newData!=NULL) delete[] newData;
}

//_____________________________________________________________________________
/**
 * Convert angles to direction cosines.
 * @param aE1 1st Euler angle.
 * @param aE2 2nd Euler angle.
 * @param aE3 3rd Euler angle.
 * @param rDirCos Vector of direction cosines.
 */
void SimbodyEngine::convertAnglesToDirectionCosines(double aE1, double aE2, double aE3, double rDirCos[3][3]) const
{

}

//_____________________________________________________________________________
/**
 * Convert angles to direction cosines.
 * @param aE1 1st Euler angle.
 * @param aE2 2nd Euler angle.
 * @param aE3 3rd Euler angle.
 * @param rDirCos Vector of direction cosines.
 */
void SimbodyEngine::convertAnglesToDirectionCosines(double aE1, double aE2, double aE3, double *rDirCos) const
{
	if(rDirCos==NULL) return;

}

//_____________________________________________________________________________
/**
 * Convert direction cosines to angles.
 * @param aDirCos Vector of direction cosines.
 * @param rE1 1st Euler angle.
 * @param rE2 2nd Euler angle.
 * @param rE3 3rd Euler angle.
 */
void SimbodyEngine::convertDirectionCosinesToAngles(double aDirCos[3][3], double *rE1, double *rE2, double *rE3) const
{

}

//_____________________________________________________________________________
/**
 * Convert direction cosines to angles.
 * @param aDirCos Vector of direction cosines.
 * @param rE1 1st Euler angle.
 * @param rE2 2nd Euler angle.
 * @param rE3 3rd Euler angle.
 */
void SimbodyEngine::convertDirectionCosinesToAngles(double *aDirCos, double *rE1, double *rE2, double *rE3) const
{
	if(!aDirCos) return;

}

//_____________________________________________________________________________
/**
 * Convert direction cosines to quaternions.
 * @param aDirCos Vector of direction cosines.
 * @param rQ1 1st Quaternion.
 * @param rQ2 2nd Quaternion.
 * @param rQ3 3rd Quaternion.
 * @param rQ4 4th Quaternion.
 */
void SimbodyEngine::convertDirectionCosinesToQuaternions(double aDirCos[3][3], double *rQ1, double *rQ2, double *rQ3, double *rQ4) const
{

}

//_____________________________________________________________________________
/**
 * Convert direction cosines to quaternions.
 * @param aDirCos Vector of direction cosines.
 * @param rQ1 1st Quaternion.
 * @param rQ2 2nd Quaternion.
 * @param rQ3 3rd Quaternion.
 * @param rQ4 4th Quaternion.
 */
void SimbodyEngine::convertDirectionCosinesToQuaternions(double *aDirCos, double *rQ1, double *rQ2, double *rQ3, double *rQ4) const
{
	if(aDirCos==NULL) return;

}

//_____________________________________________________________________________
/**
 * Convert quaternions to direction cosines.
 * @param aQ1 1st Quaternion.
 * @param aQ2 2nd Quaternion.
 * @param aQ3 3rd Quaternion.
 * @param aQ4 4th Quaternion.
 * @param rDirCos Vector of direction cosines.
 */
void SimbodyEngine::convertQuaternionsToDirectionCosines(double aQ1, double aQ2, double aQ3, double aQ4, double rDirCos[3][3]) const
{

}

//_____________________________________________________________________________
/**
 * Convert quaternions to direction cosines.
 * @param aQ1 1st Quaternion.
 * @param aQ2 2nd Quaternion.
 * @param aQ3 3rd Quaternion.
 * @param aQ4 4th Quaternion.
 * @param rDirCos Vector of direction cosines.
 */
void SimbodyEngine::convertQuaternionsToDirectionCosines(double aQ1, double aQ2, double aQ3, double aQ4, double *rDirCos) const
{
	if(rDirCos==NULL) return;

}
