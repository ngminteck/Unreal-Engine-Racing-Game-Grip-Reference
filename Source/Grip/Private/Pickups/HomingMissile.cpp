/**
*
* Scorpion homing missile implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Homing missile pickup type, one of the pickups used by vehicles in the game.
*
***********************************************************************************/

#include "pickups/homingmissile.h"
#include "vehicle/flippablevehicle.h"
#include "gamemodes/basegamemode.h"

/**
* Construct a UMissileHostInterface.
***********************************************************************************/

UMissileHostInterface::UMissileHostInterface(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{ }

/**
* Construct a homing missile.
***********************************************************************************/

AHomingMissile::AHomingMissile()
{
	PickupType = EPickupType::HomingMissile;

	MissileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MissileMesh"));

	SetRootComponent(MissileMesh);

	MissileMesh->bReturnMaterialOnMove = true;

	MissileMovement = CreateDefaultSubobject<UMissileMovementComponent>(TEXT("MissileMovement"));

	RocketTrail = CreateDefaultSubobject<UGripTrailParticleSystemComponent>(TEXT("RocketTrail"));
	GRIP_ATTACH(RocketTrail, RootComponent, NAME_None);

	RocketTrail->bAutoDestroy = false;
	RocketTrail->bAutoActivate = false;
	RocketTrail->SetHiddenInGame(true);

	RocketLightStreak = CreateDefaultSubobject<ULightStreakComponent>(TEXT("RocketLightStreak"));
	GRIP_ATTACH(RocketLightStreak, RootComponent, NAME_None);

	RocketLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RocketLight"));
	GRIP_ATTACH(RocketLight, RootComponent, NAME_None);

	RocketLight->bAutoActivate = false;
	RocketLight->SetHiddenInGame(true);

	ExplosionForce = CreateDefaultSubobject<URadialForceComponent>(TEXT("ExplosionForce"));
	ExplosionForce->bAutoActivate = false;
	GRIP_ATTACH(ExplosionForce, RootComponent, NAME_None);

	PrimaryActorTick.bCanEverTick = true;
}

#pragma region PickupMissile

/**
* Do some post initialization just before the game is ready to play.
***********************************************************************************/

void AHomingMissile::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Setup the rocket.

	RocketIntensity = RocketLight->Intensity;
	RocketLight->SetIntensity(0.0f);
	RocketLightStreak->SetAddPoints(false);

	// Setup the light streak.

	FlareSize = RocketLightStreak->Size;
	FlareAspectRatio = RocketLightStreak->AspectRatio;
	RocketLightStreak->Size = 0.0f;
	RocketLightStreak->CentralSize = 0.0f;

	// Make sure we're not colliding with anything as we'll be doing all that
	// with line traces in the movement code.

	MissileMesh->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);

	LastLocation = LastSubLocation = GetActorLocation();
}

/**
* Do some shutdown when the actor is being destroyed.
***********************************************************************************/

void AHomingMissile::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	GRIP_REMOVE_FROM_GAME_MODE_LIST(Missiles);

	Super::EndPlay(endPlayReason);
}

/**
* Do the regular update tick.
***********************************************************************************/

void AHomingMissile::Tick(float deltaSeconds)
{
	Super::Tick(deltaSeconds);

	if (GRIP_POINTER_VALID(LaunchPlatform) == true)
	{
		Timer += deltaSeconds;

		FVector location = GetActorLocation();

		float moved = (location - LastLocation).Size();

		LastLocation = location;

		// One unit every 100 meters, there's large cycle for every unit.

		OpacityNoise.Tick(moved / FMathEx::MetersToCentimeters(100.0f));

		// One unit every 300 meters, there's large cycle for every unit.

		BrightnessNoise.Tick(moved / FMathEx::MetersToCentimeters(300.0f));

		// One unit every 100 meters, there's large cycle for every unit.

		SizeNoise.Tick(moved / FMathEx::MetersToCentimeters(100.0f));

		// Update the rocket trail to give it some realistic variation.

		if (GRIP_OBJECT_VALID(RocketTrail) == true)
		{
			RocketTrail->SetFloatParameter("SmokeAlpha", GetSmokeAlpha());
			RocketTrail->SetVectorParameter("SmokeColour", GetSmokeColor());
			RocketTrail->SetVectorParameter("SmokeSize", GetSmokeSize());
		}

		// If the missile is about to hit the target vehicle, record the fact,
		// picked up by the cinematic camera system.

		if (TargetWithinReach == false &&
			Cast<ABaseVehicle>(Target) != nullptr &&
			IsInTerminalRange(Target, -1.0f, 2.0f) == true)
		{
			RecordIncoming();
		}

		switch (CurrentState)
		{
		case EState::Ejecting:
		{
			// Inherit the launch car's speed. Not at all realistic, but visually more friendly
			// when accelerating away and having the missile not drop behind you.

			FVector launcherVelocity = MissileHost->GetHostVelocity();

			// For standard missiles, just take the parent velocity as it avoids problems with hitting the ground.

			// We keep updating it while we're ejecting to effectively lock the missile's ejection impulse
			// relative to the launch vehicle. The missile isn't attached exactly like a lot of other
			// components are, we just craft the effect of attachment by moving the missile with velocity to
			// stay relative to the vehicle.

			SetLauncherVelocity(launcherVelocity);

			// Never allow the missile to point towards the floor on launching, this also avoids a lot of problems
			// with it hitting the ground once the rocket motor kicks in.

			if (GRIP_OBJECT_VALID(LaunchVehicle) == true)
			{
				FVector surfaceNormal = LaunchVehicle->GuessSurfaceNormal();

				if (surfaceNormal.IsZero() == false)
				{
					FRotator rotation = GetActorRotation();
					FQuat surfaceQuat = surfaceNormal.ToOrientationQuat();
					FVector localDirection = surfaceQuat.UnrotateVector(rotation.Vector());

					// A minimum angle of roughly 1 and 5 degrees, depending on whether we're constrained in
					// upward motion or not, and only reaching that angle by the time ignition is to begin.

					float minAngle = (ConstrainUp == true) ? 0.02f : 0.1f;

					minAngle *= FMathEx::GetRatio(Timer, 0.0f, IgnitionTime);

					if (localDirection.X < minAngle)
					{
						localDirection.X = minAngle;
						localDirection.Normalize();

						FRotator newRotation = surfaceQuat.RotateVector(localDirection).Rotation();

						newRotation.Yaw = FMathEx::GravitateToTarget(rotation.Yaw, newRotation.Yaw, deltaSeconds * 45.0f);
						newRotation.Pitch = FMathEx::GravitateToTarget(rotation.Pitch, newRotation.Pitch, deltaSeconds * 45.0f);
						newRotation.Roll = rotation.Roll;

						SetActorRotation(newRotation);
					}
				}
			}

			// Handle the initial ejection and ignition.

			if (Timer > IgnitionTime)
			{
				Timer = 0.0f;

				Ignite();
			}
		}
		break;

		case EState::Flight:
		{
			// Update the rocket light streak, mostly its flare.

			float growTime = 0.25f;
			float shrinkTime = 2.0f;

			if (Timer < growTime + shrinkTime ||
				RocketLightStreak->Size != FlareSize)
			{
				if (Timer < growTime)
				{
					float sizeScale = FMath::Lerp(0.0f, 2.0f, FMath::Pow(Timer / growTime, 2.0f));

					RocketLightStreak->Size = sizeScale * FlareSize;

					if (sizeScale > 1.0f)
					{
						RocketLightStreak->AspectRatio = (1.0f / sizeScale) * FlareAspectRatio;
					}
				}
				else if (Timer < growTime + shrinkTime)
				{
					float sizeScale = FMath::Lerp(2.0f, 1.0f, FMath::Pow((Timer - growTime) / shrinkTime, 0.5f));

					RocketLightStreak->Size = sizeScale * FlareSize;
					RocketLightStreak->AspectRatio = (1.0f / sizeScale) * FlareAspectRatio;
				}
				else
				{
					RocketLightStreak->Size = FlareSize;
					RocketLightStreak->AspectRatio = FlareAspectRatio;
				}

				RocketLightStreak->CentralSize = RocketLightStreak->Size * 0.5f;
			}

			if (GRIP_OBJECT_VALID(Target) == true)
			{
				ABaseVehicle* targetVehicle = Cast<ABaseVehicle>(Target);

				if (GRIP_OBJECT_VALID(targetVehicle) == true)
				{
					// Try to dynamically determine the terrain direction for terrain avoidance when
					// the missile is closer to the target than the launcher. This helps with terrain
					// avoidance and makes it more effective.

					FVector missileLocation = GetActorLocation();
					FVector targetLocation = GetTargetLocationFor(Target, FVector::ZeroVector);
					float d0 = (missileLocation - LaunchPlatform->GetActorLocation()).SizeSquared();
					float d1 = (missileLocation - targetLocation).SizeSquared();
					bool determineDirection = Timer > 0.5f && d1 < d0;

					if (determineDirection == true)
					{
						// Determine the direction of the surface that the target vehicle is traveling on.

						FVector surfaceDirection = FVector::ZeroVector;
						bool directionValid = targetVehicle->IsPracticallyGrounded();

						if (directionValid == true)
						{
							// This is going to be the case the vast majority of the time.

							surfaceDirection = targetVehicle->GetSurfaceDirection();
						}
						else
						{
							// This will only happen when the target vehicle is airborne, so we see if there is any
							// scenery between the missile and the target, and use the surface normal of that impact
							// point to determine the surface direction.

							FHitResult hitResult;

							if (GetWorld()->LineTraceSingleByChannel(hitResult, missileLocation, targetLocation, ABaseGameMode::ECC_LineOfSightTest, MissileToTargetQueryParams) == true)
							{
								directionValid = true;
								surfaceDirection = hitResult.ImpactNormal * -1.0f;
							}
						}

						if (directionValid == true)
						{
							if (FVector::DotProduct(MissileMovement->TerrainDirection, surfaceDirection) < 0.0f)
							{
								// Surface suddenly flipped, so just flip with it.

								MissileMovement->TerrainDirection = surfaceDirection;
							}
							else
							{
								// Otherwise smoothly, but quickly, interpolate, by converting the direction into
								// a quaternion, slerping it, and then converting it back to a direction again.

								FQuat q0 = MissileMovement->TerrainDirection.ToOrientationQuat();
								FQuat q1 = surfaceDirection.ToOrientationQuat();
								float ratio = FMathEx::GetSmoothingRatio(0.75f, deltaSeconds);

								MissileMovement->TerrainDirection = FQuat::Slerp(q1, q0, ratio).Vector();
							}
						}
					}
				}
			}

			// Handle the rocket motor audio visual effects.

			if (RocketAudio != nullptr)
			{
				RocketAudio->SetVolumeMultiplier(FMath::Min(Timer * 6.0f, 3.5f));
			}

			RocketLight->SetIntensity(FMath::Min(Timer * 2.0f, 1.0f) * RocketIntensity);

			// If it's time to die, then die.

			if (DieAt != 0.0f &&
				Timer >= DieAt)
			{
				Explode(nullptr, nullptr);
			}
		}
		break;

		case EState::Exploding:

			// Wait for ten seconds post explosion before mopping everything up in destroying the pickup.

			if (Timer > 10.0f)
			{
				DestroyPickup();
			}
			break;
		}
	}
}

/**
* Ignite the missile.
***********************************************************************************/

void AHomingMissile::Ignite()
{
	CurrentState = EState::Flight;

	UGameplayStatics::SpawnSoundAttached(MissileHost->UseHumanPlayerAudio() ? IgnitionSound : IgnitionSoundNonPlayer, MissileMesh, NAME_None);

	RocketAudio = UGameplayStatics::SpawnSoundAttached(RocketSound, MissileMesh, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true, 0.0f);

	if (GRIP_OBJECT_VALID(RocketTrail) == true)
	{
		RocketTrail->SetHiddenInGame(false);
		RocketTrail->SetActive(true);
		RocketTrail->ActivateSystem(true);
	}

	RocketLightStreak->SetAddPoints(true);

	RocketLight->SetHiddenInGame(false);
	RocketLight->Activate();

	if (GRIP_OBJECT_VALID(LaunchVehicle) == true)
	{
		FVector surfaceNormal = LaunchVehicle->GuessSurfaceNormal();

		// Do some magic to stop the missile hitting the damn floor so often.

		if (surfaceNormal.IsZero() == false)
		{
			FQuat surfaceQuat = surfaceNormal.ToOrientationQuat();
			float minVelocity = (ConstrainUp == true) ? 10.0f : 100.0f;

			// Get a predicted velocity which more closely follows the vehicle's actual trajectory on launching.

			FVector launcherVelocity = LaunchVehicle->GetPredictedVelocity();
			float launcherSpeed = launcherVelocity.Size();

			// First, ensure that the launcher velocity isn't taking us towards the ground by more than
			// minVelocity centimeters per second.

			if (launcherSpeed > 25.0f)
			{
				FVector localVelocity = surfaceQuat.UnrotateVector(launcherVelocity);

				localVelocity.X = FMath::Max(minVelocity, localVelocity.X);
				launcherVelocity = surfaceQuat.RotateVector(localVelocity);

				SetLauncherVelocity(launcherVelocity);
			}

			// Next, ensure that the missile velocity isn't taking us towards the ground by more than
			// minVelocity centimeters per second.

			FVector localVelocity = surfaceQuat.UnrotateVector(MissileMovement->Velocity);

			localVelocity.X = FMath::Max(minVelocity, localVelocity.X);
			MissileMovement->Velocity = surfaceQuat.RotateVector(localVelocity);
		}
	}

	MissileMovement->IgniteMotor();

	if (GRIP_OBJECT_VALID(Target) == true)
	{
		MissileMovement->HomingTargetComponent = Target->GetRootComponent();

		// Missile movement does its own terrain avoidance so we only switch it on here.

		MissileMovement->TerrainAvoidanceHeight = FMathEx::MetersToCentimeters(8.0f);
	}

	// Make sure we ignore this and the target in line traces.

	if (GRIP_OBJECT_VALID(Target) == true)
	{
		MissileToTargetQueryParams.AddIgnoredActor(this);
		MissileToTargetQueryParams.AddIgnoredActor(Target);
	}

	if (DieAt == 0.0f &&
		RocketDuration > KINDA_SMALL_NUMBER)
	{
		DieAt = FMath::FRandRange(RocketDuration, RocketDuration * 1.25f);
	}
}

/**
* Explode the missile.
***********************************************************************************/

void AHomingMissile::Explode(AActor* hitActor, const FHitResult* hitResult)
{
	if (hitResult != nullptr)
	{
		UE_LOG(GripLogMissile, Log, TEXT("Missile exploding after hitting something"));
		UE_LOG(GripLogMissile, Log, TEXT("InRangeOfTarget %d"), (InRangeOfTarget == true) ? 1 : 0);
		UE_LOG(GripLogMissile, Log, TEXT("TargetWithinReach %d"), (TargetWithinReach == true) ? 1 : 0);
		UE_LOG(GripLogMissile, Log, TEXT("TerrainAvoidanceHeight %0.2f"), MissileMovement->TerrainAvoidanceHeight / 100.0f);
	}

	GRIP_REMOVE_FROM_GAME_MODE_LIST(Missiles);

	// If we have a target in mind, determine if we hit it or not.

	if (GRIP_OBJECT_VALID(Target) == true)
	{
		float distance = (GetTargetLocationFor(Target, HomingTargetOffset) - GetActorLocation()).Size();
		float blastRadius = (ProximityFuse + HomingTargetOffset.Size()) * 2.0f;

		if (distance <= blastRadius ||
			InRangeOfTarget == true)
		{
			hitActor = Target;
			TargetHit = true;
		}
	}

#if GRIP_DEBUG_HOMING_MISSILE

	if (MissileHost->GetVehicleIndex() == 0)
	{
		if (DieAt != 0.0f &&
			Timer >= DieAt &&
			(Target == nullptr || Target->IsA<AAdvancedDestructibleActor>() == false))
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Missile out of juice"));
		}
	}

#endif // GRIP_DEBUG_HOMING_MISSILE

	// Shutdown a whole bunch of things that we don't want to see or hear during the explosion.

	MissileMesh->SetHiddenInGame(true);

	if (RocketAudio != nullptr &&
		RocketAudio->IsPlaying() == true)
	{
		RocketAudio->Stop();
	}

	if (RocketTrail != nullptr)
	{
		RocketTrail->DeactivateSystem();
	}

	if (RocketLight != nullptr)
	{
		RocketLight->DestroyComponent();
		RocketLight = nullptr;
	}

	MissileMovement->SetUpdatedComponent(nullptr);

	FVector explosionLocation = GetActorLocation() + (MissileMesh->GetComponentRotation().Vector() * 250.0f);

	if (GRIP_OBJECT_VALID(ExplosionSound) == true)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, ExplosionSound, explosionLocation);
	}

	if (GRIP_OBJECT_VALID(ExplosionVisual) == true)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionVisual, explosionLocation, FRotator::ZeroRotator, true);
	}

	// Now handle the physics impulses.

	if (GRIP_OBJECT_VALID(ExplosionForce) == true)
	{
		ExplosionForce->FireImpulse();
	}

	ABaseVehicle* targetVehicle = Cast<ABaseVehicle>(Target);
	float chargedScale = (IsCharged() == true) ? 1.25f : 1.0f;

	if (targetVehicle != nullptr &&
		TargetHit == true)
	{
		FGameEvent gameEvent;

		gameEvent.LaunchVehicleIndex = MissileHost->GetVehicleIndex();
		gameEvent.TargetVehicleIndex = targetVehicle->VehicleIndex;
		gameEvent.PickupUsed = EPickupType::HomingMissile;
		gameEvent.PickupUsedWasCharged = IsCharged();
		gameEvent.EventType = EGameEventType::Impacted;

		bool shieldIrrelevant = false;

		if (MissTarget == true)
		{
			targetVehicle->MissileForce(VehicleExplosionForce * chargedScale, (HitPoints >> 1) * ((LaunchVehicle != nullptr) ? LaunchVehicle->GetDamageScale() : 1.0f), MissileHost->GetVehicleIndex(), GetActorLocation(), true, false, &gameEvent);
		}
		else
		{
			targetVehicle->MissileForce(VehicleExplosionForce * chargedScale, HitPoints * ((LaunchVehicle != nullptr) ? LaunchVehicle->GetDamageScale() : 1.0f), MissileHost->GetVehicleIndex(), GetActorLocation(), false, shieldIrrelevant, &gameEvent);
		}
	}
	else if (targetVehicle != nullptr &&
		TargetHit == false)
	{
		targetVehicle->AddPoints(100, false, nullptr, GetActorLocation());
	}

	ABaseVehicle::PeripheralExplosionForce(VehicleExplosionForce * chargedScale, (HitPoints >> 1) * ((LaunchVehicle != nullptr) ? LaunchVehicle->GetDamageScale() : 1.0f), MissileHost->GetVehicleIndex(), GetActorLocation(), MissTarget, FColor(255, 64, 0), targetVehicle, GetWorld(), ExplosionForce->Radius);

	Timer = 0.0f;
	CurrentState = EState::Exploding;
	RocketLightStreak->Size = 0.0f;
	RocketLightStreak->CentralSize = 0.0f;
}

/**
* Setup a false target for the missile to aim for in the absence of a real target.
***********************************************************************************/

void AHomingMissile::SetupFalseTarget()
{
	RandomDrift.X = FMath::FRandRange(-20.0f, 20.0f);
	RandomDrift.Y = FMath::FRandRange(0.0f, 10.0f);

	MissileMovement->FalseTarget(MissileHost->GetMissileFalseTarget(), RandomDrift);

	DieAt = Timer + 2.5f + (FMath::Rand() & 255) * (2.0f / 255.0f);
}

/**
* Select a target to aim for.
***********************************************************************************/

bool AHomingMissile::SelectTarget(AActor* launchPlatform, FPlayerPickupSlot* launchPickup, AActor*& existingTarget, TArray<TWeakObjectPtr<AActor>>& targetList, float& weight, int32 maxTargets, bool speculative)
{
	FHitResult hitResult;
	float maxWeight = 0.0f;
	float maxCone = FMathEx::ConeDegreesToDotProduct(80.0f);
	APlayGameMode* gameMode = APlayGameMode::Get(launchPlatform);
	ABaseVehicle* launchVehicle = Cast<ABaseVehicle>(launchPlatform);
	ABaseVehicle* existingVehicle = Cast<ABaseVehicle>(existingTarget);
	FVector fromDirection = launchPlatform->GetActorQuat().GetAxisX();
	FVector fromLocation = (launchVehicle != nullptr) ? launchVehicle->GetTargetBullsEye() + (launchVehicle->GetLaunchDirection() * 300.0f) : launchPlatform->GetActorLocation();

	targetList.Empty();

	if ((existingTarget != nullptr) &&
		(launchVehicle->IsAIVehicle() == false || existingVehicle == nullptr || existingVehicle->CanBeAttacked() == true))
	{
		// If we've been passed a vehicle, check to see if it's still in range.

		FVector targetLocation = GetTargetLocationFor(existingTarget, FVector::ZeroVector);

		weight = FMathEx::TargetWeight(fromLocation, fromDirection, targetLocation, 35.0f * 100.0f, (existingVehicle == nullptr) ? 10000.0f * 100.0f : 750.0f * 100.0f, maxCone, true);
		weight = gameMode->ScaleOffensivePickupWeight(launchVehicle->HasAIDriver(), weight, launchPickup, gameMode->VehicleShouldFightVehicle(launchVehicle, Cast<ABaseVehicle>(existingTarget)));

		if (weight >= 0.0f)
		{
			FCollisionQueryParams queryParams("TargetSelection", false, launchVehicle);

			queryParams.AddIgnoredActor(existingTarget);

			if (launchVehicle->GetWorld()->LineTraceSingleByChannel(hitResult, fromLocation, targetLocation, ABaseGameMode::ECC_LineOfSightTest, queryParams) == false)
			{
				targetList.Add(existingTarget);

				if (maxTargets == 1)
				{
					weight = 1.0f - weight;

					return true;
				}
			}
		}
	}

	while (true)
	{
		float minCorrection = 1.0f;

		existingTarget = nullptr;

		// Search for the best target vehicle for the launch platform's current condition.

		GRIP_GAME_MODE_LIST_FOR(GetVehicles(), vehicles, launchVehicle);

		for (ABaseVehicle* vehicle : vehicles)
		{
			if (targetList.Contains(Cast<AActor>(vehicle)) == false)
			{
				if ((vehicle != launchVehicle) &&
					(vehicle->IsVehicleDestroyed() == false) &&
					(speculative == false || vehicle->IsGoodForSmacking() == true) &&
					((launchVehicle != nullptr && launchVehicle->IsAIVehicle() == false) || vehicle->CanBeAttacked() == true) &&
					(launchPickup == nullptr || launchPickup->BotWillTargetHuman == false || vehicle->IsAIVehicle() == false))
				{
					FVector targetLocation = GetTargetLocationFor(vehicle, FVector::ZeroVector);

					float thisWeight = FMathEx::TargetWeight(fromLocation, fromDirection, targetLocation, 35.0f * 100.0f, 750.0f * 100.0f, maxCone, true);

					thisWeight = gameMode->ScaleOffensivePickupWeight(launchVehicle != nullptr && launchVehicle->HasAIDriver(), thisWeight, launchPickup, gameMode->VehicleShouldFightVehicle(launchVehicle, vehicle));

					if (thisWeight >= 0.0f &&
						minCorrection > thisWeight)
					{
						FCollisionQueryParams queryParams("TargetSelection", false, launchVehicle);

						queryParams.AddIgnoredActor(vehicle);

						if (launchVehicle->GetWorld()->LineTraceSingleByChannel(hitResult, fromLocation, targetLocation, ABaseGameMode::ECC_LineOfSightTest, queryParams) == false)
						{
							minCorrection = thisWeight;
							existingTarget = vehicle;
						}
					}
				}
			}
		}

		maxWeight = FMath::Max(maxWeight, 1.0f - minCorrection);

		if (existingTarget != nullptr)
		{
			targetList.Add(existingTarget);

			if (targetList.Num() >= maxTargets)
			{
				// No more targets to find, exit.

				break;
			}
		}
		else
		{
			// Found nothing more, exit.

			break;
		}
	}

	if (targetList.Num() > 0)
	{
		existingTarget = targetList[0].Get();
	}

	weight = maxWeight;

	return (existingTarget != nullptr);
}

/**
* Set the initial torque for the missile.
***********************************************************************************/

void AHomingMissile::SetInitialTorque(FRotator rotator, float roll, bool constrainUp)
{
	MissileMovement->SetInheritedRoll(roll);
	MissileMovement->SetInitialTorque(rotator);

	ConstrainUp = constrainUp;
}

/**
* Get the current smoke trail color.
***********************************************************************************/

FVector AHomingMissile::GetSmokeColor()
{
	float intensity = BrightnessNoise.GetValue();

	intensity *= 1.0f / 13.0f;
	intensity += 0.20f;
	intensity *= GameState->TransientGameState.MapBrightness;

	intensity = FMath::Max(intensity, 0.0f);

	return FVector(intensity, intensity, intensity);
}

/**
* Get the current smoke trail alpha.
***********************************************************************************/

float AHomingMissile::GetSmokeAlpha()
{
	// +-2 generally, 2.5 on occasion.
	float intensity = OpacityNoise.GetValue();

	// +-0.29 generally, 0.35 on occasion.
	intensity *= 1.0f / 7.0f;

	return 0.4f + intensity;
}

/**
* Get the current smoke trail size.
***********************************************************************************/

FVector AHomingMissile::GetSmokeSize()
{
	float intensity = SizeNoise.GetValue();

	intensity = 150.0f + (intensity * 15.0f);

	return FVector(intensity, intensity, intensity);
}

/**
* Get the target location for a particular target.
***********************************************************************************/

FVector AHomingMissile::GetTargetLocationFor(AActor* target, const FVector& targetOffset)
{
	FVector result = FVector::ZeroVector;

	if (target != nullptr)
	{
		ITargetableInterface* targetable = Cast<ITargetableInterface>(target);

		if (targetable != nullptr)
		{
			result = targetable->GetTargetBullsEye();
		}
		else
		{
			result = target->GetActorLocation();
		}

		if (targetOffset.IsNearlyZero() == false)
		{
			FVector offset = targetOffset;
			ABaseVehicle* vehicle = Cast<ABaseVehicle>(target);

			if (vehicle != nullptr)
			{
				if (vehicle->IsFlipped() == true)
				{
					offset.Z *= -1.0f;
				}
			}

			result += target->GetActorTransform().TransformVectorNoScale(offset);
		}
	}

	return result;
}

/**
* Activate the pickup.
***********************************************************************************/

void AHomingMissile::ActivatePickup(ABaseVehicle* launchVehicle, int32 pickupSlot, EPickupActivation activation, bool charged)
{
	Super::ActivatePickup(launchVehicle, pickupSlot, activation, charged);

	LaunchPlatform = launchVehicle;

	MissileHost = Cast<IMissileHostInterface>(LaunchPlatform.Get());

	LastLocation = LastSubLocation = GetActorLocation();

	if (Target == nullptr)
	{
		// If we have no target then make a false target to head towards.

		SetupFalseTarget();
	}

	FRotator missileRotation = GetActorRotation();

	// Get some basic physics properties from the vehicle.

	const FTransform& launcherTransform = LaunchVehicle->GetTransform();
	FVector launcherDirection = LaunchVehicle->GetLaunchDirection();
	FVector launcherLocation = LaunchVehicle->VehicleMesh->GetBoneLocation((launcherDirection.Z >= 0.0f) ? "MissileBay_T_Eject" : "MissileBay_B_Eject");
	FVector launcherVelocity = MissileHost->GetHostVelocity();

	// Compute a sideways velocity to add to the missile.

	FVector sideDirection = launcherTransform.InverseTransformPosition(launcherLocation);

	sideDirection.X = sideDirection.Z = 0.0f;
	sideDirection.Normalize();

	sideDirection = launcherTransform.TransformVectorNoScale(sideDirection);

	// Get the vehicle's velocity, and compute an upwards velocity, compensating
	// if we change gravity.

	// Do some ejection impulse scaling to work consistently with a potentially
	// varying gravity setting as the game develops.

	float gravityScale = FMath::Abs(GetWorld()->GetGravityZ()) / 1500.0f;
	FVector verticalVelocity = launcherDirection * FVector(1000.0f, 1000.0f, 1000.0f * gravityScale);

	// And here we reduce the velocity on the Z axis if we're driving on the ceiling, as we
	// don't want to throw the gravity-assisted missile straight into the floor.

	float verticalVelocityRatio = (launcherDirection.Z < 0.0f) ? 1.0f + launcherDirection.Z : 1.0f;

	verticalVelocity = FMath::Lerp(verticalVelocity * FVector(1.0f, 1.0f, 0.333f), verticalVelocity, verticalVelocityRatio);

	// Apply an impulse in the direction of the missile port and upwards from the vehicle,
	// inheriting any velocity derived from angular momentum from the vehicle at the missile
	// port. This impulse is in addition to the inherited velocity of the launch vehicle as
	// a whole. So it'll be thrown to the side a little, but mostly up.

	FVector pointVelocity = LaunchVehicle->VehicleMesh->GetPhysicsLinearVelocityAtPoint(launcherLocation);
	FVector ejectionImpulse = (pointVelocity - launcherVelocity) + verticalVelocity + (333.0f * sideDirection);

	// Determine actual launch parameters here. If in a tight area then throw the missiles like
	// javelins. If in an open area, give them a nice arcing moving up and away from the car.

	float impulseScale = 1.0f;

	bool constrainSide = false;
	bool constrainUp = false;
	bool constrainImpulse = false;

	if (LaunchVehicle->IsAirborne() == false &&
		FMath::Abs(LaunchVehicle->GetSurfaceDirection().Z) < 0.75f)
	{
		// If we're riding a wall or something, then certainly constrain sideways movement as walls
		// generally means confined tunnels.

		constrainSide = true;
	}

	if (GameState->IsGameModeRace() == true)
	{
		// Always constrain sideways movement in races - we need precision, not art.

		constrainSide = true;
	}

	// Use the current racing spline to determine the environment around the missile.

	float retainPitch = 0.0f;
	FRouteFollower& routeFollower = LaunchVehicle->GetAI().RouteFollower;

	if (routeFollower.IsValid() == true)
	{
		float timeAhead = 2.0f;
		float clearanceHeightMeters = 50.0f;
		int32 splineDirection = LaunchVehicle->GetPursuitSplineDirection();
		float clearanceAhead = FMath::Max(FMathEx::MetersToCentimeters(150.0f), launcherVelocity.Size() * timeAhead);
		FVector up = routeFollower.ThisSpline->WorldSpaceToSplineSpace(LaunchVehicle->GetLaunchDirection(), routeFollower.ThisDistance, false);
		float overDistance = clearanceAhead;
		float clearanceUp = routeFollower.GetClearanceOverDistance(routeFollower.ThisDistance, overDistance, splineDirection, launcherLocation, up, 45.0f);

		// NOTE: ClearanceUp will sometimes be zero if the GetClearanceOverDistance function thinks
		// the launcherLocation is outside of the spline environment space, even if it really isn't.

		// If there's not much height clearance then constrain vertical movement.

		if (clearanceUp < FMathEx::MetersToCentimeters(clearanceHeightMeters))
		{
			constrainUp = true;
		}

		// If it's really tight, constrain the impulse too.

		if (clearanceUp < FMathEx::MetersToCentimeters(20.0f))
		{
			constrainImpulse = true;
		}

		if (constrainSide == false)
		{
			overDistance = clearanceAhead;
			float clearanceSide = routeFollower.GetClearanceOverDistance(routeFollower.ThisDistance, overDistance, splineDirection, launcherLocation, up, 120.0f);

			// If there's not much clearance in general in the upper hemisphere then
			// constrain sideways movement too.

			if (clearanceSide < FMathEx::MetersToCentimeters(clearanceHeightMeters))
			{
				constrainSide = true;
			}
		}

		float distanceAhead = timeAhead * FMathEx::MetersToCentimeters(LaunchVehicle->GetSpeedMPS());
		FRotator splineDegrees = routeFollower.GetCurvatureOverDistance(routeFollower.ThisDistance, distanceAhead, splineDirection, launcherTransform.GetRotation(), false);

		// Convert to degrees per second.

		splineDegrees *= 1.0f / timeAhead;

		if (LaunchVehicle->IsFlipped() == true)
		{
			splineDegrees.Pitch *= -1.0f;
		}

		// Pitch in splineDegrees will be zero for perfectly flat compared to the vehicle.
		// Negative for falling away in front of the vehicle.
		// Positive for climbing up in front of the vehicle.

		UE_LOG(GripLogMissile, Log, TEXT("Spline pitch %0.02f"), splineDegrees.Pitch);

		// So if we're about to enter a hill climb and we need to push the missile up
		// and away from the ground to avoid hitting the hill, then do that here.

		if (splineDegrees.Pitch > 4.0f)
		{
			constrainImpulse = false;

			retainPitch = FMathEx::GetRatio(splineDegrees.Pitch, 4.0f, 25.0f);
			impulseScale += retainPitch;

			UE_LOG(GripLogMissile, Log, TEXT("Corrected spline pitch %0.02f"), retainPitch);
		}
	}

	IgnitionTime = (constrainUp == true) ? 0.3f : 0.6f;

	float yaw = 0.0f;
	float pitch = 0.0f;
	float speed = LaunchVehicle->GetSpeedKPH();

	if (GRIP_OBJECT_VALID(Target) == true)
	{
		// Calculate yaw and pitch factors that will point to the target at the ignition time.

		FVector surfaceNormal = LaunchVehicle->GuessSurfaceNormal();
		FVector missileToTarget = Target->GetActorLocation() - GetActorLocation();

		missileToTarget.Normalize();

		if (surfaceNormal.IsZero() == false)
		{
			// Never target below the horizon line of the current driving surface.

			FQuat surfaceQuat = surfaceNormal.ToOrientationQuat();
			FVector localMissileToTarget = surfaceQuat.UnrotateVector(missileToTarget);

			UE_LOG(GripLogMissile, Log, TEXT("Local missile target offset %0.01f"), localMissileToTarget.X);

			if (localMissileToTarget.X < 0.0f)
			{
				localMissileToTarget.X = 0.0f;

				missileToTarget = surfaceQuat.RotateVector(localMissileToTarget);
				missileToTarget.Normalize();
			}
		}

		// Angles in -180 to +180.

		FRotator missileToTargetRotation = launcherTransform.InverseTransformVectorNoScale(missileToTarget).Rotation();

		yaw = missileToTargetRotation.Yaw;
		pitch = missileToTargetRotation.Pitch;

		UE_LOG(GripLogMissile, Log, TEXT("Initial yaw %0.02f, pitch %0.02f"), yaw, pitch);

		yaw = FMath::Clamp(yaw, -45.0f, 45.0f);

		float maxPitch = (constrainUp == true) ? 20.0f : 60.0f;

		pitch = FMath::Clamp(pitch, -maxPitch, maxPitch);

		yaw *= 0.666f;
		pitch *= 0.666f;

		// Ensure at least a minimum degrees away from the vehicle in pitch on launch,
		// taking into account whether we're constrained in height.

		float minPitch = (constrainUp == true) ? 1.0f : 5.0f;

		if (FMath::Abs(pitch) < minPitch)
		{
			pitch = minPitch * FMathEx::UnitSign(pitch);
		}

		UE_LOG(GripLogMissile, Log, TEXT("Launching with yaw %0.02f, pitch %0.02f"), yaw, pitch);

		float speedScale = FMathEx::GetInverseRatio(speed, 0.0f, 400.0f);

		// Reduce the yaw the faster we're going.

		yaw *= (speedScale * 0.8f) + 0.2f;

		// Reduce the pitch the faster we're going, even more so than yaw, but
		// only if we're not about to head hard uphill.

		pitch = FMath::Lerp(pitch * ((speedScale * 0.5f) + 0.5f), pitch, retainPitch);

		UE_LOG(GripLogMissile, Log, TEXT("Diminished to yaw %0.02f, pitch %0.02f"), yaw, pitch);

		float angularPitch = LaunchVehicle->GetSustainedAngularPitch();

		if (FMathEx::UnitSign(angularPitch) == FMathEx::UnitSign(pitch))
		{
			// If the launch vehicle is traveling around the track with positive pitch
			// (pressing hard on the springs) then we should try to pitch the missile
			// correspondingly upwards to avoid the track curvature.

			angularPitch = FMath::Clamp(angularPitch, -30.0f, 30.0f);

			pitch = FMath::Max(FMath::Abs(pitch), FMath::Abs(angularPitch) * 0.4f) * FMathEx::UnitSign(pitch);
		}

		UE_LOG(GripLogMissile, Log, TEXT("Track avoiding to %0.02f, pitch %0.02f"), yaw, pitch);

		// Set the yaw and pitch to the correct coordinate space and convert into degrees per second.

		missileToTarget = launcherTransform.TransformVectorNoScale(FRotator(pitch, yaw, 0.0f).Vector());

		missileRotation.Roll = 0.0f;

		missileToTargetRotation = missileRotation.UnrotateVector(missileToTarget).Rotation();

		yaw = missileToTargetRotation.Yaw / IgnitionTime;
		pitch = missileToTargetRotation.Pitch / IgnitionTime;
	}
	else
	{
		// We have no target to aim for.

		// The more speed the LaunchVehicle has, the less variance in pitch there is.
		// The reason being, it looks pretty crap otherwise.

		float ejectScale = 1.0f - (FMathEx::GetRatio(speed, 0.0f, 400.0f) * 0.75f);

		yaw = 0.0f;
		pitch = FMath::RandRange(0.3f, 0.3f + (0.3f * ejectScale));

		if (constrainUp == true)
		{
			pitch *= 0.1f;
		}

		pitch *= AngleVariance * 100.0f;

		missileRotation.Roll = 0.0f;
	}

	if (constrainImpulse == true)
	{
		ejectionImpulse *= 0.666f;
	}
	else
	{
		ejectionImpulse *= impulseScale;
	}

	// Finally, set all of this data into place.

	SetInitialImpulse(ejectionImpulse);
	SetInitialTorque(FRotator(pitch, yaw, 0.0f), missileRotation.Roll, constrainUp);

	SetLauncherVelocity(launcherVelocity);

	MissileMesh->MoveIgnoreActors.Emplace(LaunchPlatform.Get());

	if (LaunchVehicle->IsPracticallyGrounded() == true)
	{
		MissileMovement->TerrainDirection = LaunchVehicle->GetSurfaceDirection();
	}

	MissileMovement->SetLoseLockOnRear(LoseLockOnRear);

	GRIP_ADD_TO_GAME_MODE_LIST(Missiles);

	UGameplayStatics::SpawnSoundAttached(MissileHost->UseHumanPlayerAudio() ? EjectSound : EjectSoundNonPlayer, MissileMesh, NAME_None);
}

/**
* Attach to a launch platform, like a defense turret.
***********************************************************************************/

void AHomingMissile::AttachLaunchPlatform(AActor* launchPlatform)
{
	PickupSlot = 0;
	LaunchVehicle = nullptr;
	LaunchPlatform = launchPlatform;
	MissileHost = Cast<IMissileHostInterface>(LaunchPlatform.Get());
}

/**
* Manually launch the missile, normally from a defense turret.
***********************************************************************************/

void AHomingMissile::Launch(const FVector& location, const FVector& velocity)
{
	if (Target == nullptr)
	{
		// If we have no target then make a false target to head towards.

		SetupFalseTarget();
	}

	RootComponent->SetWorldLocation(location);

	RandomDrift.X = FMath::FRandRange(-20.0f, 20.0f);
	RandomDrift.Y = FMath::FRandRange(0.0f, 10.0f);
	IgnitionTime = 0.0f;

	MissileMesh->MoveIgnoreActors.Emplace(LaunchPlatform.Get());

	MissileMovement->SetLoseLockOnRear(LoseLockOnRear);

	SetInitialImpulse(FVector::ZeroVector);
	SetInitialTorque(FRotator::ZeroRotator, 0.0f, false);

	SetLauncherVelocity(velocity);

	GRIP_ADD_TO_GAME_MODE_LIST(Missiles);

	UGameplayStatics::SpawnSoundAttached(MissileHost->UseHumanPlayerAudio() ? EjectSound : EjectSoundNonPlayer, MissileMesh, NAME_None);
}

/**
* Is the missile in terminal range of the target?
***********************************************************************************/

bool AHomingMissile::IsInTerminalRange(AActor* target, float distance, float seconds) const
{
	FVector targetLocation = GetTargetLocationFor(target, HomingTargetOffset);

	if (distance < 0.0f)
	{
		distance = (targetLocation - GetActorLocation()).Size();
	}

	float closingSpeed = (MissileMovement->Velocity - target->GetVelocity()).Size();

	// Assume a minimum closing velocity of 10 meters per second.

	closingSpeed = FMath::Max(closingSpeed, FMathEx::MetersToCentimeters(10.0f));

	return (distance / closingSpeed < seconds);
}

/**
* Record that this missile is imminently incoming on its target.
***********************************************************************************/

bool AHomingMissile::RecordIncoming()
{
	if (TargetWithinReach == false)
	{
		TargetWithinReach = true;

		if (MissileHost->GetVehicleIndex() >= 0 &&
			LaunchVehicle != nullptr)
		{
			ABaseVehicle* vehicle = Cast<ABaseVehicle>(Target);

			FGameEvent gameEvent;

			gameEvent.LaunchVehicleIndex = MissileHost->GetVehicleIndex();
			gameEvent.TargetVehicleIndex = (vehicle != nullptr) ? vehicle->VehicleIndex : -1;
			gameEvent.PickupUsed = EPickupType::HomingMissile;
			gameEvent.PickupUsedWasCharged = IsCharged();
			gameEvent.EventType = EGameEventType::Incoming;

			PlayGameMode->AddGameEvent(gameEvent);
		}

		return true;
	}

	return false;
}

/**
* Called when the missile is moved at all.
***********************************************************************************/

bool AHomingMissile::OnMove()
{
	if (Target != nullptr &&
		CurrentState == EState::Flight)
	{
		// See if we're within range of the target, and explode the missile if so.

		FVector missileLocation = GetActorLocation();
		FVector targetLocation = GetTargetLocationFor(Target, HomingTargetOffset);
		float pointOnLine = 0.0f;
		float distanceMissile = FMathEx::PointToLineDistance(LastSubLocation, missileLocation - LastSubLocation, targetLocation, pointOnLine);
		float distanceVehicle = distanceMissile;
		ABaseVehicle* targetVehicle = Cast<ABaseVehicle>(Target);

		LastSubLocation = missileLocation;

		if (targetVehicle != nullptr)
		{
			distanceVehicle = FMathEx::PointToLineDistance(targetVehicle->GetAI().PrevLocation, targetVehicle->GetAI().LastLocation - targetVehicle->GetAI().PrevLocation, missileLocation, pointOnLine);
		}

		float distance = FMath::Min(distanceMissile, distanceVehicle);
		FVector missileDirection = GetTransform().GetUnitAxis(EAxis::X);

		if (InRangeOfTarget == true ||
			distance < ProximityFuse)
		{
			InRangeOfTarget = true;

			// Don't miss the target if it's going real slow as it'll just look obvious.
			// This also helps to keep players on the move and inject some urgency.

			if (MissTarget == true &&
				Target != nullptr &&
				Target->GetVelocity().Size() < FMathEx::MetersToCentimeters(25.0f))
			{
				MissTarget = false;
			}

			// Otherwise aim for the missile to get a bit ahead of the target before exploding
			// as this will look more impressive to the player being hit. If we don't do this,
			// they often don't see any of the visual effects associated with the explosion as
			// they speed forward away from it.

			bool shieldIrrelevant = false;
			bool shielded = (targetVehicle != nullptr && targetVehicle->IsShielded(missileLocation) == true) && (shieldIrrelevant == false);
			float targetVelocity = Target->GetVelocity().Size();
			FVector targetDirection = (targetVehicle != nullptr) ? targetVehicle->GetVelocityOrFacingDirection() : Target->GetTransform().GetUnitAxis(EAxis::X);
			FVector targetToMissile = missileLocation - targetLocation; targetToMissile.Normalize();

			if ((shielded == true) || // The target is shielded so we can't affect them
				(distance > ProximityFuse && FVector::DotProduct(targetToMissile, targetDirection) > -0.666f && FVector::DotProduct(missileDirection, targetDirection) < 0.666f) || // We've just gone out of the blast radius (probably a side attack gone wrong)
				(FVector::DotProduct(missileDirection, targetDirection) < 0.5f) || // The missile isn't coming up from behind, rather the side or front
				(targetVelocity <= 100.0f && FVector::DotProduct(targetToMissile, targetDirection) > 0.0f) || // The missile is coming from behind and is now visibly ahead of the target
				(targetVelocity > 100.0f && distance > ProximityFuse * 0.5f && FVector::DotProduct(targetToMissile, targetDirection) > 0.0f)) // The missile is coming from behind and is now visibly ahead of the target
			{
				Explode(Target, nullptr);

				return true;
			}
		}

		if (InRangeOfTarget == false &&
			distance < ProximityFuse * 2.0f)
		{
			FVector missileToTarget = targetLocation - missileLocation;

			if (FVector::DotProduct(missileDirection, missileToTarget) < 0.0f)
			{
				// If we've gone past the target and we're fairly close to it but not registered in range
				// for whatever reason then just explode as something went wrong.

				Explode(Target, nullptr);

				return true;
			}
		}
	}

	return false;
}

/**
* Get the time in seconds before impacting target (assuming straight terminal phase
* and constant speed).
***********************************************************************************/

float AHomingMissile::GetTimeToTarget() const
{
	return MissileMovement->GetTimeToTarget();
}

/**
* Is the missile likely to hit the target?
***********************************************************************************/

bool AHomingMissile::IsLikelyToHitTarget() const
{
	return InRangeOfTarget == true || MissileMovement->IsLikelyToHitTarget() == true;
}

#pragma region BotCombatTraining

/**
* Is this launch vehicle in a good condition to launch a missile?
***********************************************************************************/

bool AHomingMissile::GoodLaunchCondition(ABaseVehicle* launchVehicle)
{
	if (launchVehicle->IsPracticallyGrounded() == false ||
		launchVehicle->GroundedTime(2.0f) < 0.8f ||
		launchVehicle->GetAI().IsDrivingCasually(true) == false)
	{
		return false;
	}

	UGlobalGameState* gameState = UGlobalGameState::GetGlobalGameState(launchVehicle);

	if (gameState->IsGameModeRace() == true)
	{
		UPursuitSplineComponent* spline = launchVehicle->GetAI().RouteFollower.ThisSpline.Get();

		if (spline != nullptr)
		{
			FVector vehicleDirection = launchVehicle->GetFacingDirection();
			FVector splineDirection = spline->GetDirectionAtDistanceAlongSpline(launchVehicle->GetAI().RouteFollower.ThisDistance, ESplineCoordinateSpace::World);

			// Don't launch unless the vehicle is pointing in the right direction.

			if (FVector::DotProduct(splineDirection, vehicleDirection) > 0.95f)
			{
				FVector angularVelocity = launchVehicle->GetAngularVelocity();
				float yawRate = FMath::Abs(angularVelocity.Z);
				float rollRate = FMath::Abs(angularVelocity.X);
				float pitchRate = FMath::Abs(angularVelocity.Y);

				// Don't launch if the vehicle is tumbling around.

				if (rollRate < 30.0f &&
					yawRate < 30.0f &&
					pitchRate < 30.0f)
				{
					float timeAhead = 2.0f;
					FRotator rotation = launchVehicle->GetActorRotation();
					FQuat quaternion = rotation.Quaternion();
					int32 direction = launchVehicle->GetPursuitSplineDirection();
					float distanceAhead = timeAhead * FMathEx::KilometersPerHourToCentimetersPerSecond(launchVehicle->GetSpeedKPH() + 200.0f);
					FRotator splineDegrees = launchVehicle->GetAI().RouteFollower.GetCurvatureOverDistance(launchVehicle->GetAI().RouteFollower.ThisDistance, distanceAhead, direction, quaternion, true);
					float tunnelDiameter = launchVehicle->GetAI().RouteFollower.GetTunnelDiameterOverDistance(launchVehicle->GetAI().RouteFollower.ThisDistance, FMath::Max(launchVehicle->GetSpeedMPS() * timeAhead, 10.0f) * 100.0f, direction, false);

					if (tunnelDiameter > 50.0f * 100.0f)
					{
						// Convert to degrees per second.

						splineDegrees *= 1.0f / timeAhead;

						// Don't launch if the immediate route is too twisty.

						if (FMath::Abs(splineDegrees.Yaw) < 15.0f &&
							FMath::Abs(splineDegrees.Pitch) < 15.0f &&
							FMath::Abs(splineDegrees.Roll) < 15.0f)
						{
							FVector location = launchVehicle->GetActorLocation();
							FVector up = launchVehicle->GetAI().RouteFollower.ThisSpline->WorldSpaceToSplineSpace(launchVehicle->GetLaunchDirection(), launchVehicle->GetAI().RouteFollower.ThisDistance, false);
							float overDistance = distanceAhead;
							float clearanceUp = launchVehicle->GetAI().RouteFollower.GetClearanceOverDistance(launchVehicle->GetAI().RouteFollower.ThisDistance, overDistance, direction, location, up, 45.0f);

							// Don't launch if less than 12 meters height clearance over the vehicle.

							if (clearanceUp > 12.0f * 100.0f)
							{
								return true;
							}
						}
					}
				}
			}

			return false;
		}
	}

	return true;
}

/**
* Get a weighting, between 0 and 1, of how ideally a pickup can be used, optionally
* against a particular vehicle. 0 means cannot be used effectively at all, 1 means a
* very high chance of pickup efficacy.
***********************************************************************************/

float AHomingMissile::EfficacyWeighting(ABaseVehicle* launchVehicle, FPlayerPickupSlot* launchPickup, ABaseVehicle* againstVehicle)
{
	if (GoodLaunchCondition(launchVehicle) == true)
	{
		float weight = 0.0f;
		AActor* target = againstVehicle;
		TArray<TWeakObjectPtr<AActor>> targetList;

		if (SelectTarget(launchVehicle, launchPickup, target, targetList, weight, 1, true) == true)
		{
			if (launchPickup != nullptr &&
				launchPickup->IsCharged() == true)
			{
				float maxWeight = (targetList.Num() < 2) ? 0.5f : 1.0f;

				return ((target == againstVehicle || againstVehicle == nullptr)) ? ((weight >= 0.5f) ? maxWeight : weight) : 0.0f;
			}
			else
			{
				return ((target == againstVehicle || againstVehicle == nullptr)) ? ((weight >= 0.5f) ? 1.0f : weight) : 0.0f;
			}
		}
	}

	return 0.0f;
}

#pragma endregion BotCombatTraining

#pragma endregion PickupMissile
