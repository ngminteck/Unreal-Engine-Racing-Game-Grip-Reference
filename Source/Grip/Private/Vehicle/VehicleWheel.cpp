/**
*
* Wheel implementation, use for wheels attached to vehicles.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* All of the data required to manage a wheel attached to a vehicle.
*
***********************************************************************************/

#include "vehicle/vehiclewheel.h"

#pragma region VehicleSurfaceEffects

/**
* Destroy the particle system components for the surfaces.
***********************************************************************************/

void FWheelDrivingSurfaces::DestroyComponents()
{
	for (FWheelDrivingSurface& surface : Surfaces)
	{
		if (GRIP_POINTER_VALID(surface.Surface) == true)
		{
			GRIP_DETACH(surface.Surface);
			surface.Surface->DestroyComponent();
			surface.Surface.Reset();
		}
	}
}

/**
* Setup the last component from the current one, ready to transition in a new one.
***********************************************************************************/

void FWheelDrivingSurfaces::SetupLastComponent(float fadeOutTime, bool destroy)
{
	if (GRIP_POINTER_VALID(Surfaces[0].Surface) == true)
	{
		DiscardComponent(Surfaces[1]);

		Surfaces[1] = Surfaces[0];

		Surfaces[1].Timer = fadeOutTime;
		Surfaces[1].FadeTime = fadeOutTime;

		if (Surfaces[0].Timer != 0.0f)
		{
			// The surface hasn't fully faded in yet so compensate by offsetting the
			// fade out time.

			Surfaces[1].Timer *= 1.0f - (Surfaces[0].Timer / Surfaces[0].FadeTime);
		}

		Surfaces[0].Surface.Reset();

		if (destroy == true)
		{
			DestroyLastComponent();
		}
	}
}

/**
* Destroy the last component, called whenever it's clearly faded out.
***********************************************************************************/

void FWheelDrivingSurfaces::DestroyLastComponent()
{
	DiscardComponent(Surfaces[1]);
}

/**
* Discard a component, letting it die naturally one it has completed its visual
* effect.
***********************************************************************************/

void FWheelDrivingSurfaces::DiscardComponent(FWheelDrivingSurface& surface)
{
	if (GRIP_POINTER_VALID(surface.Surface) == true)
	{
		surface.Surface->Deactivate();
		surface.Surface->bAutoDestroy = true;

		GRIP_DETACH(surface.Surface);

		if (surface.Surface->bWasCompleted == true)
		{
			surface.Surface->DestroyComponent();
		}

		surface.Surface.Reset();
	}
}

#pragma endregion VehicleSurfaceEffects
