/*
 * Kinematics.h
 *
 *  Created on: 24 Apr 2017
 *      Author: David
 */

#ifndef SRC_MOVEMENT_KINEMATICS_H_
#define SRC_MOVEMENT_KINEMATICS_H_

#include <RepRapFirmware.h>
#include <Math/Matrix.h>
#include <ObjectModel/ObjectModel.h>

inline floatc_t fcsquare(floatc_t a)
{
	return a * a;
}

// Different types of kinematics we support. Each of these has a class to represent it.
// These must have the same numeric assignments as the K parameter of the M669 command, as documented in the GCodes wiki page
enum class KinematicsType : uint8_t
{
	cartesian = 0,
	coreXY,
	coreXZ,
	linearDelta,
	scara,
	coreXYU,
	hangprinter,
	polar,
	coreXYUV,
	fiveBarScara,		// was previously reserved for @sga, see https://forum.duet3d.com/topic/5775/aditional-carterian-z-axis-on-delta-printer
	rotaryDelta,
	markForged,
	collinearTriperon,	// reserved for @oliof, see https://forum.duet3d.com/topic/11646/kinematics-type-number-allocation-for-colinear-tripteron
	robot5axis,			// reserved for @joergS5, see https://forum.duet3d.com/post/172204
	reprope,

	unknown				// this one must be last!
};

// Different types of low-level motion we support
enum class MotionType : uint8_t
{
	linear,
	segmentFreeDelta
};

// Class used to define homing mode
enum class HomingMode : uint8_t
{
	homeCartesianAxes,
	homeIndividualMotors,
	homeSharedMotors
};

// Return value from limitPosition
enum class LimitPositionResult : uint8_t
{
	ok,									// the final position is reachable, so are all the intermediate positions
	adjusted,							// the final position was unreachable so it has been limited, the intermediate positions are now reachable
	intermediateUnreachable,			// the final position is reachable but intermediate positions are not
	adjustedAndIntermediateUnreachable	// we adjusted the final position to make it reachable, but intermediate positions are still urreachable
};

struct SegmentationType
{
	uint8_t useSegmentation : 1,
			useZSegmentation : 1,
			useG0Segmentation : 1,
			zero : 5;

	constexpr SegmentationType(bool useSeg, bool useZSeg, bool useG0Seg) noexcept
		: useSegmentation(useSeg), useZSegmentation(useZSeg), useG0Segmentation(useG0Seg), zero(0)
	{
	}
};

class Kinematics INHERIT_OBJECT_MODEL
{
public:
	// Functions that must be defined in each derived class that implements a kinematics

	// Return the name of the current kinematics.
	// If 'forStatusReport' is true then the string must be the one for that kinematics expected by DuetWebControl and PanelDue.
	// Otherwise it should be in a format suitable for printing.
	// For any new kinematics, the same string can be returned regardless of the parameter.
	virtual const char *GetName(bool forStatusReport = false) const noexcept = 0;

	// Set or report the parameters from a M665, M666 or M669 command
	// If 'mCode' is an M-code used to set parameters for the current kinematics (which should only ever be 665, 666, 667 or 669)
	// then search for parameters used to configure the current kinematics. If any are found, perform appropriate actions,
	// and return true if the changes affect the geometry.
	// If errors were discovered while processing parameters, put an appropriate error message in 'reply' and set 'error' to true.
	// If no relevant parameters are found, print the existing ones to 'reply' and return false.
	// If 'mCode' does not apply to this kinematics, call the base class version of this function, which will print a suitable error message.
	virtual bool Configure(unsigned int mCode, GCodeBuffer& gb, const StringRef& reply, bool& error) THROWS(GCodeException);

	// Convert Cartesian coordinates to motor positions measured in steps from reference position
	// 'machinePos' is a set of axis and extruder positions to convert
	// 'stepsPerMm' is as configured in M92. On a Scara or polar machine this would actually be steps per degree.
	// 'numAxes' is the number of machine axes to convert, which will always be at least 3
	// 'motorPos' is the output vector of motor positions
	// Return true if successful, false if we were unable to convert
	virtual bool CartesianToMotorSteps(const float machinePos[], const float stepsPerMm[], size_t numVisibleAxes, size_t numTotalAxes, int32_t motorPos[], bool isCoordinated) const noexcept = 0;

	// Convert motor positions (measured in steps from reference position) to Cartesian coordinates
	// 'motorPos' is the input vector of motor positions
	// 'stepsPerMm' is as configured in M92. On a Scara or polar machine this would actually be steps per degree.
	// 'numDrives' is the number of machine drives to convert, which will always be at least 3
	// 'machinePos' is the output set of converted axis and extruder positions
	virtual void MotorStepsToCartesian(const int32_t motorPos[], const float stepsPerMm[], size_t numVisibleAxes, size_t numTotalAxes, float machinePos[]) const noexcept = 0;

	// Return true if the kinematics supports auto calibration based on bed probing.
	// Normally returns false, but overridden for delta kinematics and kinematics with multiple independently-drive Z leadscrews.
	virtual bool SupportsAutoCalibration() const noexcept { return false; }

	// Perform auto calibration. Override this implementation in kinematics that support it. Caller already owns the movement lock.
	// Return true if an error occurred.
	virtual bool DoAutoCalibration(size_t numFactors, const RandomProbePointSet& probePoints, const StringRef& reply) noexcept
	pre(SupportsAutoCalibration()) { return false; }

	// Set the default parameters that are changed by auto calibration back to their defaults.
	// Do nothing if auto calibration is not supported.
	virtual void SetCalibrationDefaults() noexcept { }

#if HAS_MASS_STORAGE || HAS_SBC_INTERFACE
	// Write the parameters that are set by auto calibration to the config-override.g file, returning true if success
	// Just return true if auto calibration is not supported.
	virtual bool WriteCalibrationParameters(FileStore *f) const noexcept { return true; }
#endif

	// Get the bed tilt fraction for the specified axis.
	// Usually this is only relevant if we are auto calibrating the bed tilt, however you can also specify bed tilt manually if you wanted to.
	virtual float GetTiltCorrection(size_t axis) const noexcept { return 0.0; }

	// Return true if the positions specified for the axes in the AxesBitmap are reachable by the print head reference point.
	// The default implementation assumes a rectangular reachable area, so it just uses the bed dimensions give in the M208 commands.
	virtual bool IsReachable(float axesCoords[MaxAxes], AxesBitmap axes) const noexcept;

	// Limit the Cartesian position that the user wants to move to, returning true if any coordinates were changed
	// The default implementation just applies the rectangular limits set up by M208 to those axes that have been homed.
	// applyM208Limits determines whether the m208 limits are applied, otherwise just the geometric limitations of the architecture are applied.
	// If initialCoords is null, just limit the final coordinates; else limit all points on a straight line between the two.
	virtual LimitPositionResult LimitPosition(float finalCoords[], const float * null initialCoords,
												size_t numVisibleAxes, AxesBitmap axesToLimit, bool isCoordinated, bool applyM208Limits) const noexcept;

	// Return the set of axes that must have been homed before bed probing is allowed
	// The default implementation requires just X and Y, but some kinematics require additional axes to be homed (e.g. delta, CoreXZ)
	virtual AxesBitmap AxesToHomeBeforeProbing() const noexcept { return XyAxes; }

	// Return the initial Cartesian coordinates we assume after switching to this kinematics
	virtual void GetAssumedInitialPosition(size_t numAxes, float positions[]) const noexcept;

	// Override this one if any axes do not use the linear motion code (e.g. for segmentation-free delta motion)
	virtual MotionType GetMotionType(size_t axis) const noexcept { return MotionType::linear; }

	// This function is called when a request is made to home the axes in 'toBeHomed' and the axes in 'alreadyHomed' have already been homed.
	// If we can't proceed because other axes need to be homed first, return those axes.
	// If we can proceed with homing some axes, set 'filename' to the name of the homing file to be called and return 0. Optionally, update 'alreadyHomed' to indicate
	// that some additional axes should be considered not homed.
	virtual AxesBitmap GetHomingFileName(AxesBitmap toBeHomed, AxesBitmap alreadyHomed, size_t numVisibleAxes, const StringRef& filename) const noexcept;

	// This function is called from the step ISR when an endstop switch is triggered during homing.
	// Return true if the entire homing move should be terminated, false if only the motor associated with the endstop switch should be stopped.
	virtual bool QueryTerminateHomingMove(size_t axis) const noexcept = 0;

	// This function is called from the step ISR when an endstop switch is triggered during homing after stopping just one motor or all motors.
	// Take the action needed to define the current position, normally by calling dda.SetDriveCoordinate() and return false.
	virtual void OnHomingSwitchTriggered(size_t axis, bool highEnd, const float stepsPerMm[], DDA& dda) const noexcept = 0;

	// Return the type of homing we do
	virtual HomingMode GetHomingMode() const noexcept = 0;

	// Return the axes that we can assume are homed after executing a G92 command to set the specified axis coordinates
	// This default is good for Cartesian and Core printers, but not deltas or SCARA
	virtual AxesBitmap AxesAssumedHomed(AxesBitmap g92Axes) const noexcept { return g92Axes; }

	// Return the set of axes that must be homed prior to regular movement of the specified axes
	// This default is good for Cartesian and Core printers, but not deltas or SCARA
	virtual AxesBitmap MustBeHomedAxes(AxesBitmap axesMoving, bool disallowMovesBeforeHoming) const noexcept { return (disallowMovesBeforeHoming) ? axesMoving : AxesBitmap(); }

#if HAS_MASS_STORAGE || HAS_SBC_INTERFACE
	// Write any calibration data that we need to resume a print after power fail, returning true if successful. Override where necessary.
	virtual bool WriteResumeSettings(FileStore *f) const noexcept { return true; }
#endif

	// Limit the speed and acceleration of a move to values that the mechanics can handle.
	// The speeds along individual Cartesian axes have already been limited before this is called.
	// The default implementation in this class just limits the combined XY speed to the lower of the individual X and Y limits. This is appropriate for
	// many types of kinematics, but not for Cartesian.
	virtual void LimitSpeedAndAcceleration(DDA& dda, const float *normalisedDirectionVector, size_t numVisibleAxes, bool continuousRotationShortcut) const noexcept;

	// Return true if the specified axis is a continuous rotational axis and G0 commands may choose which direction to move it in
	virtual bool IsContinuousRotationAxis(size_t axis) const noexcept;

	// Return a bitmap of the motors that cause movement of a particular axis or tower.
	// This is used to determine which motors we need to enable to move a particular axis, and which motors to monitor for stall detect homing.
	// For example, the first XY move made by a CoreXY machine may be a diagonal move, and it's important to enable the non-moving motor too.
	virtual AxesBitmap GetConnectedAxes(size_t axis) const noexcept;

	// Return a bitmap of axes that move linearly in response to the correct combination of linear motor movements.
	// This is called to determine whether we can babystep the specified axis independently of regular motion.
	virtual AxesBitmap GetLinearAxes() const noexcept = 0;

	// Override this virtual destructor if your constructor allocates any dynamic memory
	virtual ~Kinematics() { }

	// Factory function to create a particular kinematics object and return a pointer to it.
	// When adding new kinematics, you will need to extend this function to handle your new kinematics type.
	static Kinematics *Create(KinematicsType k) noexcept;

	// Functions that return information held in this base class
	KinematicsType GetKinematicsType() const noexcept { return type; }

	SegmentationType GetSegmentationType() const noexcept { return segmentationType; }
	float GetSegmentsPerSecond() const noexcept pre(UseSegmentation()) { return segmentsPerSecond; }
	float GetMinSegmentLength() const noexcept pre(UseSegmentation()) { return minSegmentLength; }
	float GetReciprocalMinSegmentLength() const noexcept pre(UseSegmentation()) { return reciprocalMinSegmentLength; }

protected:
	DECLARE_OBJECT_MODEL

	Kinematics(KinematicsType t, SegmentationType segType) noexcept;

	// Apply the M208 limits to the Cartesian position that the user wants to move to for all axes from the specified one upwards
	// Return true if any coordinates were changed
	bool LimitPositionFromAxis(float coords[], size_t firstAxis, size_t numVisibleAxes, AxesBitmap axesHomed) const noexcept;

	// Try to configure the segmentation parameters
	bool TryConfigureSegmentation(GCodeBuffer& gb) noexcept;

	// Debugging functions
	static void PrintMatrix(const char* s, const MathMatrix<float>& m, size_t numRows = 0, size_t maxCols = 0) noexcept;
	static void PrintMatrix(const char* s, const MathMatrix<double>& m, size_t numRows = 0, size_t maxCols = 0) noexcept;
	static void PrintVector(const char *s, const float *v, size_t numElems) noexcept;
	static void PrintVector(const char *s, const double *v, size_t numElems) noexcept;

	static const char * const HomeAllFileName;

private:
	// Default values for those kinematics that always use segmentation
	static constexpr float DefaultSegmentsPerSecond = 100.0;
	static constexpr float DefaultMinSegmentLength = 0.2;

	float segmentsPerSecond;				// if we are using segmentation, the target number of segments/second
	float minSegmentLength;					// if we are using segmentation, the minimum segment size
	float reciprocalMinSegmentLength;		// if we are using segmentation, the reciprocal of minimum segment size

	SegmentationType segmentationType;		// the type of segmentation we are using
	KinematicsType type;
};

#endif /* SRC_MOVEMENT_KINEMATICS_H_ */
