/*
 * DriveMovement.h
 *
 *  Created on: 17 Jan 2015
 *      Author: David
 */

#ifndef DRIVEMOVEMENT_H_
#define DRIVEMOVEMENT_H_

#include <RepRapFirmware.h>
#include <Platform/Tasks.h>
#include "MoveSegment.h"

class LinearDeltaKinematics;
class PrepParams;
class ExtruderShaper;

#define DM_USE_FPU			(__FPU_USED)
#define EVEN_STEPS			(1)						// 1 to generate steps at even intervals when doing double/quad/octal stepping

enum class DMState : uint8_t
{
	idle = 0,
	stepError,

	// All higher values are various states of motion
	firstMotionState,
	cartAccelOrDecelNoReverse = firstMotionState,	// linear accelerating motion
	cartLinear,										// linear steady speed
	cartDecelForwardsReversing,						// linear decelerating motion, expect reversal
	cartDecelReverse,								// linear decelerating motion, reversed

	deltaForwardsNoReverse,							// moving forwards, no reversal in this segment
	deltaForwardsReversing,							// moving forwards but reversing in this segment
	deltaReverse,									// moving in reverse
};

// This class describes a single movement of one drive
class DriveMovement
{
public:
	friend class DDA;

	DriveMovement(DriveMovement *next) noexcept;

	void* operator new(size_t count) { return Tasks::AllocPermanent(count); }
	void* operator new(size_t count, std::align_val_t align) { return Tasks::AllocPermanent(count, align); }
	void operator delete(void* ptr) noexcept {}
	void operator delete(void* ptr, std::align_val_t align) noexcept {}

	bool CalcNextStepTime(const DDA &dda) noexcept SPEED_CRITICAL;
	bool PrepareCartesianAxis(const DDA& dda, const PrepParams& params) noexcept SPEED_CRITICAL;
	bool PrepareDeltaAxis(const DDA& dda, const PrepParams& params) noexcept SPEED_CRITICAL;
	bool PrepareExtruder(const DDA& dda) noexcept SPEED_CRITICAL;

	void DebugPrint() const noexcept;
	int32_t GetNetStepsLeft() const noexcept;
	int32_t GetNetStepsTaken() const noexcept;

#if HAS_SMART_DRIVERS
	uint32_t GetStepInterval(uint32_t microstepShift) const noexcept;	// Get the current full step interval for this axis or extruder
#endif

	static void InitialAllocate(unsigned int num) noexcept;
	static unsigned int NumCreated() noexcept { return numCreated; }
	static DriveMovement *Allocate(size_t p_drive, DMState st) noexcept;
	static void Release(DriveMovement *item) noexcept;

private:
	bool CalcNextStepTimeFull(const DDA &dda) noexcept SPEED_CRITICAL;
	bool NewCartesianSegment() noexcept SPEED_CRITICAL;
	bool NewExtruderSegment() noexcept SPEED_CRITICAL;
	bool NewDeltaSegment(const DDA& dda) noexcept SPEED_CRITICAL;

	static DriveMovement *freeList;
	static unsigned int numCreated;

	// Parameters common to Cartesian, delta and extruder moves

	DriveMovement *nextDM;								// link to next DM that needs a step
	MoveSegment *currentSegment;

	DMState state;										// whether this is active or not
	uint8_t drive;										// the drive that this DM controls
	uint8_t direction : 1,								// true=forwards, false=backwards
			directionChanged : 1,						// set by CalcNextStepTime if the direction is changed
			isDelta : 1,								// true if this DM uses segment-free delta kinematics
			isExtruder : 1;								// true if this DM is for an extruder (only matters if !isDelta)
	uint8_t stepsTillRecalc;							// how soon we need to recalculate

	uint32_t totalSteps;								// total number of steps for this move

	// These values change as the step is executed, except for reverseStartStep
	uint32_t nextStep;									// number of steps already done
	uint32_t phaseStepLimit;							// the first step number of the next phase, or the reverse start step if smaller
	uint32_t reverseStartStep;							// the step number for which we need to reverse direction due to pressure advance or delta movement
	uint32_t nextStepTime;								// how many clocks after the start of this move the next step is due
	uint32_t stepInterval;								// how many clocks between steps

	float distanceSoFar;
	float timeSoFar;
	float pA, pB, pC;

	// Parameters unique to a style of move (Cartesian, delta or extruder). Currently, extruders and Cartesian moves use the same parameters.
	union
	{
		struct DeltaParameters							// Parameters for delta movement
		{
			// The following don't depend on how the move is executed, so they could be set up in Init() if we use fixed acceleration/deceleration
#if DM_USE_FPU
			float fTwoA;
			float fTwoB;
			float fDSquaredMinusAsquaredMinusBsquaredTimesSsquared;
			float h0MinusZ0;							// the height subtended by the rod at the start of the move
			float fHmz0s;								// the starting height less the starting Z height, multiplied by the Z movement fraction (can go negative)
			float fMinusAaPlusBbTimesS;
			float reverseStartDistance;					// the overall move distance at which movement reversal occurs
#else
			int64_t dSquaredMinusAsquaredMinusBsquaredTimesKsquaredSsquared;
			int32_t hmz0sK;								// the starting step position less the starting Z height, multiplied by the Z movement fraction and K (can go negative)
			int32_t minusAaPlusBbTimesKs;
#endif
		} delta;

		struct CartesianParameters
		{
			float pressureAdvanceK;						// how much pressure advance is applied to this move
			float effectiveStepsPerMm;					// the steps/mm multiplied by the movement fraction
			float effectiveMmPerStep;					// reciprocal of [the steps/mm multiplied by the movement fraction]
			float extraExtrusionDistance;				// the extra extrusion distance in the acceleration phase
		} cart;
	} mp;

	static constexpr uint32_t NoStepTime = 0xFFFFFFFF;	// value to indicate that no further steps are needed when calculating the next step time

#if !DM_USE_FPU
	static constexpr uint32_t K1 = 1024;				// a power of 2 used to multiply the value mmPerStepTimesCdivtopSpeed to reduce rounding errors
	static constexpr uint32_t K2 = 512;					// a power of 2 used in delta calculations to reduce rounding errors (but too large makes things worse)
	static constexpr int32_t Kc = 1024 * 1024;			// a power of 2 for scaling the Z movement fraction
#endif
};

// Calculate and store the time since the start of the move when the next step for the specified DriveMovement is due.
// Return true if there are more steps to do. When finished, leave nextStep == totalSteps + 1.
// This is also used for extruders on delta machines.
// We inline this part to speed things up when we are doing double/quad/octal stepping.
inline bool DriveMovement::CalcNextStepTime(const DDA &dda) noexcept
{
	++nextStep;
	if (nextStep <= totalSteps)
	{
		if (stepsTillRecalc != 0)
		{
			--stepsTillRecalc;			// we are doing double/quad/octal stepping
#if EVEN_STEPS
			nextStepTime += stepInterval;
#endif
#if SAME70
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
#endif
			return true;
		}
		return CalcNextStepTimeFull(dda);
	}

	state = DMState::idle;
#if SAME70
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
#endif
	return false;
}

// Return the number of net steps left for the move in the forwards direction.
// We have already taken nextSteps - 1 steps, unless nextStep is zero.
inline int32_t DriveMovement::GetNetStepsLeft() const noexcept
{
	int32_t netStepsLeft;
	if (reverseStartStep > totalSteps)		// if no reverse phase
	{
		netStepsLeft = (nextStep == 0) ? (int32_t)totalSteps : (int32_t)totalSteps - (int32_t)nextStep + 1;
	}
	else if (nextStep >= reverseStartStep)
	{
		netStepsLeft = (int32_t)totalSteps - (int32_t)nextStep + 1;
	}
	else
	{
		const int32_t totalNetSteps = (int32_t)(2 * reverseStartStep) - (int32_t)totalSteps - 2;
		netStepsLeft = (nextStep == 0) ? totalNetSteps : totalNetSteps - (int32_t)nextStep + 1;
	}
	return (direction) ? netStepsLeft : -netStepsLeft;
}

// Return the number of net steps already taken for the move in the forwards direction.
// We have already taken nextSteps - 1 steps, unless nextStep is zero.
inline int32_t DriveMovement::GetNetStepsTaken() const noexcept
{
	int32_t netStepsTaken;
	if (nextStep < reverseStartStep || reverseStartStep > totalSteps)				// if no reverse phase, or not started it yet
	{
		netStepsTaken = (nextStep == 0) ? 0 : (int32_t)nextStep - 1;
	}
	else
	{
		netStepsTaken = (int32_t)nextStep - (int32_t)(2 * reverseStartStep) + 1;	// allowing for direction having changed
	}
	return (direction) ? netStepsTaken : -netStepsTaken;
}

// This is inlined because it is only called from one place
inline void DriveMovement::Release(DriveMovement *item) noexcept
{
	item->nextDM = freeList;
	freeList = item;
}

#if HAS_SMART_DRIVERS

// Get the current full step interval for this axis or extruder
inline uint32_t DriveMovement::GetStepInterval(uint32_t microstepShift) const noexcept
{
	return (nextStep < totalSteps && nextStep > (1u << microstepShift))		// if at least 1 full step done
		? stepInterval << microstepShift									// return the interval between steps converted to full steps
			: 0;
}

#endif

#endif /* DRIVEMOVEMENT_H_ */
