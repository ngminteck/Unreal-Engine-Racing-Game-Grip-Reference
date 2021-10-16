/**
*
* Vehicle debugging HUD.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
***********************************************************************************/

#include "ui/debugvehiclehud.h"
#include "vehicle/flippablevehicle.h"
#include "physicsengine/physicssettings.h"

#pragma region VehicleContactSensors

/**
* Draw the HUD.
***********************************************************************************/

void ADebugVehicleHUD::DrawHUD()
{
	Super::DrawHUD();

	HorizontalOffset = 200.0f;

	APawn* owningPawn = GetOwningPawn();
	ABaseVehicle* vehicle = Cast<ABaseVehicle>(owningPawn);

	if (vehicle != nullptr)
	{
		vehicle = vehicle->CameraTarget();
	}

	if (vehicle != nullptr)
	{
		AddBool(TEXT("IsFlipped"), vehicle->IsFlipped());
		AddBool(TEXT("IsFlippedAndWheelsOnGround"), vehicle->IsFlippedAndWheelsOnGround());
		AddBool(TEXT("IsPracticallyGrounded"), vehicle->IsPracticallyGrounded());
		AddFloat(TEXT("ContactData.ModeTime"), vehicle->Physics.ContactData.ModeTime);
		AddFloat(TEXT("GetSurfaceDistance"), FMath::Max(0.0f, vehicle->GetSurfaceDistance(false) - vehicle->GetMaxWheelRadius()));
		AddText(TEXT("GetSurfaceName"), FText::FromName(vehicle->GetSurfaceName()));
		AddFloat(TEXT("GetSpeedKPH"), vehicle->GetSpeedKPH());

#pragma region VehicleBasicForces

		AddInt(TEXT("GetJetEnginePower"), (int32)vehicle->GetJetEnginePower(vehicle->Wheels.NumWheelsInContact, vehicle->GetDirection()));
		AddInt(TEXT("GetDragForce"), (int32)vehicle->GetDragForce().Size());
		AddInt(TEXT("GetRollingResistance"), (int32)vehicle->GetRollingResistanceForce(vehicle->GetFacingDirection()).Size());
		AddFloat(TEXT("GetDownForce"), (vehicle->GetDownForce().Size() / vehicle->Physics.GravityStrength));
		AddFloat(TEXT("AutoBrakePosition"), vehicle->AutoBrakePosition(vehicle->GetFacingDirection()));

#pragma endregion VehicleBasicForces

#pragma region VehicleDrifting

		AddInt(TEXT("DriftingActive"), (int32)vehicle->Physics.Drifting.Active);
		AddInt(TEXT("RearDriftAngle"), (int32)vehicle->Physics.Drifting.RearDriftAngle);

#pragma endregion VehicleDrifting

#pragma region VehiclePhysicsTweaks

#if GRIP_ANTI_SKYWARD_LAUNCH
		AddFloat(TEXT("VelocityPitchMitigationForce"), vehicle->Physics.VelocityPitchMitigationForce);
#endif GRIP_ANTI_SKYWARD_LAUNCH

#pragma endregion VehiclePhysicsTweaks

		if (vehicle->Physics.Timing.TickCount > 0)
		{
			AddFloat(TEXT("General Clock"), vehicle->Physics.Timing.GeneralTickSum);
			AddFloat(TEXT("Physics Ticks Per Tick"), (float)vehicle->Physics.Timing.TickCount / (float)vehicle->Physics.Timing.GeneralTickCount);
			AddFloat(TEXT("Actual Tick Rate"), 1.0f / (vehicle->Physics.Timing.TickSum / (float)vehicle->Physics.Timing.TickCount));
			AddFloat(TEXT("Requested Tick Rate"), 1.0f / UPhysicsSettings::Get()->MaxSubstepDeltaTime);
		}

		AddBox(vehicle->GetActorLocation(), (vehicle->IsGrounded() == true) ? FLinearColor::Green : FLinearColor::Red);
		AddLine(vehicle->GetActorLocation(), vehicle->GetActorLocation() + vehicle->GetVelocityDirection() * 100.0f, (vehicle->IsGrounded() == true) ? FLinearColor::Green : FLinearColor::Red);

		// Show the suspension properties.

		if (vehicle->GetNumWheels() >= 4)
		{
			int32 index = 0;

			for (FVehicleWheel& wheel : vehicle->Wheels.Wheels)
			{
				const FTransform& transform = vehicle->GetPhysicsTransform();
				FVector wheelSpringPosition = vehicle->GetWheelBoneLocation(wheel, transform);
				FVector wheelPosition = vehicle->GetWheelBoneLocationFromIndex(index);

				float gripRatio = vehicle->GetGripRatio(wheel.GetActiveSensor());
				bool inContact = wheel.GetActiveSensor().IsInContact();
				float ratio = (inContact == true) ? FMath::Min(1.0f, gripRatio) : 0.0f;

				AddBox(vehicle->GetWheelBoneLocationFromIndex(index), FMath::Lerp(FLinearColor::Red, FLinearColor::Green, ratio));

#pragma region VehicleGrip

				FVector velocityDirection = vehicle->GetHorizontalVelocity(wheel);

				velocityDirection.Normalize();

				AddLine(vehicle->GetWheelBoneLocationFromIndex(index), wheelPosition + velocityDirection * 100.0f, FMath::Lerp(FLinearColor::Red, FLinearColor::Green, ratio));

#pragma endregion VehicleGrip

				FVector side = transform.TransformVector(FVector(0.0f, wheel.GetActiveSensor().GetSweepWidth(), 0.0f));

				AddLine(vehicle->GetWheelBoneLocationFromIndex(index), wheelPosition - side, FMath::Lerp(FLinearColor::Red, FLinearColor::Green, ratio));
				AddLine(vehicle->GetWheelBoneLocationFromIndex(index), wheelPosition + side, FMath::Lerp(FLinearColor::Red, FLinearColor::Green, ratio));

#pragma region VehicleAntiGravity

				if (vehicle->Antigravity == true)
				{
					AddTextFloatAt(TEXT("LF"), wheel.LateralForceStrength, wheelPosition, -10.0f, -12.0f);
					AddTextFloatAt(TEXT("NC"), wheel.GetActiveSensor().GetNormalizedCompression(), wheelPosition, -10.0f, -24.0f);
					AddTextFloatAt(TEXT("AC"), wheel.GetActiveSensor().GetAntigravityNormalizedCompression(), wheelPosition, -10.0f, -36.0f);

					for (FVehicleContactSensor& sensor : wheel.Sensors)
					{
						FVector springDirection = sensor.GetDirection();

						AddLine(vehicle->GetWheelBoneLocationFromIndex(index), vehicle->GetWheelBoneLocationFromIndex(index) + (springDirection * sensor.ForceApplied * 0.05f), FLinearColor(1.0f, 0.5f, 0.0f));

						sensor.ForceApplied = 0.0f;
					}
				}
				else

#pragma endregion VehicleAntiGravity

				{

#pragma region VehicleGrip

					AddTextFloatAt(TEXT("GR"), gripRatio, wheelPosition, -10.0f, -12.0f);

#pragma endregion VehicleGrip

					AddTextFloatAt(TEXT("CO"), wheel.GetActiveSensor().GetCompression(), wheelPosition, -10.0f, -24.0f);
					AddTextFloatAt(TEXT("NC"), wheel.GetActiveSensor().GetNormalizedCompression(), wheelPosition, -10.0f, -36.0f);

					float surfaceDistance = -transform.InverseTransformVector((wheel.GetActiveSensor().GetEndPoint() - wheelSpringPosition)).Z;

					if (vehicle->IsFlipped() == true)
					{
						surfaceDistance *= -1.0f;
					}

					AddTextIntAt(TEXT("SD"), (int32)(wheel.Radius - surfaceDistance), wheelPosition, -10.0f, 0.0f);

					for (FVehicleContactSensor& sensor : wheel.Sensors)
					{
						FVector springDirection = sensor.GetDirection();

						AddLine(vehicle->GetWheelBoneLocationFromIndex(index), vehicle->GetWheelBoneLocationFromIndex(index) + (springDirection * sensor.ForceApplied * 0.05f), FLinearColor(1.0f, 0.5f, 0.0f));

						if (sensor.ForceApplied != 0.0f)
						{
							AddTextIntAt(TEXT("FS"), (int32)(sensor.ForceApplied), vehicle->GetWheelBoneLocationFromIndex(index), -10.0f, -48.0f);
						}

						sensor.ForceApplied = 0.0f;
					}
				}

				index++;
			}
		}

#pragma region AIVehicleControl

		// Show the collision contacts.

		int32 index = 0;

		for (FVector& position : vehicle->ContactPoints[1])
		{
			FVector startPoint = vehicle->VehicleMesh->GetComponentTransform().TransformPosition(position);

			AddBox(startPoint, FLinearColor(1.0f, 0.0f, 1.0f), 10.0f);

			FVector force = vehicle->ContactForces[1][index] * 0.0001f;
			float forceReport = force.Size();

			if (force.Size() > 5.0f * 100.0f)
			{
				force.Normalize();
				force *= 5.0f * 100.0f;
			}

			FVector endPoint = startPoint + force;

			AddLine(startPoint, endPoint, FLinearColor(1.0f, 0.0f, 1.0f));

			FVector from = vehicle->Camera->GetComponentTransform().InverseTransformPositionNoScale(startPoint);
			FVector to = vehicle->Camera->GetComponentTransform().InverseTransformPositionNoScale(endPoint);
			float scale = 1.0f;

			if (from.X > to.X)
			{
				scale = FMath::Lerp(10.0f, 30.0f, FMath::Min(((from.X - to.X) / (5.0f * 100.0f)), 1.0f));
			}
			else
			{
				scale = FMath::Lerp(10.0f, 2.0f, FMath::Min(((to.X - from.X) / (5.0f * 100.0f)), 1.0f));
			}

			AddBox(endPoint, FLinearColor(0.5f, 0.0f, 1.0f), scale);

			AddTextIntAt(TEXT("FS"), (int32)(forceReport), startPoint, -10.0f, 0.0f);

			index++;
		}

#pragma endregion AIVehicleControl

	}
}

#pragma endregion VehicleContactSensors
