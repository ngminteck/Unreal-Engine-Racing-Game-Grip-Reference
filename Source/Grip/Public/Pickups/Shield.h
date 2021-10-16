/**
*
* Painkiller shield implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Shield pickup type, one of the pickups used by vehicles in the game.
*
***********************************************************************************/

#pragma once

#include "system/gameconfiguration.h"
#include "pickupbase.h"
#include "shield.generated.h"

struct FPlayerPickupSlot;

/**
* Data for linking a shield to a vehicle.
***********************************************************************************/

UCLASS(ClassGroup = Pickups)
class UVehicleShield : public UDataAsset
{
	GENERATED_BODY()

public:

	// The offset to use for rendering the front shield.
	UPROPERTY(EditAnywhere, Category = Shield)
		FVector FrontOffset = FVector::ZeroVector;

	// The rotation to use for rendering the front shield.
	UPROPERTY(EditAnywhere, Category = Shield)
		FRotator FrontRotation = FRotator::ZeroRotator;

	// The offset to use for rendering the rear shield.
	UPROPERTY(EditAnywhere, Category = Shield)
		FVector RearOffset = FVector::ZeroVector;

	// The rotation to use for rendering the rear shield.
	UPROPERTY(EditAnywhere, Category = Shield)
		FRotator RearRotation = FRotator::ZeroRotator;

	// The particle system to use for the active state.
	UPROPERTY(EditAnywhere, Category = Shield)
		UParticleSystem* ActiveEffectFront = nullptr;

	// The particle system to use for the destroyed state.
	UPROPERTY(EditAnywhere, Category = Shield)
		UParticleSystem* DestroyedEffectFront = nullptr;

	// The particle system to use for the active state.
	UPROPERTY(EditAnywhere, Category = Shield)
		UParticleSystem* ActiveEffectRear = nullptr;

	// The particle system to use for the destroyed state.
	UPROPERTY(EditAnywhere, Category = Shield)
		UParticleSystem* DestroyedEffectRear = nullptr;

	// The particle system to use for the shield hit effect
	UPROPERTY(EditAnywhere, Category = Shield)
		UParticleSystem* HitEffect = nullptr;

	// The particle system to use for the shield hit effect
	UPROPERTY(EditAnywhere, Category = Shield)
		UParticleSystem* HitPointEffect = nullptr;

	// The sound to use for the shield hit effects
	UPROPERTY(EditAnywhere, Category = Shield)
		USoundCue* HitSound = nullptr;

	// Sound cue for the activate sound.
	UPROPERTY(EditAnywhere, Category = Shield)
		USoundCue* ActivateSound = nullptr;

	// Sound cue for the active sound.
	UPROPERTY(EditAnywhere, Category = Shield)
		USoundCue* ActiveSound = nullptr;

	// Sound cue for the destroyed sound.
	UPROPERTY(EditAnywhere, Category = Shield)
		USoundCue* DestroyedSound = nullptr;
};

/**
* The shield pickup actor.
***********************************************************************************/

UCLASS(Abstract, ClassGroup = Pickups)
class GRIP_API AShield : public APickupBase
{
	GENERATED_BODY()

public:

	// Construct a shield.
	AShield();

	// Is the shield rear-end only?
	UPROPERTY(EditAnywhere, Category = Shield)
		bool RearOnly = true;

	// The hit points associated with the shield.
	UPROPERTY(EditAnywhere, Category = Shield, meta = (UIMin = "0", UIMax = "250", ClampMin = "0", ClampMax = "250"))
		int32 HitPoints = 40;

	// The duration associated with the shield.
	UPROPERTY(EditAnywhere, Category = Shield, meta = (UIMin = "5", UIMax = "30", ClampMin = "5", ClampMax = "30"))
		float Duration = 15.0f;

	// Sound cue for a charged impact.
	UPROPERTY(EditAnywhere, Category = Shield)
		USoundCue* ChargedImpact = nullptr;

#pragma region PickupShield

	// Activate the pickup.
	virtual void ActivatePickup(ABaseVehicle* launchVehicle, int32 pickupSlot, EPickupActivation activation, bool charged) override;

	// Impact the shield with a given force.
	void Impact(int32 force)
	{ HitPoints -= force; if (IsCharged() == true) HitPoints = FMath::Max(1, HitPoints); else HitPoints = FMath::Max(0, HitPoints); }

	// Destroy the shield.
	void DestroyShield();

	// Is the shield currently active?
	bool IsActive() const
	{ return DestroyedAt == 0.0f; }

	// Is the shield destroyed?
	bool IsDestroyed() const
	{ return (HitPoints <= 0); }

	// Get the strength of the shield.
	float GetStrength() const
	{ return (float)HitPoints / (float)OriginalHitPoints; }

	// Is the shield at full strength?
	bool IsFull() const
	{ return HitPoints == OriginalHitPoints; }

#pragma region BotCombatTraining

	// Get a weighting, between 0 and 1, of how ideally a pickup can be used, optionally against a particular vehicle.
	// 0 means cannot be used effectively at all, 1 means a very high chance of pickup efficacy.
	static float EfficacyWeighting(ABaseVehicle* launchVehicle);

#pragma endregion BotCombatTraining

protected:

	// Do the regular update tick.
	virtual void Tick(float deltaSeconds) override;

private:

	// Spawn a new shield effect.
	UParticleSystemComponent* SpawnShieldEffect(UParticleSystem* from);

	// Timer used for the lifetime of the shield.
	float Timer = 0.0f;

	// The time the shield was destroyed at.
	float DestroyedAt = 0.0f;

	// The original number of hit points when the shield is created.
	int32 OriginalHitPoints = 0;

#pragma endregion PickupShield

	// Looped audio for then the shield is active.
	UPROPERTY(Transient)
		UAudioComponent* ActiveAudio = nullptr;

	// The particle system to use for the active state.
	UPROPERTY(Transient)
		UParticleSystemComponent* ActiveEffectFront = nullptr;

	// The particle system to use for the active state.
	UPROPERTY(Transient)
		UParticleSystemComponent* ActiveEffectRear = nullptr;

	// The particle system to use for the destroyed state.
	UPROPERTY(Transient)
		UParticleSystemComponent* DestroyedEffectFront = nullptr;

	// The particle system to use for the destroyed state.
	UPROPERTY(Transient)
		UParticleSystemComponent* DestroyedEffectRear = nullptr;
};
