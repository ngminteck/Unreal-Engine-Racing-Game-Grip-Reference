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

#pragma once

#include "system/gameconfiguration.h"
#include "effects/lightstreakcomponent.h"
#include "pickupbase.h"
#include "gatlinggun.generated.h"

struct FPlayerPickupSlot;

/**
* Boilerplate class for the GunHostInterface.
***********************************************************************************/

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGunHostInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

/**
* Interface class for the GunHostInterface.
***********************************************************************************/

class IGunHostInterface
{
	GENERATED_IINTERFACE_BODY()

public:

	// Use human player audio?
	virtual bool UseHumanPlayerAudio() const = 0;

#pragma region PickupGun

	// Get the orientation of the gun.
	virtual FQuat GetGunOrientation() const = 0;

	// Get the direction for firing a round.
	virtual FVector GetGunRoundDirection(FVector direction) const = 0;

	// Get the round ejection properties.
	virtual FVector EjectGunRound(int32 roundLocation, bool charged) = 0;

#pragma endregion PickupGun

};

/**
* Data for linking a gun to a vehicle.
***********************************************************************************/

UCLASS(ClassGroup = Pickups)
class UVehicleGun : public UDataAsset
{
	GENERATED_BODY()

public:

	// The particle system to use for the muzzle flash.
	UPROPERTY(EditAnywhere, Category = Gun)
		UParticleSystem* MuzzleFlashEffect = nullptr;

	// The particle system to use for the shell ejection.
	UPROPERTY(EditAnywhere, Category = Gun)
		UParticleSystem* ShellEjectEffect = nullptr;

	// The sound to use for the gun.
	UPROPERTY(EditAnywhere, Category = Gun)
		USoundCue* RoundSound = nullptr;

	// The sound to use for the gun.
	UPROPERTY(EditAnywhere, Category = Gun)
		USoundCue* RoundSoundNonPlayer = nullptr;
};

/**
* The Gatling gun pickup actor.
***********************************************************************************/

UCLASS(Abstract, ClassGroup = Pickups)
class GRIP_API AGatlingGun : public APickupBase
{
	GENERATED_BODY()

public:

	// Construct a gun.
	AGatlingGun();

	// The wind up time in seconds.
	UPROPERTY(EditAnywhere, Category = Gun, meta = (UIMin = "0.0", UIMax = "60.0", ClampMin = "0.0", ClampMax = "60.0"))
		float WindUpTime = 2.0f;

	// The wind up time in seconds.
	UPROPERTY(EditAnywhere, Category = Gun, meta = (UIMin = "0.0", UIMax = "60.0", ClampMin = "0.0", ClampMax = "60.0"))
		float WindDownTime = 2.0f;

	// The length of time to fire the gun for, in seconds.
	UPROPERTY(EditAnywhere, Category = Gun, meta = (UIMin = "1.0", UIMax = "10.0", ClampMin = "1.0", ClampMax = "10.0"))
		float Duration = 5.0f;

	// The firing delay time in seconds.
	UPROPERTY(EditAnywhere, Category = Gun, meta = (UIMin = "0.0", UIMax = "60.0", ClampMin = "0.0", ClampMax = "60.0"))
		float FiringDelay = 2.0f;

	// The maximum fire rate of the gun in rounds per second.
	UPROPERTY(EditAnywhere, Category = Gun, meta = (UIMin = "0.0", UIMax = "30.0", ClampMin = "0.0", ClampMax = "30.0"))
		float FireRate = 30.0f;

	// The impact force of the round between 0 and 1.
	UPROPERTY(EditAnywhere, Category = Gun, meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
		float RoundForce = 0.25f;

	// The hit points associated with each round from this weapon.
	UPROPERTY(EditAnywhere, Category = Gun, meta = (UIMin = "0", UIMax = "100", ClampMin = "0", ClampMax = "100"))
		int32 HitPoints = 5;

	// The amount of auto-aiming to apply between 0 and 1.
	UPROPERTY(EditAnywhere, Category = Gun, meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
		float AutoAiming = 0.5f;

	// Alternate the barrels when firing?
	UPROPERTY(EditAnywhere, Category = Gun)
		bool AlternateBarrels = false;

	// Sound cue for the barrel spin sound.
	UPROPERTY(EditAnywhere, Category = Gun)
		USoundCue* BarrelSpinSound = nullptr;

	// Sound cue for the barrel spin sound. Non-Player variant.
	UPROPERTY(EditAnywhere, Category = Gun)
		USoundCue* BarrelSpinSoundNonPlayer = nullptr;

	// Perform some blueprint code when a bullet round hits a surface.
	UFUNCTION(BlueprintImplementableEvent, Category = Gun)
		void BulletHitAnimation(UPrimitiveComponent* component, const TArray<UParticleSystem*>& particleSystems, const TArray<FVector>& hitLocations, USoundCue* soundEffect, const FVector& location, const FRotator& rotation, EGameSurface surface, const FVector& colour, bool charged);

private:

	// Audio component for the barrel spin sound.
	UPROPERTY(Transient)
		UAudioComponent* BarrelSpinAudio = nullptr;

#pragma region PickupGun

public:

	// Activate the pickup.
	virtual void ActivatePickup(ABaseVehicle* launchVehicle, int32 pickupSlot, EPickupActivation activation, bool charged) override;

	// Attach to a launch platform, like a defense turret.
	void AttachLaunchPlatform(AActor* launchPlatform);

	// Begin manual firing of the gun, normally from a defense turret.
	void BeginFiring(float hitRatio);

	// End manual firing of the gun, normally from a defense turret.
	void EndFiring();

	// Sweep along projectile direction to see if it hits something along the way.
	bool GetCollision(UWorld* world, const FVector& start, const FVector& end, float& time, AActor* ignoreTarget);

	// Is the gun currently active?
	bool IsActive() const
	{ return Timer < Duration + WindUpTime + WindDownTime; }

	// Select a target for the gun.
	static AActor* SelectTarget(AActor* launchPlatform, FPlayerPickupSlot* launchPickup, float autoAiming, float& weight, bool speculative);

#pragma region BotCombatTraining

	// Get a weighting, between 0 and 1, of how ideally a pickup can be used, optionally against a particular vehicle.
	// 0 means cannot be used effectively at all, 1 means a very high chance of pickup efficacy.
	static float EfficacyWeighting(ABaseVehicle* launchVehicle, FPlayerPickupSlot* launchPickup, ABaseVehicle* againstVehicle, AActor*& targetSelected, AGatlingGun* gun);

#pragma endregion BotCombatTraining

	// The target actor that the gun is aiming for right now.
	TWeakObjectPtr<AActor> Target;

protected:

	// Do some shutdown when the actor is being destroyed.
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

	// Do the regular update tick.
	virtual void Tick(float deltaSeconds) override;

private:

	// The launch platform for the gun, not necessarily a vehicle, could be a defense turret also.
	TWeakObjectPtr<AActor> LaunchPlatform;

	// The gun host interface from the launch platform, cached for speed.
	IGunHostInterface* GunHost = nullptr;

	// Timer used for the lifetime of the pickup.
	float Timer = 0.0f;

	// Timer used between firing rounds.
	float RoundTimer = 0.0f;

	// How often a gun should hit its target, 1 being all the time.
	float HitRatio = 1.0f;

	// The gun port to use for the next round.
	int32 RoundLocation = 0;

	// The number of rounds fired.
	int32 NumRoundsFired = 0;

	// The number of rounds that hit a vehicle.
	int32 NumRoundsHitVehicle = 0;

	// Hit result for the last round fired.
	FHitResult HitResult;

	// The world location of the last round impact point.
	FVector LastImpact;

	// The number of points gained using the weapon.
	int32 NumPoints = 0;

	// The side to spin the victim around.
	float SpinSide = 0.0f;

	// Halt the firing of rounds.
	bool HaltRounds = false;

	// Collision query for target visibility.
	FCollisionQueryParams QueryParams;

	// The vehicles hit by rounds from the gun.
	TArray<ABaseVehicle*> HitVehicles;

#pragma endregion PickupGun

};
