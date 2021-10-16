/**
*
* Cinematics director.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* The code that drives the cinematic camera sequencing at the end of a race and
* during the attract mode for the game.
*
***********************************************************************************/

#include "camera/cinematicsdirector.h"

#pragma region CameraCinematics

#include "pickups/homingmissile.h"
#include "vehicle/flippablevehicle.h"
#include "ai/pursuitsplineactor.h"
#include "gamemodes/playgamemode.h"

/**
* Construct an FCinematicsDirector.
***********************************************************************************/

FCinematicsDirector::FCinematicsDirector(UAdvancedCameraComponent* camera)
	: Owner(camera->GetAttachmentRootActor())
	, Camera(camera)
	, VisibilityQueryParams("CameraVisibilityClipping", true)
{
	for (float& time : LastViewTimes)
	{
		time = 0.0f;
	}
}

/**
* Do the regular update tick.
***********************************************************************************/

void FCinematicsDirector::Tick(float deltaSeconds)
{
	float clock = Owner->GetWorld()->GetRealTimeSeconds();

	if (LastClock != 0.0f)
	{
		deltaSeconds = clock - LastClock;
	}

	LastClock = clock;
	LastViewTimes[(int32)CinematicCameraMode] = clock;

	VehicleTimer += deltaSeconds;
	CameraModeTimer += deltaSeconds;
	CameraShotTimer += deltaSeconds;

	ECinematicCameraMode mode;

	do
	{
		// Examine the current camera mode and switch to another if desired.

		mode = CinematicCameraMode;

		switch (CinematicCameraMode)
		{
		case ECinematicCameraMode::SpiritWorld:
			if (GRIP_POINTER_VALID(StaticCamera) == true &&
				CameraModeTimer >= StaticCamera->Duration)
			{
				UseSpiritCamera(false, true, FVector::ZeroVector);

				SwitchToVehicleCameraPoint();
			}
			break;

		case ECinematicCameraMode::StaticCamera:

			if (StaticCamera->HasCameraJustBeenHit() == true)
			{
				ABaseVehicle* vehicle = Cast<ABaseVehicle>(Owner);

				if (vehicle != nullptr)
				{
					bool disconnectCamera = (FMath::Rand() & 3) == 0;

					if (disconnectCamera == true)
					{
						CameraModeTimer = StaticCamera->Duration - FMath::FRandRange(1.5f, 2.0f);

						UseSpiritCamera(true, true, StaticCamera->GetCameraHitVelocity());
					}
					else
					{
						CameraModeTimer = StaticCamera->Duration - FMath::FRandRange(1.0f, 2.0f);
					}

					vehicle->Camera->CameraFeed.Initiate(5.0f, 0.0f, (FMath::Rand() & 3) == 0);
				}
			}

			AdjustedYaw = FMathEx::GravitateToTarget(AdjustedYaw, StaticCamera->GetAdjustedYaw(), deltaSeconds * 250.0f);

			if (CameraModeTimer >= StaticCamera->Duration)
			{
				SwitchToVehicleCameraPoint();
			}
			else if (CameraShotTimer > MinCameraDuration)
			{
				IdentifyCameraAction(true, false, true);
			}
			break;

		case ECinematicCameraMode::SplineFollowingVehicle:
		case ECinematicCameraMode::SplineFollowingVictimVehicle:
		{
			SplineCamera.Tick(deltaSeconds, false);

			if (SplineCamera.ViewDirection == ECameraViewDirection::Overhead)
			{
				LastOverheadView = LastClock;
			}

#pragma region VehicleTeleport

			if (SplineCamera.IsInUse() == false ||
				SplineCamera.Target.Get()->IsTeleporting() == true)
			{
				if (clock - LastViewTimes[(int32)ECinematicCameraMode::StaticCamera] < 10.0f ||
					IdentifyStaticCamera() == false)
				{
					SwitchToVehicleCameraPoint();
				}
			}
			else

#pragma endregion VehicleTeleport

			{
				// If we're in a spline-following view already, see if we can kick it into a victim
				// view and get a nice slomo of the action if possible.

				if (CinematicCameraMode == ECinematicCameraMode::SplineFollowingVehicle &&
					SplineCamera.IsEasingOut() == false &&
					SplineCamera.GetAngleToTarget() < 20.0f)
				{
					float maxImpactTime = 1.0f;
					float aboutToImpact = IdentifyImpactEvent(SplineCamera.Target.Get(), ImpactingActor, maxImpactTime);

					if (aboutToImpact != 0.0f &&
						aboutToImpact <= maxImpactTime)
					{
						if (GRIP_POINTER_VALID(ImpactingActor) == true)
						{
							CinematicCameraMode = ECinematicCameraMode::SplineFollowingVictimVehicle;
						}
					}
				}

				if (CinematicCameraMode == ECinematicCameraMode::SplineFollowingVictimVehicle &&
					CanSlowTime(false) == true &&
					SplineCamera.IsEasingOut() == false)
				{
					AHomingMissile* missile = Cast<AHomingMissile>(ImpactingActor.Get());

					if (missile != nullptr)
					{
						if (missile->HasExploded() == false)
						{
							float timeToTarget = missile->GetTimeToTarget();

							if (timeToTarget > 0.75f &&
								timeToTarget < 1.0f &&
								missile->IsLikelyToHitTarget() == true &&
								SplineCamera.GetAngleToTarget() < 20.0f &&
								SplineCamera.GetTimeLeft() > 2.0f)
							{
								TimeSlowed = true;

								APlayGameMode::Get(Owner)->ChangeTimeDilation(0.25f, 1.0f);

								// Give the view a few seconds more.

								SplineCamera.SetEndTime(8.0f, 0.25f);
							}
						}
					}
				}

				if (TimeSlowed == false &&
					CinematicCameraMode == ECinematicCameraMode::SplineFollowingVehicle &&
					SplineCamera.IsInterruptable() == true)
				{
					IdentifyCameraAction(true, false, true);
				}
			}

			break;
		}

		case ECinematicCameraMode::CameraPointVehicleToVehicle:
		{
			if ((GRIP_POINTER_VALID(CameraTarget) == false) ||
				(CurrentCameraPoint->Reposition(false, true) == true))
			{
				if (IdentifyCameraAction(false, false) == false)
				{
					SwitchMode(ECinematicCameraMode::CameraPointVehicle);

					IdentifyCameraPoint(false);
				}
			}
			else
			{
				const FTransform& transform = CurrentCameraPoint->GetComponentTransform();
				FVector fromLocation = transform.GetLocation();
				FVector targetLocation = GetCameraTargetLocation(fromLocation);
				FVector toTarget = targetLocation - fromLocation;
				float distance = toTarget.Size();
				FRotator lastRotation = ViewRotation;
				FRotator targetRotation = toTarget.ToOrientationRotator();
				float lag = FMath::Lerp(0.9f, 0.96f, FMathEx::GetRatio(DynamicFOV.FieldOfView, 35.0f, 50.0f));

				toTarget.Normalize();

				LastRotation = FMathEx::GetSmoothedRotation(LastRotation, targetRotation, deltaSeconds, lag, lag, lag);

				FVector forward = LastRotation.Vector();
				FVector up = transform.GetUnitAxis(EAxis::Z);

				if (FMathEx::GetRotationFromForwardUp(forward, up, ViewRotation) == false)
				{
					ViewRotation = lastRotation;
				}

				float angleAway = FVector::DotProduct(CurrentVehicle->GetUpDirection() * (CurrentCameraPoint->IsFlipped() == true ? -1.0f : 1.0f), toTarget);

				// Check to see if the target is visible and stop watching them after a short time if they're not.

				FHitResult hit;
				FVector testPosition = targetLocation + (Cast<ABaseVehicle>(CameraTarget)->GetLaunchDirection() * 2.0f * 100.0f);

				if (CameraTarget->GetWorld()->LineTraceSingleByChannel(hit, fromLocation, testPosition, ABaseGameMode::ECC_LineOfSightTest, VisibilityQueryParams) == false)
				{
					TargetHiddenTime = 0.0f;
				}
				else
				{
					TargetHiddenTime += deltaSeconds;
				}

				if (CameraModeTimer >= CameraDuration ||
					TargetHiddenTime > 1.5f ||
					((angleAway < -0.5f || distance > 50.0f * 100.0f) && CameraShotTimer > MinCameraDuration))
				{
					if (IdentifyCameraAction(false, false) == false)
					{
						SwitchMode(ECinematicCameraMode::CameraPointVehicle);

						IdentifyCameraPoint(false);
					}
				}
				else if (CameraShotTimer < 1.0f ||
					IdentifyWeaponLaunches() == false)
				{
					if (CameraShotTimer > MinCameraDuration)
					{
						IdentifyCameraAction(true, false, true);
					}
				}
			}
			break;
		}

		case ECinematicCameraMode::CameraPointVehicleToGun:
		{
			if (CurrentCameraPoint->Reposition(false, true) == true)
			{
				if (CameraModeTimer > CameraDuration - 1.5f)
				{
					CameraModeTimer = CameraDuration - 1.5f;
				}
			}

			if (CameraModeTimer >= CameraDuration)
			{
				SwitchToVehicleCameraPoint();
			}

			break;
		}

		case ECinematicCameraMode::CameraPointVehicleToProjectile:
		{
			if ((GRIP_POINTER_VALID(CameraTarget) == false) ||
				(CurrentCameraPoint->Reposition(false) == true))
			{
				if (IdentifyCameraAction(false, false) == false)
				{
					SwitchMode(ECinematicCameraMode::CameraPointVehicle);

					IdentifyCameraPoint(false);
				}
			}
			else
			{
				bool dynamicFOV = true;
				bool curtailView = false;
				bool preserveView = WeaponEventConcluded;
				ABaseVehicle* vehicle = Cast<ABaseVehicle>(CameraTarget);
				AHomingMissile* missile = Cast<AHomingMissile>(CameraTarget);

				if (missile != nullptr)
				{
					if (WeaponEventConcluded == false)
					{
						if (missile->HasExploded() == true)
						{
							preserveView = true;

							// We just detected the weapon coming to its end.

							WeaponEventConcluded = true;

							// Switch camera target to the weapon target is possible.

							if (missile->Target != nullptr &&
								missile->HUDTargetHit() == true)
							{
								CameraTarget = missile->Target;

								if (TimeSlowed == false)
								{
									CameraDuration = CameraModeTimer + 3.0f;
								}
							}
							else
							{
								if (TimeSlowed == false)
								{
									CameraDuration = CameraModeTimer + 2.0f;
								}
							}
						}
						else if (missile->IsTargetWithinReach() == true && missile->Target != nullptr)
						{
							preserveView = true;

							if (TimeSlowed == false)
							{
								CameraDuration = FMath::Max(CameraDuration, CameraModeTimer + 3.0f);
							}
						}
					}
					else
					{
						// The missile has exploded but didn't hit the target.

						dynamicFOV = false;
					}
				}

				const FTransform& transform = CurrentCameraPoint->GetComponentTransform();
				FVector fromLocation = transform.GetLocation();
				FVector targetLocation = GetCameraTargetLocation(fromLocation);
				FVector toTarget = targetLocation - fromLocation;
				float distance = toTarget.Size();
				FRotator lastRotation = ViewRotation;
				FRotator targetRotation = toTarget.ToOrientationRotator();
				float lag = FMath::Lerp(0.9f, 0.96f, FMathEx::GetRatio(DynamicFOV.FieldOfView, 35.0f, 50.0f));

				toTarget.Normalize();

				LastRotation = FMathEx::GetSmoothedRotation(LastRotation, targetRotation, deltaSeconds, lag, lag, lag);

				FVector forward = LastRotation.Vector();
				FVector up = transform.GetUnitAxis(EAxis::Z);

				if (FMathEx::GetRotationFromForwardUp(forward, up, ViewRotation) == false)
				{
					ViewRotation = lastRotation;
				}

				float angleAway = FVector::DotProduct(CurrentVehicle->GetUpDirection() * (CurrentCameraPoint->IsFlipped() == true ? -1.0f : 1.0f), toTarget);

				// Check to see if the target is visible and stop watching them after a short time if they're not.

				FHitResult hit;

				if (CameraTarget->GetWorld()->LineTraceSingleByChannel(hit, fromLocation, targetLocation, ABaseGameMode::ECC_LineOfSightTest, VisibilityQueryParams) == false)
				{
					TargetHiddenTime = 0.0f;
				}
				else
				{
					TargetHiddenTime += deltaSeconds;
				}

				if (CanSlowTime(false) == true)
				{
					if (missile != nullptr)
					{
						if (missile->HasExploded() == false)
						{
							float timeToTarget = missile->GetTimeToTarget();

							if (timeToTarget > 0.0f &&
								timeToTarget < 0.25f &&
								distance < 75.0f * 100.0f &&
								missile->IsLikelyToHitTarget() == true)
							{
								TimeSlowed = true;

								APlayGameMode::Get(Owner)->ChangeTimeDilation(0.25f, 1.0f);

								// Give the view a couple of seconds more.

								CameraDuration = CameraModeTimer + 8.0f;
							}
						}
					}
				}

				if (dynamicFOV == true)
				{
					if (missile != nullptr ||
						vehicle != nullptr)
					{
						UpdateDynamicFieldOfView(deltaSeconds, true, true, CameraTarget.Get(), CurrentCameraPoint->GetComponentLocation(), DynamicFOV, TimeSlowed);
					}
				}

				if ((curtailView == true && CameraShotTimer > MinCameraDuration) ||
					CameraModeTimer >= CameraDuration ||
					TargetHiddenTime > 1.5f ||
					(angleAway < -0.5f && CameraShotTimer > MinCameraDuration) ||
					(distance > 200.0f * 100.0f && CameraShotTimer > MinCameraDuration && preserveView == false))
				{
					if (missile != nullptr &&
						missile->Target != nullptr &&
						WeaponEventConcluded == false)
					{
						LastCameraTarget = missile;
						CameraTarget = missile->Target;
					}

					if (GRIP_POINTER_VALID(CameraTarget) == false ||
						CameraTarget->IsA<ABaseVehicle>() == false ||
						GRIP_POINTER_VALID(LastCameraTarget) == false ||
						LastCameraTarget->IsA<AHomingMissile>() == false ||
						HookupMissileImpactFromVehicle(Cast<AHomingMissile>(LastCameraTarget), Cast<ABaseVehicle>(CameraTarget), 3.0f) == false)
					{
						if (IdentifyCameraAction(false, false) == false)
						{
							SwitchMode(ECinematicCameraMode::CameraPointVehicle);

							IdentifyCameraPoint(false);
						}
					}
				}
			}

			break;
		}

		case ECinematicCameraMode::CameraPointVehicle:
		{
			if (Vehicles.Num() == 0 &&
				GRIP_POINTER_VALID(AttachedToVehicle) == false)
			{
				// Looks like we kicked off the cinematics too early before the vehicles / game mode was initialized.
				// This normally only happens with spectator mode in network play, so just try initializing again here.

				AttachToAnyVehicle(nullptr);
			}

			if (Vehicles.Num() > 0 ||
				GRIP_POINTER_VALID(AttachedToVehicle) == true)
			{
				bool vehicleValid = GRIP_POINTER_VALID(CurrentVehicle);
				bool switchVehicle = (vehicleValid == false || CurrentCameraPoint == nullptr || CurrentVehicle->IsVehicleDestroyed() == true);
				bool cameraIsOld = (CurrentCameraPoint != nullptr && CameraShotTimer > CurrentCameraPoint->MaximumViewSeconds);
				bool cameraIsNotGreat = (CameraShotTimer >= MinCameraDuration && vehicleValid == true && CurrentCameraPoint != nullptr && CurrentCameraPoint->WasClipped() == true);

				cameraIsNotGreat |= (CameraShotTimer >= MinCameraDuration && vehicleValid == true && CurrentCameraPoint != nullptr && CurrentCameraPoint->HighAngularVelocities == false && CurrentVehicle->IsAirborne() == true);

				if (cameraIsOld == true ||
					cameraIsNotGreat == true ||
					switchVehicle == true)
				{
					// Grab a camera point on a vehicle for now, in semi-programmed sequence to avoid repeating itself.

					IdentifyCameraPoint(switchVehicle);
				}
			}

			// Ensure the point camera is positioned correctly and not intersecting any scenery.

			bool switchedCamera = (CurrentCameraPoint != nullptr && CurrentCameraPoint->Reposition(false, true) == true);

			if (switchedCamera == true)
			{
				CameraShotTimer = 0.0f;
			}

			if (CyclingVehicles == false)
			{
				// NOTE: There's no fixed end to this camera mode, we just try to find other opportunities
				// for as long as it takes and then switch when one arrives.

				if (CameraShotTimer > MinCameraDuration)
				{
					IdentifyCameraAction(true, switchedCamera);
				}
			}

			break;
		}
		}
	}
	while (mode != CinematicCameraMode);
}

/**
* Switch to a camera point attached to a vehicle.
***********************************************************************************/

void FCinematicsDirector::SwitchToVehicleCameraPoint()
{
	if (IdentifyVehicleEvent() == false)
	{
		SwitchMode(ECinematicCameraMode::CameraPointVehicle);

		IdentifyCameraPoint(false);
	}
}

/**
* Identify camera action and switch the current camera view if found.
***********************************************************************************/

bool FCinematicsDirector::IdentifyCameraAction(bool allowVehicleTrackingCamera, bool highPriority, bool highValue)
{
	bool switched = false;
	float clock = LastClock;

	for (int32 pass = 0; pass < 2; pass++)
	{
		bool ignoreTimes = (pass != 0);

		if (allowVehicleTrackingCamera == true)
		{
			if (ignoreTimes == true ||
				(clock - LastViewTimes[(int32)ECinematicCameraMode::CameraPointVehicleToGun] > 20.0f &&
				clock - LastViewTimes[(int32)ECinematicCameraMode::CameraPointVehicleToProjectile] > 20.0f))
			{
				// Look for a weapon launch from the viewpoint of its parent vehicle.

				switched = IdentifyWeaponEvent(highValue);
			}
		}

		if (switched == false &&
			(ignoreTimes == true ||
			clock - LastViewTimes[(int32)ECinematicCameraMode::SplineFollowingVictimVehicle] > 20.0f))
		{
			// See if we have a spline target for an impact event we can use as they're interesting.

			static bool fromVehicleFirst = false;

			fromVehicleFirst ^= true;

			if (true) // fromVehicleFirst == true)
			{
				switched = IdentifyImpactEventFromVehicle();

				if (switched == false)
				{
					switched = IdentifySplineTarget(true);
				}
			}
			else
			{
				switched = IdentifySplineTarget(true);

				if (switched == false)
				{
					switched = IdentifyImpactEventFromVehicle();
				}
			}
		}

		if (highValue == false)
		{
			if (allowVehicleTrackingCamera == true)
			{
				if (switched == false)
				{
					// See if we have a vehicle passing event we can focus as they're exciting.

					switched = IdentifyVehicleEvent();
				}
			}

			if (switched == false &&
				(ignoreTimes == true ||
				clock - LastViewTimes[(int32)ECinematicCameraMode::StaticCamera] > 10.0f))
			{
				// See if we have a static camera.

				switched = IdentifyStaticCamera();
			}

			if (switched == false &&
				(ignoreTimes == true ||
				(clock - LastViewTimes[(int32)ECinematicCameraMode::SplineFollowingVehicle] > 4.0f &&
				clock - LastViewTimes[(int32)ECinematicCameraMode::SplineFollowingVictimVehicle] > 4.0f)))
			{
				// See if we have a general spline target we can use as they're interesting.

				switched = IdentifySplineTarget(false);
			}
		}

		if (switched == true ||
			highPriority == false)
		{
			break;
		}
	}

	return switched;
}

/**
* Get the world target location for the camera at a given location.
***********************************************************************************/

FVector FCinematicsDirector::GetCameraTargetLocation(const FVector& fromLocation) const
{
	if (GRIP_POINTER_VALID(CameraTarget) == true)
	{
		AHomingMissile* missile = Cast<AHomingMissile>(CameraTarget);

		if (missile != nullptr)
		{
			// All of this nonsense is to try to prevent the flare at the rear of a missile from
			// spinning around like an idiot when it's at the center of the camera view. Instead
			// we try to offset the target from the center of the camera's view a little.

			FVector targetLocation = missile->GetActorLocation();
			float distance = (targetLocation - fromLocation).Size();
			float distanceAway = 100.0f + (FMathEx::GetRatio(distance, 500.0f, 2500.0f) * 500.0f);
			float inverseZ = ((1.0f - FMathEx::GetRatio(distance, 500.0f, 2500.0f)) * 2.0f) - 1.0f;
			const FTransform& transform = missile->GetActorTransform();
			FVector offset = (transform.GetUnitAxis(EAxis::Y) + (transform.GetUnitAxis(EAxis::Z) * inverseZ)) - transform.GetUnitAxis(EAxis::X);

			offset *= distanceAway;

			return missile->GetActorLocation() + transform.TransformVector(offset);
		}

		return CameraTarget->GetActorLocation();
	}

	return FVector::ZeroVector;
}

/**
* Attach the cinematic camera manager to a specific vehicle.
***********************************************************************************/

void FCinematicsDirector::AttachToVehicle(ABaseVehicle* vehicle)
{
	CurrentVehicle = vehicle;
	AttachedToVehicle = vehicle;

	Vehicles.Reset();

	ResetVehicleTime();

	QueueCamerasForVehicle();

	SwitchMode(ECinematicCameraMode::CameraPointVehicle);
}

/**
* Attach the cinematic camera manager to a all vehicles.
***********************************************************************************/

void FCinematicsDirector::AttachToAnyVehicle(ABaseVehicle* firstVehicle)
{
	AttachedToVehicle.Reset();

	ResetVehicleTime();

	QueueVehicles();

	if (firstVehicle != nullptr &&
		firstVehicle->IsVehicleDestroyed() == false)
	{
		APlayGameMode* gameMode = APlayGameMode::Get(Owner);

		if (gameMode != nullptr)
		{
			for (int32 i = 0; i < Vehicles.Num(); i++)
			{
				if (gameMode->GetVehicle(Vehicles[i]) == firstVehicle)
				{
					VehicleIndex = i;
					QueueVehicle();
					break;
				}
			}
		}
	}

	if (CurrentCameraPoint != nullptr)
	{
		CurrentCameraPoint->Reset();
	}

	SwitchMode(ECinematicCameraMode::CameraPointVehicle);
}

/**
* Cycle to the next vehicle in the list and stay on it.
***********************************************************************************/

void FCinematicsDirector::CycleVehicle()
{
	if (Vehicles.Num() == 0)
	{
		QueueVehicles();
	}

	VehicleDuration = 0.0f;
	CyclingVehicles = true;
	VehicleIndex = (VehicleIndex + 1) % Vehicles.Num();

	QueueVehicle();

	AttachedToVehicle = CurrentVehicle;

	QueueCamerasForVehicle();

	SwitchMode(ECinematicCameraMode::CameraPointVehicle);

	IdentifyCameraPoint(false);
}

/**
* Set whether spirit camera is in use.
***********************************************************************************/

void FCinematicsDirector::UseSpiritCamera(bool use, bool fromImpact, const FVector& velocity)
{
	ACameraBallActor* cameraBall = nullptr;
	ABaseVehicle* vehicle = Cast<ABaseVehicle>(Owner);

	if (vehicle != nullptr)
	{
		cameraBall = vehicle->GetCameraBall();
	}

	if (use == true)
	{
		SwitchMode(ECinematicCameraMode::SpiritWorld);

		if (fromImpact == true &&
			GRIP_POINTER_VALID(StaticCamera) == true)
		{
			SpiritCameraFOV = StaticCamera->Camera->FieldOfView + 30.0f;

			Camera->CustomLocation = StaticCamera->GetActorLocation();
			Camera->CustomRotation = StaticCamera->GetActorRotation() + FRotator(0.0f, AdjustedYaw, -(AdjustedYaw * 0.75f));

			if (cameraBall != nullptr)
			{
				float force = velocity.Size();
				FVector direction = velocity;
				FVector baseDirection = vehicle->GetLaunchDirection();

				direction.Normalize();
				direction += baseDirection * 0.2f;
				direction.Normalize();

				cameraBall->Launch(Camera->CustomLocation, Camera->CustomRotation, direction, force, true);
			}
		}
		else
		{
			SpiritCameraFOV = 90.0f;

			Camera->CustomLocation = Camera->NativeLocation;
			Camera->CustomRotation = Camera->NativeRotation;

			if (cameraBall != nullptr)
			{
				FVector direction = Camera->GetComponentLocation() - Owner->GetActorLocation();

				direction.Normalize();

				cameraBall->Launch(Camera->CustomLocation, Camera->CustomRotation, direction, 30.0f * 100.0f, false);
			}
		}
	}
	else
	{
		if (fromImpact == false &&
			CinematicCameraMode == ECinematicCameraMode::SpiritWorld)
		{
			SwitchMode(ECinematicCameraMode::Off);
		}

		if (cameraBall != nullptr)
		{
			cameraBall->Hibernate();
		}
	}
}

/**
* Use a custom camera override.
***********************************************************************************/

void FCinematicsDirector::UseCustomOverride(bool use, const FVector& location, const FRotator& rotation, float fieldOfView)
{
	if (use == true)
	{
		SwitchMode(ECinematicCameraMode::CustomOverride);

		Camera->CustomLocation = location;
		Camera->CustomRotation = rotation;
		Camera->CustomFieldOfView = fieldOfView;
	}
	else
	{
		SwitchMode(ECinematicCameraMode::Off);
	}
}

/**
* Get the distance between the camera and its target.
***********************************************************************************/

float FCinematicsDirector::GetFocalDistance() const
{
	return (UsingSplineCamera() == true) ? (SplineCamera.Target->GetCenterLocation() - SplineCamera.WorldLocation).Size() : 1000.0f;
}

/**
* Queue a vehicle for showing.
***********************************************************************************/

void FCinematicsDirector::QueueVehicle()
{
	ABaseGameMode::SleepComponent(CurrentCameraPoint);

	CurrentVehicle.Reset();
	CurrentCameraPoint = nullptr;

	APlayGameMode* gameMode = APlayGameMode::Get(Owner);

	if (gameMode != nullptr &&
		gameMode->GetNumOpponentsLeft() > 0 &&
		Vehicles.Num() > 0)
	{
		int32 attempts = 0;

		do
		{
			int32 index = Vehicles[VehicleIndex];
			ABaseVehicle* vehicle = gameMode->GetVehicle(index);

			if (vehicle->IsVehicleDestroyed() == false)
			{
				CurrentVehicle = vehicle;

				break;
			}
			else
			{
				VehicleIndex = (VehicleIndex + 1) % Vehicles.Num();
			}
		}
		while (attempts++ < Vehicles.Num());
	}
}

/**
* Queue all vehicles ready for showing.
***********************************************************************************/

void FCinematicsDirector::QueueVehicles()
{
	VehicleIndex = 0;
	VehicleTimer = 0.0f;

	Vehicles.Reset();

	GRIP_GAME_MODE_LIST_FOR(GetVehicles(), vehicles, Owner);

	TArray<int32> vehicleIndices;

	int32 numAlive = 0;

	// Try to look only at vehicles that are locally controlled on this machines if networked game
	// (to avoid jittering) or all vehicles in non-networked game.

	for (int32 i = 0; i < vehicles.Num(); i++)
	{
		vehicleIndices.Emplace(i);

		if (vehicles[i]->IsVehicleDestroyed() == false)
		{
			numAlive++;
		}
	}

	// If we've no vehicles to watch then just choose any vehicles present.

	if (numAlive == 0)
	{
		for (int32 i = 0; i < vehicles.Num(); i++)
		{
			vehicleIndices.Emplace(i);
		}
	}

	// Add the vehicles in random order to our internal list.

	while (vehicleIndices.Num() > 0)
	{
		int32 index = FMath::Rand() % vehicleIndices.Num();

		Vehicles.Emplace(vehicleIndices[index]);

		vehicleIndices.RemoveAt(index);
	}

	QueueVehicle();
}

/**
* Queue the cameras for the current vehicle for showing.
***********************************************************************************/

void FCinematicsDirector::QueueCamerasForVehicle()
{
	VehicleCameras.Reset();

	CameraIndex = 0;

	ResetCameraTime();

	if (GRIP_POINTER_VALID(CurrentVehicle) == true)
	{
		UCameraPointComponent* lastCamera = CurrentCameraPoint;
		TArray<UActorComponent*> components;

		CurrentVehicle->GetComponents(UCameraPointComponent::StaticClass(), components);

		ABaseGameMode::SleepComponent(CurrentCameraPoint);

		CurrentCameraPoint = nullptr;

		if (components.Num() > 0)
		{
			while (components.Num() > 0)
			{
				int32 index = FMath::Rand() % components.Num();
				UCameraPointComponent* camera = Cast<UCameraPointComponent>(components[index]);

				if (camera == StockCameraPoint)
				{
					components.RemoveAt(index);
				}
				else
				{
					if (camera != lastCamera ||
						components.Num() == 1)
					{
						VehicleCameras.Emplace(camera);

						components.RemoveAt(index);

						lastCamera = nullptr;
					}
				}
			}

			ABaseGameMode::SleepComponent(CurrentCameraPoint);

			CurrentCameraPoint = VehicleCameras[CameraIndex];

			while (CurrentCameraPoint->InvertWithVehicle == false &&
				((CurrentVehicle->IsFlipped() == true) ? -1 : +1) != FMathEx::UnitSign(CurrentCameraPoint->GetRelativeLocation().Z))
			{
				if (++CameraIndex == VehicleCameras.Num())
				{
					CameraIndex = 0;
					CurrentCameraPoint = VehicleCameras[CameraIndex];
					break;
				}

				CurrentCameraPoint = VehicleCameras[CameraIndex];
			}

			ABaseGameMode::WakeComponent(CurrentCameraPoint);

			CurrentCameraPoint->Reset();
		}
	}
}

/**
* Returns camera's point of view.
***********************************************************************************/

bool FCinematicsDirector::GetCameraView(float deltaSeconds, FMinimalViewInfo& desiredView)
{
	if (IsActive() == true)
	{
		// NOTE: desiredView.FOV ignored when returned if using a HMD.

		APlayerController* controller = nullptr;
		APawn* pawn = Cast<APawn>(Camera->GetAttachmentRootActor());

		if (pawn != nullptr)
		{
			controller = Cast<APlayerController>(pawn->GetController());
		}

		if (CinematicCameraMode == ECinematicCameraMode::StaticCamera)
		{
			desiredView.FOV = UAdvancedCameraComponent::GetAdjustedFOV(controller, StaticCamera->Camera->FieldOfView + ((StaticCamera->HasCameraBeenHit() == true) ? 20.0f : 0.0f));
			desiredView.Location = StaticCamera->GetActorLocation();
			desiredView.Rotation = StaticCamera->GetActorRotation() + FRotator(0.0f, AdjustedYaw, -(AdjustedYaw * 0.75f));

			return true;
		}
		else if (CinematicCameraMode == ECinematicCameraMode::SpiritWorld)
		{
			desiredView.FOV = UAdvancedCameraComponent::GetAdjustedFOV(controller, SpiritCameraFOV);
			desiredView.Location = Camera->CustomLocation;
			desiredView.Rotation = Camera->CustomRotation;

			ABaseVehicle* vehicle = Cast<ABaseVehicle>(Owner);

			if (vehicle != nullptr)
			{
				ACameraBallActor* cameraBall = vehicle->GetCameraBall();

				if (cameraBall != nullptr)
				{
					desiredView.Location = cameraBall->CollisionShape->GetComponentLocation();
					desiredView.Rotation = cameraBall->CollisionShape->GetComponentRotation();
				}
			}

			return true;
		}
		else if (CinematicCameraMode == ECinematicCameraMode::CustomOverride)
		{
			desiredView.Location = Camera->CustomLocation;
			desiredView.Rotation = Camera->CustomRotation;
			desiredView.FOV = UAdvancedCameraComponent::GetAdjustedFOV(controller, Camera->CustomFieldOfView);

			return true;
		}
		else if (CinematicCameraMode == ECinematicCameraMode::SplineFollowingVehicle ||
			CinematicCameraMode == ECinematicCameraMode::SplineFollowingVictimVehicle)
		{
			desiredView.Location = SplineCamera.GetLocation();
			desiredView.Rotation = SplineCamera.GetRotation(false);

			desiredView.FOV = UAdvancedCameraComponent::GetAdjustedFOV(controller, SplineCamera.DynamicFOV.FieldOfView);

			return true;
		}
		else if (CinematicCameraMode == ECinematicCameraMode::CameraPointVehicleToVehicle)
		{
			desiredView.Rotation = ViewRotation;

			UCameraPointComponent* cameraPoint = GetCurrentCameraPoint();

			if (cameraPoint != nullptr)
			{
				desiredView.Location = cameraPoint->GetComponentLocation();
				desiredView.FOV = UAdvancedCameraComponent::GetAdjustedFOV(controller, cameraPoint->FieldOfView);
			}

			return true;
		}
		else if (CinematicCameraMode == ECinematicCameraMode::CameraPointVehicleToProjectile)
		{
			UCameraPointComponent* cameraPoint = GetCurrentCameraPoint();

			if (cameraPoint != nullptr)
			{
				desiredView.Location = cameraPoint->GetComponentLocation();
				desiredView.FOV = UAdvancedCameraComponent::GetAdjustedFOV(controller, DynamicFOV.FieldOfView);
			}

			desiredView.Rotation = ViewRotation;

			return true;
		}
		else
		{
			if (CyclingVehicles == true &&
				GRIP_POINTER_VALID(CurrentVehicle) == true)
			{
				pawn = Cast<APawn>(Owner);
				controller = Cast<APlayerController>(pawn->GetController());

				CurrentVehicle->Camera->GetCameraView(deltaSeconds, desiredView);
				desiredView.FOV = UAdvancedCameraComponent::GetAdjustedFOV(controller, desiredView.FOV);
			}
			else
			{
				UCameraPointComponent* cameraPoint = GetCurrentCameraPoint();

				if (cameraPoint != nullptr)
				{
					desiredView.Location = cameraPoint->GetComponentLocation();
					desiredView.Rotation = cameraPoint->GetComponentRotation();

					if (cameraPoint->InheritSpeedFieldOfView == false)
					{
						desiredView.FOV = UAdvancedCameraComponent::GetAdjustedFOV(controller, cameraPoint->FieldOfView);
					}

					return true;
				}
			}
		}
	}

	return false;
}

/**
* Identify a potential camera point.
***********************************************************************************/

void FCinematicsDirector::IdentifyCameraPoint(bool switchVehicle)
{
	// We need to switch to a new camera.

	if ((switchVehicle == true) ||
		(VehicleTimer >= VehicleDuration && VehicleDuration != 0.0f && GRIP_POINTER_VALID(AttachedToVehicle) == false))
	{
		// We also need to switch to a new vehicle.

		ResetVehicleTime();

		if (++VehicleIndex >= Vehicles.Num())
		{
			// Make a new randomized list of vehicles.

			QueueVehicles();
		}
		else
		{
			// Queue the next vehicle.

			QueueVehicle();

			if (GRIP_POINTER_VALID(CurrentVehicle) == false)
			{
				// Make a new randomized list of vehicles.

				QueueVehicles();
			}
		}

		QueueCamerasForVehicle();
	}
	else
	{
		int32 attempts = 0;

		do
		{
			if (++CameraIndex >= VehicleCameras.Num())
			{
				QueueCamerasForVehicle();
			}
			else
			{
				ResetCameraTime();

				if (GRIP_POINTER_VALID(CurrentVehicle) == true)
				{
					ABaseGameMode::SleepComponent(CurrentCameraPoint);

					CurrentCameraPoint = VehicleCameras[CameraIndex];

					while (CurrentCameraPoint->InvertWithVehicle == false &&
						((CurrentVehicle->IsFlipped() == true) ? -1 : +1) != FMathEx::UnitSign(CurrentCameraPoint->GetRelativeLocation().Z))
					{
						if (++CameraIndex == VehicleCameras.Num())
						{
							CameraIndex = 0;
							CurrentCameraPoint = VehicleCameras[CameraIndex];
							break;
						}

						CurrentCameraPoint = VehicleCameras[CameraIndex];
					}

					ABaseGameMode::WakeComponent(CurrentCameraPoint);

					CurrentCameraPoint->Reset();
				}
			}
		}
		while ((GRIP_POINTER_VALID(CurrentVehicle) == false || CurrentCameraPoint->WasClipped() == true) && ++attempts < VehicleCameras.Num());
	}

	ABaseVehicle* vehicle = Cast<ABaseVehicle>(Owner);
	APlayerController* playerController = Cast<APlayerController>(vehicle->GetController());

	if (playerController != nullptr)
	{
		playerController->PlayerCameraManager->SetGameCameraCutThisFrame();
	}

	CameraShotTimer = 0.0f;
}

/**
* Identify a potential impact event visible from a nearby vehicle.
***********************************************************************************/

bool FCinematicsDirector::IdentifyImpactEventFromVehicle()
{
	APlayGameMode* gameMode = APlayGameMode::Get(Owner);

	GRIP_GAME_MODE_LIST_FOR_FROM(GetVehicles(), vehicles, gameMode);

	float maxImpactTime = 3.0f;

	for (ABaseVehicle* vehicle : vehicles)
	{
		TWeakObjectPtr<AActor> impactingActor;

		if (IdentifyImpactEvent(vehicle, impactingActor, maxImpactTime, true) != 0.0f)
		{
			AHomingMissile* missile = Cast<AHomingMissile>(impactingActor.Get());

			if (missile != nullptr)
			{
				return HookupMissileImpactFromVehicle(missile, vehicle, maxImpactTime);
			}
		}
	}

	return false;
}

/**
* Hookup missile impact event visible from a nearby vehicle.
***********************************************************************************/

bool FCinematicsDirector::HookupMissileImpactFromVehicle(AHomingMissile* missile, ABaseVehicle* forVehicle, float maxImpactTime)
{
	if (missile != nullptr &&
		forVehicle != nullptr)
	{
		if (missile->HasExploded() == false &&
			missile->GetTimeToTarget() <= maxImpactTime)
		{
			APlayGameMode* gameMode = APlayGameMode::Get(Owner);

			GRIP_GAME_MODE_LIST_FOR_FROM(GetVehicles(), vehicles, gameMode);

			CameraTarget.Reset();

			ABaseVehicle* fromVehicle = forVehicle;
			float closestDistance = -1.0f;
			ABaseVehicle* closestVehicle = nullptr;

			if (fromVehicle->GetSpeedKPH() > 300.0f &&
				fromVehicle->IsVehicleDestroyed() == false &&
				IsVehicleSmoothlyControlled(fromVehicle) == true)
			{
				float currentVehicleSpeed = fromVehicle->GetSpeedKPH();
				FVector currentVehicleLocation = fromVehicle->GetActorLocation();

				for (ABaseVehicle* vehicle : vehicles)
				{
					if (vehicle != nullptr &&
						vehicle != fromVehicle &&
						vehicle->GetSpeedKPH() > 300.0f &&
						vehicle->IsPracticallyGrounded() == true &&
						vehicle->IsVehicleDestroyed() == false &&
						IsVehicleSmoothlyControlled(vehicle) == true)
					{
						float distanceDifference = (vehicle->GetActorLocation() - currentVehicleLocation).Size();

						if (FMath::Abs(distanceDifference) < 75.0f * 100.0f)
						{
							float speedDifference = FMath::Abs(vehicle->GetSpeedKPH() - currentVehicleSpeed);

							if (speedDifference < 75.0f)
							{
								FVector direction = fromVehicle->GetActorLocation() - vehicle->GetActorLocation(); direction.Normalize();

								if (FMath::Abs(FVector::DotProduct(vehicle->GetSurfaceDirection(), direction)) < 0.5f)
								{
									if (closestDistance < 0.0f ||
										closestDistance > distanceDifference)
									{
										closestDistance = distanceDifference;
										closestVehicle = vehicle;
									}
								}
							}
						}
					}
				}

				if (closestVehicle != nullptr)
				{
					VisibilityQueryParams.ClearIgnoredActors();
					VisibilityQueryParams.AddIgnoredActor(closestVehicle);
					VisibilityQueryParams.AddIgnoredActor(missile);

					FHitResult hit;
					FVector testPosition = closestVehicle->GetActorLocation() + closestVehicle->GetLaunchDirection() * 2.0f * 100.0f;

					if (closestVehicle->GetWorld()->LineTraceSingleByChannel(hit, testPosition, missile->GetActorLocation(), ABaseGameMode::ECC_LineOfSightTest, VisibilityQueryParams) == false)
					{
						CameraTarget = missile;

						CreateStockPointCamera();

						ABaseGameMode::SleepComponent(CurrentCameraPoint);
						ABaseGameMode::WakeComponent(StockCameraPoint);

						GRIP_DETACH(StockCameraPoint);

						GRIP_ATTACH_AT(StockCameraPoint, closestVehicle->VehicleMesh, "RootDummy", FVector(-100.0f, 0.0f, 110.0f));

						CurrentVehicle = closestVehicle;
						CurrentCameraPoint = StockCameraPoint;
						CurrentCameraPoint->ResetOriginal();
						CurrentCameraPoint->Reset();

						LastRotation = ViewRotation = (GetCameraTargetLocation(CurrentCameraPoint->GetComponentLocation()) - CurrentCameraPoint->GetComponentTransform().GetLocation()).ToOrientationRotator();

						ResetCameraTime();

						CameraDuration = FMath::FRandRange(4.0f, 6.0f);

						LastCameraTarget.Reset();
						WeaponEventConcluded = false;

						SwitchMode(ECinematicCameraMode::CameraPointVehicleToProjectile);

						return true;
					}
				}
			}
		}
	}

	return false;
}

/**
* Identify a potential vehicle event visible from a nearby vehicle.
***********************************************************************************/

bool FCinematicsDirector::IdentifyVehicleEvent()
{
	UGlobalGameState* gameState = UGlobalGameState::GetGlobalGameState(Owner);

	if (gameState->IsGameModeRace() == true)
	{
		if (LastClock - LastViewTimes[(int32)ECinematicCameraMode::CameraPointVehicleToVehicle] > 10.0f)
		{
			APlayGameMode* gameMode = APlayGameMode::Get(Owner);

			GRIP_GAME_MODE_LIST_FOR_FROM(GetVehicles(), vehicles, gameMode);

			CameraTarget.Reset();

			static int32 fromVehicleIndex = 0;

			ABaseVehicle* fromVehicle = vehicles[++fromVehicleIndex % vehicles.Num()];

			if (fromVehicle->GetSpeedKPH() > 300.0f &&
				fromVehicle->IsGrounded(1.0f) == true &&
				fromVehicle->IsVehicleDestroyed() == false &&
				IsVehicleSmoothlyControlled(fromVehicle) == true)
			{
				for (ABaseVehicle* toVehicle : vehicles)
				{
					if (toVehicle != fromVehicle &&
						toVehicle->GetSpeedKPH() > 300.0f &&
						toVehicle->IsGrounded() == true &&
						toVehicle->IsVehicleDestroyed() == false &&
						IsVehicleSmoothlyControlled(toVehicle) == true)
					{
						float distanceDifference = toVehicle->GetRaceState().EternalRaceDistance - fromVehicle->GetRaceState().EternalRaceDistance;

						if (FMath::Abs(distanceDifference) < 25.0f * 100.0f &&
							toVehicle->GetAI().RouteFollower.ThisSpline == fromVehicle->GetAI().RouteFollower.ThisSpline)
						{
							float speedDifference = toVehicle->GetSpeedKPH() - fromVehicle->GetSpeedKPH();

							if ((distanceDifference > 0.0f && speedDifference < -10.0f && speedDifference > -100.0f) ||
								(distanceDifference <= 0.0f && speedDifference > 10.0f && speedDifference < 100.0f))
							{
								FVector toDirection = toVehicle->GetActorLocation() - fromVehicle->GetActorLocation(); toDirection.Normalize();

								if (FMath::Abs(FVector::DotProduct(fromVehicle->GetSurfaceDirection(), toDirection)) < 0.5f)
								{
									CameraTarget = toVehicle;

									CreateStockPointCamera();

									ABaseGameMode::SleepComponent(CurrentCameraPoint);
									ABaseGameMode::WakeComponent(StockCameraPoint);

									GRIP_DETACH(StockCameraPoint);

									GRIP_ATTACH_AT(StockCameraPoint, fromVehicle->VehicleMesh, "RootDummy", FVector(-100.0f, 0.0f, 110.0f));

									CurrentVehicle = fromVehicle;
									CurrentCameraPoint = StockCameraPoint;
									CurrentCameraPoint->ResetOriginal();
									CurrentCameraPoint->Reset();

									const FTransform& transform = CurrentCameraPoint->GetComponentTransform();
									FVector fromLocation = transform.GetLocation();
									FVector targetLocation = GetCameraTargetLocation(fromLocation);
									FVector toTarget = targetLocation - fromLocation;

									LastRotation = toTarget.ToOrientationRotator();

									FVector forward = toTarget; forward.Normalize();
									FVector up = fromVehicle->GetLaunchDirection();

									FMathEx::GetRotationFromForwardUp(forward, up, ViewRotation);

									VisibilityQueryParams.ClearIgnoredActors();
									VisibilityQueryParams.AddIgnoredActor(toVehicle);

									ResetCameraTime();

									CameraDuration = FMath::FRandRange(4.0f, 6.0f);

									SwitchMode(ECinematicCameraMode::CameraPointVehicleToVehicle);

									return true;
								}
							}
						}
					}
				}
			}
		}
	}

	return false;
}

/**
* Identify a potential weapon event.
*
* If highValue is set it means we're happy to cut a view short a bit in order to
* switch to viewing a high-value event.
*
* If launches is set it means we're already observing a vehicle and are interested
* in following projectile launches away from the vehicle as a camera cut.
***********************************************************************************/

bool FCinematicsDirector::IdentifyWeaponEvent(bool highValue)
{
	if ((highValue == true) &&
		(CameraDuration < MinCameraDuration || LastClock - LastViewTimes[(int32)ECinematicCameraMode::CameraPointVehicleToProjectile] < 20.0f))
	{
		return false;
	}

	APlayGameMode* gameMode = APlayGameMode::Get(Owner);
	TArray<FGameEvent>& events = gameMode->GameEvents;
	float time = gameMode->GetRealTimeClock();

	// Examine the last few game events.

	for (int32 i = gameMode->GameEvents.Num() - 1; i >= 0; i--)
	{
		FGameEvent& event = events[i];

		if (event.Time < time - 0.25f)
		{
			break;
		}

		// Find a weapon event that we can watch.

		if (event.EventType == EGameEventType::Used ||
			event.EventType == EGameEventType::Preparing)
		{
			UCameraPointComponent* cameraPoint = nullptr;
			ABaseVehicle* vehicle = gameMode->GetVehicleForVehicleIndex(event.LaunchVehicleIndex);

			if (vehicle != nullptr &&
				vehicle->IsVehicleDestroyed() == false &&
				IsVehicleSmoothlyControlled(vehicle) == true)
			{
				if (event.PickupUsed == EPickupType::HomingMissile)
				{
					AHomingMissile* missile = vehicle->GetHomingMissile().Get();

					if (missile != nullptr &&
						missile->Target != nullptr &&
						missile->Target->IsA<ABaseVehicle>() == true &&
						missile->HasExploded() == false)
					{
						// Follow a Scorpion missile use.

						if ((missile->Target->GetActorLocation() - missile->GetActorLocation()).Size() < 200.0f * 100.0f)
						{
							CameraTarget = missile;

							CreateStockPointCamera();

							ABaseGameMode::SleepComponent(CurrentCameraPoint);
							ABaseGameMode::WakeComponent(StockCameraPoint);

							GRIP_DETACH(StockCameraPoint);

							GRIP_ATTACH_AT(StockCameraPoint, vehicle->VehicleMesh, "RootDummy", FVector(-100.0f, 0.0f, 110.0f));

							cameraPoint = StockCameraPoint;
							cameraPoint->ResetOriginal();
							cameraPoint->Reset();

							SwitchMode(ECinematicCameraMode::CameraPointVehicleToProjectile);
						}
					}
				}
				else if (highValue == false && event.PickupUsed == EPickupType::GatlingGun)
				{
					// Follow a Gatling gun use.

					cameraPoint = FindForeFacingCameraPoint(vehicle);

					CameraTarget.Reset();

					if (cameraPoint == nullptr)
					{
						continue;
					}

					SwitchMode(ECinematicCameraMode::CameraPointVehicleToGun);
				}

				if (cameraPoint != nullptr)
				{
					ABaseGameMode::SleepComponent(CurrentCameraPoint);
					LastCameraTarget.Reset();
					CurrentVehicle = vehicle;
					CurrentCameraPoint = cameraPoint;
					ABaseGameMode::WakeComponent(CurrentCameraPoint);
					CurrentCameraPoint->Reset();
					WeaponEventConcluded = false;

					LastRotation = CurrentCameraPoint->GetComponentTransform().TransformVector(FVector(1.0f, 0.0f, 0.0f)).ToOrientationRotator();

					ResetCameraTime();

					if (CinematicCameraMode == ECinematicCameraMode::CameraPointVehicleToProjectile)
					{
						CameraDuration = FMath::FRandRange(4.0f, 6.0f);
					}
					else
					{
						CameraDuration = FMath::FRandRange(3.0f, 5.0f);
					}

					return true;
				}
			}
		}
	}

	return false;
}

/**
* Identify a potential weapon launch from the currently observed vehicle.
***********************************************************************************/

bool FCinematicsDirector::IdentifyWeaponLaunches()
{
	APlayGameMode* gameMode = APlayGameMode::Get(Owner);
	TArray<FGameEvent>& events = gameMode->GameEvents;
	float time = gameMode->GetRealTimeClock();

	// Examine the last few game events.

	for (int32 i = gameMode->GameEvents.Num() - 1; i >= 0; i--)
	{
		FGameEvent& event = events[i];

		if (event.Time < time - 0.25f)
		{
			break;
		}

		// Find a weapon event that we can watch.

		if (event.EventType == EGameEventType::Used ||
			event.EventType == EGameEventType::Preparing)
		{
			ABaseVehicle* vehicle = gameMode->GetVehicleForVehicleIndex(event.LaunchVehicleIndex);
			AActor* cameraTarget = nullptr;

			if (vehicle != nullptr &&
				vehicle == CameraTarget &&
				vehicle->IsVehicleDestroyed() == false &&
				IsVehicleSmoothlyControlled(vehicle) == true)
			{
				// Follow a missile use.

				if (event.PickupUsed == EPickupType::HomingMissile)
				{
					TWeakObjectPtr<AHomingMissile>& missile = vehicle->GetHomingMissile();

					if (GRIP_POINTER_VALID(missile) == true &&
						missile->Target != nullptr &&
						missile->Target->IsA<ABaseVehicle>() == true)
					{
						cameraTarget = missile.Get();
					}
				}

				if (GRIP_OBJECT_VALID(cameraTarget) == true)
				{
					SwitchMode(ECinematicCameraMode::CameraPointVehicleToProjectile);

					LastCameraTarget = CameraTarget;
					CameraTarget = cameraTarget;
					WeaponEventConcluded = false;

					ResetCameraTime();

					CameraDuration = FMath::FRandRange(4.0f, 6.0f);

					return true;
				}
			}
		}
	}

	return false;
}

/**
* Identify a potential impact event.
***********************************************************************************/

float FCinematicsDirector::IdentifyImpactEvent(ABaseVehicle* vehicle, TWeakObjectPtr<AActor>& impactingActor, float maxImpactTime, bool missilesOnly) const
{
	float maxTime = 0.0f;

	impactingActor = nullptr;

	APlayGameMode* gameMode = APlayGameMode::Get(Owner);

	for (AHomingMissile* missile : gameMode->Missiles)
	{
		if (missile->Target == vehicle &&
			missile->HasExploded() == false &&
			(missilesOnly == true || missile->IsTargetWithinReach() == true))
		{
			if (missile->IsLikelyToHitTarget() == true &&
				vehicle->IsShielded(missile->GetActorLocation()) == false)
			{
				float time = missile->GetTimeToTarget();

				if (maxTime < time &&
					time < maxImpactTime)
				{
					maxTime = time;
					impactingActor = missile;
				}
			}
		}
	}

	return maxTime;
}

/**
* Identify a potential static camera.
***********************************************************************************/

bool FCinematicsDirector::IdentifyStaticCamera()
{
	APlayGameMode* gameMode = APlayGameMode::Get(Owner);
	UGlobalGameState* gameState = UGlobalGameState::GetGlobalGameState(Owner);

	if (gameState->IsGameModeRace() == true &&
		GRIP_POINTER_VALID(gameMode->MasterRacingSpline) == true)
	{
		// Identifying vehicles on pursuit splines is easy enough - if they're using them and are
		// within the correct distance range then bam.

		GRIP_GAME_MODE_LIST_FOR_FROM(TrackCameras, cameras, gameMode);

		for (AStaticTrackCamera* camera : cameras)
		{
			// Use the wide-angle cameras less often than the narrow-angle cameras.

			if (camera->Camera->FieldOfView < 45.0f ||
				(StaticCameraCount & 1) == 1)
			{
				// Looking forwards or backwards along the track.

				if (camera->AngleVsTrack < 45.0f ||
					camera->AngleVsTrack > 180.0f - 45.0f)
				{
					int32 numVehicles = 0;
					float minDistance = 0.0f;
					float maxDistance = 0.0f;
					ABaseVehicle* lastVehicle = CurrentVehicle.Get();
					float distanceLength = gameMode->MasterRacingSplineLength;
					float targetDistance = camera->DistanceAlongMasterRacingSpline;

					GRIP_GAME_MODE_LIST_FOR_FROM(GetVehicles(), vehicles, gameMode);

					for (ABaseVehicle* vehicle : vehicles)
					{
						if (vehicle->GetSpeedKPH() > 200.0f &&
							vehicle->IsVehicleDestroyed() == false)
						{
							TWeakObjectPtr<UPursuitSplineComponent>& vehicleSpline = vehicle->GetAI().RouteFollower.ThisSpline;

							if (GRIP_POINTER_VALID(camera->LinkedPursuitSpline) == false ||
								camera->LinkedPursuitSpline == vehicleSpline)
							{
								// Iterate the splines linked to this camera and see if it matches the vehicle's spline.

								for (TWeakObjectPtr<UPursuitSplineComponent>& linkedSpline : camera->LinkedPursuitSplines)
								{
									if (linkedSpline == vehicleSpline)
									{
										// Now check the distance from the vehicle to the camera, using spline distances.

										float distance = vehicle->GetRaceState().DistanceAlongMasterRacingSpline;
										float speed = vehicle->GetPhysics().VelocityData.Speed;

										if (camera->HookupDelay != 0.0f)
										{
											distance = gameMode->MasterRacingSpline->ClampDistanceAgainstLength(distance - (speed * camera->HookupDelay), distanceLength);
										}

										float difference = gameMode->MasterRacingSpline->GetDistanceDifference(distance, targetDistance, distanceLength, true);

										// difference is negative if lower than the target distance.

										float time = difference / (speed * camera->Duration);

										if (-time < 1.0f &&
											difference < 0.0f)
										{
											numVehicles++;
											lastVehicle = vehicle;

											minDistance = (numVehicles == 1) ? -time : FMath::Min(minDistance, -time);
											maxDistance = (numVehicles == 1) ? -time : FMath::Max(maxDistance, -time);
										}

										break;
									}
								}
							}
						}
					}

					if (minDistance > 0.25f &&
						minDistance < 0.5f &&
						maxDistance > 0.75f &&
						numVehicles >= camera->NumberOfVehicles)
					{
						camera->ResetCameraHit();

						StaticCameraCount++;
						StaticCamera = camera;
						CurrentVehicle = lastVehicle;

						ResetCameraTime();

						SwitchMode(ECinematicCameraMode::StaticCamera);

						return true;
					}
				}
			}
		}
	}

	return false;
}

/**
* Identify a potential spline target.
***********************************************************************************/

bool FCinematicsDirector::IdentifySplineTarget(bool impactEventsOnly)
{
	UGlobalGameState* gameState = UGlobalGameState::GetGlobalGameState(Owner);

	if (gameState->IsGameModeRace() == true)
	{
		APlayGameMode* gameMode = APlayGameMode::Get(Owner);

		struct FSplineTarget
		{
			UAdvancedSplineComponent* Spline;

			ABaseVehicle* Target;

			FSplineSection SplineSection;

			bool CustomSpline;

			float DistanceInto;
		};

		int32 maxImpactIndex = -1;
		float highestImpactTime = 0.0f;
		TArray<FSplineTarget> possibleVehicles;

		// Identifying vehicles on pursuit splines is easy enough - if they're using them and are
		// within the correct distance range then bam.

		bool overhead = (FMath::Rand() & 1) == 0 && (LastClock - LastOverheadView) > 30.0f;

		for (int32 pass = 0; pass < 2; pass++)
		{
			maxImpactIndex = -1;
			highestImpactTime = 0.0f;
			possibleVehicles.Reset();

			GRIP_GAME_MODE_LIST_FOR_FROM(GetPursuitSplines(), pursuitSplines, gameMode);

			for (APursuitSplineActor* pursuitSpline : pursuitSplines)
			{
				TArray<UActorComponent*> splines;

				pursuitSpline->GetComponents(UPursuitSplineComponent::StaticClass(), splines);

				for (UActorComponent* component : splines)
				{
					UPursuitSplineComponent* thisSpline = Cast<UPursuitSplineComponent>(component);

					if (thisSpline->Type != EPursuitSplineType::General)
					{
						continue;
					}

					TArray<FSplineSection>& sections = (overhead == true && pass == 0) ? thisSpline->DroneSections : thisSpline->StraightSections;

					for (FSplineSection& section : sections)
					{
						GRIP_GAME_MODE_LIST_FOR(GetVehicles(), vehicles, Owner);

						for (ABaseVehicle* vehicle : vehicles)
						{
							if (vehicle->GetSpeedKPH() > 200.0f &&
								vehicle->IsVehicleDestroyed() == false &&
								IsVehicleSmoothlyControlled(vehicle) == true)
							{
								if (vehicle->GetAI().RouteFollower.ThisSpline == thisSpline &&
									vehicle->GetAI().RouteFollower.NextSpline == thisSpline)
								{
									TWeakObjectPtr<AActor> impactingActor;

									float maxImpactTime = 2.0f;
									float aboutToImpact = IdentifyImpactEvent(vehicle, impactingActor, maxImpactTime);
									float minTime = (aboutToImpact != 0.0f) ? MinSplineCameraDurationIncoming : MinSplineCameraDuration;
									float minLength = minTime * FMathEx::MetersToCentimeters(FMath::Min(100.0f, vehicle->GetSpeedMPS()));
									float distanceLeft = thisSpline->GetDistanceLeft(vehicle->GetAI().RouteFollower.ThisDistance, section.StartDistance, section.EndDistance);

									if (distanceLeft >= minLength)
									{
										// Ladies and gentlemen, we have a candidate.

										float distanceInto = thisSpline->GetDistanceInto(vehicle->GetAI().RouteFollower.ThisDistance, section.StartDistance, section.EndDistance);

										FSplineTarget splineTarget = { thisSpline, vehicle, section, false, distanceInto };

										possibleVehicles.Emplace(splineTarget);

										if (aboutToImpact != 0.0f &&
											highestImpactTime < aboutToImpact &&
											aboutToImpact <= maxImpactTime)
										{
											highestImpactTime = aboutToImpact;
											maxImpactIndex = possibleVehicles.Num() - 1;
										}
									}
								}
							}
						}
					}
				}
			}

			if (pass == 0 &&
				overhead == true &&
				possibleVehicles.Num() == 0)
			{
				overhead = false;
			}
			else
			{
				break;
			}
		}

		if (impactEventsOnly == true &&
			maxImpactIndex == -1)
		{
			return false;
		}

		if (possibleVehicles.Num() > 0)
		{
			// We have some candidates, so pick one at random.

			int32 index = FMath::Rand() % possibleVehicles.Num();
			FSplineTarget splineTarget = possibleVehicles[index];

			if (maxImpactIndex != -1)
			{
				splineTarget = possibleVehicles[maxImpactIndex];
			}
			else if (GRIP_POINTER_VALID(CurrentVehicle) == true)
			{
				// Try to find a match that matches the current vehicle if possible.
				// No worries if not.

				for (FSplineTarget& possibleVehicle : possibleVehicles)
				{
					if (possibleVehicle.Target == CurrentVehicle)
					{
						splineTarget = possibleVehicle;
						break;
					}
				}
			}

			float aboutToImpact = IdentifyImpactEvent(splineTarget.Target, ImpactingActor, 4.0f);
			float minTime = (aboutToImpact != 0.0f) ? MinSplineCameraDurationIncoming : MinSplineCameraDuration;

			SplineCamera = FSplineCamera(splineTarget.SplineSection.StartDistance, splineTarget.SplineSection.EndDistance);

			SplineCamera.Spline = splineTarget.Spline;
			SplineCamera.Target = splineTarget.Target;
			SplineCamera.DistanceAlongSpline = SplineCamera.TargetDistanceAlongSpline = SplineCamera.Target->GetAI().RouteFollower.ThisDistance;
			SplineCamera.LastRotation = SplineCamera.Spline->GetDirection(SplineCamera.DistanceAlongSpline).ToOrientationRotator();
			SplineCamera.DynamicFOV.DynamicZoom = (FMath::Rand() & 1) == 0;
			SplineCamera.RollingYawTracking = (FMath::Rand() & 1) == 0;
			SplineCamera.LastTargetLocation = splineTarget.Target->GetCenterLocation();

			if (aboutToImpact != 0.0f)
			{
				// Setup the camera to view an incoming impact event.

				SplineCamera.ViewDirection = ECameraViewDirection::Backwards;
				SplineCamera.StartTransition = ECameraStartTransition::SlowUp;
				SplineCamera.DynamicFOV.DynamicZoom = false;
				SplineCamera.DynamicFOV.TargetScreenProportion *= 0.75f;
				SplineCamera.HeightAboveGround = 2.5f * 100.0f + SplineCamera.Target->HoverDistance;
			}
			else
			{
				if ((FMath::Rand() & 1) == 0)
				{
					SplineCamera.HeightAboveGround = 2.5f * 100.0f + SplineCamera.Target->HoverDistance;
				}
				else
				{
					SplineCamera.HeightAboveGround = 4.0f * 100.0f + SplineCamera.Target->HoverDistance;
				}

				switch (FMath::Rand() & 3)
				{
				case 0:
					SplineCamera.DynamicFOV.TargetScreenProportion *= 0.75f;
					break;
				case 1:
					SplineCamera.DynamicFOV.TargetScreenProportion *= 1.25f;
					break;
				default:
					break;
				}
			}

			if (SplineCamera.ViewDirection == ECameraViewDirection::Random)
			{
				if (overhead == true &&
					splineTarget.DistanceInto >= 10.0f * 100.0f)
				{
					SplineCamera.ViewDirection = ECameraViewDirection::Overhead;
				}
				else
				{
					switch (FMath::Rand() & 3)
					{
					default:
						SplineCamera.ViewDirection = ECameraViewDirection::Forwards;
						break;
					case 0:
						SplineCamera.ViewDirection = ECameraViewDirection::Backwards;
						break;
					case 1:
						if (splineTarget.Target->GetSpeedKPH() > 450.0f)
						{
							SplineCamera.ViewDirection = ECameraViewDirection::Crossover;
						}
						else
						{
							SplineCamera.ViewDirection = ECameraViewDirection::Forwards;
						}
						break;
					}
				}
			}

			if (SplineCamera.ViewDirection == ECameraViewDirection::Overhead)
			{
				SplineCamera.DynamicFOV.TargetScreenProportion = 1.0f / 8.0f;
				SplineCamera.HeightAboveGround = 66.0f * 100.0f;
				SplineCamera.DynamicFOV.FieldOfView = 50.0f;
				SplineCamera.DynamicFOV.MinFieldOfView = 30.0f;
				SplineCamera.DynamicFOV.MaxFieldOfView = 50.0f;
			}
			else if (SplineCamera.ViewDirection == ECameraViewDirection::Crossover)
			{
				SplineCamera.DynamicFOV.DynamicZoom = false;

				if ((FMath::Rand() & 3) == 0)
				{
					SplineCamera.DynamicFOV.FieldOfView = SplineCamera.DynamicFOV.MaxFieldOfView = SplineCamera.DynamicFOV.MinFieldOfView = 35.0f;
				}
				else
				{
					SplineCamera.DynamicFOV.FieldOfView = SplineCamera.DynamicFOV.MaxFieldOfView = SplineCamera.DynamicFOV.MinFieldOfView = 50.0f;
				}
			}

			if (SplineCamera.DynamicFOV.DynamicZoom == true)
			{
				SplineCamera.DynamicFOV.FieldOfView = SplineCamera.DynamicFOV.TargetFieldOfView = SplineCamera.DynamicFOV.MaxFieldOfView;
				SplineCamera.DynamicFOV.LastFieldOfViewChangeTime = gameMode->GetRealTimeClock();
			}

			SplineCamera.LongitudinalDistanceFromTarget = UAdvancedCameraComponent::GetDistanceForRadius(3.0f * 100.0f, SplineCamera.DynamicFOV.TargetScreenProportion, SplineCamera.DynamicFOV.FieldOfView);

			if (SplineCamera.ViewDirection == ECameraViewDirection::Sideways)
			{
				SplineCamera.LongitudinalDistanceFromTarget = 0.0f;
			}
			else if (SplineCamera.ViewDirection == ECameraViewDirection::Forwards)
			{
				SplineCamera.LongitudinalDistanceFromTarget *= -1.0f;
			}
			else if (SplineCamera.ViewDirection == ECameraViewDirection::Overhead)
			{
				if ((FMath::Rand() & 3) == 0 ||
					splineTarget.DistanceInto < 50.0f * 100.0f)
				{
					SplineCamera.LongitudinalDistanceFromTarget = -10.0f * 100.0f;
				}
				else
				{
					SplineCamera.HeightAboveGround = 50.0f * 100.0f;
					SplineCamera.LongitudinalDistanceFromTarget = -50.0f * 100.0f;
				}
			}

			SplineCamera.StartDistanceOffset = SplineCamera.EndDistanceOffset = SplineCamera.LongitudinalDistanceFromTarget;

			UPursuitSplineComponent* pursuitSpline = Cast<UPursuitSplineComponent>(SplineCamera.Spline.Get());

			if (SplineCamera.StartTransition == ECameraStartTransition::Random)
			{
				if (SplineCamera.ViewDirection == ECameraViewDirection::Overhead)
				{
					SplineCamera.StartTransition = ECameraStartTransition::None;
				}
				else if (SplineCamera.ViewDirection == ECameraViewDirection::Crossover)
				{
					if ((FMath::Rand() % 2) == 0)
					{
						SplineCamera.StartTransition = ECameraStartTransition::CrossoverBackwards;
					}
					else
					{
						SplineCamera.StartTransition = ECameraStartTransition::CrossoverForwards;
					}
				}
				else
				{
					int32 rand = FMath::Rand() & 7;

					if (rand == 0 &&
						SplineCamera.ViewDirection == ECameraViewDirection::Backwards)
					{
						// Take a look at the car and see if it's close to the spline and heading in the right direction.

						if (SplineCamera.Target->IsDrivingStraightAndNarrow() == true)
						{
							SplineCamera.StartTransition = ECameraStartTransition::Rotate;
							SplineCamera.RollingYawTracking = false;
						}
					}

					if (SplineCamera.StartTransition == ECameraStartTransition::Random)
					{
						rand &= 3;

						if (rand == 1 &&
							pursuitSpline != nullptr &&
							pursuitSpline->GetTunnelDiameterOverDistance(splineTarget.Target->GetAI().RouteFollower.ThisDistance, 250.0f * 100.0f, 1, true) > 30.0f * 100.0f &&
							SplineCamera.Target->GetLaunchDirection().Z > 0.0f)
						{
							SplineCamera.StartTransition = ECameraStartTransition::Lower;
						}
						else if (rand == 2)
						{
							SplineCamera.StartTransition = ECameraStartTransition::SlowUp;
						}
						else
						{
							SplineCamera.StartTransition = ECameraStartTransition::SpeedUp;
						}
					}
				}
			}

			if (SplineCamera.ViewDirection == ECameraViewDirection::Crossover)
			{
				if (SplineCamera.StartTransition == ECameraStartTransition::CrossoverBackwards)
				{
					SplineCamera.EasingDelta = 1.0f / 5.0f;
					SplineCamera.StartDistanceOffset = SplineCamera.LongitudinalDistanceFromTarget * -5.0f;
					SplineCamera.EndDistanceOffset = SplineCamera.LongitudinalDistanceFromTarget * 2.0f;
				}
				else
				{
					SplineCamera.EasingDelta = 1.0f / 3.5f;
					SplineCamera.StartDistanceOffset = SplineCamera.LongitudinalDistanceFromTarget * 7.5f;
					SplineCamera.EndDistanceOffset = SplineCamera.StartDistanceOffset * -1.0f;
				}

				SplineCamera.LongitudinalDistanceFromTarget = 0.0f;
				SplineCamera.RollingYawTracking = false;
			}
			else if (SplineCamera.StartTransition == ECameraStartTransition::SpeedUp)
			{
				SplineCamera.StartDistanceOffset -= 25.0f * 100.0f;
			}
			else if (SplineCamera.StartTransition == ECameraStartTransition::SlowUp)
			{
				if (SplineCamera.ViewDirection == ECameraViewDirection::Forwards)
				{
					SplineCamera.StartDistanceOffset += SplineCamera.LongitudinalDistanceFromTarget * 1.3f;
				}
				else
				{
					SplineCamera.StartDistanceOffset += SplineCamera.LongitudinalDistanceFromTarget * 2.0f;
				}
			}

			if (splineTarget.DistanceInto + SplineCamera.StartDistanceOffset >= 0.0f)
			{
				// Only use this spline if the starting distance is still inside of the spline section we identified.

				SplineCamera.VisibilityQueryParams.AddIgnoredActor(SplineCamera.Target.Get());

				SplineCamera.Target->StartWatchingOnSpline();

				float speedSeconds = FMathEx::MetersToCentimeters(FMath::Max(100.0f, SplineCamera.Target->GetSpeedMPS()));
				float timeLeft = SplineCamera.Spline->GetDistanceLeft(SplineCamera.Target->GetAI().RouteFollower.ThisDistance, splineTarget.SplineSection.StartDistance, splineTarget.SplineSection.EndDistance) / speedSeconds;

				if (SplineCamera.ViewDirection == ECameraViewDirection::Crossover)
				{
					SplineCamera.EndClock = (1.0f / SplineCamera.EasingDelta);
					SplineCamera.EasingDelta *= 0.4f;
				}
				else
				{
					SplineCamera.EndClock = FMath::FRandRange((float)minTime, FMath::Min(timeLeft, (float)MaxSplineCameraDuration));
				}

				SplineCamera.Tick(0.0f, true);

				CurrentVehicle = SplineCamera.Target;

				ResetCameraTime();

				SwitchMode((impactEventsOnly == true) ? ECinematicCameraMode::SplineFollowingVictimVehicle : ECinematicCameraMode::SplineFollowingVehicle);

				return true;
			}
		}
	}

	return false;
}

/**
* Find a good forward facing camera point on a given vehicle.
***********************************************************************************/

UCameraPointComponent* FCinematicsDirector::FindForeFacingCameraPoint(ABaseVehicle* vehicle) const
{
	float maxX = -100.0f * 100.0f;
	TArray<UActorComponent*> components;
	UCameraPointComponent* cameraPoint = nullptr;

	vehicle->GetComponents(UCameraPointComponent::StaticClass(), components);

	for (UActorComponent* component : components)
	{
		UCameraPointComponent* camera = Cast<UCameraPointComponent>(component);

		if ((camera != StockCameraPoint) &&
			(camera->InvertWithVehicle == true))
		{
			float x = camera->GetRelativeLocation().X * camera->GetRelativeScale3D().X;
			float z = camera->GetRelativeLocation().Z * camera->GetRelativeScale3D().Z;

			if (maxX < x &&
				FMath::Abs(z) > 50.0f &&
				FMath::Abs(camera->GetRelativeRotation().Yaw) < 25.0f)
			{
				maxX = x;
				cameraPoint = camera;
			}
		}
	}

	return cameraPoint;
}

/**
* Switch the current cinematic camera mode.
***********************************************************************************/

void FCinematicsDirector::SwitchMode(ECinematicCameraMode mode)
{
	ABaseVehicle* vehicle = Cast<ABaseVehicle>(Owner);

	CameraModeTimer = 0.0f;
	CameraShotTimer = 0.0f;
	AdjustedYaw = 0.0f;
	TargetHiddenTime = 0.0f;
	CinematicCameraMode = mode;

	DynamicFOV.DynamicZoom = false;

	APlayGameMode* gameMode = APlayGameMode::Get(Owner);

	if (mode == ECinematicCameraMode::CameraPointVehicleToProjectile)
	{
		DynamicFOV.DynamicZoom = true;
		DynamicFOV.FieldOfViewBreakIn = 5.0f;
		DynamicFOV.FieldOfViewBreakOut = 10.0f;
		DynamicFOV.MinFieldOfView = CurrentCameraPoint->FieldOfView * 0.5f;
		DynamicFOV.MaxFieldOfView = CurrentCameraPoint->FieldOfView;
		DynamicFOV.FieldOfView = DynamicFOV.TargetFieldOfView = DynamicFOV.MaxFieldOfView;
		DynamicFOV.LastFieldOfViewChangeTime = gameMode->GetRealTimeClock();
		DynamicFOV.TargetScreenProportion = 1.0f / 6.0f;
	}

	if (TimeSlowed == true)
	{
		TimeSlowed = false;

		gameMode->ChangeTimeDilation(1.0f, 0.0f);
	}

	if (mode == ECinematicCameraMode::CameraPointVehicle ||
		mode == ECinematicCameraMode::CameraPointVehicleToGun)
	{
		if (GRIP_POINTER_VALID(CurrentVehicle) == true &&
			CurrentVehicle->IsVehicleDestroyed() == true)
		{
			QueueVehicle();
		}

		QueueCamerasForVehicle();
	}

	if (vehicle != nullptr)
	{
		vehicle->Camera->CameraFeed.Reset();

		APlayerController* playerController = Cast<APlayerController>(vehicle->GetController());

		if (playerController != nullptr)
		{
			playerController->PlayerCameraManager->SetGameCameraCutThisFrame();
		}
	}
}

/**
* Can we slow time at this point?
***********************************************************************************/

bool FCinematicsDirector::CanSlowTime(bool atAnyPoint) const
{
	UGlobalGameState* gameState = UGlobalGameState::GetGlobalGameState(Owner);

	if (gameState->UsingSplitScreen() == false)
	{
		if (atAnyPoint == true)
		{
			return true;
		}

		APlayGameMode* gameMode = APlayGameMode::Get(Owner);

		if (TimeSlowed == false &&
			gameMode->IsUsingTimeDilation() == false)
		{
			return true;
		}
	}

	return false;
}

/**
* Does the current camera view require this vehicle's spring-arm to be active?
***********************************************************************************/

bool FCinematicsDirector::RequiresActiveSpringArm(ABaseVehicle* vehicle)
{
	// Ideally we'd like these spline following cameras back in without them jittering about with remote vehicles.

	if (IsActive() == true &&
		CurrentVehicle == vehicle &&
		IsVehicleSmoothlyControlled(vehicle) == false &&
		(CinematicCameraMode == ECinematicCameraMode::SplineFollowingVehicle || CinematicCameraMode == ECinematicCameraMode::SplineFollowingVictimVehicle))
	{
		return true;
	}

	if (IsActive() == true &&
		CurrentVehicle == vehicle &&
		CyclingVehicles == true)
	{
		return true;
	}

	return false;
}

/**
* Create a stock camera point on a vehicle to be used for a viewing platform.
***********************************************************************************/

void FCinematicsDirector::CreateStockPointCamera()
{
	if (GRIP_OBJECT_VALID(StockCameraPoint) == false &&
		GRIP_POINTER_VALID(CurrentVehicle) == true)
	{
		StockCameraPoint = NewObject<UCameraPointComponent>(CurrentVehicle.Get());

		StockCameraPoint->ClipLocation = false;
		StockCameraPoint->FieldOfView = 75.0f;

		StockCameraPoint->RegisterComponent();

		GRIP_ATTACH_AT(StockCameraPoint, CurrentVehicle->VehicleMesh, "RootDummy", FVector(-100.0f, 0.0f, 110.0f));
	}
}

/**
* Update a dynamic field of view to keep a target in screen proportion bounds within
* the guidelines that we're given.
***********************************************************************************/

void FCinematicsDirector::UpdateDynamicFieldOfView(float deltaSeconds, bool allowInChanges, bool allowOutChanges, AActor* cameraTarget, const FVector& location, FDynamicFOV& dynamicFOV, bool timeSlowed)
{
	// Manage the dynamic zoom.

	if (dynamicFOV.DynamicZoom == true &&
		GRIP_OBJECT_VALID(cameraTarget) == true)
	{
		float fov = UAdvancedCameraComponent::GetFieldOfViewForRadius(location, cameraTarget->GetActorLocation(), 3.0f * 100.0f, dynamicFOV.TargetScreenProportion);

		fov = FMath::Clamp(fov, dynamicFOV.MinFieldOfView, dynamicFOV.MaxFieldOfView);

		if ((fov > dynamicFOV.FieldOfView && fov - dynamicFOV.FieldOfView > dynamicFOV.FieldOfViewBreakOut && allowOutChanges == true) ||
			(fov < dynamicFOV.FieldOfView && dynamicFOV.FieldOfView - fov > dynamicFOV.FieldOfViewBreakIn && allowInChanges == true))
		{
			// Only change if the field of view is different enough.

			APlayGameMode* gameMode = APlayGameMode::Get(cameraTarget);
			float clock = gameMode->GetRealTimeClock();
			float minTime = (timeSlowed == true && fov > dynamicFOV.FieldOfView) ? 3.0f : 2.0f;

			if (clock - dynamicFOV.LastFieldOfViewChangeTime > minTime)
			{
				// Only change a minimum of two seconds after the last change.

				dynamicFOV.LastFieldOfViewChangeTime = clock;
				dynamicFOV.TargetFieldOfView = fov;
				dynamicFOV.FieldOfViewChangeRate = FMath::Abs(fov - dynamicFOV.FieldOfView) * 3.0f;
			}
		}

		if (dynamicFOV.FieldOfView != dynamicFOV.TargetFieldOfView)
		{
			// If we're currently animating the field of view.

			if (dynamicFOV.FieldOfView < dynamicFOV.TargetFieldOfView &&
				dynamicFOV.TargetFieldOfView < fov)
			{
				// If the current field of view is less than the target,
				// and the target is less than the desired field of view,
				// then extend the target out to meet it.

				dynamicFOV.TargetFieldOfView = fov;
			}
			else if (dynamicFOV.FieldOfView > dynamicFOV.TargetFieldOfView &&
				dynamicFOV.TargetFieldOfView > fov)
			{
				// If the current field of view is greater than the target,
				// and the target is greater than the desired field of view,
				// then reduce the target down to meet it.

				dynamicFOV.TargetFieldOfView = fov;
			}

			// Animate the current field of view up to the target.

			dynamicFOV.FieldOfView = FMathEx::GravitateToTarget(dynamicFOV.FieldOfView, dynamicFOV.TargetFieldOfView, deltaSeconds * dynamicFOV.FieldOfViewChangeRate);
		}
	}
}

/**
* Do the regular update tick.
***********************************************************************************/

void FSplineCamera::Tick(float deltaSeconds, bool modeReset)
{
	if (IsInUse() == true)
	{
		modeReset |= (Clock == 0.0f);

		Clock += deltaSeconds;

		float endTransitionTime = EndTransitionTime;
		float distance = Target->GetAI().RouteFollower.ThisDistance;
		float speedSeconds = FMathEx::MetersToCentimeters(FMath::Max(100.0f, Target->GetSpeedMPS())) * Target->GetActorTimeDilation();

		if (Target->GetAI().RouteFollower.ThisSpline != Spline)
		{
			distance = Spline->GetNearestDistance(Target->GetActorLocation(), 0.0f, 0.0f, 4, 50, 0.0f);
		}

		float distanceLeft = Spline->GetDistanceLeft(distance, StartDistance, EndDistance);
		float timeLeft = distanceLeft / speedSeconds;

		if (EasingDirection != -1 &&
			EndClock - Clock > timeLeft &&
			ViewDirection != ECameraViewDirection::Crossover)
		{
			// Ensure that the end time doesn't exceed the amount of time we have left on the spline.

			EndClock = Clock + timeLeft;
		}

		if (EasingDirection != -1 &&
			DistanceOffsetTime == 1.0f &&
			Clock > EndClock - endTransitionTime &&
			ViewDirection != ECameraViewDirection::Crossover)
		{
			// Setup the easing out of the camera. We decide that here and not when the camera is
			// created as it can make more dynamic decisions based on the current state of the
			// target vehicle.

			if (EndTransition == ECameraEndTransition::Random)
			{
				float speedKPH = Target->GetSpeedKPH();
				UPursuitSplineComponent* pursuitSpline = Cast<UPursuitSplineComponent>(Spline.Get());

				if ((FMath::Rand() % 3) == 0 &&
					speedKPH > 100.0f &&
					Target->GetLaunchDirection().Z > 0.0f &&
					ViewDirection != ECameraViewDirection::Overhead &&
					pursuitSpline != nullptr &&
					pursuitSpline->GetTunnelDiameterOverDistance(distance, endTransitionTime * speedSeconds * 1.25f, 1, true) > 30.0f * 100.0f)
				{
					EndTransition = ECameraEndTransition::Raise;
				}
				else if (FMath::Rand() & 1 &&
					speedKPH > 100.0f &&
					ViewDirection == ECameraViewDirection::Forwards)
				{
					EndTransition = ECameraEndTransition::SpeedUp;
				}
				else if (speedKPH > 100.0f)
				{
					EndTransition = ECameraEndTransition::SlowUp;
				}
				else
				{
					EndTransition = ECameraEndTransition::None;
				}
			}

			StartDistanceOffset = CurrentDistanceOffset;

			// Ease in / out over endTransitionTime seconds.

			EasingDelta = 1.0f / endTransitionTime;
			EndDistanceOffset = CurrentDistanceOffset;

			if (EndTransition == ECameraEndTransition::SlowUp)
			{
				if (ViewDirection == ECameraViewDirection::Forwards)
				{
					// Let the target vehicle speed off into the distance.

					EndDistanceOffset -= LongitudinalDistanceFromTarget * 2.0f;
				}
				else
				{
					// Slow the camera just enough to get behind the car and no more.

					EndDistanceOffset -= LongitudinalDistanceFromTarget * 1.5f;
				}

				if (timeLeft >= endTransitionTime)
				{
					// Allow up to 4 seconds for the end transition if we have the room left on the spline for it.

					EasingDelta = 1.0f / FMath::Min(timeLeft, 4.0f);
					EndClock = Clock + (1.0f / EasingDelta);
				}
			}
			else if (EndTransition == ECameraEndTransition::SpeedUp)
			{
				// Speed up the camera just enough to get in front of the car and no more.

				EndDistanceOffset += LongitudinalDistanceFromTarget * 1.5f;

				if (timeLeft >= endTransitionTime)
				{
					// Allow up to 4 seconds for the end transition if we have the room left on the spline for it.

					EasingDelta = 1.0f / FMath::Min(timeLeft, 4.0f);
					EndClock = Clock + (1.0f / EasingDelta);
				}
			}

			EasingDirection = -1;
			DistanceOffsetTime = 0.0f;
		}

		// Handle the transition into and out of the shot.

		DistanceOffsetTime = FMath::Min(DistanceOffsetTime + (deltaSeconds * EasingDelta), 1.0f);

		if (EasingDirection == -1)
		{
			// Easing out at end of shot.

			if (EndTransition == ECameraEndTransition::Raise)
			{
				DistanceAboveSpline = FMath::InterpEaseIn(0.0f, 10.0f * 100.0f, DistanceOffsetTime, 3);
			}

			CurrentDistanceOffset = FMath::InterpEaseIn(StartDistanceOffset, EndDistanceOffset, DistanceOffsetTime, 3);
		}
		else if (EasingDirection == 1)
		{
			// Easing in at beginning of shot.

			if (StartTransition == ECameraStartTransition::Lower)
			{
				DistanceAboveSpline = FMath::InterpEaseOut(8.0f * 100.0f, 0.0f, DistanceOffsetTime, 3);
			}

			if (StartTransition == ECameraStartTransition::CrossoverBackwards)
			{
				CurrentDistanceOffset = FMath::InterpEaseOut(StartDistanceOffset, EndDistanceOffset, DistanceOffsetTime, 4);
			}
			else
			{
				CurrentDistanceOffset = FMath::InterpEaseOut(StartDistanceOffset, EndDistanceOffset, DistanceOffsetTime, 3);
			}
		}
		else
		{
			// Not easing at all, in the meat of the shot.

			CurrentDistanceOffset = EndDistanceOffset;
		}

		// Determine how far the target is along the spline and whether it is still a valid target.

		bool invalidTarget = false;
		TWeakObjectPtr<UPursuitSplineComponent>& targetSpline = Target->GetAI().RouteFollower.ThisSpline;

		{
			FVector targetLocation = Target->GetCenterLocation();
			float distanceThisFrame = (targetLocation - LastTargetLocation).Size();
			float range = distanceThisFrame * 10.0f;
			float t0 = TargetDistanceAlongSpline - range;
			float t1 = TargetDistanceAlongSpline + range;

			LastTargetLocation = targetLocation;
			TargetDistanceAlongSpline = Spline->GetNearestDistance(targetLocation, t0, t1, 5, Spline->GetNumSamplesForRange(range * 2.0f, 5), 0.0f);

			if (targetSpline != Spline)
			{
				// If the target has transitioned to another spline then we need to check it's still
				// usable as a viewing target.

				if (TargetHiddenTime > 1.5f)
				{
					invalidTarget = true;
				}
				else
				{
					// Check the direction vectors for the two splines and make sure they're broadly parallel.

					FVector d0 = Spline->GetDirection(TargetDistanceAlongSpline);
					FVector d1 = targetSpline->GetDirection(Target->GetAI().RouteFollower.ThisDistance);

					if (FMathEx::DotProductToDegrees(FVector::DotProduct(d0, d1)) > 45.0f)
					{
						invalidTarget = true;
					}
				}
			}
		}

		bool reachedEndTime = Clock >= EndClock;
		bool reachedEndDistance = distanceLeft < KINDA_SMALL_NUMBER;

		if (reachedEndTime == true ||
			reachedEndDistance == true ||
			invalidTarget == true ||
			(Target->GetSpeedKPH() < 100.0f && Clock > FCinematicsDirector::MinCameraDuration))
		{
			// Kill the camera if it's no longer any good.

			Target->StopWatchingOnSpline();

			Spline = nullptr;
			Target = nullptr;
		}
		else
		{
			DistanceAlongSpline = TargetDistanceAlongSpline;

			float trackingOffset = FMathEx::UpdateOscillator(TrackingOffset1, Target->PerlinNoise, deltaSeconds) * 2.0f;
			trackingOffset += FMathEx::UpdateOscillator(TrackingOffset2, Target->PerlinNoise, deltaSeconds) * 5.0f;

			// Calculate the location again if smoothing in or out in order to clamp rotations.
			// Note that targetLocation is where the camera should be in its natural viewing position - irrespective of easing in / out.

			FVector targetLocation;

			GetSplineWorldLocation(Target->GetCenterLocation(), CurrentDistanceOffset + trackingOffset, deltaSeconds, modeReset, WorldLocation, targetLocation);

			if (ProjectedDistanceAlongSpline < 0.0f)
			{
				// This spline is bad for following right now, so switch.

				// Kill the camera if it's no longer any good.

				Target->StopWatchingOnSpline();

				Spline = nullptr;
				Target = nullptr;
			}
			else
			{
				// The rotation focuses on the targetLocation, which isn't always the target itself but some
				// offset from it during transitions.

				FRotator targetRotation = (Target->GetCenterLocation() - targetLocation).ToOrientationRotator();

				if (ViewDirection == ECameraViewDirection::Overhead)
				{
					float scale = FMathEx::GetRatio(DynamicFOV.FieldOfView, DynamicFOV.MinFieldOfView, DynamicFOV.MaxFieldOfView);
					FVector cameraAimPoint = Target->GetCenterLocation() + (Target->GetVelocity() * FMath::Lerp(0.01f, 0.05f, scale));
					float offset = FMath::Lerp(200.0f, 400.0f, scale);

					cameraAimPoint += FVector(FMath::Sin(Clock * 1.0f) * offset, FMath::Sin((Clock + 1.0f) * 1.25f) * offset, 0.0f);

					targetRotation = (cameraAimPoint - targetLocation).ToOrientationRotator();
				}
				else if (ViewDirection == ECameraViewDirection::Crossover)
				{
					targetRotation = (Target->GetCenterLocation() - WorldLocation).ToOrientationRotator();
				}

				if (modeReset == true)
				{
					LastRotation = targetRotation;
				}
				else
				{
					float lag = FMath::Lerp(0.8f, 0.9f, (DynamicFOV.FieldOfView - 40.0f) / 20.0f);

					if (ViewDirection == ECameraViewDirection::Overhead)
					{
						lag = 0.95f;
					}
					else if (ViewDirection == ECameraViewDirection::Crossover)
					{
						if (StartTransition == ECameraStartTransition::CrossoverBackwards)
						{
							lag = FMath::Lerp(0.875f, 0.925f, FMathEx::GetRatio(DynamicFOV.FieldOfView, 35.0f, 50.0f));
						}
						else
						{
							lag = FMath::Lerp(0.85f, 0.9f, FMathEx::GetRatio(DynamicFOV.FieldOfView, 35.0f, 50.0f));
						}
					}

					lag = FMath::Clamp(lag, 0.25f, 0.95f);

					if (EasingDirection == -1 &&
						EndTransition == ECameraEndTransition::Raise)
					{
						lag = FMath::Lerp(lag, 0.999f, FMath::Min(1.0f, DistanceOffsetTime * 1.5f));
					}

					LastRotation = FMathEx::GetSmoothedRotation(LastRotation, targetRotation, deltaSeconds, lag, lag, lag);
				}

				float speedShakeAmount = Target->Camera->RadialSpeedBlurVsSpeed.GetRichCurve()->Eval(Target->GetSpeedKPH());
				FVector speedShakeOffset = FVector::ZeroVector;
				float speedShakeAmplitude = 0.175f;
				float speedShakeFrequency = 0.5f;

				if (speedShakeAmount > 0.0f)
				{
					float deltaTime = deltaSeconds / speedShakeFrequency;

					speedShakeOffset.Y = FMathEx::UpdateOscillator(SpeedShakeX, Target->PerlinNoise, deltaTime) * 0.3f;
					speedShakeOffset.Z = FMathEx::UpdateOscillator(SpeedShakeY, Target->PerlinNoise, deltaTime);

					speedShakeOffset *= speedShakeAmount * speedShakeAmplitude;
				}

				WorldLocation += speedShakeOffset;

				if (targetSpline != Spline)
				{
					// Check to see if the target is visible and stop watching them after a short time
					// if they're not.

					FHitResult hit;
					FVector testPosition = Target->GetCenterLocation() + Target->GetLaunchDirection() * 2.0f * 100.0f;

					if (Target->GetWorld()->LineTraceSingleByChannel(hit, WorldLocation, testPosition, ABaseGameMode::ECC_LineOfSightTest, VisibilityQueryParams) == false)
					{
						TargetHiddenTime = 0.0f;
					}
					else
					{
						TargetHiddenTime += deltaSeconds;
					}
				}
			}
		}

		// Manage the dynamic zoom.

		if (IsInUse() == true)
		{
			FCinematicsDirector::UpdateDynamicFieldOfView(deltaSeconds, DistanceOffsetTime >= 1.0f - KINDA_SMALL_NUMBER && EasingDirection != -1, DistanceOffsetTime >= 1.0f - KINDA_SMALL_NUMBER || EasingDirection == -1, Target.Get(), WorldLocation, DynamicFOV, false);
		}
	}
}

/**
* Get the distance along a spline for a given vehicle location.
***********************************************************************************/

float FSplineCamera::GetSplineDistance(float distanceAlongSpline, const FVector& vehicleLocation, float offset, float deltaSeconds, FVector& newLocation)
{
	float offsetDistanceAlongSpline = Spline->ClampDistance(distanceAlongSpline + offset);

	if (StartTransition == ECameraStartTransition::CrossoverForwards)
	{
		// NOTE: This method jerks around a little bit because of the spline parametrization
		// table not having enough accuracy. We've already set it to be as accurate as possible.

		newLocation = Spline->GetWorldLocationAtDistanceAlongSpline(offsetDistanceAlongSpline);
	}
	else
	{
		// NOTE: This method is smoother but is susceptible to sudden slides for a moment which
		// we attempt to dampen here, mostly successfully.

		float cmsRange = FMath::Abs(offset) + (50.0f * 100.0f);
		FVector direction = Spline->GetDirection(distanceAlongSpline);

		direction.Normalize();

		if (SplineDirection != FVector::ZeroVector)
		{
			// Dampen spline direction changes to stop shifting around.

			float lag = 0.99f;

			direction = FMathEx::GetSmoothedRotation(SplineDirection.ToOrientationRotator(), direction.ToOrientationRotator(), deltaSeconds, lag, lag, lag).Vector();

			direction.Normalize();
		}

		SplineDirection = direction;

		// Imagine a plane where the vehicle is, pointing in the direction of the spline
		// at the point where the camera is on that spline.

		// Use a plane parallel to it, at the desired distance away on which the camera
		// should sit.

		// Find the closest point on that plane that the estimated camera position is.
		// This becomes the new spline point.

		// Now get a really accurate position on this spline of newLocation.

		// Get the nearest distance to the plane at newLocation and direction.

		newLocation = vehicleLocation + direction * offset;
		offsetDistanceAlongSpline = Spline->GetNearestDistance(newLocation, direction, offsetDistanceAlongSpline - cmsRange, offsetDistanceAlongSpline + cmsRange, 5, Spline->GetNumSamplesForRange(cmsRange * 2.0f, 5), 0.0f);

		// Get the world location at the distance and reproject onto the original plane.

		FVector oldLocation = Spline->GetWorldLocationAtDistanceAlongSpline(offsetDistanceAlongSpline);

		newLocation = FVector::PointPlaneProject(oldLocation, newLocation, direction);

		if ((oldLocation - newLocation).Size() > 100.0f)
		{
			// Something ain't right, probably the spline direction is damped too much and way
			// out of alignment with the spline itself. In this case, signal an error so we
			// ditch the spline camera.

			return -1.0f;
		}

		// newLocation is now the location of the camera on the spline as projected onto
		// the plane described by the vehicle location and the spline direction. This will
		// be very close to the world location described by offsetDistanceAlongSpline and
		// only really different at all because we don't want to see any jitter in the
		// camera's movement compared to its target vehicle as it can be very noticeable.
	}

	return offsetDistanceAlongSpline;
}

/**
* Get the world location for given point along the spline taking into account smoothing.
***********************************************************************************/

void FSplineCamera::GetSplineWorldLocation(const FVector& vehicleLocation, float splineDistanceOffset, float deltaSeconds, bool reset, FVector& currentLocation, FVector& targetLocation)
{
	float ratio1 = FMathEx::GetSmoothingRatio(0.99f, deltaSeconds);
	UPursuitSplineComponent* pursuitSpline = Cast<UPursuitSplineComponent>(Spline.Get());

	if (reset == true)
	{
		ratio1 = 0.0f;
	}

	float lastDistance = ProjectedDistanceAlongSpline;

	for (int32 index = 0; index < 2; index++)
	{
		FVector location = vehicleLocation;
		float thisDistanceOffset = ((index == 0) ? splineDistanceOffset : LongitudinalDistanceFromTarget);
		float thisDistanceAlongSpline = GetSplineDistance(DistanceAlongSpline, vehicleLocation, thisDistanceOffset, deltaSeconds, location);

		if (index == 0)
		{
			if (reset == false &&
				deltaSeconds != 0.0f)
			{
				if (ViewDirection != ECameraViewDirection::Crossover)
				{
					float length = Spline->GetSplineLength();
					float quarterLength = length * 0.25f;

					if (thisDistanceAlongSpline < lastDistance)
					{
						if ((Spline->IsClosedLoop() == false) ||
							((thisDistanceAlongSpline < quarterLength && lastDistance > length - quarterLength) == false))
						{
							// No going backwards.

							thisDistanceAlongSpline = lastDistance;
						}
					}
				}
			}

			ProjectedDistanceAlongSpline = thisDistanceAlongSpline;
		}

		FVector splineDirection = Spline->GetDirection(thisDistanceAlongSpline);
		FRotator splineRotation = splineDirection.ToOrientationRotator();
		FVector splineLocation = Spline->GetWorldLocationAtDistanceAlongSpline(thisDistanceAlongSpline);
		FVector groundOffset = (pursuitSpline != nullptr) ? pursuitSpline->GetWorldClosestOffset(thisDistanceAlongSpline, false) : FVector(0.0f, 0.0f, -500.0f);

		// Smooth the groundOffset so that it doesn't snap around - in spline space.

		groundOffset = splineRotation.UnrotateVector(groundOffset);

		// Smooth the change of the ground offset.

		float groundOffsetLength = groundOffset.Size();
		float offsetLength = FMath::Lerp(groundOffsetLength, LastSplineOffset[index].Size(), ratio1);
		float maxOffsetLengthLag = 0.0f * 100.0f;

		if (offsetLength > groundOffsetLength + maxOffsetLengthLag)
		{
			// Ensure we never penetrate the "floor".

			offsetLength = groundOffsetLength + maxOffsetLengthLag;
		}

		groundOffset = FMath::Lerp(groundOffset, LastSplineOffset[index], ratio1);
		groundOffset.Normalize();

		// NOTE: This line is more technically correct but somehow produced jolting movement.
		// groundOffset = FQuat::Slerp(groundOffset.ToOrientationQuat(), LastSplineOffset[index].ToOrientationQuat(), ratio1).Vector();

		FVector groundDirection = groundOffset;

		groundOffset *= offsetLength;

		// groundOffset is the smoothed maneuvering offset, along with its length stored in offsetLength.

		// Now compute the clearances from the smoothed offset.

		float minDistance = 1.0f * 100.0f;
		float loClearance = offsetLength;
		float hiClearance = (pursuitSpline != nullptr) ? pursuitSpline->GetClearance(thisDistanceAlongSpline, groundDirection * -1.0f, 0.0f) : 100.0f * 100.0f;
		float clampedHeadRoom = FMath::Max(0.0f, (hiClearance + loClearance) - minDistance);

		LastSplineOffset[index] = groundOffset;

		groundOffset = splineRotation.RotateVector(groundOffset);
		groundDirection = splineRotation.RotateVector(groundDirection);

		FVector up = FVector::UpVector;
		float offsetFromGround = HeightAboveGround + ((index == 0) ? DistanceAboveSpline : 0.0f);

		if (ViewDirection != ECameraViewDirection::Overhead)
		{
			up = groundDirection * -1.0f;
			offsetFromGround = FMath::Min(offsetFromGround, clampedHeadRoom);
		}

		if (reset == true ||
			OffsetFromGround[index] > offsetFromGround)
		{
			// Always clamp down on the head room to ensure we don't bust through the ceiling.

			OffsetFromGround[index] = offsetFromGround;
		}
		else
		{
			// Interpolate back from a clamped value so we don't jolt unnecessarily between
			// clamped and unclamped states.

			float ratio0 = FMathEx::GetSmoothingRatio(0.99f, deltaSeconds);

			OffsetFromGround[index] = FMath::Lerp(offsetFromGround, OffsetFromGround[index], ratio0);
		}

		location = (splineLocation + groundOffset) + (up * OffsetFromGround[index]);

		if (index == 0)
		{
			currentLocation = location;
		}
		else
		{
			targetLocation = location;
		}
	}
}

/**
* Set the end time for viewing from a spline camera.
***********************************************************************************/

void FSplineCamera::SetEndTime(float secondsFromNow, float timeScale)
{
	if (IsEasingOut() == false)
	{
		float distance = Target->GetAI().RouteFollower.ThisDistance;
		float speedSeconds = FMathEx::MetersToCentimeters(FMath::Max(100.0f, Target->GetSpeedMPS())) * timeScale;

		if (Target->GetAI().RouteFollower.ThisSpline != Spline)
		{
			distance = Spline->GetNearestDistance(Target->GetActorLocation(), 0.0f, 0.0f, 4, 50, 0.0f);
		}

		float distanceLeft = Spline->GetDistanceLeft(distance, StartDistance, EndDistance);
		float timeLeft = distanceLeft / speedSeconds;

		secondsFromNow = FMath::Max(secondsFromNow, EndTransitionTime);

		if (timeLeft > secondsFromNow ||
			EndClock - Clock > secondsFromNow)
		{
			EndClock = Clock + secondsFromNow;
		}
	}
}

/**
* Get the current rotation of the spline camera.
***********************************************************************************/

FRotator FSplineCamera::GetRotation(bool locked)
{
	if (locked == true)
	{
		FRotator rotation = Spline->GetDirection(DistanceAlongSpline).ToOrientationRotator();

		if (LocalDirection == FVector::ZeroVector)
		{
			LocalDirection = rotation.UnrotateVector(LastRotation.Vector());
		}

		rotation = rotation.RotateVector(LocalDirection).ToOrientationRotator();

		return rotation;
	}
	else
	{
		FRotator targetRotation = LastRotation;
		FVector splineDirection = Spline->GetDirection(DistanceAlongSpline);

		if (ViewDirection == ECameraViewDirection::Sideways)
		{
			// In this case, the spline should be positioned such that vehicles can only
			// drive on one side of it otherwise we could get gimbal lock, and it would
			// be unsightly in any case as the vehicle traverses from one side of the
			// spline to the other.
		}
		else
		{
			if (ViewDirection == ECameraViewDirection::Backwards)
			{
				splineDirection *= -1.0f;
			}

			FRotator splineRotation = splineDirection.ToOrientationRotator();
			float scale = FMath::Max(FMathEx::GetUnsignedDegreesDifference(splineRotation.Pitch, targetRotation.Pitch) / (AngleRange * 0.5f), FMathEx::GetUnsignedDegreesDifference(splineRotation.Yaw, targetRotation.Yaw) / (AngleRange * 0.5f));

			if (scale > 1.0f &&
				ViewDirection != ECameraViewDirection::Overhead &&
				ViewDirection != ECameraViewDirection::Crossover)
			{
				// Lock the rotation to the bounds of its possible movement.

				targetRotation = FMath::RInterpTo(splineRotation, targetRotation, 1.0f, 1.0f / scale);
			}

			if (EasingDirection == 1 &&
				StartTransition == ECameraStartTransition::Rotate)
			{
				// Roll the camera in from upside-down on the start transition.

				float ratio = FMath::Min(1.0f, Clock / 4.0f);

				ratio = FMathEx::EaseInOut(ratio, 2.5f);

				targetRotation.Roll = (1.0f - ratio) * -180.0f;
			}
			else if (RollingYawTracking == true)
			{
				// Bend the roll over to match the yaw to produce nice-looking shots.

				FRotator rotationDifference = (targetRotation - splineRotation).GetNormalized();

				targetRotation.Roll = rotationDifference.Yaw * 0.5f;
				targetRotation.Roll = FMath::Clamp(targetRotation.Roll, -10.0f, +10.0f);
			}

			if (EasingDirection == 1 &&
				DistanceOffsetTime < 1.0f &&
				StartTransition == ECameraStartTransition::Lower)
			{
				// Pitch the camera down on lowering.

				targetRotation.Pitch += FMath::InterpEaseIn(0.0f, 15.0f, 1.0f - DistanceOffsetTime, 2.0f);
			}

			if (EasingDirection == -1 &&
				EndTransition == ECameraEndTransition::Raise)
			{
				// Pitch the camera up on raising.

				targetRotation.Pitch += FMath::InterpEaseInOut(0.0f, 20.0f, DistanceOffsetTime, 2.0f);
			}
		}

		return targetRotation;
	}
}

/**
* Get the angle difference between where the camera is looking and where the target
* is.
***********************************************************************************/

float FSplineCamera::GetAngleToTarget() const
{
	if (GRIP_POINTER_VALID(Target) == false)
	{
		return 0.0f;
	}

	FVector difference = Target->GetCenterLocation() - GetLocation(); difference.Normalize();
	FVector direction = Spline->GetDirection(DistanceAlongSpline);

	if (ViewDirection == ECameraViewDirection::Backwards)
	{
		direction *= -1.0f;
	}

	return FMathEx::DotProductToDegrees(FVector::DotProduct(direction, difference));
}

#pragma endregion CameraCinematics
