/**
*
* Raptor Gatling gun implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Gatling gun pickup type, one of the pickups used by vehicles in the game.
*
***********************************************************************************/

#include "pickups/gatlinggun.h"
#include "vehicle/flippablevehicle.h"
#include "ui/hudwidget.h"
#include "gamemodes/basegamemode.h"

/**
* Construct a UGunHostInterface.
***********************************************************************************/

UGunHostInterface::UGunHostInterface(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{ }

/**
* Construct a gun.
***********************************************************************************/

AGatlingGun::AGatlingGun()
{
	PickupType = EPickupType::GatlingGun;

	PrimaryActorTick.bCanEverTick = true;

	BarrelSpinAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("BarrelSpinAudio"));

	SetRootComponent(BarrelSpinAudio);
}

#pragma region PickupGun

/**
* Do some shutdown when the actor is being destroyed.
***********************************************************************************/

void AGatlingGun::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	GRIP_DETACH(BarrelSpinAudio);

	Super::EndPlay(endPlayReason);
}

/**
* Do the regular update tick.
***********************************************************************************/

void AGatlingGun::Tick(float deltaSeconds)
{
	Super::Tick(deltaSeconds);

	if (Duration >= 0.0f &&
		GRIP_POINTER_VALID(LaunchPlatform) == true)
	{
		Timer += deltaSeconds;

		if (Duration != 0.0f &&
			Timer > Duration + WindUpTime + WindDownTime)
		{
			// Handle the destruction of the gun when it's come to an end.

			if (BarrelSpinAudio != nullptr)
			{
				BarrelSpinAudio->Stop();
			}

			if (LaunchVehicle != nullptr)
			{
				if (NumPoints > 0)
				{
					if (LaunchVehicle->IsAccountingClosed() == false)
					{
						LaunchVehicle->ShowStatusMessage(FStatusMessage(PlayGameMode->GetXPMessage(PickupType, NumPoints)), true, false);
					}
				}

				LaunchVehicle->ReleasePickupSlot(PickupSlot);

				DestroyPickup();
			}
		}

		if (Timer < WindUpTime)
		{
			// Handle winding up the gun.

			if (BarrelSpinAudio != nullptr)
			{
				BarrelSpinAudio->SetPitchMultiplier(0.5f + (Timer / WindUpTime) * 0.5f);
				BarrelSpinAudio->SetVolumeMultiplier(0.5f + (Timer / WindUpTime) * 0.5f);
			}
		}

		if (Timer >= FiringDelay)
		{
			// Handle the main firing of the gun once wound up.

			if (Timer > WindUpTime + Duration)
			{
				// Handle the winding down of the gun.

				if (BarrelSpinAudio != nullptr)
				{
					BarrelSpinAudio->SetPitchMultiplier(1.0f - ((Timer - (WindUpTime + Duration)) / WindDownTime) * 0.5f);
					BarrelSpinAudio->SetVolumeMultiplier(1.0f - ((Timer - (WindUpTime + Duration)) / WindDownTime));
				}
			}
			else if (Timer >= WindUpTime)
			{
				// Set nominal audio parameters if in the core firing duration.

				if (BarrelSpinAudio != nullptr)
				{
					BarrelSpinAudio->SetPitchMultiplier(1.0f);
					BarrelSpinAudio->SetVolumeMultiplier(1.0f);
				}
			}

			if (Duration == 0.0f ||
				Timer < (WindUpTime + Duration + WindDownTime) - FiringDelay)
			{
				// Determine if we need to fire a round this frame.

				if (HaltRounds == false)
				{
					if (BarrelSpinAudio != nullptr)
					{
						RoundTimer += deltaSeconds * BarrelSpinAudio->PitchMultiplier;
					}
					else
					{
						RoundTimer += deltaSeconds;
					}

					float invFireRate = 1.0f / FireRate;

					while (RoundTimer > invFireRate)
					{
						// Yes, we need to fire a round.

						int32 launchVehicleIndex = -1;

						if (LaunchVehicle != nullptr)
						{
							launchVehicleIndex = LaunchVehicle->VehicleIndex;
						}

						RoundTimer -= invFireRate;

						RoundLocation ^= 1;

						float weight = 0.0f;
						float distance = 0.0f;
						float forwards = 0.995f;
						FQuat orientation = GunHost->GetGunOrientation();
						FVector up = orientation.GetAxisZ();
						FVector side = orientation.GetAxisY();
						FVector surfaceDirection = up * -1.0f;
						FVector direction = GunHost->GetGunRoundDirection(orientation.GetAxisX());
						FVector location = GunHost->EjectGunRound((AlternateBarrels == false) ? 0 : RoundLocation, IsCharged());
						AActor* target = Target.Get();
						AActor* ignoreTarget = nullptr;

						if (LaunchVehicle != nullptr)
						{
							target = SelectTarget(LaunchPlatform.Get(), nullptr, AutoAiming, weight, false);
						}

						if (target != nullptr &&
							FMath::FRand() > HitRatio)
						{
							ignoreTarget = target;
						}

						if (GRIP_OBJECT_VALID(target) == true)
						{
							// Point the gun directly towards the target.

							ITargetableInterface* targetable = Cast<ITargetableInterface>(target);

							if (targetable != nullptr)
							{
								direction = targetable->GetTargetBullsEye() - location;
							}
							else
							{
								direction = target->GetActorLocation() - location;
							}

							distance = FMath::Max(100.0f, direction.Size());

							direction.Normalize();

							forwards = 1.0f;

							// Manage the sideways spread based on whether we have a target in our sights or not.

							// Add sideways offset when trying to hit a target, the close we are to the target, the
							// more sideways offset we add. The further away, the more it tightens up.

							side *= FMath::FRandRange(-1.0f, 1.0f) * 0.1f / (distance / (20.0f * 100.0f));

							if (ignoreTarget != nullptr)
							{
								ABaseVehicle* vehicle = Cast<ABaseVehicle>(ignoreTarget);

								if (vehicle != nullptr)
								{
									// We've been told to explicitly miss this target vehicle, so let's aim around it
									// causing a lot of excitement without actually hitting it.

									side = vehicle->GetSideDirection() * FMath::FRandRange(2.0f * 100.0f, 5.0f * 100.0f) * ((FMath::Rand() & 1) ? 1.0f : -1.0f);
									side += vehicle->GetVelocityOrFacingDirection() * FMath::Max(FMathEx::MetersToCentimeters(vehicle->GetSpeedMPS()) * 0.333f, 3.0f * 300.0f);

									direction *= distance;
								}
							}
						}
						else
						{
							side *= (FMath::FRand() - 0.5f) * 0.2f;
						}

						// Vary the vertical offset just a tiny bit.

						up *= FMath::FRandRange(-0.25f, 0.75f) * 0.01f;

						if (ignoreTarget != nullptr)
						{
							// We've been told to explicitly miss this target vehicle, we're already aiming
							// around it, so let's not mess with that by upsetting the up vector.

							target = nullptr;
							forwards = 1.0f;
							up = FVector::ZeroVector;
						}

						// Form a composite direction from the forwards, side and up vectors.

						// surfaceDirection will push the round down towards the ground just a little if we're
						// not targeting something.

						direction = FMath::Lerp(surfaceDirection, direction, forwards) + side + up;

						direction.Normalize();

						float time = 0.0f;
						FVector end = location + (direction * 100.0f * 1000.0f);

						// Let's see if we hit something.

						if (GetCollision(GetWorld(), location, end, time, ignoreTarget) == true)
						{
							USoundCue* hitSound = nullptr;
							TArray<FVector> hitLocations;
							TArray<UParticleSystem*> hitParticleSystems;
							EGameSurface surface = EGameSurface::Default;
							UPrimitiveComponent* hitComponent = HitResult.GetComponent();
							FVector impactPoint = FMath::Lerp(location, end, time);
							FRotator impactRotation = HitResult.ImpactNormal.Rotation();

							LastImpact = impactPoint;

							if (HitResult.GetActor()->IsA<ABaseVehicle>() == true)
							{
								// Handle the hitting of a vehicle with a round.

								NumRoundsHitVehicle++;

								ABaseVehicle* vehicle = Cast<ABaseVehicle>(HitResult.GetActor());

								if (HitResult.GetActor() == target)
								{
									vehicle->ResetAttackTimer();
								}

								const FTransform& vehicleTransform = vehicle->VehicleMesh->GetComponentTransform();

								// Ask the vehicle to process a bullet round striking it.

								if (vehicle->BulletRound(RoundForce, HitPoints * ((LaunchVehicle != nullptr) ? LaunchVehicle->GetDamageScale() : 1.0f), launchVehicleIndex, impactPoint, location, IsCharged(), SpinSide) == true)
								{
									// We struck the vehicle.

									if (LaunchVehicle != nullptr &&
										LaunchVehicle->IsAccountingClosed() == false)
									{
										int32 numPoints = 5;

										if (LaunchVehicle->AddPoints(numPoints, true, vehicle, impactPoint) == true)
										{
											NumPoints += numPoints;

											if (HitVehicles.Find(vehicle) == INDEX_NONE)
											{
												HitVehicles.Emplace(vehicle);
											}
										}
									}

									surface = EGameSurface::Vehicle;
								}

#pragma region PickupShield

								else
								{
									// We can assume here that we struck the vehicle's shield.

									surface = EGameSurface::Shield;
									hitComponent = vehicle->VehicleMesh;

									float standardOffset = -300.0f;
									FVector additionalOffset = vehicle->VehicleShield->RearOffset;

									if (vehicleTransform.InverseTransformPosition(impactPoint).X > 0.0f)
									{
										standardOffset *= -1.0f;
										additionalOffset = vehicle->VehicleShield->FrontOffset;
									}

									if (vehicle->VehicleShield->HitEffect != nullptr)
									{
										hitParticleSystems.Emplace(vehicle->VehicleShield->HitEffect);
										hitLocations.Emplace(additionalOffset);
									}

									if (vehicle->VehicleShield->HitPointEffect != nullptr)
									{
										FVector pointOffset = FVector(standardOffset, FMath::FRandRange(-150.0f, 150.0f), FMath::FRandRange(-50.0f, 50.0f));

										hitParticleSystems.Emplace(vehicle->VehicleShield->HitPointEffect);
										hitLocations.Emplace(additionalOffset + pointOffset);
									}

									hitSound = vehicle->VehicleShield->HitSound;
								}

#pragma endregion PickupShield

								// Calculate a reflection vector between the incoming round and the vehicle it's
								// hit to determine how to orient any visual hit effects.

								FVector strikeNormal = end - location; strikeNormal.Normalize();
								FVector reflectNormal = FMath::GetReflectionVector(strikeNormal, HitResult.ImpactNormal); reflectNormal.Normalize();

								impactRotation = reflectNormal.Rotation();
							}
							else if (HitResult.GetComponent()->IsA<UMeshComponent>() == true)
							{
								// Handle the hitting of a mesh component with a round.

								// If this is a mesh component and it's simulating physics then apply an
								// impulse to it to push it around.

								UMeshComponent* mesh = Cast<UMeshComponent>(HitResult.GetComponent());

								if (mesh->IsSimulatingPhysics() == true)
								{
									mesh->AddImpulseAtLocation(direction * 100.0f * 10000.0f * RoundForce, impactPoint);
								}
							}

							if (surface == EGameSurface::Default)
							{
								surface = (EGameSurface)UGameplayStatics::GetSurfaceType(HitResult);
							}

							FVector color = GameState->TransientGameState.MapSurfaceColor * GameState->TransientGameState.MapLightingColor * 0.75f;

							if (LaunchVehicle != nullptr)
							{
								color = LaunchVehicle->GetDustColor(true);
							}

							// Process the main audio / visual effects of the round striking a surface.

							BulletHitAnimation(hitComponent, hitParticleSystems, hitLocations, hitSound, impactPoint, impactRotation, surface, color, IsCharged());
						}

						NumRoundsFired++;
					}
				}
			}
		}
	}
}

/**
* Activate the pickup.
***********************************************************************************/

void AGatlingGun::ActivatePickup(ABaseVehicle* launchVehicle, int32 pickupSlot, EPickupActivation activation, bool charged)
{
	Super::ActivatePickup(launchVehicle, pickupSlot, activation, charged);

	LaunchPlatform = launchVehicle;

	GRIP_ATTACH(BarrelSpinAudio, launchVehicle->GetRootComponent(), "RootDummy");

	GunHost = Cast<IGunHostInterface>(LaunchPlatform.Get());

	QueryParams = FCollisionQueryParams(TEXT("Bullet"), true, LaunchPlatform.Get());
	QueryParams.bReturnPhysicalMaterial = true;

	if (BarrelSpinAudio != nullptr)
	{
		BarrelSpinAudio->SetSound(GunHost->UseHumanPlayerAudio() ? BarrelSpinSound : BarrelSpinSoundNonPlayer);

		BarrelSpinAudio->SetPitchMultiplier(1.0f);
		BarrelSpinAudio->SetVolumeMultiplier(0.0f);

		BarrelSpinAudio->Play();
	}

	SpinSide = (FMath::RandBool() == true) ? +1 : -1;

	// Just grab the current best target for the game event created after this
	// pickup is activated.

	float weight = 0.0f;

	Target = SelectTarget(launchVehicle, nullptr, AutoAiming, weight, false);
}

/**
* Attach to a launch platform, like a defense turret.
***********************************************************************************/

void AGatlingGun::AttachLaunchPlatform(AActor* launchPlatform)
{
	Duration = -1.0f;
	PickupSlot = 0;
	LaunchVehicle = nullptr;
	LaunchPlatform = launchPlatform;
	GunHost = Cast<IGunHostInterface>(launchPlatform);

	QueryParams = FCollisionQueryParams(TEXT("Bullet"), true, LaunchPlatform.Get());
	QueryParams.bReturnPhysicalMaterial = true;

	GRIP_ATTACH(BarrelSpinAudio, launchPlatform->GetRootComponent(), NAME_None);
}

/**
* Begin manual firing of the gun, normally from a defense turret.
***********************************************************************************/

void AGatlingGun::BeginFiring(float hitRatio)
{
	if (BarrelSpinAudio != nullptr)
	{
		BarrelSpinAudio->SetSound(GunHost->UseHumanPlayerAudio() ? BarrelSpinSound : BarrelSpinSoundNonPlayer);

		BarrelSpinAudio->SetPitchMultiplier(1.0f);
		BarrelSpinAudio->SetVolumeMultiplier(0.0f);

		BarrelSpinAudio->Play();
	}

	Timer = 0.0f;
	Duration = 0.0f;
	RoundTimer = 0.0f;
	HaltRounds = false;
	HitRatio = hitRatio;
	SpinSide = (FMath::RandBool() == true) ? +1 : -1;
}

/**
* End manual firing of the gun, normally from a defense turret.
***********************************************************************************/

void AGatlingGun::EndFiring()
{
	Timer = FMath::Max(Timer, WindUpTime + Duration);

	HaltRounds = true;
}

/**
* Select a target for the gun.
***********************************************************************************/

AActor* AGatlingGun::SelectTarget(AActor* launchPlatform, FPlayerPickupSlot* launchPickup, float autoAiming, float& weight, bool speculative)
{
	AActor* result = nullptr;
	float minCorrection = 1.0f;
	float spread = (autoAiming * 0.05f);
	FVector fromPosition = launchPlatform->GetActorLocation();
	FVector fromDirection = launchPlatform->GetTransform().GetUnitAxis(EAxis::X);
	APlayGameMode* gameMode = APlayGameMode::Get(launchPlatform);
	ABaseVehicle* launchVehicle = Cast<ABaseVehicle>(launchPlatform);

	// Search for the best target vehicle for the launch platform's current condition.

	GRIP_GAME_MODE_LIST_FOR(GetVehicles(), vehicles, launchPlatform);

	for (ABaseVehicle* vehicle : vehicles)
	{
		if ((vehicle != launchVehicle) &&
			(vehicle->IsVehicleDestroyed() == false) &&
			(speculative == false || vehicle->IsGoodForSmacking() == true) &&
			((launchVehicle != nullptr && launchVehicle->IsAIVehicle() == false) || vehicle->CanBeAttacked() == true) &&
			(launchPickup == nullptr || launchPickup->BotWillTargetHuman == false || vehicle->IsAIVehicle() == false))
		{
			FVector targetPosition = vehicle->GetTargetBullsEye();
			float thisWeight = FMathEx::TargetWeight(fromPosition, fromDirection, targetPosition, 5.0f * 100.0f, 250.0f * 100.0f, 1.0f - spread, true);

			thisWeight = gameMode->ScaleOffensivePickupWeight(launchVehicle != nullptr && launchVehicle->HasAIDriver(), thisWeight, launchPickup, gameMode->VehicleShouldFightVehicle(launchVehicle, vehicle));

			if (thisWeight >= 0.0f &&
				minCorrection > thisWeight)
			{
				minCorrection = thisWeight;
				result = vehicle;
			}
		}
	}

	weight = 1.0f - minCorrection;

	return result;
}

/**
* Sweep along projectile direction to see if it hits something along the way.
***********************************************************************************/

bool AGatlingGun::GetCollision(UWorld* world, const FVector& start, const FVector& end, float& time, AActor* ignoreTarget)
{
	if ((end - start).Size() > SMALL_NUMBER)
	{
		QueryParams.ClearIgnoredActors();
		QueryParams.AddIgnoredActor(LaunchPlatform.Get());

		if (ignoreTarget != nullptr)
		{
			QueryParams.AddIgnoredActor(ignoreTarget);
		}

		if (world->LineTraceSingleByChannel(HitResult, start, end, ABaseGameMode::ECC_LineOfSightTestIncVehicles, QueryParams) == true)
		{
			time = HitResult.GetComponent() ? HitResult.Time : 1.f;

			return HitResult.GetActor() != nullptr;
		}
	}

	return false;
}

#pragma region BotCombatTraining

/**
* Get a weighting, between 0 and 1, of how ideally a pickup can be used, optionally
* against a particular vehicle. 0 means cannot be used effectively at all, 1 means a
* very high chance of pickup efficacy.
***********************************************************************************/

float AGatlingGun::EfficacyWeighting(ABaseVehicle* launchVehicle, FPlayerPickupSlot* launchPickup, ABaseVehicle* againstVehicle, AActor*& targetSelected, AGatlingGun* gun)
{
	targetSelected = nullptr;

	if (launchVehicle->IsGrounded() == true)
	{
		float weight = 0.0f;

		targetSelected = SelectTarget(launchVehicle, launchPickup, gun->AutoAiming, weight, true);

		if (targetSelected != nullptr)
		{
			FHitResult hitResult;
			FCollisionQueryParams queryParams(TEXT("GunVisibilityTest"), true);

			queryParams.AddIgnoredActor(launchVehicle);
			queryParams.AddIgnoredActor(targetSelected);

			FVector position = launchVehicle->GetCenterLocation();
			ABaseVehicle* vehicle = Cast<ABaseVehicle>(targetSelected);
			FVector offset = (vehicle != nullptr) ? vehicle->GetSurfaceDirection() * -100.0f : FVector(0.0f, 0.0f, -100.0f);
			FVector targetPosition = ((vehicle != nullptr) ? vehicle->GetCenterLocation() : targetSelected->GetActorLocation()) + offset;

			if (launchVehicle->GetWorld()->LineTraceSingleByChannel(hitResult, position + launchVehicle->GetSurfaceDirection() * -100.0f, targetPosition, ABaseGameMode::ECC_LineOfSightTest, queryParams) == true)
			{
				weight = 0.0f;
			}
		}

		return (targetSelected != nullptr && (targetSelected == againstVehicle || againstVehicle == nullptr)) ? ((weight >= 0.5f) ? 1.0f : weight) : 0.0f;
	}

	return 0.0f;
}

#pragma endregion BotCombatTraining

#pragma endregion PickupGun
