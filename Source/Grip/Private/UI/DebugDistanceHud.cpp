/**
*
* Race distances debugging HUD.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
***********************************************************************************/

#include "ui/debugdistancehud.h"
#include "vehicle/flippablevehicle.h"

#pragma region VehicleRaceDistance

/**
* Draw the HUD.
***********************************************************************************/

void ADebugDistanceHUD::DrawHUD()
{
	Super::DrawHUD();

	HorizontalOffset = 200.0f;

	APawn* owningPawn = GetOwningPawn();
	ABaseVehicle* Vehicle = Cast<ABaseVehicle>(owningPawn);

	if (Vehicle != nullptr)
	{
		Vehicle = Vehicle->CameraTarget();
	}

	if (Vehicle != nullptr)
	{
		APlayGameMode* gameMode = APlayGameMode::Get(GetWorld());
		UGlobalGameState* gameState = UGlobalGameState::GetGlobalGameState(this);

		if (GRIP_POINTER_VALID(Vehicle->GetAI().RouteFollower.ThisSpline) == true)
		{
			AddText(TEXT("This Spline"), FText::FromString(Vehicle->GetAI().RouteFollower.ThisSpline->ActorName));
			AddInt(TEXT("This Spline Distance"), (int32)Vehicle->GetAI().RouteFollower.ThisDistance);
		}

		if (GRIP_POINTER_VALID(Vehicle->GetAI().RouteFollower.NextSpline) == true &&
			Vehicle->GetAI().RouteFollower.NextSpline != Vehicle->GetAI().RouteFollower.ThisSpline)
		{
			AddText(TEXT("Next Spline"), FText::FromString(Vehicle->GetAI().RouteFollower.NextSpline->ActorName));
			AddInt(TEXT("Next Spline Distance"), (int32)Vehicle->GetAI().RouteFollower.NextDistance);
			AddInt(TEXT("This Switch Distance"), (int32)Vehicle->GetAI().RouteFollower.ThisSwitchDistance);
			AddInt(TEXT("Next Switch Distance"), (int32)Vehicle->GetAI().RouteFollower.NextSwitchDistance);
		}

#pragma region VehicleRaceDistance

		if (gameState->IsGameModeRace() == true)
		{
			if (gameMode->MasterRacingSpline != nullptr)
			{
				AddText(TEXT("Master Spline"), FText::FromString(gameMode->MasterRacingSpline->ActorName));
				AddInt(TEXT("Master Spline Length"), (int32)gameMode->MasterRacingSplineLength);
				AddInt(TEXT("Master Spline Distance"), (int32)Vehicle->GetRaceState().DistanceAlongMasterRacingSpline);
				AddInt(TEXT("Master Spline Start Distance"), (int32)gameMode->MasterRacingSplineStartDistance);
			}

			AddInt(TEXT("Lap Number"), (int32)Vehicle->GetRaceState().EternalLapNumber + 1);
			AddInt(TEXT("Lap Distance"), (int32)Vehicle->GetRaceState().LapDistance);
			AddInt(TEXT("Race Distance"), (int32)Vehicle->GetRaceState().EternalRaceDistance);
			AddInt(TEXT("Race Position"), (int32)Vehicle->GetRaceState().RacePosition + 1);
			AddFloat(TEXT("Checkpoints Reached"), (float)Vehicle->GetRaceState().CheckpointsReached);
		}

#pragma endregion VehicleRaceDistance

	}
}

#pragma endregion VehicleRaceDistance
