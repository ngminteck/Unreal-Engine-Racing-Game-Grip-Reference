/**
*
* Turbo implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Turbo pickup type, one of the pickups used by vehicles in the game.
*
***********************************************************************************/

#include "pickups/turbo.h"
#include "vehicle/flippablevehicle.h"

/**
* Construct a turbo.
***********************************************************************************/

ATurbo::ATurbo()
{
	PickupType = EPickupType::TurboBoost;

	PrimaryActorTick.bCanEverTick = true;

	ActiveAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("ActiveSound"));
	ActiveAudio->SetVolumeMultiplier(0.0f);

	SetRootComponent(ActiveAudio);

	BoostVsTime.GetRichCurve()->AddKey(0, 0.0f);
	BoostVsTime.GetRichCurve()->AddKey(1, 0.2f);
	BoostVsTime.GetRichCurve()->AddKey(2, 0.2f);
	BoostVsTime.GetRichCurve()->AddKey(3, 0.1f);
	BoostVsTime.GetRichCurve()->AddKey(4, 0.05f);
	BoostVsTime.GetRichCurve()->AddKey(5, 0.0f);
}

#pragma region PickupTurbo

/**
* Activate the pickup.
***********************************************************************************/

void ATurbo::ActivatePickup(ABaseVehicle* launchVehicle, int32 pickupSlot, EPickupActivation activation, bool charged)
{
	Super::ActivatePickup(launchVehicle, pickupSlot, activation, charged);

	GRIP_ATTACH(ActiveAudio, launchVehicle->VehicleMesh, "RootDummy");

	if (GRIP_OBJECT_VALID(ActivateSound) == true)
	{
		UGameplayStatics::SpawnSoundAttached(launchVehicle->IsHumanPlayer() ? ActivateSound : ActivateSoundNonPlayer, launchVehicle->VehicleMesh, NAME_None);
	}

	float minScale = 0.0f;

	BoostVsTime.GetRichCurve()->GetValueRange(minScale, NormalizeScale);
	Duration = BoostVsTime.GetRichCurve()->GetLastKey().Time;

	launchVehicle->TurboEngaged();
}

/**
* Do the regular update tick.
***********************************************************************************/

void ATurbo::Tick(float deltaSeconds)
{
	Super::Tick(deltaSeconds);

	if (GRIP_OBJECT_VALID(LaunchVehicle) == true)
	{
		Timer += deltaSeconds;

		// Manage the playing and volume level of the active sound.

		if (ActivateSoundPlayed == false &&
			Timer > ActiveSoundDelayTime)
		{
			ActivateSoundPlayed = true;

			if (GRIP_OBJECT_VALID(ActiveSound) == true)
			{
				ActiveAudio->SetSound(LaunchVehicle->IsHumanPlayer() ? ActiveSound : ActiveSoundNonPlayer);
				ActiveAudio->Play();
			}
		}

		if (ActivateSoundPlayed == true)
		{
			if (Timer - ActiveSoundDelayTime < ActiveSoundFadeInTime)
			{
				ActiveAudio->SetVolumeMultiplier((Timer - ActiveSoundDelayTime) / ActiveSoundFadeInTime);
			}
			else if (Timer > Duration - ActiveSoundFadeOutTime)
			{
				ActiveAudio->SetVolumeMultiplier((Duration - Timer) / ActiveSoundFadeOutTime);
			}
			else
			{
				ActiveAudio->SetVolumeMultiplier(1.0f);
			}
		}

		// If we're on the brake then cancel any turbo.

		if (LaunchVehicle->GetVehicleControl().BrakeInput > 0.5f &&
			(LaunchVehicle->GetRealTimeClock() - LaunchVehicle->GetVehicleControl().HandbrakePressed) > 0.333f)
		{
			if (Timer < Duration - ActiveSoundFadeOutTime)
			{
				Timer = Duration - ActiveSoundFadeOutTime;
			}
		}

		if (IsActive() == false)
		{
			ActiveAudio->Stop();

			LaunchVehicle->SetTurboBoost(0.0f, 1.0f, 0.0f, 0.0f);
			LaunchVehicle->TurboDisengaged();
			LaunchVehicle->ReleasePickupSlot(PickupSlot);

			DestroyPickup();
		}
		else
		{
			float boost = BoostVsTime.GetRichCurve()->Eval(Timer, 0.0f);
			float raiseFrontScale = RaiseFrontScale;

#pragma region VehicleAntiGravity

			if (LaunchVehicle->Antigravity == true)
			{
				// If we're an antigravity vehicle then raise the front more than usual because
				// otherwise it's not very discernible.

				raiseFrontScale *= 1.333f;
			}

#pragma endregion VehicleAntiGravity

			if (LaunchVehicle->GetLaunchDirection().Z < -0.5f)
			{
				// If we're on the ceiling then lower the front raising scale as we'll likely
				// fall straight off if we don't.

				raiseFrontScale *= 0.666f;
			}

			// Set the properties of the turbo boost on the launch vehicle.

			LaunchVehicle->SetTurboBoost(boost, GripScale, raiseFrontScale, FMath::Min(ActiveAudio->VolumeMultiplier, FMathEx::GetRatio(boost, 0.0f, NormalizeScale)));
		}
	}
}

#pragma region BotCombatTraining

/**
* Get a weighting, between 0 and 1, of how ideally a pickup can be used, optionally
* against a particular vehicle. 0 means cannot be used effectively at all, 1 means a
* very high chance of pickup efficacy.
***********************************************************************************/

float ATurbo::EfficacyWeighting(ABaseVehicle* launchVehicle)
{
	if (launchVehicle->IsDrifting() == false &&
		launchVehicle->IsGrounded(2.0f) == true &&
		launchVehicle->GetSpeedKPH() > 100.0f &&
		launchVehicle->GetAI().IsDrivingCasually() == true &&
		launchVehicle->GetAI().IsGoodForHighSpeed() == true)
	{
		if (FMath::Abs(launchVehicle->GetVehicleControl().SteeringPosition) < GRIP_STEERING_PURPOSEFUL)
		{
			if (launchVehicle->GetAI().RouteFollower.IsValid() == true)
			{
				float speedScale = 1.5f;
				float curvatureTimeAhead = 1.5f;

				if (WithinCurvatureAhead(curvatureTimeAhead, speedScale, launchVehicle, 10.0f) == true)
				{
					float speedTimeAhead = 3.0f;
					float result = FMathEx::GetRatio(GetSpeedAhead(speedTimeAhead, speedScale, launchVehicle), 500.0f, 700.0f);

					return result;
				}
			}
		}
	}

	return 0.0f;
}

#pragma endregion BotCombatTraining

#pragma endregion PickupTurbo
