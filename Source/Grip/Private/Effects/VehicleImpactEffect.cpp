/**
*
* Vehicle impact effects.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* The vehicles need to understand something of the characteristics of the surfaces
* in the game for impact effects. These characteristics are held in a central
* data asset for the game, derived from UDrivingSurfaceImpactCharacteristics.
* This asset is then referenced directly from each vehicle, so that it knows how to
* produce such impact effects.
*
* The effects themselves, are generally spawned into the world via the
* AVehicleImpactEffect actor.
*
***********************************************************************************/

#include "effects/vehicleimpacteffect.h"
#include "vehicle/basevehicle.h"

#pragma region VehicleSurfaceImpacts

/**
* Spawn an impact effect.
***********************************************************************************/

void UDrivingSurfaceImpactCharacteristics::SpawnImpact(ABaseVehicle* vehicle, const FDrivingSurfaceImpact& surface, bool tireImpact, const FVector& location, const FRotator& rotation, const FVector& velocity, const FVector& surfaceColor, const FVector& lightColor)
{
	UParticleSystem* effect = (tireImpact == true) ? surface.TireEffect : surface.BodyEffect;

	if (effect != nullptr)
	{
		// Spawn the visual effect.

		UMovingParticleSystemComponent* component = NewObject<UMovingParticleSystemComponent>(vehicle);

		if (component != nullptr)
		{
			component->bAutoActivate = true;
			component->bAutoDestroy = true;
			component->Velocity = velocity;

			// Assign the new effect.

			component->SetTemplate(effect);
			component->SetWorldLocationAndRotation(location, rotation);
			component->SetVectorParameter("SurfaceColour", surfaceColor);
			component->SetVectorParameter("LightColour", lightColor);

			// Don't forget to register the component.

			component->RegisterComponent();

			// And now activate it.

			component->Activate();
		}
	}

	USoundCue* sound = (tireImpact == true) ? surface.TireSound : sound = surface.BodySound;

	if (sound != nullptr)
	{
		// Spawn the sound effect.

		UGameplayStatics::PlaySoundAtLocation(vehicle, sound, location, vehicle->GlobalVolume);
	}
}

/**
* Do the regular update tick, to move the particle system along.
***********************************************************************************/

void UMovingParticleSystemComponent::TickComponent(float deltaSeconds, enum ELevelTick tickType, FActorComponentTickFunction* thisTickFunction)
{
	Super::TickComponent(deltaSeconds, tickType, thisTickFunction);

	if (Velocity.IsNearlyZero() == false)
	{
		MoveComponent(Velocity * deltaSeconds, GetComponentRotation(), false);
	}
}

#pragma endregion VehicleSurfaceImpacts
