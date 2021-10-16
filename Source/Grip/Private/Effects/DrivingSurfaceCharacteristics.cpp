/**
*
* Driving surface characteristics.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* The vehicles need to understand something of the characteristics of the surfaces
* that they are driving on. Like friction, and how the tires interact with them
* both visually and how they sound too. These characteristics are held in a central
* data asset for the game, derived from UDrivingSurfaceCharacteristics. This asset
* is then referenced directly from each vehicle, so that it knows how to interact.
*
* There ought to be an instance of the ADrivingSurfaceProperties in each level too,
* which describes the average color of the level's dusty surfaces, and the average
* lighting levels too.
*
***********************************************************************************/

#include "effects/drivingsurfacecharacteristics.h"
#include "game/globalgamestate.h"

/**
* Get the tire friction for a surface type.
***********************************************************************************/

float UDrivingSurfaceCharacteristics::GetTireFriction(EGameSurface surfaceType) const
{

#pragma region VehicleSurfaceEffects

	const FDrivingSurface* surface = Surfaces.FindByKey(surfaceType);

	if (surface != nullptr)
	{
		return surface->TireFriction;
	}

#pragma endregion VehicleSurfaceEffects

	return 0.9f;
}

#pragma region VehicleSurfaceEffects

/**
* Get the visual effect to use for a surface type and vehicle speed.
***********************************************************************************/

UParticleSystem* UDrivingSurfaceCharacteristics::GetVisualEffect(EGameSurface surfaceType, float currentSpeed, bool wheelSkidding, bool wheelSpinning, bool fixedToWheel) const
{
	if (surfaceType >= EGameSurface::Num)
	{
		// If the surface type isn't valid then quit.

		return nullptr;
	}
	else
	{
		float minSpeed = GetMinSpeed(surfaceType);

		if (minSpeed > 0.1f &&
			currentSpeed < minSpeed &&
			wheelSpinning == false)
		{
			// If the speed isn't suitable then quit.

			return nullptr;
		}
		else
		{
			// Create a visual effect for the surface type.

			return GetVisualEffect(surfaceType, wheelSkidding, wheelSpinning, fixedToWheel);
		}
	}
}

/**
* Get the visual effect for a surface type.
***********************************************************************************/

UParticleSystem* UDrivingSurfaceCharacteristics::GetVisualEffect(EGameSurface surfaceType, bool wheelSkidding, bool wheelSpinning, bool fixedToWheel) const
{
	const FDrivingSurface* surface = Surfaces.FindByKey(surfaceType);

	if (surface != nullptr)
	{
		if (fixedToWheel == true)
		{
			return surface->FixedEffect;
		}
		else
		{
			if (wheelSpinning == true)
			{
				return surface->WheelSpinningEffect;
			}
			else if (wheelSkidding == true)
			{
				return surface->WheelSkiddingEffect;
			}
			else
			{
				return surface->Effect;
			}
		}
	}

	return nullptr;
}

/**
* Get the skidding sound for a surface type.
***********************************************************************************/

USoundCue* UDrivingSurfaceCharacteristics::GetSkiddingSound(EGameSurface surfaceType) const
{
	const FDrivingSurface* surface = Surfaces.FindByKey(surfaceType);

	return (surface != nullptr) ? surface->SkiddingSound : nullptr;
}

/**
* Get the min speed for a surface type.
***********************************************************************************/

float UDrivingSurfaceCharacteristics::GetMinSpeed(EGameSurface surfaceType) const
{
	const FDrivingSurface* surface = Surfaces.FindByKey(surfaceType);

	return (surface != nullptr) ? surface->MinSpeed : 0.0f;
}

/**
* Is the effect for this surface type contactless?
***********************************************************************************/

bool UDrivingSurfaceCharacteristics::GetContactless(EGameSurface surfaceType) const
{
	const FDrivingSurface* surface = Surfaces.FindByKey(surfaceType);

	return (surface != nullptr) ? surface->Contactless : false;
}

#pragma endregion VehicleSurfaceEffects
