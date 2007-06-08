// SimbodyRotationDof.cpp
// Author: Peter Loan, Frank C. Anderson
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
#include "SimbodyRotationDof.h"
#include <OpenSim/Simulation/Model/AbstractCoordinate.h>

//=============================================================================
// STATICS
//=============================================================================
using namespace std;
using namespace SimTK;
using namespace OpenSim;

//=============================================================================
// CONSTRUCTOR(S) AND DESTRUCTOR
//=============================================================================
//_____________________________________________________________________________
/**
 * Default constructor.
 */
SimbodyRotationDof::SimbodyRotationDof() :
   _axis(_axisProp.getValueDblArray())
{
	setNull();
	setupProperties();
}

//_____________________________________________________________________________
/**
 * Destructor.
 */
SimbodyRotationDof::~SimbodyRotationDof()
{
}

//_____________________________________________________________________________
/**
 * Copy constructor.
 *
 * @param aDof SimbodyRotationDof to be copied.
 */
SimbodyRotationDof::SimbodyRotationDof(const SimbodyRotationDof &aDof) :
   AbstractDof(aDof),
   _axis(_axisProp.getValueDblArray())
{
	setNull();
	setupProperties();
	copyData(aDof);
}

//_____________________________________________________________________________
/**
 * Copy this dof and return a pointer to the copy.
 * The copy constructor for this class is used.
 *
 * @return Pointer to a copy of this SimbodyRotationDof.
 */
Object* SimbodyRotationDof::copy() const
{
	SimbodyRotationDof *dof = new SimbodyRotationDof(*this);
	return(dof);
}

//=============================================================================
// CONSTRUCTION METHODS
//=============================================================================
//_____________________________________________________________________________
/**
 * Copy data members from one SimbodyRotationDof to another.
 *
 * @param aDof SimbodyRotationDof to be copied.
 */
void SimbodyRotationDof::copyData(const SimbodyRotationDof &aDof)
{
	_axis = aDof._axis;
}

//_____________________________________________________________________________
/**
 * Set the data members of this SimbodyRotationDof to their null values.
 */
void SimbodyRotationDof::setNull()
{
	setType("SimbodyRotationDof");
}

//_____________________________________________________________________________
/**
 * Connect properties to local pointers.
 */
void SimbodyRotationDof::setupProperties()
{
	const double defaultAxis[] = {1.0, 0.0, 0.0};
	_axisProp.setName("axis");
	_axisProp.setValue(3, defaultAxis);
	_propertySet.append(&_axisProp);
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
SimbodyRotationDof& SimbodyRotationDof::operator=(const SimbodyRotationDof &aDof)
{
	// BASE CLASS
	AbstractDof::operator=(aDof);

	copyData(aDof);

	return(*this);
}

//=============================================================================
// GET AND SET
//=============================================================================
//_____________________________________________________________________________
/**
 * Get the rotation axis.
 *
 * @param rAxis the rotation axis is returned here.
 */
void SimbodyRotationDof::getAxis(double rAxis[3]) const
{
	rAxis[0] = _axis[0];
	rAxis[1] = _axis[1];
	rAxis[2] = _axis[2];
}

//_____________________________________________________________________________
/**
 * Get the current value of the rotation dof
 *
 * @return The current value of the dof.
 */
double SimbodyRotationDof::getValue()
{
	if (_coordinate)
		return _function->evaluate(0, _coordinate->getValue(), 0.0, 0.0);
	else
		return _function->evaluate(0, 0.0, 0.0, 0.0);
}

void SimbodyRotationDof::peteTest()
{
	cout << "RotationDof: " << getName() << endl;
	cout << "   value: " << getValue() << endl;
	cout << "   coordinate: " << _coordinateName << endl;
	if (_function) cout << "   function: " << *_function << endl;
}
