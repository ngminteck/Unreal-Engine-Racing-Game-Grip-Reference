/**
*
* The AI context for a player.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
***********************************************************************************/

#pragma once

#include "ai/pursuitsplineactor.h"
#include "system/timesmoothing.h"
#include "system/avoidable.h"
#include "effects//drivingsurfacecharacteristics.h"

/**
* The driving mode for an AI bot.
***********************************************************************************/

enum class EVehicleAIDrivingMode : uint8
{
	// The general maneuvering mode.
	GeneralManeuvering,

	// The vehicle is out of control and we're trying to recover.
	RecoveringControl,

	// Reversing away from a blockage as we can't make forward progress.
	ReversingFromBlockage,

	// Reversing the vehicle to reorient to the correct driving direction.
	ReversingToReorient,

	// Launch to reorient to the correct driving direction.
	LaunchToReorient,

	// J turn to reorient to the correct driving direction (handbrake turn in reverse gear).
	JTurnToReorient,

	// The number of driving modes.
	Num
};

/**
* The roll control stage for a vehicle.
***********************************************************************************/

enum class ERollControlStage : uint8
{
	// Inactive.
	Inactive,

	// Damping roll to the target angle.
	Damping,

	// Actively rolling towards the target angle.
	Rolling
};

/**
* Collision indications for a vehicle.
***********************************************************************************/

using VehicleBlocked = int32;

const VehicleBlocked VehicleUnblocked = 0;
const VehicleBlocked VehicleBlockedFront = 1;
const VehicleBlocked VehicleBlockedRear = 2;
const VehicleBlocked VehicleBlockedLeft = 4;
const VehicleBlocked VehicleBlockedRight = 8;

#pragma region PickupGun

/**
* Class for management of a vehicle following another vehicle in the form of
* a general attractable.
***********************************************************************************/

class FVehicleFollower : public IAttractableInterface
{
public:

	// Is the attraction currently active?
	virtual bool IsAttractionActive() const override
	{ return GRIP_POINTER_VALID(FollowingVehicle); }

	// Get the attraction location.
	virtual FVector GetAttractionLocation() const override;

	// Get the attraction direction, or FVector::ZeroVector if no direction.
	virtual FVector GetAttractionDirection() const override
	{ return FVector::ZeroVector; }

	// Get the attraction distance range from the location.
	virtual float GetAttractionDistanceRange() const override
	{ return 250.0f * 100.0f; }

	// Get the attraction minimum distance at which capture can start.
	virtual float GetAttractionMinCaptureDistanceRange() const override
	{ return 5.0f * 100.0f; }

	// Get the attraction angle range from the direction.
	virtual float GetAttractionAngleRange() const override
	{ return MaxAngle; }

	// Which other vehicle is the vehicle currently following.
	TWeakObjectPtr<ABaseVehicle> FollowingVehicle;

	// If we're following a vehicle because of weapon use, which pickup slot is it?
	int32 LinkedToPickupSlot = -1;

	// How long has the vehicle we're following been hidden from view?
	float VehicleHiddenTimer = 0.0f;

	// The maximum angle we will follow the vehicle to.
	float MaxAngle = 0.0f;

	// The distance at which we should follow the vehicle.
	float TrackingDistance = 0.0f;
};

#pragma endregion PickupGun

/**
* Class for managing the general state of AI for a vehicle.
***********************************************************************************/

struct FVehicleAI
{
public:

	// Construct an AI context.
	FVehicleAI();

	// When was the last time we were in a particular driving mode?
	float LastTime(EVehicleAIDrivingMode mode)
	{ return DrivingModeTimes[(int32)mode]; }

	// How long has it been since we were in a particular driving mode?
	float TimeSince(EVehicleAIDrivingMode mode, float clock)
	{ return clock - DrivingModeTimes[(int32)mode]; }

	// How long has it been since we began the current driving mode?
	float TimeInDrivingMode() const
	{ return DrivingModeTime; }

	// How long have we traveled since we began the current driving mode?
	float DistanceInDrivingMode() const
	{ return DrivingModeDistance; }

	//  Is this vehicle in a condition where high speed is possible?
	bool IsGoodForHighSpeed(bool ignoreVehicles = false) const
	{ return (ignoreVehicles == true) ? (NonVehicleTurboObstacles == false) : (TurboObstacles == false); }

	// Is there a transition in progress between normal spline following and something else?
	bool PursuitSplineTransitionInProgress() const
	{ return PursuitSplineFollowingRatio != 0.0f && PursuitSplineFollowingRatio != 1.0f; }

	// Is the vehicle currently under bot control? If this flag is set, car may have been human at some point, but is a bot now (end of game for example).
	bool BotDriver = false;

	// If this flag is set, the vehicle started off wit h /has always been a bot - it's never been human controlled (this is set for remote bot vehicles also, even though we don't control them as a bot locally here).
	bool BotVehicle = false;

	// The location of the vehicle on the last frame.
	FVector LastLocation;
	FVector PrevLocation;

	// How far away from the spline we currently are, in cm.
	float DistanceFromPursuitSpline = 0.0f;

	// The amount of weaving to apply to the spline location;
	float PursuitSplineWeavingRatio = 0.0f;

	// The ratio to use between following the pursuit spline and the last target.
	// 0 for weaving position on the pursuit spline, and 1 for the alternative target.
	float PursuitSplineFollowingRatio = 1.0f;

	// The speed to transition between following the pursuit spline and the last target.
	float PursuitSplineTransitionSpeed = 0.0f;

	// Reset the width offset.
	bool ResetPursuitSplineWidthOffset = true;

	// The maximum width offset, in cm.
	float PursuitSplineWidthOffset = 0.0f;

	// The smoothed maximum width offset, in cm.
	float SmoothedPursuitSplineWidthOffset = 0.0f;

	// The time counter for maneuvering across a spline's width.
	float PursuitSplineWidthTime = 1.0f;

	// The increment used for the time counter for maneuvering across a spline's width.
	float PursuitSplineWidthOverTime = 1.0f;

	// The current optimum speed in KPH the vehicle is attempting to match.
	float OptimumSpeed = 0.0f;

	// The reduced optimum speed in KPH the vehicle is attempting to match.
	float TrackOptimumSpeed = 0.0f;

	// The current minimum speed in KPH the vehicle should be traveling.
	float MinimumSpeed = 0.0f;

	// Are there any obstacles in front to stop us using the turbo?
	bool TurboObstacles = false;

	// Are there any non-vehicle obstacles in front to stop us using the turbo?
	bool NonVehicleTurboObstacles = false;

	// Where the vehicle is heading towards.
	FVector HeadingTo = FVector::ZeroVector;

	// Where the notional track heading the vehicle is following.
	FVector WeavingPosition = FVector::ZeroVector;

	// The closest world location on the spline that the vehicle is following.
	FVector SplineWorldLocation = FVector::ZeroVector;

	// The closest world direction on the spline that the vehicle is following.
	FVector SplineWorldDirection = FVector(1.0f, 0.0f, 0.0f);

	// A starting delay for AI cars, so that they don't all robotically hit the accelerator at the same time.
	float StartDelay = 0.0f;

	// The current driving mode of the vehicle.
	EVehicleAIDrivingMode DrivingMode = EVehicleAIDrivingMode::GeneralManeuvering;

	// How much ahead or behind to track a vehicle.
	float TrackingVehicleDistance = 0.0f;

	// The time when a pickup was last used.
	float LastUsedPickupTime = 0.0f;

	// The time spent in this particular mode.
	float DrivingModeTime = 0.0f;

	// The distance traveled in this particular mode that the driver intended to do.
	// (like going backwards when in reverse for example)
	float DrivingModeDistance = 0.0f;

	// The last clock time for each driving mode, measured against VehicleClock.
	float DrivingModeTimes[(int32)EVehicleAIDrivingMode::Num];

	// Will this vehicle rev their engine on the start line?
	bool WillRevOnStartLine = false;

	// Will this vehicle burnout on the start line?
	bool WillBurnoutOnStartLine = false;

	// Is the vehicle currently revving its engine?
	bool Revving = false;

	// How long we've been rev / not rev for.
	float RevvingTime = 0.0f;

	// How long we've been revving / not revving for.
	float RevvingTimer = 0.0f;

	// The amount of torque roll revving is inducing on the body.
	float TorqueRoll = 0.0f;

	// When should this vehicle play with their wheels?
	float WheelplayStartTime = 0.0f;

	// How much should this vehicle play with their wheels? (0 for not at all)
	float WheelplayCycles;

	// An offset used to induce variable speed for this vehicle around its current speed limit.
	float VariableSpeedOffset;

	// The amount of time the vehicle has been outside the width of its current spline.
	float OutsideSplineCount = 0.0f;

	// Lock the steering to spline direction?
	bool LockSteeringToSplineDirection = false;

	// Avoid static objects while steering is locked to spline direction?
	bool LockSteeringAvoidStaticObjects = false;

	// Indicator from the level volumes whether to lock the steering to spline direction?
	bool VolumeLockSteeringToSplineDirection = false;

	// Indicator from the level volumes whether to avoid static objects while steering is locked to spline direction?
	bool VolumeLockSteeringAvoidStaticObjects = false;

	// Use engine boost to attain minimum speed?
	bool BoostForMinimumSpeed = false;

	// Set of flags describing collision blockages so we can make decisions more effectively.

	// Unqualified blockages on each side of the vehicle.
	VehicleBlocked CollisionBlockage = VehicleUnblocked;
	VehicleBlocked LastCollisionBlockage = VehicleUnblocked;

	// Non-vehicle blockages on each side of the vehicle.
	VehicleBlocked HardCollisionBlockage = VehicleUnblocked;
	VehicleBlocked LastHardCollisionBlockage = VehicleUnblocked;

	// Vehicle blockages on each side of the vehicle.
	VehicleBlocked VehicleContacts = VehicleUnblocked;
	VehicleBlocked LastVehicleContacts = VehicleUnblocked;

	// Should we evaluate the closest spline to our vehicle right now?
	bool ClosestSplineEvaluationEnabled = true;

	// Are we spontaneously fishtailing on this frame?
	bool Fishtailing = false;

	// How long we've been fishtailing for.
	float FishtailingOnTime = 0.0f;

	// How long we've not been fishtailing for.
	float FishtailingOffTime = 0.0f;

	// How much fishtail recovery to apply.
	float FishtailRecovery = 0.0f;

	// Record of thrust values (VehicleClock).
	FTimedFloatList Thrust = FTimedFloatList(21, 30);

	// Record of speed values over time (VehicleClock).
	FTimedFloatList Speed = FTimedFloatList(21, 10);

	// Record of forward speed values over time (VehicleClock).
	FTimedFloatList ForwardSpeed = FTimedFloatList(21, 10);

	// Record of backward speed values over time (VehicleClock).
	FTimedFloatList BackwardSpeed = FTimedFloatList(21, 10);

	// Record of distance traveled when vaguely moving forwards over time (VehicleClock).
	FTimedFloatList ForwardDistanceTraveled = FTimedFloatList(21, 10);

	// Record of distance traveled when vaguely moving backwards over time (VehicleClock).
	FTimedFloatList BackwardDistanceTraveled = FTimedFloatList(21, 10);

	// Record of the race distances over time (VehicleClock).
	FTimedFloatList RaceDistances = FTimedFloatList(21, 10);

	// Record of the facing direction being valid over time (VehicleClock).
	FTimedFloatList FacingDirectionValid = FTimedFloatList(21, 10);

	// Record of the yaw direction away from velocity vector over time (VehicleClock).
	FTimedFloatList YawDirectionVsVelocity = FTimedFloatList(21, 10);

	// The driving stage of reorienting the vehicle.
	// 0 gathering speed, 1 turning, 2 braking
	int32 ReorientationStage = 0;

	// The steering direction in which to perform the reorientation.
	float ReorientationDirection = 1.0f;

	// How much to extend the optimum speed by when cornering.
	float OptimumSpeedExtension = 0.0f;

	// Should we reassess which spline we should use once we're grounded again after being airborne?
	bool ReassessSplineWhenGrounded = false;

	// Use a pro maneuver to correct the vehicle after recovering control?
	bool UseProRecovery = false;

	// The level of difficulty, starting from 0 for easiest and up to whatever value we see fit.
	int32 DifficultyLevel = 0;

	// The angular rate at which the roll velocity requires damping when under AI bot control.
	const float RollVelocityRequiresDamping = 20.0f;

#pragma region NavigationSplines

	// The follower used to navigate a route.
	FRouteFollower RouteFollower;

#pragma endregion NavigationSplines

#pragma region AINavigation

	// Update the variables used for spline weaving and speed variation.
	void UpdateSplineFollowing(float deltaSeconds, float speedKPH);

	// Get the distance from the spline that the vehicle is currently driving.
	float GetSplineWeavingOffset(bool includeTransition) const
	{ return FMath::Sin(PursuitSplineWidthTime) * SmoothedPursuitSplineWidthOffset; }

	// Is this bot driver driving casually, and not in a tight driving situation?
	bool IsDrivingCasually(bool ignoreVehicles = false) const;

#pragma endregion AINavigation

#pragma region AIVehicleControl

	// Request a new driving mode for the vehicle.
	void SetDrivingMode(EVehicleAIDrivingMode mode);

	// Update the start-line engine revving.
	void UpdateRevving(float deltaSeconds, bool gameStarted);

#pragma endregion AIVehicleControl

#pragma region AIVehicleRollControl

	// The airborne roll control stage for enacting air control.
	ERollControlStage AirborneRollControl = ERollControlStage::Inactive;

	// Do we have a roll target surfaced detected?
	bool RollTargetDetected;

	// The world location of the surface we're using for the roll target.
	FVector RollControlLocation;

	// The world normal of the surface we're using for the roll target.
	FVector RollControlNormal;

	// The game surface we're using for the roll target.
	EGameSurface RollControlSurfaceType;

	// Timer used for roll control.
	float RollControlTime = 0.0f;

#pragma endregion AIVehicleRollControl

#pragma region AIAttraction

	// Setup a smooth transition between a world location for a spline.
	void SetupPursuitSplineTransition();

	// Remove any pursuit spline transition that might be in effect.
	void RemovePursuitSplineTransition();

	// What actor is the vehicle currently attracted to.
	TWeakObjectPtr<AActor> AttractedToActor;

	// What actor is the vehicle currently attracted to, a cached IAttractableInterface for speed.
	IAttractableInterface* AttractedTo = nullptr;

#pragma region PickupGun

	// Follow a vehicle while using a particular pickup against them.
	void FollowVehicleWithPickup(ABaseVehicle* vehicle, int32 pickupSlot, float maxAngle, float trackingDistance);

	// The follower used to trail another vehicle.
	FVehicleFollower VehicleFollower;

#pragma endregion PickupGun

#pragma endregion AIAttraction

#pragma region VehicleTeleport

	// Reset the object after a teleport has taken place.
	void TeleportReset(const FVector& location);

#pragma endregion VehicleTeleport

};

/**
* Structure for describing an object that an AI bot should avoid.
***********************************************************************************/

struct FVehicleAvoidableContext
{
	// The avoidable to avoid.
	IAvoidableInterface* Avoidable;

	// Vehicle location.
	FVector T0;

	// Avoidable location.
	FVector T1;

	// T1 - T0.
	FVector Difference;

	// Distance between T1 and T0.
	float Distance;

	// The closing velocity between the vehicle and avoidable.
	FVector ClosingVelocity;

	// The closing speed CMPS between the vehicle and avoidable.
	float ClosingSpeed;

	// The closing speed in KPH between the vehicle and avoidable.
	float ClosingSpeedKPH;

	// The time in second before collision.
	float TimeToCollision;

	// A ratio between 1.0 and 1.33 depending on closing speed.
	float AvoidanceRatio;

	// The radius of the avoidable scaled with closing speed (using AvoidanceRatio so higher speeds give a third extra radii).
	float AvoidableRadius;

	// The minimum separation required between the vehicle and the avoidable (scaled for closing speed).
	float MinSeparation;

	// How quickly will they hit each other in seconds if running intersecting courses.
	float AvoidableRanking;
};
