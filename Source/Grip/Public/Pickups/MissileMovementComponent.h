/**
*
* Missile movement implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* We move the homing missile around using velocity changes and sub-stepping to
* ensure that we maintain a smooth movement arc. The UMissileMovementComponent
* does this work, inheriting from UAdvancedMovementComponent for some of that
* functionality.
*
***********************************************************************************/

#pragma once

#include "system/gameconfiguration.h"
#include "advancedmovementcomponent.h"
#include "missilemovementcomponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(GripLogMissile, Warning, All);

/**
* Component for controlling the movement of a homing missile.
***********************************************************************************/

UCLASS()
class GRIP_API UMissileMovementComponent : public UAdvancedMovementComponent
{
	GENERATED_BODY()

public:

	// Construct a missile movement component.
	UMissileMovementComponent();

	// The maximum turn rate of the missile at start speed in degrees per second.
	// Remember this will be limited by Direction Smoothing Ratio.
	UPROPERTY(EditAnywhere, Category = MissileMovement, meta = (UIMin = "0.0", UIMax = "250.0", ClampMin = "0.0", ClampMax = "2500.0"))
		float StartSpeedTurnRate = 250.0f;

	// The maximum turn rate of the missile at top speed in degrees per second.
	// Remember this will be limited by Direction Smoothing Ratio.
	UPROPERTY(EditAnywhere, Category = MissileMovement, meta = (UIMin = "0.0", UIMax = "250.0", ClampMin = "0.0", ClampMax = "2500.0"))
		float TopSpeedTurnRate = 180.0f;

	// Acceleration time in seconds, or 0 for no limit.
	UPROPERTY(EditAnywhere, Category = MissileMovement)
		float AccelerationTime = 3.0f;

	// The magnitude of our acceleration towards the homing target.
	UPROPERTY(EditAnywhere, Category = MissileMovement)
		float HomingAccelerationMagnitude = 0.0f;

	// The maximum speed of the missile in KPH, or 0 for no limit.
	UPROPERTY(EditAnywhere, Category = MissileMovement)
		float MaximumSpeed = 0.0f;

	// The current target we are homing towards. Can only be set at runtime (when projectile is spawned or updating).
	UPROPERTY(Transient)
		USceneComponent* HomingTargetComponent = nullptr;

#pragma region PickupMissile

	// Get the maximum speed we can travel.
	virtual float GetMaxSpeed() const override
	{ return FMathEx::KilometersPerHourToCentimetersPerSecond(MaximumSpeed); }

	// When the simulation stops, just disconnect from the missile and stop updating it.
	virtual void StopSimulating(const FHitResult& hitResult) override;

	// Setup a false target to head towards when no real target is present.
	void FalseTarget(const FVector& location, const FVector2D randomDrift)
	{ TargetLocation = location; RandomDrift = randomDrift; }

	// Get the current target location.
	const FVector& GetTargetLocation() const
	{ return TargetLocation; }

	// Get the current homing target location.
	FVector GetHomingTargetLocation() const;

	// Ignite the motor of the homing missile.
	void IgniteMotor()
	{ Thrusting = true; Timer = 0.0f; }

	// Set whether to lose lock on a target once the missile passes it.
	void SetLoseLockOnRear(bool loseLock)
	{ LoseLockOnRear = loseLock; }

	// Get the time in seconds before impacting target (assuming straight terminal phase and constant speed).
	float GetTimeToTarget() const;

	// Is the missile likely to hit the target?
	bool IsLikelyToHitTarget();

	// Has the missile lock been lost for the target?
	bool HasLostLock() const
	{ return LockLost; }

	// The target speed of the missile in KPH, or 0 for no target.
	// Setting to non-zero will override AccelerationTime, and the missile will slow up as well speed up.
	float TargetSpeed = 0.0f;

	// The terrain direction for the missile, the direction that we should look to avoid collisions in, in world space.
	FVector TerrainDirection = FVector(0.0f, 0.0f, -1.0f);

	// Should we do terrain avoidance, and if non-zero then at which height?
	float TerrainAvoidanceHeight = 0.0f;

protected:

	// Do the regular update tick.
	virtual void TickComponent(float deltaSeconds, enum ELevelTick tickType, FActorComponentTickFunction* thisTickFunction) override;

	// If the missile hits anything, then just stop simulating the movement on it.
	virtual void HandleImpact(const FHitResult& hitResult, float deltaSeconds, const FVector& moveDelta) override;

	// Compute the acceleration in meters per second that you want to apply to the projectile. This adjusts the current velocity.
	virtual FVector ComputeAcceleration(const FVector& velocity, float deltaSeconds) override;

private:

	// Is the missile currently thrusting?
	bool Thrusting = false;

	// Lose lock on a target once the missile passes it?
	bool LoseLockOnRear = true;

	// Has the lock been lost with the target?
	bool LockLost = false;

	// Timer used for wobble in the missile path while tracking.
	float TrackingWobble = 0.0f;

	// The world location of the target if no actor is being tracked.
	FVector TargetLocation = FVector::ZeroVector;

	// The aim point when avoiding terrain.
	FVector TerrainAimLocation = FVector::ZeroVector;

	// The direction that the target is from the missile.
	FVector TargetDirection = FVector::ZeroVector;

	// The direction to aim for from the missile that avoids terrain.
	FVector TerrainAimDirection = FVector::ZeroVector;

	// Drift factor for missiles that have no target lock.
	FVector2D RandomDrift = FVector2D::ZeroVector;

	// Is the turning rate of the missile currently being arrested because it was trying to maneuver too hard?
	bool ArrestingTurn = false;

#pragma endregion PickupMissile

	friend class ADebugMissileHUD;
};
