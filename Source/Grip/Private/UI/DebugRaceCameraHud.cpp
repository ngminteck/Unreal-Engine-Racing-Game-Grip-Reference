/**
*
* Race camera debugging HUD.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
***********************************************************************************/

#include "ui/debugracecamerahud.h"
#include "vehicle/flippablevehicle.h"

#pragma region CameraCinematics

/**
* Handy macro to convert an enumeration to a string.
***********************************************************************************/

#define ENUM_TO_STRING(enumeration, value) case enumeration::value: return TEXT(#value)

/**
* Convert an ECinematicCameraMode to an FString.
***********************************************************************************/

static FString CinematicCameraModeAsString(ECinematicCameraMode mode)
{
	switch (mode)
	{
	default:
		return TEXT("Unknown");

		ENUM_TO_STRING(ECinematicCameraMode, Off);
		ENUM_TO_STRING(ECinematicCameraMode, SplineFollowingVehicle);
		ENUM_TO_STRING(ECinematicCameraMode, SplineFollowingVictimVehicle);
		ENUM_TO_STRING(ECinematicCameraMode, CameraPointVehicle);
		ENUM_TO_STRING(ECinematicCameraMode, CameraPointVehicleToProjectile);
		ENUM_TO_STRING(ECinematicCameraMode, CameraPointVehicleToGun);
		ENUM_TO_STRING(ECinematicCameraMode, CameraPointVehicleToVehicle);
		ENUM_TO_STRING(ECinematicCameraMode, StaticCamera);
		ENUM_TO_STRING(ECinematicCameraMode, SpiritWorld);
		ENUM_TO_STRING(ECinematicCameraMode, CustomOverride);
	}
}

/**
* Convert an ECameraStartTransition to an FString.
***********************************************************************************/

static FString CameraStartTransitionAsString(ECameraStartTransition transition)
{
	switch (transition)
	{
	default:
		return TEXT("Unknown");

		ENUM_TO_STRING(ECameraStartTransition, Random);
		ENUM_TO_STRING(ECameraStartTransition, None);
		ENUM_TO_STRING(ECameraStartTransition, Lower);
		ENUM_TO_STRING(ECameraStartTransition, SpeedUp);
		ENUM_TO_STRING(ECameraStartTransition, SlowUp);
		ENUM_TO_STRING(ECameraStartTransition, Rotate);
		ENUM_TO_STRING(ECameraStartTransition, CrossoverForwards);
		ENUM_TO_STRING(ECameraStartTransition, CrossoverBackwards);
	}
}

/**
* Convert an ECameraEndTransition to an FString.
***********************************************************************************/

static FString CameraEndTransitionAsString(ECameraEndTransition transition)
{
	switch (transition)
	{
	default:
		return TEXT("Unknown");

		ENUM_TO_STRING(ECameraEndTransition, Random);
		ENUM_TO_STRING(ECameraEndTransition, None);
		ENUM_TO_STRING(ECameraEndTransition, Raise);
		ENUM_TO_STRING(ECameraEndTransition, SpeedUp);
		ENUM_TO_STRING(ECameraEndTransition, SlowUp);
	}
}

/**
* Convert an ECameraViewDirection to an FString.
***********************************************************************************/

static FString CameraViewDirectionAsString(ECameraViewDirection direction)
{
	switch (direction)
	{
	default:
		return TEXT("Unknown");

		ENUM_TO_STRING(ECameraViewDirection, Random);
		ENUM_TO_STRING(ECameraViewDirection, Forwards);
		ENUM_TO_STRING(ECameraViewDirection, Backwards);
		ENUM_TO_STRING(ECameraViewDirection, Sideways);
		ENUM_TO_STRING(ECameraViewDirection, Overhead);
		ENUM_TO_STRING(ECameraViewDirection, Crossover);
	}
}

#undef ENUM_TO_STRING

#pragma endregion CameraCinematics

#pragma region VehicleCamera

/**
* Draw the HUD.
***********************************************************************************/

void ADebugRaceCameraHUD::DrawHUD()
{
	Super::DrawHUD();

	HorizontalOffset = 200.0f;

	APawn* owningPawn = GetOwningPawn();
	ABaseVehicle* vehicle = Cast<ABaseVehicle>(owningPawn);

	if (vehicle != nullptr)
	{
		URaceCameraComponent* camera = vehicle->Camera;

		vehicle = vehicle->CameraTarget();

#pragma region CameraCinematics

		if (camera->GetCinematicsDirector().IsActive() == true)
		{
			FCinematicsDirector& director = camera->GetCinematicsDirector();

			AddText(TEXT("CinematicCameraMode"), FText::FromString(CinematicCameraModeAsString(director.CinematicCameraMode)));
			AddInt(TEXT("VehicleIndex"), (int32)director.VehicleIndex);
			AddInt(TEXT("CameraIndex"), (int32)director.CameraIndex);

			if (director.SplineCamera.IsInUse() == true)
			{
				AddText(TEXT("StartTransition"), FText::FromString(CameraStartTransitionAsString(director.SplineCamera.StartTransition)));
				AddText(TEXT("EndTransition"), FText::FromString(CameraEndTransitionAsString(director.SplineCamera.EndTransition)));
				AddText(TEXT("ViewDirection"), FText::FromString(CameraViewDirectionAsString(director.SplineCamera.ViewDirection)));
				AddInt(TEXT("EasingDirection"), director.SplineCamera.EasingDirection);
				AddFloat(TEXT("EasingTime"), (director.SplineCamera.EasingDelta == 0.0f) ? 0.0f : 1.0f / director.SplineCamera.EasingDelta);
				AddFloat(TEXT("DistanceOffsetTime"), director.SplineCamera.DistanceOffsetTime);
				AddFloat(TEXT("CurrentDistanceOffset"), director.SplineCamera.CurrentDistanceOffset);
				AddFloat(TEXT("StartDistanceOffset"), director.SplineCamera.StartDistanceOffset);
				AddFloat(TEXT("EndDistanceOffset"), director.SplineCamera.EndDistanceOffset);
				AddFloat(TEXT("LongitudinalDistance"), director.SplineCamera.LongitudinalDistanceFromTarget);
				AddVector(TEXT("LastSplineOffsetL"), director.SplineCamera.LastSplineOffset[0]);
				AddFloat(TEXT("LastSplineLengthA"), director.SplineCamera.LastSplineOffset[1].Size());
				AddFloat(TEXT("OffsetFromGround"), director.SplineCamera.OffsetFromGround[0]);
				AddFloat(TEXT("HeightAboveGround"), director.SplineCamera.HeightAboveGround);
				AddFloat(TEXT("DistanceAboveSpline"), director.SplineCamera.DistanceAboveSpline);
				AddFloat(TEXT("DistanceAlongSpline"), director.SplineCamera.DistanceAlongSpline);
				AddFloat(TEXT("ProjectedDistanceAlong"), director.SplineCamera.ProjectedDistanceAlongSpline);
				AddVector(TEXT("TargetLocation"), director.SplineCamera.Target->GetCenterLocation());
			}
		}
		else

#pragma endregion CameraCinematics

		{
			AddBool(TEXT("IsFlipped"), vehicle->IsFlipped());
			AddBool(TEXT("IsFlippedAndWheelsOnGround"), vehicle->IsFlippedAndWheelsOnGround());
			AddInt(TEXT("FlipDetection"), vehicle->GetWheels().FlipDetection);
			AddBool(TEXT("IsAirborne"), vehicle->IsAirborne(false));

#pragma region VehicleSpringArm

			UFlippableSpringArmComponent* arm = vehicle->SpringArm;

			AddBool(TEXT("HasSmashedIntoSomething"), vehicle->HasSmashedIntoSomething(150.0f));
			AddBool(TEXT("ArmAirborne"), arm->Airborne);
			AddInt(TEXT("FromFollowingMode"), (int32)arm->FromFollowingMode);
			AddInt(TEXT("FollowingMode"), (int32)arm->FollowingMode);
			AddFloat(TEXT("NoAirborneContactTime"), arm->NoAirborneContactTime);
			AddFloat(TEXT("FollowingModeTime"), arm->FollowingModeTime);
			AddFloat(TEXT("ThisModeTransitionTime"), arm->ThisModeTransitionTime);
			AddFloat(TEXT("GetFollowingTransitionRatio"), arm->GetFollowingTransitionRatio());
			const FTransform& vehicleTransform = vehicle->VehicleMesh->GetComponentTransform();
			FRotator r0 = vehicleTransform.Rotator();
			AddVector(TEXT("VehicleRotation"), FVector(r0.Yaw, r0.Pitch, r0.Roll));
			r0 = arm->TransitionRotations[(int32)arm->FromFollowingMode][0];
			AddVector(TEXT("FromRotation"), FVector(r0.Yaw, r0.Pitch, r0.Roll));
			r0 = arm->TransitionRotations[(int32)arm->FollowingMode][0];
			AddVector(TEXT("ToRotation"), FVector(r0.Yaw, r0.Pitch, r0.Roll));
			r0 = arm->Rotations[(int32)UFlippableSpringArmComponent::EFollowingMode::Normal];
			AddVector(TEXT("NormalRotation"), FVector(r0.Yaw, r0.Pitch, r0.Roll));
			r0 = arm->Rotations[(int32)UFlippableSpringArmComponent::EFollowingMode::Airborne];
			AddVector(TEXT("AirborneRotation"), FVector(r0.Yaw, r0.Pitch, r0.Roll));
			r0 = arm->Rotations[(int32)UFlippableSpringArmComponent::EFollowingMode::Crashed];
			AddVector(TEXT("CrashedRotation"), FVector(r0.Yaw, r0.Pitch, r0.Roll));
			r0 = arm->Rotations[(int32)arm->FollowingMode];
			AddVector(TEXT("SelectedRotation"), FVector(r0.Yaw, r0.Pitch, r0.Roll));
			r0 = arm->SmoothedRotations[(int32)arm->FollowingMode];
			AddVector(TEXT("SmoothedRotation"), FVector(r0.Yaw, r0.Pitch, r0.Roll));
			r0 = arm->TargetRotation;
			AddVector(TEXT("TargetRotation"), FVector(r0.Yaw, r0.Pitch, r0.Roll));
			AddFloat(TEXT("LastClippingDistance"), arm->LastClippingDistance);
			AddFloat(TEXT("LaunchDirectionFlipTime"), arm->LaunchDirectionFlipTime);
			AddFloat(TEXT("AirToGroundTime"), arm->AirToGroundTime);
			AddFloat(TEXT("NativeFieldOfView"), camera->NativeFieldOfView);
			AddVector(TEXT("ArmRoot"), arm->ArmRoot);
			AddInt(TEXT("ArmRootMode"), (int32)arm->ArmRootMode);

#pragma endregion VehicleSpringArm

			AddLine(vehicle->GetCenterLocation(), vehicle->GetCenterLocation() + vehicle->GetActorRotation().RotateVector(FVector(0.0f, 0.0f, (vehicle->IsFlipped() == true) ? 5.0f : -5.0f) * 33.0f), FLinearColor(1.0f, 0.0f, 0.0f), 6.0f);
			AddLine(vehicle->GetCenterLocation(), vehicle->GetCenterLocation() + vehicle->GetActorRotation().RotateVector(FVector(0.0f, 0.0f, (vehicle->IsFlippedAndWheelsOnGround() == true) ? 5.0f : -5.0f) * 33.0f), FLinearColor(0.0f, 1.0f, 0.0f), 2.0f);

			int32 index = 0;

			for (const FVehicleWheel& wheel : vehicle->GetWheels().Wheels)
			{
				bool inContact = wheel.GetActiveSensor().IsInContact();
				bool inEffect = wheel.GetActiveSensor().IsInEffect();
				bool inPossibleContact = wheel.GetActiveSensor().HasNearestContactPoint(wheel.Velocity, 2.0f);

				AddBox(vehicle->GetWheelBoneLocationFromIndex(index), FMath::Lerp(FLinearColor::Red, FLinearColor::Green, (inEffect == true) ? 1.0f : 0.0f), 5.0f);
				AddBox(vehicle->GetWheelBoneLocationFromIndex(index), FMath::Lerp(FLinearColor::Red, FLinearColor::Green, (inContact == true) ? 1.0f : 0.0f), 15.0f);
				AddBox(vehicle->GetWheelBoneLocationFromIndex(index), FMath::Lerp(FLinearColor::Red, FLinearColor::Green, (inPossibleContact == true) ? 1.0f : 0.0f), 25.0f);

				for (const FVehicleContactSensor& sensor : wheel.Sensors)
				{
					if (sensor.HasNearestContactPoint(wheel.Velocity, 0.0f) == true)
					{
						AddLine(vehicle->GetWheelBoneLocationFromIndex(index), sensor.GetNearestContactPoint(), FMath::Lerp(FLinearColor::Red, FLinearColor::Green, (sensor.IsInEffect() == true) ? 1.0f : 0.0f), 2.0f);
						AddBox(sensor.GetNearestContactPoint(), FMath::Lerp(FLinearColor::Red, FLinearColor::Green, (sensor.IsInEffect() == true) ? 1.0f : 0.0f), 5.0f);
					}
				}

				index++;
			}
		}
	}
}

#pragma endregion VehicleCamera
