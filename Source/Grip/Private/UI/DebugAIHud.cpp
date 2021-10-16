/**
*
* AI debugging HUD.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
***********************************************************************************/

#include "ui/debugaihud.h"
#include "vehicle/flippablevehicle.h"

#pragma region AINavigation

#define SHOW_ENVIRONMENT_PROBES 0

/**
* Get a text string from a driving mode enumeration.
***********************************************************************************/

static char* GetDrivingMode(EVehicleAIDrivingMode mode)
{
	switch (mode)
	{
	case EVehicleAIDrivingMode::GeneralManeuvering:
		return "General Maneuvering";
	case EVehicleAIDrivingMode::RecoveringControl:
		return "Recovering Control";
	case EVehicleAIDrivingMode::ReversingToReorient:
		return "Reversing To Reorient";
	case EVehicleAIDrivingMode::ReversingFromBlockage:
		return "Reversing From Blockage";
	case EVehicleAIDrivingMode::LaunchToReorient:
		return "Launch To Reorient";
	case EVehicleAIDrivingMode::JTurnToReorient:
		return "J Turn To Reorient";
	}

	return "";
}

/**
* Draw the HUD.
***********************************************************************************/

void ADebugAIHUD::DrawHUD()
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

		AddInt(TEXT("Speed"), (int32)Vehicle->GetSpeedKPH());
		AddInt(TEXT("Optimum Speed"), (int32)Vehicle->GetAI().OptimumSpeed);
		AddInt(TEXT("Track Optimum Speed"), (int32)Vehicle->GetAI().TrackOptimumSpeed);
		AddText(TEXT("Driving Mode"), FText::FromString(GetDrivingMode(Vehicle->GetAI().DrivingMode)));
		AddInt(TEXT("Mode Distance"), (int32)Vehicle->GetAI().DistanceInDrivingMode());

#pragma region AIVehicleControl

		AddFloat(TEXT("Steering"), Vehicle->Control.SteeringPosition);
		AddFloat(TEXT("Throttle"), Vehicle->Control.ThrottleInput);
		AddFloat(TEXT("Brake"), Vehicle->Control.BrakePosition);
		AddBool(TEXT("Drifting"), Vehicle->IsDrifting());
		AddBool(TEXT("Fishtailing"), Vehicle->AI.Fishtailing);

#pragma endregion AIVehicleControl

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

		UPursuitSplineComponent* spline = Vehicle->GetAI().RouteFollower.ThisSpline.Get();

		if (spline != nullptr)
		{
			AddBool(TEXT("In Spline Space"), spline->IsWorldLocationWithinRange(Vehicle->GetAI().RouteFollower.ThisDistance, Vehicle->GetActorLocation()));

			AddRouteSpline(spline, Vehicle->GetAI().RouteFollower.ThisDistance, 250.0f * 100.0f, false);

			APlayerController* controller = Cast<APlayerController>(owningPawn->GetController());

			if (controller != nullptr)
			{
				FVector2D position;
				FVector location = Vehicle->GetAI().RouteFollower.NextSpline->GetWorldLocationAtDistanceAlongSpline(Vehicle->GetAI().RouteFollower.NextDistance);

				if (controller->ProjectWorldLocationToScreen(location, position))
				{
#if SHOW_ENVIRONMENT_PROBES
					TArray<float> clearances = Vehicle->GetAI().RouteFollower.NextSpline->GetClearances(Vehicle->GetAI().RouteFollower.NextDistance);
					FQuat rotation = Vehicle->GetAI().RouteFollower.NextSpline->GetQuaternionAtDistanceAlongSpline(Vehicle->GetAI().RouteFollower.NextDistance, ESplineCoordinateSpace::World);

					double time = fmod(FWindowsPlatformTime::Seconds(), 2.0) * 32.0;

					for (int32 i = 0; i < clearances.Num(); i++)
					{
						float angle = ((float)i / clearances.Num()) * PI * 2.0f;

						if (clearances[i] < 0.0f)
						{
							clearances[i] = 100.0f * 100.0f;
						}

						if (clearances[i] > 100.0f * 100.0f)
						{
							clearances[i] = 100.0f * 100.0f;
						}

						FVector offset(0.0f, FMath::Sin(angle), FMath::Cos(angle));
						FVector start = location;
						FVector end = location + (rotation.RotateVector(offset) * clearances[i]);

						if (i == 0)
						{
							AddLine(start, end, FLinearColor::Blue, 2.0f);
						}
						else
						{
							float ratio = (time < i) ? 0.0f : 1.0f - FMath::Clamp(((float)time - (float)i) * 0.1f, 0.0f, 1.0f);

							AddLine(start, end, FMath::Lerp(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f), FLinearColor::White, ratio), 2.0f);
						}
					}
#endif // SHOW_ENVIRONMENT_PROBES

					// Blue is the world location on the spline.
					DrawDebugSolidBox(Vehicle->GetWorld(), Vehicle->GetAI().SplineWorldLocation, FVector(50.0f, 50.0f, 50.0f), FColor::Blue);

					// Orange is the location that the vehicle is heading towards.
					DrawDebugSolidBox(Vehicle->GetWorld(), Vehicle->GetAI().HeadingTo, FVector(60.0f, 60.0f, 60.0f), FColor(255, 64, 0, 255));

					// Yellow is the spline location with weaving that the vehicle is heading towards.
					DrawDebugSolidBox(Vehicle->GetWorld(), Vehicle->GetAI().WeavingPosition, FVector(50.0f, 50.0f, 50.0f), FColor::Yellow, false, -1.0f, 1);

					// Magenta is the spline location that the vehicle is heading towards, with no weaving.
					FVector splinePosition = Vehicle->GetAI().RouteFollower.NextSpline->GetWorldLocationAtDistanceAlongSpline(Vehicle->GetAI().RouteFollower.NextDistance);
					DrawDebugSolidBox(Vehicle->GetWorld(), splinePosition, FVector(50.0f, 50.0f, 50.0f), FColor::Magenta);
				}
			}
		}
	}
}

#pragma endregion AINavigation
