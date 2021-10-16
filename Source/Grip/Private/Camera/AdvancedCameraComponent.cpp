/**
*
* Advanced camera implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* An advanced camera component to give a lot of helper functionality to generalized
* camera work. It has 3 modes of operation, native, custom and mouse control, and
* has transition capabilities to interpolate between each mode when required. Native
* mode has the actor to which it's attached controlling its location and
* orientation. Custom, for when you want complete control like in cinematic cameras.
* And mouse, often used during the game's development but rarely in the field.
*
***********************************************************************************/

#include "camera/advancedcameracomponent.h"
#include "vehicle/flippablevehicle.h"
#include "gamemodes/menugamemode.h"

/**
* Construct an advanced camera component.
***********************************************************************************/

UAdvancedCameraComponent::UAdvancedCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

#pragma region VehicleCamera

	for (float& value : EffectSources)
	{
		value = 0.0f;
	}

	SetEffectsFader(FaderNative, 1.0f);

	for (float& value : RotationSources)
	{
		value = 0.0f;
	}

	SetRotationFader(FaderNative, 1.0f);

	for (float& value : FieldOfViewSources)
	{
		value = 0.0f;
	}

	SetFieldOfViewFader(FaderNative, 1.0f);

#pragma endregion VehicleCamera

	bConstrainAspectRatio = false;
}

#pragma region VehicleCamera

/**
* Restore the relative transform once the camera has been used.
***********************************************************************************/

void UAdvancedCameraComponent::RestoreRelativeTransform()
{
	if (RelativeTransformSet == true)
	{
		SetRelativeTransform(RelativeTransform);
	}
}

/**
* Fade camera effects over to custom control.
***********************************************************************************/

void UAdvancedCameraComponent::FadeEffectsToCustomControl(float transitionDuration, float transitionEasing)
{
	EffectsFaderIndex = FaderCustom;
	EffectsFaderDelta = 1.0f / transitionDuration;
	EffectsFaderEasing = transitionEasing;
}

/**
* Fade camera effects over to native control.
***********************************************************************************/

void UAdvancedCameraComponent::FadeEffectsToNativeControl(float transitionDuration, float transitionEasing)
{
	EffectsFaderIndex = FaderNative;
	EffectsFaderDelta = 1.0f / transitionDuration;
	EffectsFaderEasing = transitionEasing;
}

/**
* Switch camera effects over to custom control.
***********************************************************************************/

void UAdvancedCameraComponent::SwitchEffectsToCustomControl()
{
	EffectsFaderIndex = FaderCustom;
	EffectsFaderDelta = 0.0f;

	SetEffectsFader(FaderCustom, 1.0f);
}

/**
* Switch camera effects over to native control.
***********************************************************************************/

void UAdvancedCameraComponent::SwitchEffectsToNativeControl()
{
	EffectsFaderIndex = FaderNative;
	EffectsFaderDelta = 0.0f;

	SetEffectsFader(FaderNative, 1.0f);
}

/**
* Fade camera rotation over to mouse control.
***********************************************************************************/

void UAdvancedCameraComponent::FadeRotationToMouseControl(float transitionDuration, float transitionEasing, bool inheritCurrentRotation)
{
	RotationFaderIndex = FaderMouse;
	RotationFaderDelta = 1.0f / transitionDuration;

	if (inheritCurrentRotation == true)
	{
		MouseRotation = CurrentMouseRotation = GetComponentRotation();
	}
}

/**
* Fade camera rotation over to custom control.
***********************************************************************************/

void UAdvancedCameraComponent::FadeRotationToCustomControl(float transitionDuration, float transitionEasing)
{
	RotationFaderIndex = FaderCustom;
	RotationFaderDelta = 1.0f / transitionDuration;
	RotationFaderEasing = transitionEasing;
}

/**
* Fade camera rotation over to native control.
***********************************************************************************/

void UAdvancedCameraComponent::FadeRotationToNativeControl(float transitionDuration, float transitionEasing)
{
	RotationFaderIndex = FaderNative;
	RotationFaderDelta = 1.0f / transitionDuration;
	RotationFaderEasing = transitionEasing;
}

/**
* Switch camera rotation over to mouse control.
***********************************************************************************/

void UAdvancedCameraComponent::SwitchRotationToMouseControl(bool inheritCurrentRotation)
{
	RotationFaderIndex = FaderMouse;
	RotationFaderDelta = 0.0f;

	SetRotationFader(FaderMouse, 1.0f);

	if (inheritCurrentRotation == true)
	{
		MouseRotation = CurrentMouseRotation = GetComponentRotation();
	}
}

/**
* Switch camera rotation over to custom control.
***********************************************************************************/

void UAdvancedCameraComponent::SwitchRotationToCustomControl()
{
	RotationFaderIndex = FaderCustom;
	RotationFaderDelta = 0.0f;

	SetRotationFader(FaderCustom, 1.0f);
}

/**
* Switch camera rotation over to native control.
***********************************************************************************/

void UAdvancedCameraComponent::SwitchRotationToNativeControl()
{
	RotationFaderIndex = FaderNative;
	RotationFaderDelta = 0.0f;

	SetRotationFader(FaderNative, 1.0f);
}

/**
* Switch camera location over to mouse control.
***********************************************************************************/

void UAdvancedCameraComponent::SwitchLocationToMouseControl(bool inheritCurrentLocation)
{
	LocationIndex = FaderMouse;

	if (inheritCurrentLocation == true)
	{
		MouseLocation = GetComponentLocation();
	}
}

/**
* Switch camera location over to custom control.
***********************************************************************************/

void UAdvancedCameraComponent::SwitchLocationToCustomControl()
{
	LocationIndex = FaderCustom;
}

/**
* Switch camera location over to native control.
***********************************************************************************/

void UAdvancedCameraComponent::SwitchLocationToNativeControl()
{
	LocationIndex = FaderNative;
}

/**
* Fade camera field of view over to custom control.
***********************************************************************************/

void UAdvancedCameraComponent::FadeFieldOfViewToCustomControl(float transitionDuration, float transitionEasing)
{
	FieldOfViewFaderIndex = FaderCustom;
	FieldOfViewFaderDelta = 1.0f / transitionDuration;
	FieldOfViewFaderEasing = transitionEasing;
}

/**
* Fade camera field of view over to native control.
***********************************************************************************/

void UAdvancedCameraComponent::FadeFieldOfViewToNativeControl(float transitionDuration, float transitionEasing)
{
	FieldOfViewFaderIndex = FaderNative;
	FieldOfViewFaderDelta = 1.0f / transitionDuration;
	FieldOfViewFaderEasing = transitionEasing;
}

/**
* Switch camera field of view over to custom control.
***********************************************************************************/

void UAdvancedCameraComponent::SwitchFieldOfViewToCustomControl()
{
	FieldOfViewFaderIndex = FaderCustom;
	FieldOfViewFaderDelta = 0.0f;

	SetFieldOfViewFader(FaderCustom, 1.0f);
}

/**
* Switch camera field of view over to native control.
***********************************************************************************/

void UAdvancedCameraComponent::SwitchFieldOfViewToNativeControl()
{
	FieldOfViewFaderIndex = FaderNative;
	FieldOfViewFaderDelta = 0.0f;

	SetFieldOfViewFader(FaderNative, 1.0f);
}

/**
* Have the custom rotation look at a particular location.
***********************************************************************************/

void UAdvancedCameraComponent::LookAtCustomLocation(FVector location)
{
	LookAtCustomTarget = ECameraTarget::TargetLocation;
	CustomTargetLocation = location;
}

/**
* Have the custom rotation look at a particular actor.
***********************************************************************************/

void UAdvancedCameraComponent::LookAtCustomActor(AActor* actor)
{
	LookAtCustomTarget = ECameraTarget::TargetActor;
	CustomTargetActor = actor;
}

/**
* Have the custom rotation reset back to none.
***********************************************************************************/

void UAdvancedCameraComponent::LookAtCustomNone()
{
	LookAtCustomTarget = ECameraTarget::TargetNone;
}

/**
* Set an effects fader to a particular amount and set the others accordingly.
***********************************************************************************/

void UAdvancedCameraComponent::SetEffectsFader(int32 faderIndex, float amount)
{
	amount = FMath::Clamp(amount, 0.0f, 1.0f);

	EffectSources[faderIndex] = amount;

	for (int32 i = 0; i < NumFaders; i++)
	{
		if (i != faderIndex)
		{
			EffectSources[i] = FMath::Min(EffectSources[i], 1.0f - EffectSources[faderIndex]);
		}
	}
}

/**
* Set a rotation fader to a particular amount and set the others accordingly.
***********************************************************************************/

void UAdvancedCameraComponent::SetRotationFader(int32 faderIndex, float amount)
{
	amount = FMath::Clamp(amount, 0.0f, 1.0f);

	RotationSources[faderIndex] = amount;

	for (int32 i = 0; i < NumFaders; i++)
	{
		if (i != faderIndex)
		{
			RotationSources[i] = FMath::Min(RotationSources[i], 1.0f - RotationSources[faderIndex]);
		}
	}
}

/**
* Set a field of view fader to a particular amount and set the others accordingly.
***********************************************************************************/

void UAdvancedCameraComponent::SetFieldOfViewFader(int32 faderIndex, float amount)
{
	amount = FMath::Clamp(amount, 0.0f, 1.0f);

	FieldOfViewSources[faderIndex] = amount;

	for (int32 i = 0; i < NumFaders; i++)
	{
		if (i != faderIndex)
		{
			FieldOfViewSources[i] = FMath::Min(FieldOfViewSources[i], 1.0f - FieldOfViewSources[faderIndex]);
		}
	}
}

/**
* Do the regular update tick.
***********************************************************************************/

void UAdvancedCameraComponent::TickComponent(float deltaSeconds, enum ELevelTick tickType, FActorComponentTickFunction* thisTickFunction)
{
	Super::TickComponent(deltaSeconds, tickType, thisTickFunction);

	UWorld* world = GetWorld();
	UGlobalGameState* gameState = UGlobalGameState::GetGlobalGameState(world);

	if (gameState == nullptr)
	{
		return;
	}

#pragma region CameraCinematics

	CinematicsDirector.Tick(deltaSeconds);

	if (APlayGameMode::Get(this) != nullptr)
	{
		if (CinematicsDirector.UsingCameraPointCamera(false) == true)
		{
			// If we've a camera point on a vehicle then render it with some
			// appropriate depth-of-field.

			PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
			PostProcessSettings.DepthOfFieldFocalDistance = 250.0f;
			PostProcessSettings.bOverride_DepthOfFieldDepthBlurRadius = true;
			PostProcessSettings.DepthOfFieldDepthBlurRadius = 2.0f;
			PostProcessSettings.bOverride_DepthOfFieldDepthBlurAmount = true;
			PostProcessSettings.DepthOfFieldDepthBlurAmount = 0.5f;
		}
		else if (CinematicsDirector.UsingSplineCamera() == true)
		{
			// If we're using a spline camera to view a target then render it with some
			// appropriate depth-of-field.

			PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
			PostProcessSettings.DepthOfFieldFocalDistance = CinematicsDirector.GetFocalDistance();
			PostProcessSettings.bOverride_DepthOfFieldDepthBlurRadius = true;
			PostProcessSettings.DepthOfFieldDepthBlurRadius = 1.0f;
			PostProcessSettings.bOverride_DepthOfFieldDepthBlurAmount = true;
			PostProcessSettings.DepthOfFieldDepthBlurAmount = 1.0f;
		}
		else
		{
			// Kill all depth-of-field.

			PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
			PostProcessSettings.DepthOfFieldFocalDistance = 0.0f;
			PostProcessSettings.bOverride_DepthOfFieldDepthBlurRadius = true;
			PostProcessSettings.DepthOfFieldDepthBlurRadius = 0.0f;
			PostProcessSettings.bOverride_DepthOfFieldDepthBlurAmount = true;
			PostProcessSettings.DepthOfFieldDepthBlurAmount = 0.0f;
		}

		// Setup the motion blur for the scene.

		float scale = 0.25f;

		switch (gameState->GraphicsOptions.MotionBlur)
		{
		default:
			scale = 0.0f;
			break;
		case EQualityLevel::Low:
			scale = 0.125f;
			break;
		case EQualityLevel::Medium:
			scale = 0.25f;
			break;
		case EQualityLevel::High:
			scale = 0.5f;
			break;
		case EQualityLevel::Epic:
			scale = 1.0f;
			break;
		}

		if (CinematicsDirector.IsActive() == true)
		{
			PostProcessSettings.bOverride_MotionBlurAmount = true;
			PostProcessSettings.MotionBlurAmount = FMath::Min(scale * 2.0f, 1.0f);
		}
		else
		{
			PostProcessSettings.bOverride_MotionBlurAmount = true;
			PostProcessSettings.MotionBlurAmount = scale;
		}
	}
	else
	{
		// Setup the depth-of-field for the menu scene.

		PostProcessSettings.bOverride_DepthOfFieldFstop = true;
		PostProcessSettings.DepthOfFieldFstop = 32.0f;
		PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
		PostProcessSettings.DepthOfFieldFocalDistance = 250.0f;
	}

	UCameraPointComponent* cameraPoint = CinematicsDirector.GetCurrentCameraPoint();

	if (cameraPoint != nullptr &&
		CinematicsDirector.IsActive() == false)
	{
		cameraPoint->Reposition(false);
	}

	if ((CinematicsDirector.UsingSplineCamera() == true) ||
		(CinematicsDirector.UsingCustomOverride() == true) ||
		(cameraPoint != nullptr && cameraPoint->InheritNativeEffects == false))
	{
		ViewingActor = CinematicsDirector.GetCurrentVehicle();

		CustomEffectsAmount = 0.0f;

		SwitchEffectsToCustomControl();
	}
	else

#pragma endregion CameraCinematics

	{
		ViewingActor = nullptr;

		SwitchEffectsToNativeControl();
	}

	// Do the setting of the general post processing properties.

	float contrast = gameState->GraphicsOptions.GetContrastLevel();
	float brightness = gameState->GraphicsOptions.GetBrightnessLevel();
	float gamma = gameState->GraphicsOptions.GetGammaLevel();
	float saturation = gameState->GraphicsOptions.GetSaturationLevel();

	PostProcessSettings.bOverride_ColorGain = FMath::Abs(0.5f - gameState->GraphicsOptions.BrightnessLevel) > 0.01f;
	PostProcessSettings.bOverride_ColorContrast = FMath::Abs(0.5f - gameState->GraphicsOptions.ContrastLevel) > 0.01f;
	PostProcessSettings.bOverride_ColorSaturation = FMath::Abs(0.5f - gameState->GraphicsOptions.SaturationLevel) > 0.01f;
	PostProcessSettings.bOverride_ColorGamma = FMath::Abs(0.5f - gameState->GraphicsOptions.GammaLevel) > 0.01f;

	PostProcessSettings.ColorGain = FVector(brightness, brightness, brightness);
	PostProcessSettings.ColorContrast = FVector(contrast, contrast, contrast);
	PostProcessSettings.ColorSaturation = FVector(saturation, saturation, saturation);
	PostProcessSettings.ColorGamma = FVector(gamma, gamma, gamma);

	PostProcessSettings.bOverride_AmbientOcclusionIntensity = true;

	if (gameState->GraphicsOptions.AmbientOcclusion == EOffOnSwitch::On)
	{
		PostProcessSettings.AmbientOcclusionIntensity = 0.8f;
	}
	else
	{
		PostProcessSettings.AmbientOcclusionIntensity = 0.0f;
	}

	if (RelativeTransformSet == false)
	{
		RelativeTransformSet = true;

		RelativeTransform = GetRelativeTransform();
	}

	// Update the mouse rotation.

	APlayerController* controller = UGameplayStatics::GetPlayerController(GetWorld(), 0);

	if (controller != nullptr)
	{
		float x = 0.0f, y = 0.0f;

		controller->GetInputMouseDelta(x, y);

		MouseRotation.Add(y, x, 0.0f);

		if (SmoothMouseRotation == true)
		{
			CurrentMouseRotation = GetSmoothedRotation(CurrentMouseRotation, MouseRotation, deltaSeconds);
		}
		else
		{
			CurrentMouseRotation = MouseRotation;
		}
	}

	// Calculate the composite location, rotation and field of view for the current state of the camera.

	SetRotationFader(RotationFaderIndex, RotationSources[RotationFaderIndex] + deltaSeconds * RotationFaderDelta);
	SetFieldOfViewFader(FieldOfViewFaderIndex, FieldOfViewSources[FieldOfViewFaderIndex] + deltaSeconds * FieldOfViewFaderDelta);
	SetFieldOfView(NativeFieldOfView * FMathEx::EaseInOut(FieldOfViewSources[FaderNative], FieldOfViewFaderEasing) + CustomFieldOfView * FMathEx::EaseInOut(FieldOfViewSources[FaderCustom], FieldOfViewFaderEasing));

	SmoothedLocation = (LocationIndex == FaderNative) ? NativeLocation : CustomLocation;

	FVector initialLoc = GetComponentLocation();
	FVector desiredLoc = initialLoc;

	if (SmoothLocation == true)
	{
		desiredLoc = FMathEx::GetSmoothedVector(desiredLoc, SmoothedLocation, LocationLagRatio, deltaSeconds);
	}
	else
	{
		desiredLoc = SmoothedLocation;
	}

	if (LookAtCustomTarget != ECameraTarget::TargetNone)
	{
		FVector difference;

		if (LookAtCustomTarget == ECameraTarget::TargetLocation)
		{
			difference = CustomTargetLocation - desiredLoc;
		}
		else if (LookAtCustomTarget == ECameraTarget::TargetActor && GRIP_POINTER_VALID(CustomTargetActor) == true)
		{
			difference = CustomTargetActor->GetActorLocation() - desiredLoc;
		}

		difference.Normalize();

		CustomRotation = difference.Rotation();
	}

	int32 numQuats = 0;
	FQuat quats[NumFaders];
	float faders[NumFaders];

	if (RotationSources[FaderNative] > 0.001f)
	{
		quats[numQuats] = FQuat(NativeRotation);
		faders[numQuats++] = RotationSources[FaderNative];
	}

	if (RotationSources[FaderCustom] > 0.001f)
	{
		quats[numQuats] = FQuat(CustomRotation);
		faders[numQuats++] = RotationSources[FaderCustom];
	}

	if (RotationSources[FaderMouse] > 0.001f)
	{
		quats[numQuats] = FQuat(CurrentMouseRotation);
		faders[numQuats++] = RotationSources[FaderMouse];
	}

	if (numQuats == 1)
	{
		SmoothedRotation = quats[0].Rotator();
	}
	else if (numQuats == 2)
	{
		FQuat quat = FQuat::Slerp(quats[0], quats[1], FMathEx::EaseInOut(faders[1], RotationFaderEasing));

		quat.Normalize();

		SmoothedRotation = quat.Rotator();
	}

	FRotator initialRot = GetComponentRotation();
	FRotator desiredRot = initialRot;

	if (SmoothRotation == true)
	{
		desiredRot = GetSmoothedRotation(desiredRot, SmoothedRotation, deltaSeconds);
	}
	else
	{
		desiredRot = SmoothedRotation;
	}

	if (LockRollAxis == true)
	{
		desiredRot.Roll = 0.0f;
	}

	if (desiredLoc != initialLoc ||
		desiredRot != initialRot)
	{
		SetWorldLocationAndRotation(desiredLoc, FQuat(desiredRot));
	}
}

/**
* Returns camera's point of view.
***********************************************************************************/

void UAdvancedCameraComponent::GetCameraView(float deltaSeconds, FMinimalViewInfo& desiredView)
{
	// If the game is paused then use the last active view.

	APlayGameMode* gameMode = APlayGameMode::Get(this);

	if (gameMode != nullptr &&
		gameMode->GamePaused == true)
	{
		desiredView = LastView;

		return;
	}

	// Handle the viewing of other vehicle targets.

	APawn* pawn = Cast<APawn>(GetOwner());
	APlayerController* playerController = nullptr;
	ABaseVehicle* vehicle = nullptr;

	if (pawn != nullptr)
	{
		AController* controller = pawn->GetController();

		if (controller != nullptr)
		{
			playerController = Cast<APlayerController>(pawn->GetController());

			if (playerController != nullptr)
			{
				vehicle = Cast<ABaseVehicle>(playerController->GetViewTarget());

				if (vehicle != nullptr &&
					vehicle->Camera != this)
				{
					// If we're looking at another vehicle then simply get the camera view from that vehicle.
					// This is most often not the case, it's normally used during development only.

					vehicle->Camera->GetCameraView(deltaSeconds, desiredView);

					desiredView.FOV = UAdvancedCameraComponent::GetAdjustedFOV(playerController, desiredView.FOV);

					LastView = desiredView;

					return;
				}
			}
		}
	}

	Super::GetCameraView(deltaSeconds, desiredView);

	desiredView.FOV = UAdvancedCameraComponent::GetAdjustedFOV(playerController, desiredView.FOV);

#pragma region CameraCinematics

	// Let the cinematic camera do whatever it likes with the view if it's in control.

	CinematicsDirector.GetCameraView(deltaSeconds, desiredView);

#pragma endregion CameraCinematics

	LastView = desiredView;
}

/**
* Calculate an adjusted FOV taking into account the viewport for the local player.
* Ensuring that we get a reasonable field of view no matter how distorted the aspect
* ratio of the viewport.
***********************************************************************************/

float UAdvancedCameraComponent::GetAdjustedFOV(APlayerController* controller, float degrees)
{
	if (controller != nullptr)
	{
		ULocalPlayer* player = controller->GetLocalPlayer();

		if (player != nullptr)
		{
			FVector2D viewportSize;

			player->ViewportClient->GetViewportSize(viewportSize);

			if (viewportSize.X == 0.0f)
			{
				// Normally we get something from GetViewportSize, but we have to have a fallback
				// if somehow it fails, normally if called during initialization or something.

				if (GEngine && GEngine->GameViewport)
				{
					GEngine->GameViewport->GetViewportSize(viewportSize);
				}

				if (viewportSize.X == 0.0f)
				{
					viewportSize.X = GSystemResolution.ResX;
					viewportSize.Y = GSystemResolution.ResY;
				}
			}

			float authoredAR = 1920.0f / 1080.0f;
			float observedAR = viewportSize.X / viewportSize.Y;

			FVector2D size = player->Size;

			observedAR = (viewportSize.X * size.X) / (viewportSize.Y * size.Y);

			if (observedAR > authoredAR)
			{
				// Viewport is wider than authored, so widen the field of view accordingly (done below).
			}
			else if (authoredAR > observedAR)
			{
				// Viewport is slimmer than authored, so keep as-is as we don't want to lose the peripheral vision.

				if (AMenuGameMode::Get(controller) != nullptr)
				{
					observedAR = FMath::Max(observedAR, 16.0f / 9.0f);
				}
				else
				{
					observedAR = FMath::Max(observedAR, 4.0f / 3.0f);
				}
			}

			float fovRadiansX = FMath::DegreesToRadians(degrees);
			float fovRadiansY = FMath::Atan(FMath::Tan(fovRadiansX * 0.5f) / authoredAR) * 2.0f;

			degrees = FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(fovRadiansY * 0.5f) * observedAR) * 2.0f);
			degrees = FMath::Min(degrees, 160.0f);
		}
	}

	return degrees;
}

/**
* Set whether a component (and its children) can be seen by their owner.
***********************************************************************************/

void UAdvancedCameraComponent::SetOwnerNoSee(UPrimitiveComponent* component, bool nosee) const
{
	if (component->IsA<UStaticMeshComponent>() == false &&
		component->IsA<UVehicleMeshComponent>() == false)
	{
		component->SetOwnerNoSee(nosee);
	}

	for (int32 i = 0; i < component->GetNumChildrenComponents(); i++)
	{
		USceneComponent* child = component->GetChildComponent(i);
		UChildActorComponent* childActor = Cast<UChildActorComponent>(child);

		if (childActor != nullptr)
		{
			ACanard* canard = Cast<ACanard>(childActor->GetChildActor());

			if (canard != nullptr)
			{
				// NOTE: This doesn't work because child actors are broken.

				SetOwnerNoSee(canard->CanardMesh, nosee);
			}
		}
		else
		{
			UPrimitiveComponent* primitive = Cast<UPrimitiveComponent>(child);

			if (primitive != nullptr)
			{
				SetOwnerNoSee(primitive, nosee);
			}
			else
			{
				ULightStreakComponent* lightStreak = Cast<ULightStreakComponent>(child);

				if (lightStreak != nullptr)
				{
					lightStreak->SetOwnerNoSee(nosee);
				}
			}
		}
	}
}

/**
* Set whether a component (and its children) can be seen only by their owner.
***********************************************************************************/

void UAdvancedCameraComponent::SetOnlyOwnerSee(UPrimitiveComponent* component, bool nosee) const
{
	if (component->bOnlyOwnerSee != nosee)
	{
		component->SetOnlyOwnerSee(nosee);

		for (int32 i = 0; i < component->GetNumChildrenComponents(); i++)
		{
			UPrimitiveComponent* primitive = Cast<UPrimitiveComponent>(component->GetChildComponent(i));

			if (primitive != nullptr)
			{
				SetOnlyOwnerSee(primitive, nosee);
			}
			else
			{
				ULightStreakComponent* lightStreak = Cast<ULightStreakComponent>(component->GetChildComponent(i));

				if (lightStreak != nullptr)
				{
					lightStreak->SetOnlyOwnerSee(nosee);
				}
			}
		}
	}
}

/**
* Calculate the field of view to view an object of a given radius so that it covers
* a given proportion of the screen.
***********************************************************************************/

float UAdvancedCameraComponent::GetFieldOfViewForRadius(const FVector& cameraLocation, const FVector& actorLocation, float radius, float screenProportion)
{
	float worldProportion = (radius * 2.0f) / screenProportion;
	float distance = (actorLocation - cameraLocation).Size();
	float fov = FMath::Atan((worldProportion / distance) * 0.5f) * 2.0f;

	return FMath::RadiansToDegrees(fov);
}

/**
* Calculate the distance to an object of a given radius using a given field of view
* so that it covers a given proportion of the screen.
***********************************************************************************/

float UAdvancedCameraComponent::GetDistanceForRadius(float radius, float screenProportion, float fov)
{
	float worldProportion = (radius * 2.0f) / screenProportion;
	float distance = (worldProportion / FMath::Tan(FMath::DegreesToRadians(fov) * 0.5f)) * 0.5f;

	return distance;
}

#pragma endregion VehicleCamera
