/**
*
* Pickups debugging HUD.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
***********************************************************************************/

#include "ui/debugpickupshud.h"
#include "vehicle/flippablevehicle.h"

#pragma region VehiclePickups

/**
* Draw a pickup slot on the HUD.
***********************************************************************************/

void ADebugPickupsHUD::DrawSlot(int32 slotIndex, FPlayerPickupSlot& slot, ABaseVehicle* vehicle)
{
	AActor* target = nullptr;

	AddFloat(TEXT("Timer"), slot.Timer);
	AddFloat(TEXT("EfficacyTimer"), slot.EfficacyTimer);
	AddFloat(TEXT("UseAfter"), slot.UseAfter);
	AddFloat(TEXT("UseBefore"), slot.UseBefore);
	AddFloat(TEXT("DumpAfter"), slot.DumpAfter);

#pragma region BotCombatTraining

	AddFloat(TEXT("Weight"), vehicle->GetPickupEfficacyWeighting(slotIndex, target));

#pragma endregion BotCombatTraining

	float efficacyTime = APickup::GetEfficacyDelayBeforeUse(slot.Type, vehicle);

	AddFloat(TEXT("Efficacy"), (efficacyTime > 0.0f) ? slot.EfficacyTimer / efficacyTime : 1.0f);

	ABaseVehicle* targetVehicle = Cast<ABaseVehicle>(target);

	if (slot.Type == EPickupType::GatlingGun ||
		slot.Type == EPickupType::HomingMissile)
	{
		APlayGameMode* gameMode = APlayGameMode::Get(this);

		if (targetVehicle == nullptr)
		{
			targetVehicle = Cast<ABaseVehicle>(vehicle->HUD.GetCurrentMissileTargetActor(slotIndex));
		}

		if (targetVehicle != nullptr)
		{
			AddBool(TEXT("IsGoodForSmacking"), targetVehicle->IsGoodForSmacking());
			AddBool(TEXT("CanBeAttacked"), vehicle->IsAIVehicle() == false || targetVehicle->CanBeAttacked() == true);
			AddBool(TEXT("BotWillTargetHuman"), slot.BotWillTargetHuman == false || targetVehicle->IsAIVehicle() == false);

			float aggressionRatio = gameMode->VehicleShouldFightVehicle(vehicle, targetVehicle);

			AddFloat(TEXT("AggressionRatio"), aggressionRatio);

			float thisWeight = 1.0f;
			FVector fromPosition = vehicle->GetActorLocation();
			FVector fromDirection = vehicle->GetTransform().GetUnitAxis(EAxis::X);
			FVector targetPosition = targetVehicle->GetTargetBullsEye();

#pragma region PickupGun

			if (slot.Type == EPickupType::GatlingGun)
			{
				float spread = (vehicle->Level1GatlingGunBlueprint->GetDefaultObject<AGatlingGun>()->AutoAiming * 0.05f);

				thisWeight = FMathEx::TargetWeight(fromPosition, fromDirection, targetPosition, 5.0f * 100.0f, 250.0f * 100.0f, 1.0f - spread, true);

				AddFloat(TEXT("TargetWeight"), 1.0f - thisWeight);
			}

#pragma endregion PickupGun

			if (slot.Type == EPickupType::HomingMissile)
			{
				thisWeight = FMathEx::TargetWeight(fromPosition, fromDirection, targetPosition, 50.0f * 100.0f, 750.0f * 100.0f, 0.75f, true);

				AddFloat(TEXT("TargetWeight"), 1.0f - thisWeight);

				AddBool(TEXT("Good Launch Condition"), AHomingMissile::GoodLaunchCondition(vehicle));
			}

			AddFloat(TEXT("ScaleOffensive"), gameMode->ScaleOffensivePickupWeight(vehicle->HasAIDriver(), thisWeight, &slot, aggressionRatio));
		}
	}
}

/**
* Draw the HUD.
***********************************************************************************/

void ADebugPickupsHUD::DrawHUD()
{
	Super::DrawHUD();

	AActor* owningPawn = GetOwningPawn();
	ABaseVehicle* vehicle = Cast<ABaseVehicle>(owningPawn);

	if (vehicle != nullptr)
	{
		vehicle = vehicle->CameraTarget();
	}

	if (vehicle != nullptr)
	{
		FPlayerPickupSlot& slot1 = vehicle->PickupSlots[0];
		FPlayerPickupSlot& slot2 = vehicle->PickupSlots[1];

		if (slot1.State != EPickupSlotState::Empty)
		{
			AddBool(TEXT("Slot 1 humans only"), slot1.BotWillTargetHuman);

			DrawSlot(0, slot1, vehicle);

			Y += LineHeight;
		}

		if (slot2.State != EPickupSlotState::Empty)
		{
			AddBool(TEXT("Slot 2 humans only"), slot2.BotWillTargetHuman);

			DrawSlot(1, slot2, vehicle);

			Y += LineHeight;
		}

		AddBool(TEXT("Turbo obstacles"), vehicle->GetAI().TurboObstacles);
		AddBool(TEXT("IsGrounded"), vehicle->IsGrounded());
		AddBool(TEXT("IsPracticallyGrounded"), vehicle->IsPracticallyGrounded());
		AddFloat(TEXT("GroundedTime"), vehicle->GroundedTime(2.0f));
		AddFloat(TEXT("GetModeTime"), vehicle->GetModeTime());
		AddFloat(TEXT("SteeringPosition"), vehicle->GetVehicleControl().SteeringPosition);
		AddFloat(TEXT("OptimumSpeed"), vehicle->GetAI().OptimumSpeed);

		Y += LineHeight;

		float timeAhead = 2.0f;
		FRotator rotation = vehicle->GetActorRotation();
		FQuat quaternion = rotation.Quaternion();
		int32 direction = vehicle->GetPursuitSplineDirection();
		float distanceAhead = timeAhead * FMathEx::KilometersPerHourToCentimetersPerSecond(vehicle->GetSpeedKPH() + 200.0f);
		FRotator splineDegrees = vehicle->GetAI().RouteFollower.GetCurvatureOverDistance(vehicle->GetAI().RouteFollower.ThisDistance, distanceAhead, direction, quaternion, true);
		float tunnelDiameter = vehicle->GetAI().RouteFollower.GetTunnelDiameterOverDistance(vehicle->GetAI().RouteFollower.ThisDistance, FMath::Max(vehicle->GetSpeedMPS() * timeAhead, 10.0f) * 100.0f, direction, false);

		splineDegrees *= 1.0f / timeAhead;

		AddInt(TEXT("TunnelDiameter"), (int32)(tunnelDiameter / 100.0f));
		AddRotator(TEXT("SplineDegrees"), splineDegrees);

#pragma region PickupMissile

		if (vehicle->GetAI().RouteFollower.IsValid() == true)
		{
			FVector location = vehicle->GetActorLocation();
			FVector up = vehicle->GetAI().RouteFollower.ThisSpline->WorldSpaceToSplineSpace(vehicle->GetLaunchDirection(), vehicle->GetAI().RouteFollower.ThisDistance, false);
			float overDistance = distanceAhead;
			float clearanceUp = vehicle->GetAI().RouteFollower.GetClearanceOverDistance(vehicle->GetAI().RouteFollower.ThisDistance, overDistance, direction, location, up, 45.0f);

			AddInt(TEXT("Clearance"), (int32)(clearanceUp / 100.0f));
		}

#pragma endregion PickupMissile

	}
}

#pragma endregion VehiclePickups
