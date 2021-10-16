/**
*
* General pickup pad implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Basic pickup type, inherited by all of the different pickups in the game.
*
***********************************************************************************/

#include "pickups/pickupbase.h"
#include "vehicle/basevehicle.h"
#include "gamemodes/playgamemode.h"

#pragma region VehiclePickups

/**
* Do some post initialization just before the game is ready to play.
***********************************************************************************/

void APickupBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	PlayGameMode = APlayGameMode::Get(this);
	GameState = UGlobalGameState::GetGlobalGameState(this);
}

/**
* Activate the pickup.
***********************************************************************************/

void APickupBase::ActivatePickup(ABaseVehicle* launchVehicle, int32 pickupSlot, EPickupActivation activation, bool charged)
{
	Charged = charged;

	if (LaunchVehicle == nullptr)
	{
		if (PlayGameMode != nullptr &&
			PickupType != EPickupType::None)
		{
			PlayGameMode->AddPickupType(PickupType);
		}

		LaunchVehicle = launchVehicle;
		PickupSlot = pickupSlot;
	}
}

/**
* Destroy the pickup.
***********************************************************************************/

void APickupBase::DestroyPickup()
{
	if (PlayGameMode != nullptr &&
		PickupType != EPickupType::None)
	{
		PlayGameMode->RemovePickupType(PickupType);
	}

	Destroy();
}

/**
* Get the curvature ahead of the vehicle over the period of time given.
***********************************************************************************/

FRotator APickupBase::GetCurvatureAhead(float overTime, float speedScale, ABaseVehicle* launchVehicle)
{
	FQuat quaternion = launchVehicle->GetActorRotation().Quaternion();
	int32 direction = launchVehicle->GetPursuitSplineDirection();
	float curvatureDistanceAhead = overTime * FMathEx::KilometersPerHourToCentimetersPerSecond(launchVehicle->GetSpeedKPH() * speedScale);
	FRouteFollower& follower = launchVehicle->GetAI().RouteFollower;
	FRotator splineDegrees = follower.GetCurvatureOverDistance(follower.ThisDistance, curvatureDistanceAhead, direction, quaternion, true);

	return splineDegrees;
}

/**
* Is a vehicle within the bounds of the curvature ahead of the vehicle over the
* period of time given.
***********************************************************************************/

bool APickupBase::WithinCurvatureAhead(float overTime, float speedScale, ABaseVehicle* launchVehicle, float yawDegreesPerSecond, float pitchDegreesPerSecond, float rollDegreesPerSecond)
{
	FRotator splineDegrees = GetCurvatureAhead(overTime, speedScale, launchVehicle);

	// Convert to degrees per second.

	splineDegrees *= 1.0f / overTime;

	bool result = true;

	result &= (yawDegreesPerSecond == 0.0f || splineDegrees.Yaw < yawDegreesPerSecond);
	result &= (pitchDegreesPerSecond == 0.0f || splineDegrees.Pitch < pitchDegreesPerSecond);
	result &= (rollDegreesPerSecond == 0.0f || splineDegrees.Roll < rollDegreesPerSecond);

	return result;
}

/**
* Get the minimum optimum speed ahead of the vehicle over the period of time given.
***********************************************************************************/

float APickupBase::GetSpeedAhead(float overTime, float speedScale, ABaseVehicle* launchVehicle)
{
	FVehicleAI& ai = launchVehicle->GetAI();
	FRouteFollower& follower = ai.RouteFollower;

	float maxSpeed = 1000.0f;
	float minSpeed = ai.OptimumSpeed;

	if (minSpeed < KINDA_SMALL_NUMBER)
	{
		minSpeed = maxSpeed;
	}

	float speedTimeAhead = 3.0f;
	float distanceAhead = speedTimeAhead * FMathEx::KilometersPerHourToCentimetersPerSecond(launchVehicle->GetSpeedKPH() * speedScale);
	float speed = follower.GetMinimumOptimumSpeedOverDistance(follower.ThisDistance, distanceAhead, launchVehicle->GetPursuitSplineDirection());

	if (speed < KINDA_SMALL_NUMBER)
	{
		speed = maxSpeed;
	}

	return FMath::Min(minSpeed, speed);
}

#pragma endregion VehiclePickups
