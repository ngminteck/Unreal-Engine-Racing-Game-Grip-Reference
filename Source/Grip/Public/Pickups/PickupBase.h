/**
*
* Base pickup implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Basic pickup type, inherited by all of the different pickups in the game.
*
***********************************************************************************/

#pragma once

#include "pickups/pickup.h"
#include "pickupbase.generated.h"

class ABaseVehicle;
class UGlobalGameState;
class APlayGameMode;

/**
* Activation type for handling the activation of pickups.
***********************************************************************************/

enum class EPickupActivation : uint8
{
	None,

	// The pickup input has just been pressed down
	Pressed,

	// The pickup input has just been released
	Released
};

/**
* Basic pickup type, inherited by all of the different pickups in the game.
***********************************************************************************/

UCLASS(Abstract, ClassGroup = Pickups)
class GRIP_API APickupBase : public AActor
{
	GENERATED_BODY()

public:

	// Get the launch vehicle for this pickup.
	UFUNCTION(BlueprintCallable, Category = Pickup)
		ABaseVehicle* GetLaunchVehicle() const
	{ check(LaunchVehicleIsValid() == true); return LaunchVehicle; }

	// Is the launch vehicle valid for this pickup?
	bool LaunchVehicleIsValid() const
	{ return (LaunchVehicle != nullptr); }

#pragma region VehiclePickups

	// Activate the pickup.
	virtual void ActivatePickup(ABaseVehicle* launchVehicle, int32 pickupSlot, EPickupActivation activation, bool charged);

	// Destroy the pickup.
	virtual void DestroyPickup();

	// What type is this pickup?
	EPickupType GetPickupType() const
	{ return PickupType; }

	// Is this pickup charged?
	bool IsCharged() const
	{ return Charged; }

	// Get the curvature ahead of the vehicle over the period of time given.
	static FRotator GetCurvatureAhead(float overTime, float speedScale, ABaseVehicle* launchVehicle);

	// Is a vehicle within the bounds of the curvature ahead of the vehicle over the period of time given.
	static bool WithinCurvatureAhead(float overTime, float speedScale, ABaseVehicle* launchVehicle, float yawDegreesPerSecond, float pitchDegreesPerSecond = 0.0f, float rollDegreesPerSecond = 0.0f);

	// Get the minimum optimum speed ahead of the vehicle over the period of time given.
	static float GetSpeedAhead(float overTime, float speedScale, ABaseVehicle* launchVehicle);

protected:

	// Do some post initialization just before the game is ready to play.
	virtual void PostInitializeComponents() override;

	// Which slot in the launch vehicle this pickup is assigned to.
	int32 PickupSlot = 0;

private:

	// Is this pickup charged?
	bool Charged = false;

#pragma endregion VehiclePickups

protected:

	// Naked pointer to game state for performance reasons.
	UPROPERTY(Transient)
		UGlobalGameState* GameState = nullptr;

	// Naked pointer to play game mode for performance reasons.
	UPROPERTY(Transient)
		APlayGameMode* PlayGameMode = nullptr;

	// What type is this pickup?
	EPickupType PickupType = EPickupType::None;

	// The launch vehicle for this pickup.
	UPROPERTY(Transient)
		ABaseVehicle* LaunchVehicle = nullptr;
};
