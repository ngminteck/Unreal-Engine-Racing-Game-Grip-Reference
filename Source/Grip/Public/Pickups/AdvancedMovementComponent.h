/**
*
* Advanced movement implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* An advanced movement component to give a lot of helper functionality to generalized
* movement work. Mostly, this will center on terrain avoidance.
*
***********************************************************************************/

#pragma once

#include "system/gameconfiguration.h"
#include "gameframework/movementcomponent.h"
#include "system/mathhelpers.h"
#include "advancedmovementcomponent.generated.h"

/**
* Component for controlling advanced movement of other components.
***********************************************************************************/

UCLASS(Abstract)
class GRIP_API UAdvancedMovementComponent : public UMovementComponent
{
	GENERATED_BODY()

public:

	// Construct an advanced movement component.
	UAdvancedMovementComponent();

	// How to smooth direction changes.
	UPROPERTY(EditAnywhere, Category = AdvancedMovement, meta = (UIMin = "0.5", UIMax = "1.0", ClampMin = "0.5", ClampMax = "1.0"))
		float DirectionSmoothingRatio = 0.85f;

	// How to smooth direction changes when avoiding terrain.
	UPROPERTY(EditAnywhere, Category = AdvancedMovement, meta = (UIMin = "0.5", UIMax = "1.0", ClampMin = "0.5", ClampMax = "1.0"))
		float AvoidanceSmoothingRatio = 0.85f;

	UPROPERTY(EditAnywhere, Category = AdvancedMovement)
		bool TerrainHugging = false;

	// How swiftly to get down onto the terrain when above it.
	UPROPERTY(EditAnywhere, Category = AdvancedMovement, meta = (EditCondition = "TerrainHugging"))
		float TerrainHuggingSpeed = 1.0f;

	// The maximum rate at which we try to hug terrain when above it.
	UPROPERTY(EditAnywhere, Category = AdvancedMovement, meta = (EditCondition = "TerrainHugging"))
		float TerrainHuggingMaxSpeed = 3000.0f;

	// Max time delta for each discrete simulation step.
	UPROPERTY(EditAnywhere, Category = AdvancedMovement, meta = (ClampMin = "0.01", ClampMax = "0.1", UIMin = "0.01", UIMax = "0.1"))
		float MaxSimulationTimeStep = 0.05f;

	// Max number of iterations used for each discrete simulation step.
	UPROPERTY(EditAnywhere, Category = AdvancedMovement, meta = (ClampMin = "1", ClampMax = "25", UIMin = "1", UIMax = "25"))
		int32 MaxSimulationIterations = 8;

#pragma region PickupMissile

	// Initialize the component.
	virtual void InitializeComponent() override;

	// Is the controlled component still within the world?
	virtual bool CheckStillInWorld();

	// Compute the velocity of the projectile.
	virtual FVector ComputeVelocity(const FVector& velocity, float deltaSeconds)
	{ return VelocityFromAcceleration(velocity, ComputeAcceleration(velocity, deltaSeconds), deltaSeconds); }

	// Stop simulating the projectile.
	virtual void StopSimulating(const FHitResult& hitResult)
	{ }

	// Use sub - stepping on a number of conditions, not just if it is explicitly switched on.
	virtual bool ShouldUseSubStepping() const
	{ return true; }

	// Get the time - step for the simulation.
	float GetSimulationTimeStep(float deltaSeconds) const;

	// Set the velocity inherited from the launcher.
	void SetInheritedVelocity(const FVector& velocity, bool absolute = false);

	// Set the initial torque of the projectile.
	void SetInitialTorque(const FRotator& torque)
	{ InitialTorque = torque; }

	// Set the inherited roll of the projectile.
	void SetInheritedRoll(float roll)
	{ InheritedRoll = roll; }

	// Avoid and optionally hug the terrain towards a particular target location.
	bool AvoidTerrain(float deltaSeconds, float terrainAvoidanceHeight, float forwardDistance, USceneComponent* targetComponent, const FVector& projectileLocation, const FVector& projectileDirection, FVector& terrainDirection, FVector& targetLocation, bool updateTerrainDirection);

	// How much rotation follows velocity.
	float RotationFollowsVelocity = 0.0f;

protected:

	// Compute the distance we should move in the given time, at a given a velocity.
	FVector ComputeMovementDelta(const FVector& velocity, float deltaSeconds);

	// Compute the acceleration in meters per second that you want to apply to the projectile. This adjusts the current velocity.
	virtual FVector ComputeAcceleration(const FVector& velocity, float deltaSeconds)
	{ return FVector::ZeroVector; }

	// Compute a new velocity from the existing velocity and acceleration over a given time.
	FVector VelocityFromAcceleration(FVector velocity, const FVector& acceleration, float deltaSeconds) const;

	// Determine if the projectile has collided with anything.
	bool GetCollision(UWorld* world, const FVector& start, const FVector& end, float& time, FVector& hitNormal, ECollisionChannel channel);

	// Minimum delta time considered when ticking. Delta times below this are not considered.
	// This is a very small non-zero positive value to avoid potential divide-by-zero in simulation code.
	static const float MinimumTickTime;

	// Transition from one direction to another clamped to a maximum rate of change in turning rate.
	FVector ClampedDirectionChange(const FVector& from, const FVector& to, float turningRate, float deltaSeconds, float smoothingRatio, bool hardLock, bool& clamped);

	// Calculate the acceleration from the original velocity, the new direction vector and the homing acceleration magnitude.
	FVector AccelerationFromDirection(const FVector& velocity, const FVector& direction, float homingAccelerationMagnitude, float deltaSeconds);

	// Merge the terrain avoidance factors into the general direction following.
	FVector MergeTerrainAvoidance(const FVector& targetForward, FVector avoidingNormal, const FVector& originalDirection, const FVector& avoidingDirection);

	// Timer used during the lifetime of the movement.
	float Timer = 0.0f;

	// The velocity inherited from the parent launcher.
	FVector InheritedVelocity = FVector::ZeroVector;

	// The initial torque of the projectile, to apply before it starts to thrust.
	FRotator InitialTorque = FRotator::ZeroRotator;

	// Collision query for terrain avoidance.
	FCollisionQueryParams TerrainQueryParams = FCollisionQueryParams(TEXT("MissileTerrainSensor"), true);

	// The last ground location detected when avoiding terrain.
	FVector LastGroundLocation = FVector::ZeroVector;

	// Is LastGroundLocation valid?
	bool LastGroundLocationValid = false;

	// The rate used to drop down towards terrain when hugging it.
	float DropRate = 0.0f;

	// The roll value we've inherited from the launcher.
	float InheritedRoll = 0.0f;

	// Are we seeking a surface for terrain hugging? -1 for no, 0 or +1 for yes, depending on the seeking direction.
	int32 SeekingSurface = -1;

#pragma endregion PickupMissile

};
