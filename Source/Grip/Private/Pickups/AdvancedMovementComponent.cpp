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

#include "pickups/advancedmovementcomponent.h"
#include "gamemodes/basegamemode.h"

/**
* Construct an advanced movement component.
***********************************************************************************/

UAdvancedMovementComponent::UAdvancedMovementComponent()
{
	bUpdateOnlyIfRendered = false;
	bWantsInitializeComponent = true;

	Velocity = FVector::ZeroVector;
}

#pragma region PickupMissile

const float UAdvancedMovementComponent::MinimumTickTime = 0.0002f;

/**
* Initialize the component.
***********************************************************************************/

void UAdvancedMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (Velocity.SizeSquared() > 0.0f)
	{
		UpdateComponentVelocity();
	}
}

/**
* Compute the distance we should move in the given time, at a given a velocity.
***********************************************************************************/

FVector UAdvancedMovementComponent::ComputeMovementDelta(const FVector& velocity, float deltaSeconds)
{
	// Velocity Verlet integration (http://en.wikipedia.org/wiki/Verlet_integration#Velocity_Verlet)

	return (Velocity * deltaSeconds) + (velocity - Velocity) * (0.5f * deltaSeconds);
}

/**
* Compute a new velocity from the existing velocity and acceleration over a given
* time.
***********************************************************************************/

FVector UAdvancedMovementComponent::VelocityFromAcceleration(FVector velocity, const FVector& acceleration, float deltaSeconds) const
{
	velocity += (acceleration * deltaSeconds);

	float speed = GetMaxSpeed();

	if (speed > 0.0f &&
		IsExceedingMaxSpeed(speed) == true)
	{
		velocity = velocity.GetClampedToMaxSize(speed);
	}

	return ConstrainDirectionToPlane(velocity);
}

/**
* Check to see if the projectile is still in the world.
***********************************************************************************/

bool UAdvancedMovementComponent::CheckStillInWorld()
{
	if (GRIP_OBJECT_VALID(UpdatedComponent) == false)
	{
		return false;
	}

	AWorldSettings* settings = GetWorld()->GetWorldSettings(true);

	if (settings->bEnableWorldBoundsChecks == 0)
	{
		return true;
	}

	AActor* owner = UpdatedComponent->GetOwner();

	if (owner == nullptr)
	{
		return false;
	}

	if (owner->GetActorLocation().Z < settings->KillZ)
	{
		auto type = settings->KillZDamageType ? settings->KillZDamageType->GetDefaultObject<UDamageType>() : GetDefault<UDamageType>();

		owner->FellOutOfWorld(*type);

		return false;
	}
	else if (UpdatedComponent->IsRegistered() == true)
	{
		FBox box = UpdatedComponent->Bounds.GetBox();

		if (box.Min.X < -HALF_WORLD_MAX || box.Max.X > HALF_WORLD_MAX ||
			box.Min.Y < -HALF_WORLD_MAX || box.Max.Y > HALF_WORLD_MAX ||
			box.Min.Z < -HALF_WORLD_MAX || box.Max.Z > HALF_WORLD_MAX)
		{
			owner->OutsideWorldBounds();
			owner->SetActorEnableCollision(false);

			FHitResult hitResult(1.0f);

			StopSimulating(hitResult);

			return false;
		}
	}

	return true;
}

/**
* Get the time-step for the simulation.
***********************************************************************************/

float UAdvancedMovementComponent::GetSimulationTimeStep(float deltaSeconds) const
{
	// No less than MinimumTickTime (to avoid potential divide-by-zero during simulation).

	float iterations = FMath::Min(FMath::CeilToInt(FMath::Max(MinimumTickTime, deltaSeconds) / MaxSimulationTimeStep), MaxSimulationIterations);

	return deltaSeconds / iterations;
}

/**
* Determine if the projectile has collided with anything.
***********************************************************************************/

bool UAdvancedMovementComponent::GetCollision(UWorld* world, const FVector& start, const FVector& end, float& time, FVector& hitNormal, ECollisionChannel channel)
{
	if ((end - start).Size() > SMALL_NUMBER)
	{
		FHitResult hitResult;

		if (world->LineTraceSingleByChannel(hitResult, start, end, channel, FCollisionQueryParams(TEXT("CollisionSensor"), true, UpdatedComponent->GetOwner())) == true)
		{
			time = hitResult.GetComponent() ? hitResult.Time : 1.f;
			hitNormal = hitResult.Normal;

			return true;
		}
	}

	return false;
}

/**
* Set the velocity inherited from the launcher.
***********************************************************************************/

void UAdvancedMovementComponent::SetInheritedVelocity(const FVector& velocity, bool absolute)
{
	if (absolute == true)
	{
		Velocity = velocity; InheritedVelocity = velocity;
	}
	else
	{
		Velocity -= InheritedVelocity; InheritedVelocity = velocity; Velocity += InheritedVelocity;
	}
}

/**
* Avoid and optionally hug the terrain towards a particular target location.
***********************************************************************************/

bool UAdvancedMovementComponent::AvoidTerrain(float deltaSeconds, float terrainAvoidanceHeight, float forwardDistance, USceneComponent* targetComponent, const FVector& projectileLocation, const FVector& projectileDirection, FVector& terrainDirection, FVector& targetLocation, bool updateTerrainDirection)
{
	bool result = false;

	if (terrainAvoidanceHeight > KINDA_SMALL_NUMBER)
	{
		if (GRIP_OBJECT_VALID(targetComponent) == true &&
			TerrainQueryParams.GetIgnoredActors().Num() == 0)
		{
			TerrainQueryParams.AddIgnoredActor(targetComponent->GetAttachmentRootActor());
		}

		FHitResult hitResult;
		FVector toTarget = targetLocation - projectileLocation;
		float distance = toTarget.Size();

		// Reduce the terrain avoidance height as we close in on the target.

		bool groundLocationValid = false;
		float minHeight = terrainAvoidanceHeight;
		FVector groundLocation = FVector::ZeroVector;

		// If we're seeking a surface in the opposite direction then likewise send the
		// line trace in the opposite direction.

		FVector useTerrainDirection = terrainDirection * ((SeekingSurface == 1) ? -1.0f : 1.0f);

		if (TerrainHugging == true)
		{
			// Look down towards the terrain from where we are to identify where the ground is
			// beneath the projectile.

			FVector end = projectileLocation + (useTerrainDirection * 50.0f * 100.0f);

			if (GetWorld()->LineTraceSingleByChannel(hitResult, projectileLocation, end, ABaseGameMode::ECC_TerrainFollowing, TerrainQueryParams) == true && hitResult.bBlockingHit == true)
			{
				if (SeekingSurface == 1)
				{
					// If we were seeking before, then set the terrainDirection into the direction to use
					// next time to prevent unnecessary seeking.

					terrainDirection = useTerrainDirection;
				}

				SeekingSurface = -1;

				// We've found the ground underneath the projectile.

				groundLocation = LastGroundLocation = hitResult.ImpactPoint;
				groundLocationValid = LastGroundLocationValid = true;

				if (updateTerrainDirection == true)
				{
					// If we've been told to update the terrainDirection because we want it to be dynamic,
					// usually for the Hydra or the RamRaider, then we update it here but only if our
					// new direction isn't massively away from what it is already.

					FVector newTerrainDirection = hitResult.ImpactNormal * -1.0f;

					if (FVector::DotProduct(terrainDirection, newTerrainDirection) > 0.5f)
					{
						terrainDirection = newTerrainDirection;
					}
				}
			}
			else
			{
				// If we're already seeking a surface, then next time try in the opposite direction.

				if (SeekingSurface != -1)
				{
					SeekingSurface ^= 1;
				}

				// If we couldn't find a surface and we're not already seeking a surface, then set
				// us to surface seeking and next time try in the opposite direction.

				if (SeekingSurface == -1 &&
					updateTerrainDirection == true)
				{
					SeekingSurface = 1;
				}

				// Reuse the last known ground position if possible.

				groundLocationValid = LastGroundLocationValid;

				if (groundLocationValid == true)
				{
					groundLocation = LastGroundLocation = FVector::PointPlaneProject(projectileLocation, LastGroundLocation, terrainDirection * -1.0f);
				}
			}

			if (groundLocationValid == true)
			{
				// Adjust the target position to head towards the ground.

				// So the target dropRate should be to close the gap in two seconds.

				// Look at the angle difference between the velocity vector and the forward vector.
				// Look at the ground distance and the current speed.
				// Ensure we adjust terrain direction velocity in time.

				groundLocation += terrainDirection * -terrainAvoidanceHeight;

				float dropRate = FVector::PointPlaneDist(projectileLocation, groundLocation, terrainDirection * -1.0f) / 4.0f;

				dropRate = FMath::Min(dropRate, TerrainHuggingMaxSpeed * deltaSeconds);

				float ratio = FMathEx::GetSmoothingRatio(0.5f, deltaSeconds);

				DropRate = FMath::Lerp(dropRate, DropRate, ratio);

				targetLocation += terrainDirection * DropRate * TerrainHuggingSpeed;

				result = true;
			}
		}

		// So, trace out forwardDistance ahead of the projectile, looking down by the terrain avoidance height
		// and see if there's an impact point to avoid.

		FVector end = projectileDirection;

		end *= FMath::Min(distance, forwardDistance);
		end += terrainDirection * minHeight;
		end += projectileLocation;

		// TODO: More casts to avoid terrain close to the current projectile location.

		if (GetWorld()->LineTraceSingleByChannel(hitResult, projectileLocation, end, ABaseGameMode::ECC_TerrainFollowing, TerrainQueryParams) == true)
		{
			// So this is where we want to be above the ground.

			if (FVector::DotProduct(hitResult.ImpactNormal, terrainDirection) > 0.5f)
			{
				// Avoid hitting the ceiling opposite our given down direction.
			}
			else
			{
				FVector avoidVector = (hitResult.ImpactPoint - (terrainDirection * minHeight)) - projectileLocation;

				avoidVector.Normalize();
				avoidVector *= distance;

				// Adjust the target position to simply avoid the terrain instead.

				targetLocation = projectileLocation + avoidVector;

				result = true;
			}
		}
	}

	return result;
}

/**
* Transition from one direction to another clamped to a maximum rate of change in
* turning rate.
***********************************************************************************/

FVector UAdvancedMovementComponent::ClampedDirectionChange(const FVector& from, const FVector& to, float turningRate, float deltaSeconds, float smoothingRatio, bool hardLock, bool& clamped)
{
	FRotator fromRotation = from.Rotation();
	FRotator toRotation = to.Rotation();

	if (hardLock == true)
	{
		// Make the missile over-compensate the steering to get a harder lock.

		FRotator difference = FMathEx::GetSignedDegreesDifference(fromRotation, toRotation);

		difference *= 0.5f;

		// No more than 20 degrees per second additional compensation.

		difference.Yaw = FMath::Clamp(difference.Yaw, -20.0f, 20.0f);
		difference.Pitch = FMath::Clamp(difference.Pitch, -20.0f, 20.0f);

		toRotation += difference * deltaSeconds; toRotation.Normalize();
	}

	// Perform the rotation rate clamping so we don't get drastic direction changes, showing a realistic turning circle.

	FRotator difference = FMathEx::GetSignedDegreesDifference(fromRotation, toRotation);

	turningRate *= deltaSeconds;
	clamped = FMath::Abs(difference.Yaw) > turningRate || FMath::Abs(difference.Pitch) > turningRate;

	if (clamped == true)
	{
		difference.Yaw = FMath::Clamp(difference.Yaw, -turningRate, turningRate);
		difference.Pitch = FMath::Clamp(difference.Pitch, -turningRate, turningRate);

		toRotation = fromRotation + difference; toRotation.Normalize();
	}

	if (smoothingRatio > KINDA_SMALL_NUMBER)
	{
		// Smooth rotation changes from where we are to where we want to be. This avoids harsh juddering only.
		// NOTE: Try not to smooth it too much as this just causes the projectile to hit things when cornering hard.

		// We could do with a better smoothing algorithm here which avoid juddering but allows large but sustained,
		// consistent changes in rotation direction. That way we can avoid juddering and allow high maneuverability.

		smoothingRatio = FMathEx::GetSmoothingRatio(smoothingRatio, deltaSeconds);

		toRotation = FMath::Lerp(toRotation, fromRotation, smoothingRatio);
	}

	return toRotation.Vector();
}

/**
* Calculate the acceleration from the original velocity, the new direction vector
* and the homing acceleration magnitude.
***********************************************************************************/

FVector UAdvancedMovementComponent::AccelerationFromDirection(const FVector& velocity, const FVector& direction, float homingAccelerationMagnitude, float deltaSeconds)
{
	FVector acceleration;

	acceleration = direction * velocity.Size();
	acceleration -= velocity;
	acceleration /= deltaSeconds;
	acceleration += direction * homingAccelerationMagnitude;

	return acceleration;
}

/**
* Merge the terrain avoidance factors into the general direction following.
***********************************************************************************/

FVector UAdvancedMovementComponent::MergeTerrainAvoidance(const FVector& targetForward, FVector avoidingNormal, const FVector& originalDirection, const FVector& avoidingDirection)
{
	// Do the terrain-avoidance pitch-merging in the projectile's direction space.

	FRotator avoidingRotation = FRotator::ZeroRotator;

	FMathEx::GetRotationFromForwardUp(targetForward, avoidingNormal, avoidingRotation);

	FRotator targetLocal = avoidingRotation.UnrotateVector(originalDirection).Rotation();
	FRotator avoidingLocal = avoidingRotation.UnrotateVector(avoidingDirection).Rotation();

	// Follow the original to-target direction before it was smoothed (this smoothing will be done again after this function).

	targetLocal.Pitch = avoidingLocal.Pitch;

	return avoidingRotation.RotateVector(targetLocal.Vector());
}

#pragma endregion PickupMissile
