/**
*
* Camera point components.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Camera points attached to vehicles normally, so that we can get a good view of
* the action when the cinematic camera is active.
*
***********************************************************************************/

#include "camera/camerapointcomponent.h"
#include "vehicle/flippablevehicle.h"
#include "gamemodes/basegamemode.h"

/**
* Construct a camera point component.
***********************************************************************************/

UCameraPointComponent::UCameraPointComponent()
{
	SetVisibleFlag(false);
	SetHiddenInGame(true);
}

#pragma region CameraCinematics

/**
* Reset the camera point ready for viewing.
***********************************************************************************/

void UCameraPointComponent::Reset()
{
	MinClip = -1.0f;

	if (LinkedToRootBone == false)
	{
		LinkedToRootBone = GetAttachSocketName() == "RootBone";
	}
	else
	{
		ABaseVehicle* launcher = Cast<ABaseVehicle>(GetAttachmentRootActor());

		if (launcher != nullptr)
		{
			AttachToComponent(launcher->VehicleMesh, FAttachmentTransformRules::KeepRelativeTransform, FName("RootBone"));
		}
	}

	Reposition(true);
}

/**
* Flip the camera with the parent actor and clip it to the environment.
***********************************************************************************/

bool UCameraPointComponent::Reposition(bool initialize, bool updateFlippedRotation)
{
	AActor* parent = GetAttachmentRootActor();
	ABaseVehicle* launcher = Cast<ABaseVehicle>(parent);
	bool wasFlipped = StateFlipped;

	if (launcher != nullptr)
	{
		static FName rootBone = "RootBone";

		if (SetupOriginalState == false)
		{
			SetupOriginalState = true;

			OriginalLocation = GetRelativeLocation() * GetRelativeScale3D();
			OriginalRotation = GetRelativeRotation();
		}

		if (LinkedToRootBone == true &&
			GetAttachSocketName() == rootBone &&
			FMath::Abs(launcher->VehicleOffset.Z) > KINDA_SMALL_NUMBER)
		{
			// If we're attached to the root bone but scraping along the floor then reattach to the root dummy
			// so we're off the floor and not watching the body of the vehicle juddering around.

			AttachToComponent(launcher->VehicleMesh, FAttachmentTransformRules::KeepRelativeTransform, FName("RootDummy"));
		}

		bool stateFlipped = launcher->IsFlipped() == true && InvertWithVehicle == true;

		if (initialize == true ||
			launcher->HasRespawnLanded() == false)
		{
			StateFlipped = stateFlipped;
		}
		else
		{
			if (StateFlipped != stateFlipped)
			{
				float distance = launcher->GetSurfaceDistance(false, true);

				if (distance != 0.0f &&
					distance < 3.0f * 100.0f)
				{
					StateFlipped = stateFlipped;
				}
			}
		}

		StateClipped = false;

		bool updateComponent = false;
		FVector location = OriginalLocation;

		if (StateFlipped == true)
		{
			location.Z *= -1.0f;
		}

		if (ClipLocation == true)
		{
			updateComponent = true;

			SetRelativeLocation(location);
			SetRelativeRotation(FRotator::ZeroRotator);

			UpdateComponentToWorld(EUpdateTransformFlags::SkipPhysicsUpdate);

			float probeSize = 5.0f;
			float padding = probeSize * 15.0f;
			FVector armRoot = launcher->GetCenterLocation();
			FVector desiredLocation = GetComponentLocation();
			const FTransform& launcherTransform = launcher->VehicleMesh->GetComponentTransform();
			FVector pointLocation = launcherTransform.InverseTransformPosition(desiredLocation);

			armRoot += launcher->VehicleMesh->GetBoneTransform(launcher->RootDummyBoneIndex).TransformVector(FVector(0.0f, 0.0f, StateFlipped == true ? -1.0f : 1.0f)) * 2.0f * 100.0f;

			if (ClipVertically == true)
			{
				pointLocation.Z = 0.0f;

				armRoot = launcherTransform.TransformPosition(pointLocation);
			}
			else if (ClipHorizontally == true)
			{
				pointLocation.Y = 0.0f;

				armRoot = launcherTransform.TransformPosition(pointLocation);
			}
			else if (ClipLongitudinally == true)
			{
				pointLocation.X = 0.0f;

				armRoot = launcherTransform.TransformPosition(pointLocation);
			}

			FVector toCamera = desiredLocation - armRoot;
			FVector toDirection = toCamera;

			toDirection.Normalize();

			FVector armEnd = desiredLocation + (toDirection * padding);

			FHitResult hitResult;

			// Do a sweep to ensure we are not penetrating the world.

			if (GetWorld()->SweepSingleByChannel(hitResult, armRoot, armEnd, FQuat::Identity, ABaseGameMode::ECC_VehicleCamera, FCollisionShape::MakeSphere(probeSize), launcher->SpringArm->GetClippingQueryParams()) == true)
			{
				if ((ClipHorizontally == false && ClipVertically == false && ClipLongitudinally == false) ||
					(ClipHorizontally == true && FMath::Abs(FVector::DotProduct(hitResult.ImpactNormal, launcher->GetSideDirection())) > 0.5f) ||
					(ClipVertically == true && FMath::Abs(FVector::DotProduct(hitResult.ImpactNormal, launcher->GetUpDirection())) > 0.5f) ||
					(ClipLongitudinally == true && FMath::Abs(FVector::DotProduct(hitResult.ImpactNormal, launcher->GetFacingDirection())) > 0.5f))
				{
					StateClipped = true;
				}
			}

			if (MinClip >= 0.0f ||
				StateClipped == true)
			{
				if (MinClip < 0.0f)
				{
					MinClip = hitResult.Time;
				}
				else if (StateClipped ==  true)
				{
					MinClip = FMath::Min(hitResult.Time, MinClip);
				}

				StateClipped = true;

				float toSize = (armEnd - armRoot).Size();
				float distance = FMath::Max((toSize * MinClip) - padding, 0.0f);
				FVector clippedLocation = armRoot + (toDirection * distance);

				location += GetComponentTransform().InverseTransformPosition(clippedLocation);
			}
		}

		FRotator rotation = OriginalRotation;

		if (initialize == true ||
			updateFlippedRotation == true)
		{
			LockFlipped = StateFlipped;
		}

		if (LockFlipped == true)
		{
			rotation.Pitch *= -1.0f;
			rotation.Roll += 180.0f;

			rotation.Normalize();
		}

		updateComponent |= (location != GetRelativeLocation() || rotation != GetRelativeRotation());

		if (updateComponent == true)
		{
			SetRelativeLocation(location);
			SetRelativeRotation(rotation);

			UpdateComponentToWorld(EUpdateTransformFlags::SkipPhysicsUpdate);
		}
	}

	return StateFlipped != wasFlipped;
}

#pragma endregion CameraCinematics
