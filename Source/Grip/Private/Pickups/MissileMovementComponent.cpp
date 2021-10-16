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

#include "pickups/missilemovementcomponent.h"
#include "vehicle/flippablevehicle.h"
#include "gamemodes/basegamemode.h"

DEFINE_LOG_CATEGORY(GripLogMissile);

/**
* Construct a missile movement component.
***********************************************************************************/

UMissileMovementComponent::UMissileMovementComponent()
{
	if (FMath::RandBool() == true)
	{
		TrackingWobble = FMath::FRandRange(0.5f, 1.0f);
	}
}

#pragma region PickupMissile

/**
* Do the regular update tick.
***********************************************************************************/

void UMissileMovementComponent::TickComponent(float deltaSeconds, enum ELevelTick tickType, FActorComponentTickFunction* thisTickFunction)
{
	Super::TickComponent(deltaSeconds, tickType, thisTickFunction);

	// Skip if don't want component updated when not rendered or updated component can't move.

	if (ShouldSkipUpdate(deltaSeconds) == true)
	{
		return;
	}

	// If we've lost the updated component or physics simulation is on for that then don't
	// bother updating it.

	if (GRIP_OBJECT_VALID(UpdatedComponent) == false ||
		UpdatedComponent->IsSimulatingPhysics())
	{
		return;
	}

	AActor* actorOwner = UpdatedComponent->GetOwner();

	if (actorOwner == nullptr ||
		CheckStillInWorld() == false)
	{
		return;
	}

	if (RandomDrift.IsZero() == false)
	{
		// We'll only have drift if we don't have a target. The target will be specified in advance
		// so we just drift away from it over time as we don't have a lock on anything.

		FVector offset = TargetLocation - actorOwner->GetActorLocation();
		FRotator rotate(0.0f, RandomDrift.X * deltaSeconds, 0.0f);

		offset = rotate.RotateVector(offset);
		offset.Z += RandomDrift.Y * 10000.0f * deltaSeconds;

		TargetLocation = actorOwner->GetActorLocation() + offset;
	}

	FHitResult hitResult;
	float remainingTime = deltaSeconds;
	AHomingMissile* missile = Cast<AHomingMissile>(actorOwner);
	FCollisionQueryParams queryParams("MissileTest", false);

	if (missile != nullptr)
	{
		queryParams.AddIgnoredActor(missile->GetLaunchPlatform());
	}

	// Handle the main update of the movement.

	while (remainingTime >= MinimumTickTime && actorOwner->IsPendingKill() == false && GRIP_OBJECT_VALID(UpdatedComponent) == true)
	{
		// Perform sub-stepping to improve the movement accuracy.

		float iterationSeconds = ShouldUseSubStepping() ? GetSimulationTimeStep(deltaSeconds) : remainingTime;

		remainingTime -= iterationSeconds;

		if (Thrusting == true)
		{
			Timer += iterationSeconds;
		}

		FVector velocity = ComputeVelocity(Velocity, iterationSeconds);
		FVector movementDelta = ComputeMovementDelta(velocity, iterationSeconds);

		if (movementDelta.IsZero() == false)
		{
			FRotator newRotation = actorOwner->GetActorRotation();

			if (Thrusting == false)
			{
				// Twist the missile around to its initial torque value.

				newRotation += InitialTorque * iterationSeconds;
			}

			newRotation = FMath::Lerp(newRotation, velocity.Rotation(), RotationFollowsVelocity);
			newRotation.Normalize();
			newRotation.Roll = InheritedRoll;

			// Determine if we've hit something in the scenery.

			FVector location = actorOwner->GetActorLocation();

			if (GetWorld()->LineTraceSingleByChannel(hitResult, location, location + movementDelta, ABaseGameMode::ECC_Missile, queryParams) == true && hitResult.bBlockingHit == true)
			{
				// If so, handle the impact.

				HandleImpact(hitResult, iterationSeconds, movementDelta);

				// If we've no UpdatedComponent any more then we know the impact knocked it out.

				if (GRIP_OBJECT_VALID(UpdatedComponent) == false)
				{
					break;
				}
			}

			MoveUpdatedComponent(movementDelta, newRotation, false);

			if (missile != nullptr &&
				missile->OnMove() == true)
			{
				break;
			}
		}

		// Only calculate new velocity if events didn't change it during the movement update.

		Velocity = velocity;
	}

	UpdateComponentVelocity();
}

/**
* Compute the acceleration in meters per second that you want to apply to the
* projectile. This adjusts the current velocity.
***********************************************************************************/

FVector UMissileMovementComponent::ComputeAcceleration(const FVector& velocity, float deltaSeconds)
{
	ArrestingTurn = false;

	if (Thrusting == false)
	{
		// If we're not thrusting then just apply gravity.

		// The initial impulse and the constantly adjusting launcher velocity takes care of the rest,
		// these are provided constantly from the homing missile until ignition and thrusting begins.

		return FVector(0.0f, 0.0f, GetWorld()->GetGravityZ());
	}
	else
	{
		// Between 0 and 1 for how much low speed vs. high speed turning rate we should use.

		float accelerationRatio = FMathEx::GetInverseRatio(Timer, 0.0f, AccelerationTime);

		// The turning rate ratio, to scale turns between StartSpeedTurnRate and TopSpeedTurnRate.

		float turnRateRatio = 1.0f - accelerationRatio;

		// How much acceleration to apply, initially high and decreasing until we've hit the acceleration time.

		float homingAccelerationMagnitude = HomingAccelerationMagnitude * accelerationRatio;

		if (TargetSpeed > KINDA_SMALL_NUMBER)
		{
			// If we're using maximum speed instead then compute turn rate from that and current speed.
			// Also slack off the acceleration as we get closer to the maximum speed. Capping the velocity
			// against maximum speed is done elsewhere, in VelocityFromAcceleration.

			float speed = velocity.Size();
			float speedRatio = speed / FMathEx::KilometersPerHourToCentimetersPerSecond(TargetSpeed);

			if (speedRatio > 1.0f)
			{
				homingAccelerationMagnitude = -HomingAccelerationMagnitude * FMath::Pow(FMath::Min(speedRatio - 1.0f, 1.0f), 0.5f);
			}
			else
			{
				homingAccelerationMagnitude = HomingAccelerationMagnitude * (1.0f - speedRatio);
			}

			turnRateRatio = FMathEx::GetRatio(speed, 0.0f, FMathEx::KilometersPerHourToCentimetersPerSecond(TargetSpeed));
		}

		// Geometry of movement.

		FVector targetLocation = TargetLocation;
		FVector missileLocation = UpdatedComponent->GetComponentLocation();
		FVector missileDirection = UpdatedComponent->GetComponentTransform().GetUnitAxis(EAxis::X);

		// Cancel the snaky sine movement as you get close to the target.
		// It'll be at 1 until 2 seconds out, then drop to 0 as it closes in.

		float sineRatio = FMathEx::GetRatio(GetTimeToTarget(), 0.0f, 2.0f);

		// The safe height ratio comes down as we reach the target to as to head more directly to it.

		float safeHeightRatio = FMathEx::EaseInOut(sineRatio);

		if (LockLost == false &&
			GRIP_OBJECT_VALID(HomingTargetComponent) == true)
		{
			// If we have something to home against, then process with the targeting.

			targetLocation = GetHomingTargetLocation();

			ABaseVehicle* vehicle = Cast<ABaseVehicle>(HomingTargetComponent->GetAttachmentRootActor());

			if (vehicle != nullptr)
			{
				vehicle->ResetAttackTimer();

				if (vehicle->IsVehicleDestroyed() == true)
				{
					LockLost = true;
				}
				else
				{
					// If the target is a vehicle, then let's do some intelligent targeting to try to
					// make sure we hit the damn thing.

					float aimHigh = FMathEx::MetersToCentimeters(5.0f);
					FVector targetVelocity = vehicle->GetPredictedVelocity();
					FVector launchDirection = vehicle->GetLaunchDirection();

					if (TerrainAvoidanceHeight > KINDA_SMALL_NUMBER)
					{
						aimHigh = FMath::Min(aimHigh, TerrainAvoidanceHeight);
					}

					// Aim ahead of the target using its velocity.

					targetLocation += targetVelocity * 0.25f;

					// We want to target 150cm over the car with respect to its driving surface.
					// And we also add in a degree of safety with aim high and safe height ratio.

					targetLocation += launchDirection * (150.0f + (aimHigh * safeHeightRatio));
				}
			}
		}

		// Handle the loss of lock to the target.

		if (LockLost == false &&
			LoseLockOnRear == true &&
			FVector::DotProduct(targetLocation - missileLocation, missileDirection) < 0.0f)
		{
			LockLost = true;
		}

		if (LockLost == true)
		{
			TerrainAvoidanceHeight = 0.0f;
			targetLocation = missileLocation + (missileDirection * FMathEx::MetersToCentimeters(33.0f));
		}

		// So targetLocation is where we are aiming for.

		TargetLocation = TerrainAimLocation = targetLocation;

		// Update how much rotation follows velocity, merging into fully following
		// after the ejection phase has completed.

		RotationFollowsVelocity = FMathEx::GetRatio(Timer, 0.0f, 0.666f);
		RotationFollowsVelocity = FMathEx::EaseInOut(RotationFollowsVelocity);

		FRotator sineRotation(0.25f, 0.7f, 0.0f);

		// No wobbling when closing in tight on the target because it can screw with
		// the effectiveness of the targeting.

		float minWobbleDistance = FMathEx::MetersToCentimeters(100.0f);
		float targetDistance = (targetLocation - missileLocation).Size();

		if (targetDistance < minWobbleDistance)
		{
			sineRotation *= 0.25f + ((1.0f - (targetDistance / minWobbleDistance)) * 0.75f);
		}

		// Fade the wobble with time anyway.

		TrackingWobble -= deltaSeconds * 0.15f;
		TrackingWobble = FMath::Max(TrackingWobble, 0.0f);

		sineRotation *= TrackingWobble * sineRatio * FMath::Sin(Timer * 8.0f);

		// Firstly, calculate a maximum turn rate based on the speed of the missile.
		// The amount of turning ability the missile has, based on two numbers for low speed and high speed turning.

		bool clamped = false;
		float turnRate = FMath::Lerp(StartSpeedTurnRate, TopSpeedTurnRate, turnRateRatio);
		FVector idealDirection = FMathEx::LocationsToNormal(missileLocation, targetLocation);
		FVector newDirection = ClampedDirectionChange(missileDirection, idealDirection, turnRate, deltaSeconds, DirectionSmoothingRatio, true, clamped);

		ArrestingTurn |= clamped;

		FRotator newRotation = newDirection.Rotation() + sineRotation;

		newDirection = FMath::Lerp(missileDirection.Rotation(), newRotation, RotationFollowsVelocity).Vector();

		FVector acceleration = AccelerationFromDirection(velocity, newDirection, homingAccelerationMagnitude, deltaSeconds);

		// Up to now, the important tracking has been done and we now have a new missile direction.
		// We now need to do terrain avoidance to ensure we don't hit the terrain by following that
		// direction and possibly adjust that new acceleration / direction.

		TargetDirection = VelocityFromAcceleration(velocity, acceleration, deltaSeconds);
		TargetDirection.Normalize();
		TerrainAimDirection = TargetDirection;

		if (TerrainAvoidanceHeight > KINDA_SMALL_NUMBER)
		{
			// Aim ahead at least 50 meters or 1.5 seconds at current velocity, capped at the target distance.

			float aimAhead = FMath::Min(FMath::Max(50.0f * 100.0f, velocity.Size()) * 1.5f, targetDistance);
			FVector aimLocation = missileLocation + (TargetDirection * aimAhead);
			float avoidanceHeight = FMath::Max(TerrainAvoidanceHeight * safeHeightRatio, 3.0f * 100.0f);

			if (AvoidTerrain(deltaSeconds, avoidanceHeight, aimAhead, HomingTargetComponent, missileLocation, TargetDirection, TerrainDirection, aimLocation, false) == true)
			{
				// Now we need to munge the terrain avoidance direction with the original missile direction.

				TerrainAimDirection = FMathEx::LocationsToNormal(missileLocation, aimLocation);
				TerrainAimDirection = ClampedDirectionChange(missileDirection, TerrainAimDirection, turnRate, deltaSeconds, AvoidanceSmoothingRatio, true, clamped);
				TerrainAimDirection = MergeTerrainAvoidance(TargetDirection, TerrainDirection * -1.0f, newDirection, TerrainAimDirection);

				ArrestingTurn |= clamped;

				newRotation = TerrainAimDirection.Rotation() + sineRotation;

				TerrainAimLocation = aimLocation;
				TerrainAimDirection = FQuat::Slerp(missileDirection.Rotation().Quaternion(), newRotation.Quaternion(), RotationFollowsVelocity).Rotator().Vector();

				acceleration = AccelerationFromDirection(velocity, TerrainAimDirection, homingAccelerationMagnitude, deltaSeconds);
			}
		}

		return acceleration;
	}
}

/**
* When the simulation stops, just disconnect from the missile and stop updating it.
***********************************************************************************/

void UMissileMovementComponent::StopSimulating(const FHitResult& hitResult)
{
	if (GRIP_OBJECT_VALID(UpdatedComponent) == true)
	{
		AHomingMissile* missile = Cast<AHomingMissile>(UpdatedComponent->GetAttachmentRootActor());

		if (missile != nullptr)
		{
			missile->Explode(hitResult.GetActor(), &hitResult);
		}
	}

	SetUpdatedComponent(nullptr);

	Velocity = FVector::ZeroVector;
}

/**
* If the missile hits anything, then just stop simulating the movement on it.
***********************************************************************************/

void UMissileMovementComponent::HandleImpact(const FHitResult& hitResult, float deltaSeconds, const FVector& moveDelta)
{
	AHomingMissile* missile = Cast<AHomingMissile>(UpdatedComponent->GetAttachmentRootActor());

	// Missiles can't hit the launch platform, ever.

	if (missile != nullptr &&
		missile->GetLaunchPlatform() != nullptr &&
		hitResult.GetActor() == missile->GetLaunchPlatform())
	{
		return;
	}

#if GRIP_DEBUG_HOMING_MISSILE

	if (otherVehicle != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Missile hit a vehicle"));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Missile hit something"));
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, *hitResult.GetActor()->GetName());
	}

	UE_LOG(GripLogMissile, Log, TEXT("Missile hit %s"), *hitResult.GetActor()->GetName());

#endif // GRIP_DEBUG_HOMING_MISSILE

	StopSimulating(hitResult);
}

/**
* Get the current homing target location.
***********************************************************************************/

FVector UMissileMovementComponent::GetHomingTargetLocation() const
{
	if (GRIP_OBJECT_VALID(HomingTargetComponent) == true)
	{
		AHomingMissile* missile = Cast<AHomingMissile>(UpdatedComponent->GetAttachmentRootActor());

		return AHomingMissile::GetTargetLocationFor(HomingTargetComponent->GetOwner(), missile->HomingTargetOffset);
	}
	else
	{
		return TargetLocation;
	}
}

/**
* Get the time in seconds before impacting target (assuming straight terminal phase
* and constant speed).
***********************************************************************************/

float UMissileMovementComponent::GetTimeToTarget() const
{
	const float MaxTime = 1000000.0f;

	if (GRIP_OBJECT_VALID(UpdatedComponent) == true)
	{
		FVector vd = FVector::ZeroVector;
		FVector ld = FVector::ZeroVector;

		if (GRIP_OBJECT_VALID(HomingTargetComponent) == true)
		{
			AActor* target = HomingTargetComponent->GetOwner();
			FVector v0 = Velocity;
			FVector v1 = target->GetVelocity();

			ld = AHomingMissile::GetTargetLocationFor(target, FVector::ZeroVector) - UpdatedComponent->GetComponentLocation();
			vd = v0 - v1;
		}
		else
		{
			ld = TargetLocation - UpdatedComponent->GetComponentLocation();
			vd = Velocity;
		}

		FQuat frame = UpdatedComponent->GetComponentQuat().Inverse();

		float distance = frame.RotateVector(ld).X;
		float velocity = frame.RotateVector(vd).X;

		if (FMath::Abs(velocity) < KINDA_SMALL_NUMBER)
		{
			// Not closing at all, so just return a very large number while also avoiding a nasty
			// divide by zero.

			return MaxTime;
		}
		else
		{
			float time = distance / velocity;

			return (time < 0.0f) ? MaxTime : time;
		}
	}
	else
	{
		return MaxTime;
	}
}

/**
* Is the missile likely to hit the target?
***********************************************************************************/

bool UMissileMovementComponent::IsLikelyToHitTarget()
{
	if (GRIP_OBJECT_VALID(UpdatedComponent) == true &&
		GRIP_OBJECT_VALID(HomingTargetComponent) == true)
	{
		FVector missileToTarget = GetHomingTargetLocation() - UpdatedComponent->GetComponentLocation();
		FVector missileVelocity = Velocity;
		FVector targetVelocity = HomingTargetComponent->GetOwner()->GetVelocity();

		missileToTarget.Normalize();
		missileVelocity.Normalize();
		targetVelocity.Normalize();

		// Check the geometry of the relative velocities and the direction the target is
		// from the missile to see if they're in rough alignment.

		if (FVector::DotProduct(missileVelocity, targetVelocity) > 0.0f)
		{
			if (FVector::DotProduct(missileVelocity, missileToTarget) > 0.8f)
			{
				return true;
			}
		}
	}

	return false;
}

#pragma endregion PickupMissile
