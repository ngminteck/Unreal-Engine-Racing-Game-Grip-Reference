/**
*
* Missile debugging HUD.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
***********************************************************************************/

#include "ui/debugmissilehud.h"
#include "vehicle/flippablevehicle.h"
#include "pickups/homingmissile.h"

#pragma region PickupMissile

/**
* Draw the HUD.
***********************************************************************************/

void ADebugMissileHUD::DrawHUD()
{
	Super::DrawHUD();

	// Get our vehicle so we can check if we are in car. If we are we don't want on-screen HUD
	AActor* owningPawn = GetOwningPawn();
	ABaseVehicle* Vehicle = Cast<ABaseVehicle>(owningPawn);

	if (Vehicle != nullptr)
	{
		Vehicle = Vehicle->CameraTarget();
	}

	if (Vehicle != nullptr &&
		GRIP_POINTER_VALID(Vehicle->GetHomingMissile()) == true)
	{
		AHomingMissile* missile = Vehicle->GetHomingMissile().Get();
		float overDistance = missile->MissileMovement->Velocity.Size() * 3.0f;
		float distance = overDistance;

		AddFloat(TEXT("Speed kph"), FMathEx::CentimetersPerSecondToKilometersPerHour(missile->MissileMovement->Velocity.Size()));
		AddFloat(TEXT("Time to target"), missile->MissileMovement->GetTimeToTarget());
		AddFloat(TEXT("Following height m"), missile->MissileMovement->TerrainAvoidanceHeight / 100.0f);
		AddFloat(TEXT("Target Speed"), missile->MissileMovement->TargetSpeed);
		AddBool(TEXT("Lock lost"), missile->MissileMovement->LockLost);
		AddBool(TEXT("In range of target"), missile->InRangeOfTarget);
		AddBool(TEXT("Target within reach"), missile->TargetWithinReach);
		AddBool(TEXT("Target hit"), missile->TargetHit);
		AddBool(TEXT("Avoiding terrain"), missile->MissileMovement->TerrainAimLocation != missile->MissileMovement->GetTargetLocation());
		AddBool(TEXT("Arresting turn"), missile->MissileMovement->ArrestingTurn);

		if (missile->IsHoming() == true)
		{
			FMinimalViewInfo view;

			Vehicle->Camera->GetCameraViewNoPostProcessing(0.0f, view);

			AddBox(missile->MissileMovement->GetTargetLocation(), FLinearColor::Green);
			AddBox(missile->MissileMovement->TerrainAimLocation, FLinearColor::Yellow);
			AddBox(missile->GetActorLocation() + (missile->MissileMovement->TargetDirection * 1000.0f), FLinearColor::Red);
			AddBox(missile->GetActorLocation() + (missile->MissileMovement->TerrainAimDirection * 1000.0f), FLinearColor::Blue);
		}
	}
}

#pragma endregion PickupMissile
