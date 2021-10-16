/**
*
* Catchup debugging HUD.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
***********************************************************************************/

#include "ui/debugcatchuphud.h"
#include "vehicle/flippablevehicle.h"

#pragma region VehicleCatchup

/**
* Draw the HUD.
***********************************************************************************/

void ADebugCatchupHUD::DrawHUD()
{
	Super::DrawHUD();

	HorizontalOffset = 200.0f;

	APawn* owningPawn = GetOwningPawn();
	ABaseVehicle* ownerVehicle = Cast<ABaseVehicle>(owningPawn);

	if (ownerVehicle != nullptr)
	{
		ownerVehicle = ownerVehicle->CameraTarget();
	}

	if (ownerVehicle != nullptr)
	{
		APlayGameMode* gameMode = APlayGameMode::Get(GetWorld());

		AddFloat(TEXT("Optimum Speed"), ownerVehicle->GetAI().OptimumSpeed);
		AddFloat(TEXT("Track Optimum Speed"), ownerVehicle->GetAI().TrackOptimumSpeed);

		if (gameMode != nullptr)
		{
			GRIP_GAME_MODE_LIST(GetVehicles(), vehicles);

			Y += LineHeight;

			AddText(TEXT(""), FText::FromString(TEXT("P  RCR  DCR   DS   B")));

			for (int32 i = 0; i < vehicles.Num(); i++)
			{
				for (ABaseVehicle* vehicle : vehicles)
				{
					if (i == vehicle->RaceState.RacePosition)
					{
						FString string = FString::Printf(TEXT("%2d  %+03d  %+03d  %+0.02f  %1d"), vehicle->RaceState.RacePosition, FMath::RoundToInt(vehicle->RaceState.RaceCatchupRatio * 10), FMath::RoundToInt(vehicle->RaceState.DragCatchupRatio * 10), vehicle->RaceState.DragScale - 1.0f, (vehicle->GetAutoBoostState() == EAutoBoostState::Discharging) ? 1 : 0);

						AddText(*vehicle->GetPlayerName(false, false), FText::FromString(string));

						AddTextFloatAt(TEXT("DS"), vehicle->RaceState.DragScale - 1.0f, vehicle->GetCenterLocation(), -10.0f, 0.0f);
					}
				}
			}
		}
	}
}

#pragma endregion VehicleCatchup
