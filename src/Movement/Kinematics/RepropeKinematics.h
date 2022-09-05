/*
 * RepropeKinematics.h
 *
 *  Created on: 4 Sep 2022
 *      Author: jtimon
 */

#ifndef SRC_MOVEMENT_KINEMATICS_REPROPEKINEMATICS_H_
#define SRC_MOVEMENT_KINEMATICS_REPROPEKINEMATICS_H_

#include "RoundBedKinematics.h"

#if SUPPORT_REPROPE

class RepropeKinematics : public RoundBedKinematics
{
public:
	// Constructors
	RepropeKinematics() noexcept;

	// Overridden base class functions. See Kinematics.h for descriptions.
	const char *GetName(bool forStatusReport) const noexcept override;
	bool Configure(unsigned int mCode, GCodeBuffer& gb, const StringRef& reply, bool& error) THROWS(GCodeException) override;
	bool CartesianToMotorSteps(const float machinePos[], const float stepsPerMm[], size_t numVisibleAxes, size_t numTotalAxes, int32_t motorPos[], bool isCoordinated) const noexcept override;
	void MotorStepsToCartesian(const int32_t motorPos[], const float stepsPerMm[], size_t numVisibleAxes, size_t numTotalAxes, float machinePos[]) const noexcept override;
	bool SupportsAutoCalibration() const noexcept override { return true; }
	bool IsReachable(float axesCoords[MaxAxes], AxesBitmap axes) const noexcept override;
	void SetCalibrationDefaults() noexcept override { Init(); }
#if HAS_MASS_STORAGE || HAS_SBC_INTERFACE
	bool WriteCalibrationParameters(FileStore *f) const noexcept override;
#endif
	LimitPositionResult LimitPosition(float finalCoords[], const float * null initialCoords, size_t numAxes, AxesBitmap axesToLimit, bool isCoordinated, bool applyM208Limits) const noexcept override;
	void GetAssumedInitialPosition(size_t numAxes, float positions[]) const noexcept override;
	HomingMode GetHomingMode() const noexcept override { return HomingMode::homeIndividualMotors; }
	AxesBitmap AxesAssumedHomed(AxesBitmap g92Axes) const noexcept override;
	AxesBitmap MustBeHomedAxes(AxesBitmap axesMoving, bool disallowMovesBeforeHoming) const noexcept override;
	AxesBitmap GetHomingFileName(AxesBitmap toBeHomed, AxesBitmap alreadyHomed, size_t numVisibleAxes, const StringRef& filename) const noexcept override;
	bool QueryTerminateHomingMove(size_t axis) const noexcept override;
	void OnHomingSwitchTriggered(size_t axis, bool highEnd, const float stepsPerMm[], DDA& dda) const noexcept override;
#if HAS_MASS_STORAGE || HAS_SBC_INTERFACE
	bool WriteResumeSettings(FileStore *f) const noexcept override;
#endif

protected:
	DECLARE_OBJECT_MODEL
	OBJECT_MODEL_ARRAY(anchors)
	OBJECT_MODEL_ARRAY(anchorCoordinates)

private:
	// Fixed to 8 motors for now
	static constexpr size_t REPROPE_AXES = 8;
	static constexpr size_t A_AXIS = 0;
	static constexpr size_t B_AXIS = 1;
	static constexpr size_t C_AXIS = 2;
	static constexpr size_t D_AXIS = 3;
	static constexpr size_t E_AXIS = 4;
	static constexpr size_t F_AXIS = 5;
	static constexpr size_t G_AXIS = 6;
	static constexpr size_t H_AXIS = 7;

	void Init() noexcept;
	void Recalc() noexcept;
	float LineLengthSquared(const float machinePos[3], const float anchor[3]) const noexcept;		// Calculate the square of the line length from a spool from a Cartesian coordinate
	void ForwardTransform(float a, float b, float c, float d, float e, float f, float g, float h, float machinePos[3]) const noexcept;
	float MotorPosToLinePos(const int32_t motorPos, size_t axis) const noexcept;

	void PrintParameters(const StringRef& reply) const noexcept;									// Print all the parameters for debugging

	float anchors[REPROPE_AXES][3];				// XYZ coordinates of the anchors
	float printRadius;
	// Line buildup compensation
	float spoolBuildupFactor;
	float spoolRadii[REPROPE_AXES];
	uint32_t mechanicalAdvantage[REPROPE_AXES], linesPerSpool[REPROPE_AXES];
	uint32_t motorGearTeeth[REPROPE_AXES], spoolGearTeeth[REPROPE_AXES], fullStepsPerMotorRev[REPROPE_AXES];

	// Derived parameters
	float k0[REPROPE_AXES], spoolRadiiSq[REPROPE_AXES], k2[REPROPE_AXES], lineLengthsOrigin[REPROPE_AXES];
	float printRadiusSquared;
};

#endif // SUPPORT_REPROPE

#endif /* SRC_MOVEMENT_KINEMATICS_REPROPEKINEMATICS_H_ */
