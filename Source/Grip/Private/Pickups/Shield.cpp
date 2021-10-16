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

#include "pickups/shield.h"
#include "pickups/homingmissile.h"
#include "vehicle/flippablevehicle.h"

/**
* Construct a shield.
***********************************************************************************/

AShield::AShield()
{
	PickupType = EPickupType::Shield;

	PrimaryActorTick.bCanEverTick = true;

	ActiveAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("ActiveSound"));
}

#pragma region PickupShield

/**
* Activate the pickup.
***********************************************************************************/

void AShield::ActivatePickup(ABaseVehicle* launchVehicle, int32 pickupSlot, EPickupActivation activation, bool charged)
{
	Super::ActivatePickup(launchVehicle, pickupSlot, activation, charged);

	OriginalHitPoints = HitPoints;

	ActiveEffectRear = SpawnShieldEffect(launchVehicle->VehicleShield->ActiveEffectRear);
	DestroyedEffectRear = SpawnShieldEffect(launchVehicle->VehicleShield->DestroyedEffectRear);

	SetRootComponent(ActiveEffectRear);

	if (GRIP_OBJECT_VALID(ActiveEffectRear) == true)
	{
		GRIP_VEHICLE_EFFECT_ATTACH(ActiveEffectRear, launchVehicle, "RootDummy", false);

		ActiveEffectRear->SetOwnerNoSee(LaunchVehicle->IsCockpitView());

		ActiveEffectRear->SetRelativeLocation(launchVehicle->VehicleShield->RearOffset);
		ActiveEffectRear->SetRelativeRotation(launchVehicle->VehicleShield->RearRotation);
		ActiveEffectRear->SetActive(true);
		ActiveEffectRear->SetHiddenInGame(false);
	}

	if (GRIP_OBJECT_VALID(DestroyedEffectRear) == true)
	{
		GRIP_VEHICLE_EFFECT_ATTACH(DestroyedEffectRear, launchVehicle, "RootDummy", false);

		DestroyedEffectRear->SetOwnerNoSee(LaunchVehicle->IsCockpitView());

		DestroyedEffectRear->SetRelativeLocation(launchVehicle->VehicleShield->RearOffset);
		DestroyedEffectRear->SetRelativeRotation(launchVehicle->VehicleShield->RearRotation);
	}

	if (RearOnly == false)
	{
		ActiveEffectFront = SpawnShieldEffect(launchVehicle->VehicleShield->ActiveEffectFront);
		DestroyedEffectFront = SpawnShieldEffect(launchVehicle->VehicleShield->DestroyedEffectFront);

		if (GRIP_OBJECT_VALID(ActiveEffectFront) == true)
		{
			GRIP_VEHICLE_EFFECT_ATTACH(ActiveEffectFront, launchVehicle, "RootDummy", false);

			ActiveEffectFront->SetOwnerNoSee(LaunchVehicle->IsCockpitView());

			ActiveEffectFront->SetRelativeLocation(launchVehicle->VehicleShield->FrontOffset);
			ActiveEffectFront->SetRelativeRotation(launchVehicle->VehicleShield->FrontRotation);
			ActiveEffectFront->SetActive(true);
			ActiveEffectFront->SetHiddenInGame(false);
		}

		if (GRIP_OBJECT_VALID(DestroyedEffectFront) == true)
		{
			GRIP_VEHICLE_EFFECT_ATTACH(DestroyedEffectFront, launchVehicle, "RootDummy", false);

			DestroyedEffectFront->SetOwnerNoSee(LaunchVehicle->IsCockpitView());

			DestroyedEffectFront->SetRelativeLocation(launchVehicle->VehicleShield->FrontOffset);
			DestroyedEffectFront->SetRelativeRotation(launchVehicle->VehicleShield->FrontRotation);
		}
	}

	UGameplayStatics::SpawnSoundAttached(launchVehicle->VehicleShield->ActivateSound, launchVehicle->VehicleMesh);

	if (GRIP_OBJECT_VALID(ActiveAudio) == true)
	{
		GRIP_ATTACH(ActiveAudio, launchVehicle->VehicleMesh, NAME_None);

		ActiveAudio->SetSound(launchVehicle->VehicleShield->ActiveSound);
		ActiveAudio->Play();
	}
}

/**
* Destroy the shield.
***********************************************************************************/

void AShield::DestroyShield()
{
	if (GRIP_OBJECT_VALID(ActiveEffectRear) == true)
	{
		ActiveEffectRear->DestroyComponent();
		ActiveEffectRear = nullptr;
	}

	if (GRIP_OBJECT_VALID(DestroyedEffectRear) == true)
	{
		DestroyedEffectRear->SetActive(true, true);
		DestroyedEffectRear->SetHiddenInGame(false);
		DestroyedEffectRear->SetVectorParameter("Velocity", LaunchVehicle->GetVelocity());
	}

	if (RearOnly == false)
	{
		if (GRIP_OBJECT_VALID(ActiveEffectFront) == true)
		{
			ActiveEffectFront->DestroyComponent();
			ActiveEffectFront = nullptr;
		}

		if (GRIP_OBJECT_VALID(DestroyedEffectFront) == true)
		{
			DestroyedEffectFront->SetActive(true, true);
			DestroyedEffectFront->SetHiddenInGame(false);
			DestroyedEffectFront->SetVectorParameter("Velocity", LaunchVehicle->GetVelocity());
		}
	}

	UGameplayStatics::SpawnSoundAttached(LaunchVehicle->VehicleShield->DestroyedSound, LaunchVehicle->VehicleMesh);

	DestroyedAt = Timer;

	LaunchVehicle->ReleasePickupSlot(PickupSlot);
}

/**
* Do the regular update tick.
***********************************************************************************/

void AShield::Tick(float deltaSeconds)
{
	Super::Tick(deltaSeconds);

	if (GRIP_OBJECT_VALID(LaunchVehicle) == true)
	{
		Timer += deltaSeconds;

		if (DestroyedAt != 0.0f)
		{
			if (GRIP_OBJECT_VALID(ActiveAudio) == true)
			{
				ActiveAudio->SetVolumeMultiplier(1.0f - FMathEx::GetRatio(Timer, 0.0f, 0.5f));
			}

			if (Timer - DestroyedAt > 2.0f)
			{
				DestroyPickup();
			}
		}
		else
		{
			if (GRIP_OBJECT_VALID(ActiveAudio) == true)
			{
				ActiveAudio->SetVolumeMultiplier(FMathEx::GetRatio(Timer, 0.0f, 0.5f));
			}

			if (Timer > Duration)
			{
				DestroyShield();
			}
		}
	}
}

/**
* Spawn a new shield effect.
***********************************************************************************/

UParticleSystemComponent* AShield::SpawnShieldEffect(UParticleSystem* from)
{
	if (from != nullptr)
	{
		UParticleSystemComponent* component = NewObject<UParticleSystemComponent>(this);

		if (component != nullptr)
		{
			component->bAutoActivate = false;
			component->bAutoDestroy = false;
			component->SetHiddenInGame(true);
			component->SetTemplate(from);

			component->RegisterComponent();

			return component;
		}
	}

	return nullptr;
}

#pragma region BotCombatTraining

/**
* Get a weighting, between 0 and 1, of how ideally a pickup can be used, optionally
* against a particular vehicle. 0 means cannot be used effectively at all, 1 means a
* very high chance of pickup efficacy.
***********************************************************************************/

float AShield::EfficacyWeighting(ABaseVehicle* launchVehicle)
{
	return (launchVehicle->AIShouldRaiseShield() == true) ? 1.0f : 0.0f;
}

#pragma endregion BotCombatTraining

#pragma endregion PickupShield
