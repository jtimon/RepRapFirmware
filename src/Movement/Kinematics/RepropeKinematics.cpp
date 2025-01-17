/*
 * RepropeKinematics.cpp
 *
 *  Created on: 4 Sep 2022
 *      Author: jtimon
 */

#include "RepropeKinematics.h"

#if SUPPORT_REPROPE

#include <Platform/RepRap.h>
#include <Platform/Platform.h>
#include <GCodes/GCodeBuffer/GCodeBuffer.h>
#include <Movement/Move.h>
#include <CAN/CanInterface.h>

#include <General/Portability.h>

constexpr float DefaultAnchorZ = 260.0;
// TODO Move away from RoundBedKinematics, allow max_x and max_y to be different
// max_x = 260
// max_y = 260
// pulley_distance_from_corner = 20 + 15 + 42/2 = 56
// pulley_distance_from_frame = 15
// 260 / 2 - pulley_distance_from_corner = 130 - 56 = 74
// 260 / 2 - pulley_distance_from_frame = 130 - 15 = 115

// Default anchor coordinates
// These are only placeholders. Each machine must have these values calibrated in order to work correctly.
constexpr float DefaultAnchors[8][3] = {{ -115.0,    74.0, DefaultAnchorZ},
                                        {  -74.0,   115.0, DefaultAnchorZ},
                                        {   74.0,   115.0, DefaultAnchorZ},
                                        {  115.0,    74.0, DefaultAnchorZ},
                                        {  115.0,   -74.0, DefaultAnchorZ},
                                        {   74.0,  -115.0, DefaultAnchorZ},
                                        {  -74.0,  -115.0, DefaultAnchorZ},
                                        { -115.0,   -74.0, DefaultAnchorZ}};
constexpr float DefaultPrintRadius = 1500.0;

#if SUPPORT_OBJECT_MODEL

// Object model table and functions
// Note: if using GCC version 7.3.1 20180622 and lambda functions are used in this table, you must compile this file with option -std=gnu++17.
// Otherwise the table will be allocated in RAM instead of flash, which wastes too much RAM.

// Macro to build a standard lambda function that includes the necessary type conversions
#define OBJECT_MODEL_FUNC(...) OBJECT_MODEL_FUNC_BODY(RepropeKinematics, __VA_ARGS__)

constexpr ObjectModelArrayDescriptor RepropeKinematics::anchorCoordinatesArrayDescriptor =
{
	nullptr,					// no lock needed
	[] (const ObjectModel *self, const ObjectExplorationContext&) noexcept -> size_t { return 3; },
	[] (const ObjectModel *self, ObjectExplorationContext& context) noexcept -> ExpressionValue { return ExpressionValue(((const RepropeKinematics *)self)->anchors[context.GetIndex(1)][context.GetLastIndex()], 1); }
};

constexpr ObjectModelArrayDescriptor RepropeKinematics::anchorsArrayDescriptor =
{
	nullptr,					// no lock needed
	[] (const ObjectModel *self, const ObjectExplorationContext&) noexcept -> size_t { return REPROPE_AXES; },
	[] (const ObjectModel *self, ObjectExplorationContext& context) noexcept -> ExpressionValue { return ExpressionValue(&anchorCoordinatesArrayDescriptor); }
};

constexpr ObjectModelTableEntry RepropeKinematics::objectModelTable[] =
{
	// Within each group, these entries must be in alphabetical order
	// 0. kinematics members
	{ "anchors",		OBJECT_MODEL_FUNC_NOSELF(&anchorsArrayDescriptor), 	ObjectModelEntryFlags::none },
	{ "name",			OBJECT_MODEL_FUNC(self->GetName(true)), 			ObjectModelEntryFlags::none },
	{ "printRadius",	OBJECT_MODEL_FUNC(self->printRadius, 1), 			ObjectModelEntryFlags::none },
};

constexpr uint8_t RepropeKinematics::objectModelTableDescriptor[] = { 1, 3 };

DEFINE_GET_OBJECT_MODEL_TABLE_WITH_PARENT(RepropeKinematics, Kinematics)

#endif

// Constructor
RepropeKinematics::RepropeKinematics() noexcept
	: RoundBedKinematics(KinematicsType::reprope, SegmentationType(true, true, true))
{
	Init();
}

void RepropeKinematics::Init() noexcept
{
	/* Naive buildup factor calculation (assumes cylindrical, straight line)
	 * line diameter: 0.5 mm
	 * spool height: 8.0 mm
	 * (line_cross_section_area)/(height*pi): ((0.5/2)*(0.5/2)*pi)/(8.0*pi) = 0.0078 mm
	 * Default buildup factor for 0.50 mm FireLine: 0.0078
	 * Default buildup factor for 0.39 mm FireLine: 0.00475
	 * In practice you might want to compensate a bit more or a bit less */
	constexpr float DefaultSpoolBuildupFactor = 0.007;
	/* Measure and set spool radii with M669 to achieve better accuracy */
	constexpr float DefaultSpoolRadii[8] = { 15.0, 15.0, 15.0, 15.0, 15.0, 15.0, 15.0, 15.0};
	/* If axis runs lines back through pulley system, set mechanical advantage accordingly with M669 */
	constexpr uint32_t DefaultMechanicalAdvantage[8] = { 2, 2, 2, 2, 2, 2, 2, 2};
	constexpr uint32_t DefaultLinesPerSpool[8] = { 1, 1, 1, 1, 1, 1, 1, 1};
	constexpr uint32_t DefaultMotorGearTeeth[8] = {  20,  20,  20,  20,  20,  20,  20,  20};
	constexpr uint32_t DefaultSpoolGearTeeth[8] = { 255, 255, 255, 255, 255, 255, 255, 255};
	constexpr uint32_t DefaultFullStepsPerMotorRev[8] = { 25, 25, 25, 25, 25, 25, 25, 25};
	ARRAY_INIT(anchors, DefaultAnchors);
	printRadius = DefaultPrintRadius;
	spoolBuildupFactor = DefaultSpoolBuildupFactor;
	ARRAY_INIT(spoolRadii, DefaultSpoolRadii);
	ARRAY_INIT(mechanicalAdvantage, DefaultMechanicalAdvantage);
	ARRAY_INIT(linesPerSpool, DefaultLinesPerSpool);
	ARRAY_INIT(motorGearTeeth, DefaultMotorGearTeeth);
	ARRAY_INIT(spoolGearTeeth, DefaultSpoolGearTeeth);
	ARRAY_INIT(fullStepsPerMotorRev, DefaultFullStepsPerMotorRev);
	Recalc();
}

// Recalculate the derived parameters
void RepropeKinematics::Recalc() noexcept
{
	printRadiusSquared = fsquare(printRadius);

	// This is the difference between a "line length" and a "line position"
	// "line length" == ("line position" + "line length in origin")
	for (size_t i = 0; i < REPROPE_AXES; ++i)
	{
		lineLengthsOrigin[i] = fastSqrtf(fsquare(anchors[i][0]) + fsquare(anchors[i][1]) + fsquare(anchors[i][2]));
	}

	// Line buildup compensation
	float stepsPerUnitTimesRTmp[REPROPE_AXES] = { 0.0 };
	Platform& platform = reprap.GetPlatform(); // No const because we want to set drive steper per unit
	for (size_t i = 0; i < REPROPE_AXES; i++)
	{
		const uint8_t driver = platform.GetAxisDriversConfig(i).driverNumbers[0].localDriver; // Only supports single driver
		bool dummy;
		stepsPerUnitTimesRTmp[i] =
			(
				(float)(mechanicalAdvantage[i])
				* fullStepsPerMotorRev[i]
				* platform.GetMicrostepping(driver, dummy)
				* spoolGearTeeth[i]
			)
			/ (2.0 * Pi * motorGearTeeth[i]);

		k2[i] = -(float)(mechanicalAdvantage[i] * linesPerSpool[i]) * spoolBuildupFactor;
		k0[i] = 2.0 * stepsPerUnitTimesRTmp[i] / k2[i];
		spoolRadiiSq[i] = spoolRadii[i] * spoolRadii[i];

		// Calculate the steps per unit that is correct at the origin
		platform.SetDriveStepsPerUnit(i, stepsPerUnitTimesRTmp[i] / spoolRadii[i], 0);
	}
}

// Return the name of the current kinematics
const char *RepropeKinematics::GetName(bool forStatusReport) const noexcept
{
	return "Reprope";
}

// Set the parameters from a M665, M666 or M669 command
// Return true if we changed any parameters that affect the geometry. Set 'error' true if there was an error, otherwise leave it alone.
bool RepropeKinematics::Configure(unsigned int mCode, GCodeBuffer& gb, const StringRef& reply, bool& error) THROWS(GCodeException) /*override*/
{
	bool seen = false;
	if (mCode == 669)
	{
		const bool seenNonGeometry = TryConfigureSegmentation(gb);
		if (gb.TryGetFloatArray('A', 3, anchors[A_AXIS], reply, seen))
		{
			error = true;
			return true;
		}
		if (gb.TryGetFloatArray('B', 3, anchors[B_AXIS], reply, seen))
		{
			error = true;
			return true;
		}
		if (gb.TryGetFloatArray('C', 3, anchors[C_AXIS], reply, seen))
		{
			error = true;
			return true;
		}
		if (gb.TryGetFloatArray('D', 3, anchors[D_AXIS], reply, seen))
		{
			error = true;
			return true;
		}
		if (gb.TryGetFloatArray('E', 3, anchors[E_AXIS], reply, seen))
		{
			error = true;
			return true;
		}
		if (gb.TryGetFloatArray('F', 3, anchors[F_AXIS], reply, seen))
		{
			error = true;
			return true;
		}
		if (gb.TryGetFloatArray('G', 3, anchors[G_AXIS], reply, seen))
		{
			error = true;
			return true;
		}
		if (gb.TryGetFloatArray('H', 3, anchors[H_AXIS], reply, seen))
		{
			error = true;
			return true;
		}

		if (gb.Seen('P'))
		{
			printRadius = gb.GetFValue();
			seen = true;
		}

		if (seen)
		{
			Recalc();
		}
		else if (!seenNonGeometry && !gb.Seen('K'))
		{
			Kinematics::Configure(mCode, gb, reply, error);
			reply.lcatf(
				"A:%.2f, %.2f, %.2f\n"
				"B:%.2f, %.2f, %.2f\n"
				"C:%.2f, %.2f, %.2f\n"
				"D:%.2f, %.2f, %.2f\n"
				"E:%.2f, %.2f, %.2f\n"
				"F:%.2f, %.2f, %.2f\n"
				"G:%.2f, %.2f, %.2f\n"
				"H:%.2f, %.2f, %.2f\n"
				"P:Print radius: %.1f",
				(double)anchors[A_AXIS][X_AXIS], (double)anchors[A_AXIS][Y_AXIS], (double)anchors[A_AXIS][Z_AXIS],
				(double)anchors[B_AXIS][X_AXIS], (double)anchors[B_AXIS][Y_AXIS], (double)anchors[B_AXIS][Z_AXIS],
				(double)anchors[C_AXIS][X_AXIS], (double)anchors[C_AXIS][Y_AXIS], (double)anchors[C_AXIS][Z_AXIS],
				(double)anchors[D_AXIS][X_AXIS], (double)anchors[D_AXIS][Y_AXIS], (double)anchors[D_AXIS][Z_AXIS],
				(double)anchors[E_AXIS][X_AXIS], (double)anchors[E_AXIS][Y_AXIS], (double)anchors[E_AXIS][Z_AXIS],
				(double)anchors[B_AXIS][X_AXIS], (double)anchors[B_AXIS][Y_AXIS], (double)anchors[B_AXIS][Z_AXIS],
				(double)anchors[C_AXIS][X_AXIS], (double)anchors[C_AXIS][Y_AXIS], (double)anchors[C_AXIS][Z_AXIS],
				(double)anchors[D_AXIS][X_AXIS], (double)anchors[D_AXIS][Y_AXIS], (double)anchors[D_AXIS][Z_AXIS],
				(double)printRadius
				);
		}
	}
	else if (mCode == 666)
	{
		gb.TryGetFValue('Q', spoolBuildupFactor, seen);
		if (gb.TryGetFloatArray('R', REPROPE_AXES, spoolRadii, reply, seen))
		{
			error = true;
			return true;
		}
		if (gb.TryGetUIArray('U', REPROPE_AXES, mechanicalAdvantage, reply, seen))
		{
			error = true;
			return true;
		}
		if (gb.TryGetUIArray('O', REPROPE_AXES, linesPerSpool, reply, seen))
		{
			error = true;
			return true;
		}
		if (gb.TryGetUIArray('L', REPROPE_AXES, motorGearTeeth, reply, seen))
		{
			error = true;
			return true;
		}
		if (gb.TryGetUIArray('H', REPROPE_AXES, spoolGearTeeth, reply, seen))
		{
			error = true;
			return true;
		}
		if (gb.TryGetUIArray('J', REPROPE_AXES, fullStepsPerMotorRev, reply, seen))
		{
			error = true;
			return true;
		}
		if (seen)
		{
			Recalc();
		}
		else
		{
			reply.printf(
				"Q:Buildup fac %.4f\n"
				"R:Spool r %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f\n"
				"U:Mech Adv %d, %d, %d, %d, %d, %d, %d, %d\n"
				"O:Lines/spool %d, %d, %d, %d, %d, %d, %d, %d\n"
				"L:Motor gear teeth %d, %d, %d, %d, %d, %d, %d, %d\n"
				"H:Spool gear teeth %d, %d, %d, %d, %d, %d, %d, %d\n"
				"J:Full steps/rev %d, %d, %d, %d, %d, %d, %d, %d",
				(double)spoolBuildupFactor,
				(double)spoolRadii[A_AXIS], (double)spoolRadii[B_AXIS], (double)spoolRadii[C_AXIS], (double)spoolRadii[D_AXIS],
				(double)spoolRadii[E_AXIS], (double)spoolRadii[F_AXIS], (double)spoolRadii[G_AXIS], (double)spoolRadii[H_AXIS],
				(int)mechanicalAdvantage[A_AXIS], (int)mechanicalAdvantage[B_AXIS], (int)mechanicalAdvantage[C_AXIS], (int)mechanicalAdvantage[D_AXIS],
				(int)mechanicalAdvantage[E_AXIS], (int)mechanicalAdvantage[F_AXIS], (int)mechanicalAdvantage[G_AXIS], (int)mechanicalAdvantage[H_AXIS],
				(int)linesPerSpool[A_AXIS], (int)linesPerSpool[B_AXIS], (int)linesPerSpool[C_AXIS], (int)linesPerSpool[D_AXIS],
				(int)linesPerSpool[E_AXIS], (int)linesPerSpool[F_AXIS], (int)linesPerSpool[G_AXIS], (int)linesPerSpool[H_AXIS],
				(int)motorGearTeeth[A_AXIS], (int)motorGearTeeth[B_AXIS], (int)motorGearTeeth[C_AXIS], (int)motorGearTeeth[D_AXIS],
				(int)motorGearTeeth[E_AXIS], (int)motorGearTeeth[F_AXIS], (int)motorGearTeeth[G_AXIS], (int)motorGearTeeth[H_AXIS],
				(int)spoolGearTeeth[A_AXIS], (int)spoolGearTeeth[B_AXIS], (int)spoolGearTeeth[C_AXIS], (int)spoolGearTeeth[D_AXIS],
				(int)spoolGearTeeth[E_AXIS], (int)spoolGearTeeth[F_AXIS], (int)spoolGearTeeth[G_AXIS], (int)spoolGearTeeth[H_AXIS],
				(int)fullStepsPerMotorRev[A_AXIS], (int)fullStepsPerMotorRev[B_AXIS], (int)fullStepsPerMotorRev[C_AXIS], (int)fullStepsPerMotorRev[D_AXIS],
				(int)fullStepsPerMotorRev[E_AXIS], (int)fullStepsPerMotorRev[F_AXIS], (int)fullStepsPerMotorRev[G_AXIS], (int)fullStepsPerMotorRev[H_AXIS]
				);
		}
	}
	else
	{
		return Kinematics::Configure(mCode, gb, reply, error);
	}
	return seen;
}

// Calculate the square of the line length from a spool from a Cartesian coordinate
inline float RepropeKinematics::LineLengthSquared(const float machinePos[3], const float anchors[3]) const noexcept
{
	return fsquare(anchors[Z_AXIS] - machinePos[Z_AXIS]) + fsquare(anchors[Y_AXIS] - machinePos[Y_AXIS]) + fsquare(anchors[X_AXIS] - machinePos[X_AXIS]);
}

// Convert Cartesian coordinates to motor coordinates, returning true if successful
bool RepropeKinematics::CartesianToMotorSteps(const float machinePos[], const float stepsPerMm[],
													size_t numVisibleAxes, size_t numTotalAxes, int32_t motorPos[], bool isCoordinated) const noexcept
{
	float squaredLineLengths[REPROPE_AXES];
	float linePos[REPROPE_AXES];
	for (size_t i = 0; i < REPROPE_AXES; ++i)
	{
		squaredLineLengths[i] = LineLengthSquared(machinePos, anchors[i]);
		linePos[i] = fastSqrtf(squaredLineLengths[i]) - lineLengthsOrigin[i];
		motorPos[i] = lrintf(k0[i] * (fastSqrtf(spoolRadiiSq[i] + linePos[i] * k2[i]) - spoolRadii[i]));
	}

	return true;
}


inline float RepropeKinematics::MotorPosToLinePos(const int32_t motorPos, size_t axis) const noexcept
{
	return (fsquare(motorPos / k0[axis] + spoolRadii[axis]) - spoolRadiiSq[axis]) / k2[axis];
}

// Convert motor coordinates to machine coordinates.
// Assumes lines are tight and anchor location norms are followed
void RepropeKinematics::MotorStepsToCartesian(const int32_t motorPos[], const float stepsPerMm[], size_t numVisibleAxes, size_t numTotalAxes, float machinePos[]) const noexcept
{
	ForwardTransform(
		MotorPosToLinePos(motorPos[A_AXIS], A_AXIS) + lineLengthsOrigin[A_AXIS],
		MotorPosToLinePos(motorPos[B_AXIS], B_AXIS) + lineLengthsOrigin[B_AXIS],
		MotorPosToLinePos(motorPos[C_AXIS], C_AXIS) + lineLengthsOrigin[C_AXIS],
		MotorPosToLinePos(motorPos[D_AXIS], D_AXIS) + lineLengthsOrigin[D_AXIS],
		MotorPosToLinePos(motorPos[E_AXIS], E_AXIS) + lineLengthsOrigin[E_AXIS],
		MotorPosToLinePos(motorPos[F_AXIS], F_AXIS) + lineLengthsOrigin[F_AXIS],
		MotorPosToLinePos(motorPos[G_AXIS], G_AXIS) + lineLengthsOrigin[G_AXIS],
		MotorPosToLinePos(motorPos[H_AXIS], H_AXIS) + lineLengthsOrigin[H_AXIS],
		machinePos);
}

static bool isSameSide(float const v0[3], float const v1[3], float const v2[3], float const v3[3], float const p[3]){
	float const h0[3] = {v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2]};
	float const h1[3] = {v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2]};

	float const normal[3] = {
		h0[1]*h1[2] - h0[2]*h1[1],
		h0[2]*h1[0] - h0[0]*h1[2],
		h0[0]*h1[1] - h0[1]*h1[0]
	};

	float const dh0[3] = {v3[0] - v0[0], v3[1] - v0[1], v3[2] - v0[2]};
	float const dh1[3] = { p[0] - v0[0],  p[1] - v0[1],  p[2] - v0[2]};

	float const dot0 = dh0[0]*normal[0] + dh0[1]*normal[1] + dh0[2]*normal[2];
	float const dot1 = dh1[0]*normal[0] + dh1[1]*normal[1] + dh1[2]*normal[2];
	return dot0*dot1 > 0.0F;
}

static bool isInsideTetrahedron(float const point[3], float const tetrahedron[4][3]){
	return isSameSide(tetrahedron[0], tetrahedron[1], tetrahedron[2], tetrahedron[3], point) &&
	       isSameSide(tetrahedron[2], tetrahedron[1], tetrahedron[3], tetrahedron[0], point) &&
	       isSameSide(tetrahedron[2], tetrahedron[3], tetrahedron[0], tetrahedron[1], point) &&
	       isSameSide(tetrahedron[0], tetrahedron[3], tetrahedron[1], tetrahedron[2], point);
}

bool RepropeKinematics::IsReachable(float axesCoords[MaxAxes], AxesBitmap axes) const noexcept /*override*/
{
	float const coords[3] = {axesCoords[X_AXIS], axesCoords[Y_AXIS], axesCoords[Z_AXIS]};
	return isInsideTetrahedron(coords, anchors);
}

// Limit the Cartesian position that the user wants to move to returning true if we adjusted the position
LimitPositionResult RepropeKinematics::LimitPosition(float finalCoords[], const float * null initialCoords,
															size_t numVisibleAxes, AxesBitmap axesToLimit, bool isCoordinated, bool applyM208Limits) const noexcept
{
	bool limited = false;
	if ((axesToLimit & XyzAxes) == XyzAxes)
	{
		// If axes have been homed on a delta printer and this isn't a homing move, check for movements outside limits.
		// Skip this check if axes have not been homed, so that extruder-only moves are allowed before homing
		// Constrain the move to be within the build radius
		const float diagonalSquared = fsquare(finalCoords[X_AXIS]) + fsquare(finalCoords[Y_AXIS]);
		if (diagonalSquared > printRadiusSquared)
		{
			const float factor = fastSqrtf(printRadiusSquared / diagonalSquared);
			finalCoords[X_AXIS] *= factor;
			finalCoords[Y_AXIS] *= factor;
			limited = true;
		}

		if (applyM208Limits)
		{
			if (finalCoords[Z_AXIS] < reprap.GetPlatform().AxisMinimum(Z_AXIS))
			{
				finalCoords[Z_AXIS] = reprap.GetPlatform().AxisMinimum(Z_AXIS);
				limited = true;
			}
			else if (finalCoords[Z_AXIS] > reprap.GetPlatform().AxisMaximum(Z_AXIS))
			{
				finalCoords[Z_AXIS] = reprap.GetPlatform().AxisMaximum(Z_AXIS);
				limited = true;
			}
		}
	}

	//TODO check intermediate positions, especially uif.when we support an offset radius
	return (limited) ? LimitPositionResult::adjusted : LimitPositionResult::ok;
}

// Return the initial Cartesian coordinates we assume after switching to this kinematics
void RepropeKinematics::GetAssumedInitialPosition(size_t numAxes, float positions[]) const noexcept
{
	for (size_t i = 0; i < numAxes; ++i)
	{
		positions[i] = 0.0;
	}
}

// This function is called when a request is made to home the axes in 'toBeHomed' and the axes in 'alreadyHomed' have already been homed.
// If we can proceed with homing some axes, return the name of the homing file to be called.
// If we can't proceed because other axes need to be homed first, return nullptr and pass those axes back in 'mustBeHomedFirst'.
AxesBitmap RepropeKinematics::GetHomingFileName(AxesBitmap toBeHomed, AxesBitmap alreadyHomed, size_t numVisibleAxes, const StringRef& filename) const noexcept
{
	filename.copy("homeall.g");
	return AxesBitmap();
}

// This function is called from the step ISR when an endstop switch is triggered during homing.
// Return true if the entire homing move should be terminated, false if only the motor associated with the endstop switch should be stopped.
bool RepropeKinematics::QueryTerminateHomingMove(size_t axis) const noexcept
{
	return false;
}

// This function is called from the step ISR when an endstop switch is triggered during homing after stopping just one motor or all motors.
// Take the action needed to define the current position, normally by calling dda.SetDriveCoordinate() and return false.
void RepropeKinematics::OnHomingSwitchTriggered(size_t axis, bool highEnd, const float stepsPerMm[], DDA& dda) const noexcept
{
	// Reprope homing is not supported
}

// Return the axes that we can assume are homed after executing a G92 command to set the specified axis coordinates
AxesBitmap RepropeKinematics::AxesAssumedHomed(AxesBitmap g92Axes) const noexcept
{
	// If all of X, Y and Z have been specified then we know the positions of all 8 spool motors, otherwise we don't
	if ((g92Axes & XyzAxes) == XyzAxes)
	{
		for (size_t i = D_AXIS; i < REPROPE_AXES; ++i)
		{
			g92Axes.SetBit(i);
		}
	}
	else
	{
		g92Axes &= ~XyzAxes;
	}
	return g92Axes;
}

// Return the set of axes that must be homed prior to regular movement of the specified axes
AxesBitmap RepropeKinematics::MustBeHomedAxes(AxesBitmap axesMoving, bool disallowMovesBeforeHoming) const noexcept
{
	if (axesMoving.Intersects(XyzAxes))
	{
		axesMoving |= XyzAxes;
	}
	return axesMoving;
}

#if HAS_MASS_STORAGE || HAS_SBC_INTERFACE

// Write the parameters to a file, returning true if success
bool RepropeKinematics::WriteCalibrationParameters(FileStore *f) const noexcept
{
	bool ok = f->Write("; Reprope parameters\n");
	if (ok)
	{
		String<100> scratchString;
		scratchString.printf("M669 K6 A%.3f:%.3f:%.3f B%.3f:%.3f:%.3f",
							(double)anchors[A_AXIS][X_AXIS], (double)anchors[A_AXIS][Y_AXIS], (double)anchors[A_AXIS][Z_AXIS],
							(double)anchors[B_AXIS][X_AXIS], (double)anchors[B_AXIS][Y_AXIS], (double)anchors[B_AXIS][Z_AXIS]);
		ok = f->Write(scratchString.c_str());
		if (ok)
		{
			scratchString.printf(" C%.3f:%.3f:%.3f D%.3f:%.3f:%.3f",
								(double)anchors[C_AXIS][X_AXIS], (double)anchors[C_AXIS][Y_AXIS], (double)anchors[C_AXIS][Z_AXIS],
								(double)anchors[D_AXIS][X_AXIS], (double)anchors[D_AXIS][Y_AXIS], (double)anchors[D_AXIS][Z_AXIS]);
			ok = f->Write(scratchString.c_str());
			if (!ok)
			{
				return ok;
			}

			scratchString.printf(" E%.3f:%.3f:%.3f F%.3f:%.3f:%.3f",
							(double)anchors[E_AXIS][X_AXIS], (double)anchors[E_AXIS][Y_AXIS], (double)anchors[E_AXIS][Z_AXIS],
							(double)anchors[F_AXIS][X_AXIS], (double)anchors[F_AXIS][Y_AXIS], (double)anchors[F_AXIS][Z_AXIS]);
			ok = f->Write(scratchString.c_str());
			if (!ok)
			{
				return ok;
			}

			scratchString.printf(" G%.3f:%.3f:%.3f H%.3f:%.3f:%.3f P%.1f\n",
								(double)anchors[G_AXIS][X_AXIS], (double)anchors[G_AXIS][Y_AXIS], (double)anchors[G_AXIS][Z_AXIS],
								(double)anchors[H_AXIS][X_AXIS], (double)anchors[H_AXIS][Y_AXIS], (double)anchors[H_AXIS][Z_AXIS],
								(double)printRadius);
			ok = f->Write(scratchString.c_str());
			if (!ok)
			{
				return ok;
			}

			if (ok)
			{
				scratchString.printf("M666 Q%.6f R%.3f:%.3f:%.3f:%.3f%.3f:%.3f:%.3f:%.3f",
									(double)spoolBuildupFactor, (double)spoolRadii[A_AXIS],
									(double)spoolRadii[B_AXIS], (double)spoolRadii[C_AXIS], (double)spoolRadii[D_AXIS],
									(double)spoolRadii[E_AXIS], (double)spoolRadii[F_AXIS],
									(double)spoolRadii[G_AXIS], (double)spoolRadii[H_AXIS]
						);
				ok = f->Write(scratchString.c_str());
				if (!ok)
				{
					return ok;
				}

				scratchString.printf(" U%d:%d:%d:%d%d:%d:%d:%d",
									(int)mechanicalAdvantage[A_AXIS], (int)mechanicalAdvantage[B_AXIS],
									(int)mechanicalAdvantage[C_AXIS], (int)mechanicalAdvantage[D_AXIS],
									(int)mechanicalAdvantage[E_AXIS], (int)mechanicalAdvantage[F_AXIS],
									(int)mechanicalAdvantage[G_AXIS], (int)mechanicalAdvantage[H_AXIS]
						);
				ok = f->Write(scratchString.c_str());
				if (!ok)
				{
					return ok;
				}

				scratchString.printf(" O%d:%d:%d:%d%d:%d:%d:%d",
									(int)linesPerSpool[A_AXIS], (int)linesPerSpool[B_AXIS],
									(int)linesPerSpool[C_AXIS], (int)linesPerSpool[D_AXIS],
									(int)linesPerSpool[E_AXIS], (int)linesPerSpool[F_AXIS],
									(int)linesPerSpool[G_AXIS], (int)linesPerSpool[H_AXIS]
						);
				ok = f->Write(scratchString.c_str());
				if (!ok)
				{
					return ok;
				}

				scratchString.printf(" L%d:%d:%d:%d%d:%d:%d:%d",
									(int)motorGearTeeth[A_AXIS], (int)motorGearTeeth[B_AXIS],
									(int)motorGearTeeth[C_AXIS], (int)motorGearTeeth[D_AXIS],
									(int)motorGearTeeth[E_AXIS], (int)motorGearTeeth[F_AXIS],
									(int)motorGearTeeth[G_AXIS], (int)motorGearTeeth[H_AXIS]
						);
				ok = f->Write(scratchString.c_str());
				if (!ok)
				{
					return ok;
				}

				scratchString.printf(" H%d:%d:%d:%d%d:%d:%d:%d",
									(int)spoolGearTeeth[A_AXIS], (int)spoolGearTeeth[B_AXIS],
									(int)spoolGearTeeth[C_AXIS], (int)spoolGearTeeth[D_AXIS],
									(int)spoolGearTeeth[E_AXIS], (int)spoolGearTeeth[F_AXIS],
									(int)spoolGearTeeth[G_AXIS], (int)spoolGearTeeth[H_AXIS]
						);
				ok = f->Write(scratchString.c_str());
				if (!ok)
				{
					return ok;
				}

				if (ok)
				{
					scratchString.printf(" J%d:%d:%d:%d:%d:%d:%d:%d\n",
										(int)fullStepsPerMotorRev[A_AXIS], (int)fullStepsPerMotorRev[B_AXIS],
										(int)fullStepsPerMotorRev[C_AXIS], (int)fullStepsPerMotorRev[D_AXIS],
										(int)fullStepsPerMotorRev[E_AXIS], (int)fullStepsPerMotorRev[F_AXIS],
										(int)fullStepsPerMotorRev[G_AXIS], (int)fullStepsPerMotorRev[H_AXIS]
							);
					ok = f->Write(scratchString.c_str());
				}
			}
		}
	}
	return ok;
}

// Write any calibration data that we need to resume a print after power fail, returning true if successful
bool RepropeKinematics::WriteResumeSettings(FileStore *f) const noexcept
{
	return WriteCalibrationParameters(f);
}

#endif

/**
 * Reprope forward kinematics
 * Basic idea is to subtract squared line lengths to get linear equations,
 * and then to solve with variable substitution.
 *
 * If we assume anchor location norms are followed
 * Ax=0 Dx=0 Dy=0
 * then
 * we get a fairly clean derivation by
 * subtracting d*d from a*a, b*b, and c*c:
 *
 *  a*a - d*d = k1        +  k2*y +  k3*z     <---- a line  (I)
 *  b*b - d*d = k4 + k5*x +  k6*y +  k7*z     <---- a plane (II)
 *  c*c - d*d = k8 + k9*x + k10*y + k11*z     <---- a plane (III)
 *
 * Use (I) to reduce (II) and (III) into lines. Eliminate y, keep z.
 *
 *  (II):  b*b - d*d = k12 + k13*x + k14*z
 *  <=>            x = k0b + k1b*z,           <---- a line  (IV)
 *
 *  (III): c*c - d*d = k15 + k16*x + k17*z
 *  <=>            x = k0c + k1c*z,           <---- a line  (V)
 *
 * where k1, k2, ..., k17, k0b, k0c, k1b, and k1c are known constants.
 *
 * These two straight lines are not parallel, so they will cross in exactly one point.
 * Find z by setting (IV) = (V)
 * Find x by inserting z into (V)
 * Find y by inserting z into (I)
 *
 * Warning: truncation errors will typically be in the order of a few tens of microns.
 *
 * TODO Adapt to reprope anchor positions
 * We don't really need the 8 lengths, that's overkill.
 * 3 should be enough (3 spheres lead to two potential points, but one will be above the anchor and that would mean the ropes are going upwards and we have antigravity or something, so one of them can be easily discarded)
 * https://en.wikipedia.org/wiki/True-range_multilateration#Three_Cartesian_dimensions,_three_measured_slant_ranges
 * But if the hangprinter was able to do an optimization by using an extra distance an some extra assumptions, hopefully we can
 * get one too by using the 8 rope lengths?
 * instead of "Ax=0 Dx=0 Dy=0" as extra assumptions, we have: "Az=Bz=Cz=Dz=Ez=Fz=Gz=Hz"
 * REM for better understanding of this code and the math behind more speeds for lower anchors
 * https://gitlab.com/tobben/hangprinter-forward-transform/-/blob/main/motorstepstocartesiantest.cpp
 */
void RepropeKinematics::ForwardTransform(float const a, float const b, float const c, float const d,
					 float const e, float const f, float const g, float const h, float machinePos[3]) const noexcept
{
	// Force the anchor location norms Ax=0, Dx=0, Dy=0
	// through a series of rotations.
	float const x_angle = atanf(anchors[D_AXIS][Y_AXIS]/anchors[D_AXIS][Z_AXIS]);
	float const rxt[3][3] = {{1, 0, 0}, {0, cosf(x_angle), sinf(x_angle)}, {0, -sinf(x_angle), cosf(x_angle)}};
	float anchors_tmp0[4][3] = { 0 };
	for (size_t row{0}; row < 4; ++row) {
		for (size_t col{0}; col < 3; ++col) {
			anchors_tmp0[row][col] = rxt[0][col]*anchors[row][0] + rxt[1][col]*anchors[row][1] + rxt[2][col]*anchors[row][2];
		}
	}
	float const y_angle = atanf(-anchors_tmp0[D_AXIS][X_AXIS]/anchors_tmp0[D_AXIS][Z_AXIS]);
	float const ryt[3][3] = {{cosf(y_angle), 0, -sinf(y_angle)}, {0, 1, 0}, {sinf(y_angle), 0, cosf(y_angle)}};
	float anchors_tmp1[4][3] = { 0 };
	for (size_t row{0}; row < 4; ++row) {
		for (size_t col{0}; col < 3; ++col) {
			anchors_tmp1[row][col] = ryt[0][col]*anchors_tmp0[row][0] + ryt[1][col]*anchors_tmp0[row][1] + ryt[2][col]*anchors_tmp0[row][2];
		}
	}
	float const z_angle = atanf(anchors_tmp1[A_AXIS][X_AXIS]/anchors_tmp1[A_AXIS][Y_AXIS]);
	float const rzt[3][3] = {{cosf(z_angle), sinf(z_angle), 0}, {-sinf(z_angle), cosf(z_angle), 0}, {0, 0, 1}};
	for (size_t row{0}; row < 4; ++row) {
		for (size_t col{0}; col < 3; ++col) {
			anchors_tmp0[row][col] = rzt[0][col]*anchors_tmp1[row][0] + rzt[1][col]*anchors_tmp1[row][1] + rzt[2][col]*anchors_tmp1[row][2];
		}
	}

	const float Asq = fsquare(lineLengthsOrigin[A_AXIS]);
	const float Bsq = fsquare(lineLengthsOrigin[B_AXIS]);
	const float Csq = fsquare(lineLengthsOrigin[C_AXIS]);
	const float Dsq = fsquare(lineLengthsOrigin[D_AXIS]);
	const float aa = fsquare(a);
	const float dd = fsquare(d);
	const float k0b = (-fsquare(b) + Bsq - Dsq + dd) / (2.0 * anchors_tmp0[B_AXIS][X_AXIS]) + (anchors_tmp0[B_AXIS][Y_AXIS] / (2.0 * anchors_tmp0[A_AXIS][Y_AXIS] * anchors_tmp0[B_AXIS][X_AXIS])) * (Dsq - Asq + aa - dd);
	const float k0c = (-fsquare(c) + Csq - Dsq + dd) / (2.0 * anchors_tmp0[C_AXIS][X_AXIS]) + (anchors_tmp0[C_AXIS][Y_AXIS] / (2.0 * anchors_tmp0[A_AXIS][Y_AXIS] * anchors_tmp0[C_AXIS][X_AXIS])) * (Dsq - Asq + aa - dd);
	const float k1b = (anchors_tmp0[B_AXIS][Y_AXIS] * (anchors_tmp0[A_AXIS][Z_AXIS] - anchors_tmp0[D_AXIS][Z_AXIS])) / (anchors_tmp0[A_AXIS][Y_AXIS] * anchors_tmp0[B_AXIS][X_AXIS]) + (anchors_tmp0[D_AXIS][Z_AXIS] - anchors_tmp0[B_AXIS][Z_AXIS]) / anchors_tmp0[B_AXIS][X_AXIS];
	const float k1c = (anchors_tmp0[C_AXIS][Y_AXIS] * (anchors_tmp0[A_AXIS][Z_AXIS] - anchors_tmp0[D_AXIS][Z_AXIS])) / (anchors_tmp0[A_AXIS][Y_AXIS] * anchors_tmp0[C_AXIS][X_AXIS]) + (anchors_tmp0[D_AXIS][Z_AXIS] - anchors_tmp0[C_AXIS][Z_AXIS]) / anchors_tmp0[C_AXIS][X_AXIS];

	float machinePos_tmp0[3];
	machinePos_tmp0[Z_AXIS] = (k0b - k0c) / (k1c - k1b);
	machinePos_tmp0[X_AXIS] = k0c + k1c * machinePos_tmp0[Z_AXIS];
	machinePos_tmp0[Y_AXIS] = (Asq - Dsq - aa + dd) / (2.0 * anchors_tmp0[A_AXIS][Y_AXIS]) + ((anchors_tmp0[D_AXIS][Z_AXIS] - anchors_tmp0[A_AXIS][Z_AXIS]) / anchors_tmp0[A_AXIS][Y_AXIS]) * machinePos_tmp0[Z_AXIS];

	//// Rotate machinePos_tmp back to original coordinate system
	float machinePos_tmp1[3];
	for (size_t row{0}; row < 3; ++row) {
		machinePos_tmp1[row] = rzt[row][0]*machinePos_tmp0[0] + rzt[row][1]*machinePos_tmp0[1] + rzt[row][2]*machinePos_tmp0[2];
	}
	for (size_t row{0}; row < 3; ++row) {
		machinePos_tmp0[row] = ryt[row][0]*machinePos_tmp1[0] + ryt[row][1]*machinePos_tmp1[1] + ryt[row][2]*machinePos_tmp1[2];
	}
	for (size_t row{0}; row < 3; ++row) {
		machinePos[row] = rxt[row][0]*machinePos_tmp0[0] + rxt[row][1]*machinePos_tmp0[1] + rxt[row][2]*machinePos_tmp0[2];
	}
}

// Print all the parameters for debugging
void RepropeKinematics::PrintParameters(const StringRef& reply) const noexcept
{
	reply.printf("Anchor coordinates");
	for (size_t i = 0; i < REPROPE_AXES; ++i)
	{
		reply.printf(" (%.2f,%.2f,%.2f)", (double)anchors[i][X_AXIS], (double)anchors[i][Y_AXIS], (double)anchors[i][Z_AXIS]);
	}
	reply.printf("\n");
}

#endif // SUPPORT_REPROPE

// End
