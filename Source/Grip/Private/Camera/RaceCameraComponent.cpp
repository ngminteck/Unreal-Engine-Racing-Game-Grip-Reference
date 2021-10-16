/**
*
* Vehicle race camera implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* A specialist camera for racing vehicles, which contains a lot gizmos for
* enhancing the sensation of speed and works in conjunction with a specialist
* post processing material.
*
***********************************************************************************/

#include "camera/racecameracomponent.h"
#include "camera/flippablespringarmcomponent.h"
#include "game/globalgamestate.h"
#include "gamemodes/playgamemode.h"
#include "kismet/kismetmateriallibrary.h"
#include "vehicle/flippablevehicle.h"

/**
* Construct a race camera component.
***********************************************************************************/

URaceCameraComponent::URaceCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	FieldOfViewVsSpeed.GetRichCurve()->AddKey(0.0f, 70.0f);
	FieldOfViewVsSpeed.GetRichCurve()->AddKey(120.0f, 82.0f);
	FieldOfViewVsSpeed.GetRichCurve()->AddKey(230.0f, 92.0f);
	FieldOfViewVsSpeed.GetRichCurve()->AddKey(350.0f, 97.0f);
	FieldOfViewVsSpeed.GetRichCurve()->AddKey(500.0f, 100.0f);

	RadialSpeedBlurVsSpeed.GetRichCurve()->AddKey(0.0f, 0.0f);
	RadialSpeedBlurVsSpeed.GetRichCurve()->AddKey(200.0f, 0.0f);
	RadialSpeedBlurVsSpeed.GetRichCurve()->AddKey(400.0f, 0.66f);
	RadialSpeedBlurVsSpeed.GetRichCurve()->AddKey(500.0f, 0.9f);
	RadialSpeedBlurVsSpeed.GetRichCurve()->AddKey(600.0f, 1.0f);

	IonisationVsSpeed.GetRichCurve()->AddKey(0.0f, 0.0f);
	IonisationVsSpeed.GetRichCurve()->AddKey(200.0f, 0.0f);
	IonisationVsSpeed.GetRichCurve()->AddKey(400.0f, 0.25f);
	IonisationVsSpeed.GetRichCurve()->AddKey(500.0f, 0.75f);
	IonisationVsSpeed.GetRichCurve()->AddKey(600.0f, 1.0f);

	StreaksVsSpeed.GetRichCurve()->AddKey(0.0f, 0.0f);
	StreaksVsSpeed.GetRichCurve()->AddKey(200.0f, 0.0f);
	StreaksVsSpeed.GetRichCurve()->AddKey(400.0f, 0.25f);
	StreaksVsSpeed.GetRichCurve()->AddKey(500.0f, 0.75f);
	StreaksVsSpeed.GetRichCurve()->AddKey(600.0f, 1.0f);
}

#pragma region VehicleCamera

/**
* Initiate some tampering.
***********************************************************************************/

void FCameraFeedTamperer::Initiate(float duration, float delay, bool includeStatic)
{
	Delay = delay;
	Clock = 0.0f;
	ModeClock = 0.0f;
	ModeClockDuration = 0.0f;
	TamperingDuration = duration;
	TamperMode = false;
	IncludeStatic = includeStatic;
}

/**
* Update the camera feed tamperer for a particular vehicle.
***********************************************************************************/

bool FCameraFeedTamperer::Update(float deltaSeconds, ABaseVehicle* vehicle)
{
	if (IsActive() == true)
	{
		if (Delay > 0.0f)
		{
			Delay -= deltaSeconds;
		}

		if (Delay <= 0.0f)
		{
			Clock += deltaSeconds;

			ModeClock += deltaSeconds;

			if (ModeClock > ModeClockDuration)
			{
				TamperMode ^= true;

				if (TamperMode == true)
				{
					ModeClockDuration = FMath::FRandRange(2.0f, 3.0f);
				}
				else
				{
					ModeClockDuration = FMath::FRandRange(1.0f, 2.0f);
				}
			}
		}

		return (IsActive() == false);
	}

	return false;
}

/**
* Get the amount of distortion currently being applied to the camera feed tamperer.
***********************************************************************************/

float FCameraFeedTamperer::GetDistortionAmount() const
{
	if (IsActive() == true &&
		Delay <= 0.0f)
	{
		const float fadeIn = 0.15f;
		const float fadeOut = 0.25f;

		if (Clock < fadeIn)
		{
			return Clock / fadeIn;
		}
		else if (Clock < TamperingDuration - fadeOut)
		{
			return 1.0f;
		}
		else if (Clock < TamperingDuration)
		{
			return 1.0f - ((Clock - (TamperingDuration - fadeOut)) / fadeOut);
		}
	}

	return 0.0f;
}

/**
* Do some initialization when the game is ready to play.
***********************************************************************************/

void URaceCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	SetupMaterials(GetOwningVehicle());
}

/**
* Do some shutdown when the component is being destroyed.
***********************************************************************************/

void URaceCameraComponent::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	// Ensure that the camera material for the blendable is reset to what it was
	// when the blendable was created.

	if (CheapCameraMaterial != nullptr &&
		PostProcessSettings.WeightedBlendables.Array.Num() > 0)
	{
		PostProcessSettings.WeightedBlendables.Array[0].Object = CheapCameraMaterial;
	}

	Super::EndPlay(endPlayReason);
}

/**
* Do the regular update tick.
***********************************************************************************/

void URaceCameraComponent::TickComponent(float deltaSeconds, enum ELevelTick tickType, FActorComponentTickFunction* thisTickFunction)
{
	Super::TickComponent(deltaSeconds, tickType, thisTickFunction);

	UGlobalGameState* gameState = UGlobalGameState::GetGlobalGameState(GetWorld());

	if (gameState == nullptr)
	{
		return;
	}

	// Establish the vehicle we're interacting with for this camera.

	ABaseVehicle* owningVehicle = GetOwningVehicle();
	ABaseVehicle* anyVehicle = owningVehicle;
	APlayGameMode* gameMode = APlayGameMode::Get(GetWorld());

	if (anyVehicle == nullptr &&
		gameMode != nullptr)
	{

#pragma region CameraCinematics

		if (GetCinematicsDirector().IsActive() == true)
		{
			anyVehicle = GetCinematicsDirector().GetCurrentVehicle();
		}
		else

#pragma endregion CameraCinematics

		{
			TArray<ABaseVehicle*>& vehicles = gameMode->GetVehicles();

			if (vehicles.Num() > 0)
			{
				anyVehicle = vehicles[0];
			}
		}
	}

	FMinimalViewInfo desiredView;

	GetCameraViewNoPostProcessing(deltaSeconds, desiredView);

	float environmentFilmNoiseAmount = 0.0f;
	FLinearColor environmentSceneTint(1.0f, 1.0f, 1.0f);

	if (gameMode != nullptr)
	{
		environmentFilmNoiseAmount = gameMode->GetEnvironmentFilmNoiseAmount(desiredView.Location);
		environmentSceneTint = gameMode->GetEnvironmentSceneTint(desiredView.Location);
	}

	// Setup the materials if necessary.

	SetupMaterials(anyVehicle);

	// Update the tampering of the camera feed.

	CameraFeed.Update(deltaSeconds, anyVehicle);

	// Update the general post-process settings.

	float neutralFilmContrast = 0.03f;
	float colorTintR = environmentSceneTint.R;
	float colorTintG = environmentSceneTint.G;
	float colorTintB = environmentSceneTint.B;
	float fringe = 0.0f;
	float shockBlur = 0.0f;
	float eliminationRatio = 0.0f;

	if (owningVehicle != nullptr)
	{
		eliminationRatio = owningVehicle->GetVehicleElimination().Ratio;
	}

	if ((eliminationRatio > 0.0f) ||
		(ShockTimer > KINDA_SMALL_NUMBER && GetNativeEffectsAmount() > KINDA_SMALL_NUMBER))
	{
		float ratio = ShockTimer / ShockTime;

		ShockTimer -= deltaSeconds;

		if (ShockTimer < 0.0f)
		{
			ShockTimer = 0.0f;
		}

		float ratio0 = FMath::Max(ratio, eliminationRatio * 0.125f);
		float ratio1 = FMath::Max(ratio, eliminationRatio * 0.50f);
		float ratio2 = FMath::Max(ratio, eliminationRatio * 1.00f);

		shockBlur = ratio0 * ratio0 * ratio0 * GetNativeEffectsAmount();

		PostProcessSettings.bOverride_FilmContrast = true;
		PostProcessSettings.bOverride_VignetteIntensity = true;

		fringe = FMath::Max(fringe, ratio1 * 8);

		if (ShockShielded == true)
		{
			colorTintG *= FMath::Lerp(1.0f, 0.800f, ratio2);
			colorTintB *= FMath::Lerp(1.0f, 0.136f, ratio2);
		}
		else
		{
			colorTintG *= FMath::Lerp(1.0f, 0.212f, ratio2);
			colorTintB *= FMath::Lerp(1.0f, 0.136f, ratio2);
		}

		if (ShockShielded == true)
		{
			PostProcessSettings.FilmContrast = FMath::Lerp(neutralFilmContrast, 0.25f, ratio0);
		}
		else
		{
			PostProcessSettings.FilmContrast = FMath::Lerp(neutralFilmContrast, 0.33f, ratio0);
		}

		PostProcessSettings.VignetteIntensity = FMath::Lerp(0.0f, 0.6f, ratio2);
	}
	else
	{
		PostProcessSettings.FilmContrast = neutralFilmContrast;
		PostProcessSettings.VignetteIntensity = 0.0f;
		PostProcessSettings.bOverride_FilmContrast = false;
		PostProcessSettings.bOverride_VignetteIntensity = false;
	}

	PostProcessSettings.bOverride_SceneFringeIntensity = (fringe > 0.0f);
	PostProcessSettings.SceneFringeIntensity = fringe;

	PostProcessSettings.SceneColorTint.R = colorTintR;
	PostProcessSettings.SceneColorTint.G = colorTintG;
	PostProcessSettings.SceneColorTint.B = colorTintB;

	PostProcessSettings.bOverride_SceneColorTint = colorTintR != environmentSceneTint.R || colorTintG != environmentSceneTint.G || colorTintB != environmentSceneTint.B;

	bool updatePostProcess = (owningVehicle == nullptr) || (owningVehicle != nullptr && owningVehicle->LocalPlayerIndex >= 0);
	ABaseVehicle* viewingVehicle = GRIP_POINTER_VALID(ViewingActor) == true ? Cast<ABaseVehicle>(ViewingActor.Get()) : owningVehicle;

	if (viewingVehicle == nullptr)
	{
		// It's very unusual not to have a vehicle, maybe in multiplayer spectator or something,
		// but we handle it here by setting some sensible defaults and disabling the blendable.

		NativeFieldOfView = 90.0f;

		if (PostProcessSettings.WeightedBlendables.Array.Num() > 0)
		{
			PostProcessSettings.WeightedBlendables.Array[0].Weight = 0.0f;
		}
	}
	else
	{
		float speedKPH = viewingVehicle->GetSpeedKPH();
		float speedMitigation = (viewingVehicle->GetRaceState().DragScale > 1.0f) ? 1.0f - viewingVehicle->GetRaceState().DragScale : 0.0f;

		// Scale all the effects down according to how forward facing the camera is
		// with respect to the parent vehicle.

		// Mitigation is used to reduce the apparent effects of speed as we don't
		// want to emphasize the catchup effect more than it already is.

		float scale = FMath::Max(FVector::DotProduct(desiredView.Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f)), viewingVehicle->GetVelocityOrFacingDirection()), 0.0f);
		float reduceSpeed = FMath::Max(1.0f - (speedMitigation * 3.0f), 0.0f) * scale;
		float effects = GetNativeEffectsAmount();
		float blur = (float)gameState->GraphicsOptions.SpeedBlur / 3.0f;
		float blurAmount = FMath::Max(RadialSpeedBlurVsSpeed.GetRichCurve()->Eval(speedKPH), shockBlur * 2.5f) * effects * reduceSpeed * (blur * 0.666f);
		float ionizationAmount = IonisationVsSpeed.GetRichCurve()->Eval(speedKPH) * effects * reduceSpeed;
		float speedStreakingAmount = StreaksVsSpeed.GetRichCurve()->Eval(speedKPH) * effects * reduceSpeed * 1.33f * FMath::Min(blur, 0.5f);

		if (gameState->IsGameModeRace() == true)
		{
			// In race mode make the field of view widen as we get faster.

			// Also take into account the user preferences in reaching the maximum FOV
			// more quickly if that's what they want. We also allow them to get a higher
			// FOV of 150 if they're amplifying it up.

			// FieldOfViewVsSpeed returns something between 70 and 120, normally.

			// Add another 50% to that for full FOV scaling preference and you get 180 degrees,
			// so we do indeed have to clamp it at 150 degrees.

			// The default FOV scale is another 20%, so the normal range will be 84 to 144 degrees.

			NativeFieldOfView = FieldOfViewVsSpeed.GetRichCurve()->Eval(speedKPH);
			NativeFieldOfView += (NativeFieldOfView - FieldOfViewVsSpeed.GetRichCurve()->GetFirstKey().Value) * gameState->GeneralOptions.RaceCameraFOVScale * 0.5f;
			NativeFieldOfView = FMath::Min(NativeFieldOfView, 150.0f);
		}

#pragma region VehicleSpringArm

		if (owningVehicle != nullptr)
		{
			if (owningVehicle->SpringArm->IsBumperView() == true ||
				owningVehicle->SpringArm->IsCockpitView() == true)
			{
				// Override the field-of-view to a fixed value if this vehicle is
				// using the bumper or cockpit view for the spring-arm.

				NativeFieldOfView = FMath::Lerp(90.0f, 120.0f, gameState->GeneralOptions.RaceCameraFOVScale);
			}
		}

		// Manage the spring arm.

		UFlippableSpringArmComponent* springArm = Cast<UFlippableSpringArmComponent>(GetAttachParent());

		if (springArm != nullptr)
		{
			// This allows the spring arm to bring the camera closer to the car to counteract
			// the effects of perspective foreshortening.

			// Make the camera wobble about at high speed.

			float v0 = FieldOfViewVsSpeed.GetRichCurve()->GetFirstKey().Value;
			float v1 = FieldOfViewVsSpeed.GetRichCurve()->GetLastKey().Value;

			springArm->SpeedShakeAmount = RadialSpeedBlurVsSpeed.GetRichCurve()->Eval(speedKPH);
			springArm->SpeedShakeAmount = FMath::Max(springArm->SpeedShakeAmount, RadialSpeedBlurVsSpeed.GetRichCurve()->Eval(speedKPH * 2.0f * viewingVehicle->GetAutoBoostShake()));

			float shakeSpeedRatio = viewingVehicle->GetAutoBoostShake() * FMath::Min(1.0f, (speedKPH / 400.0f));

			springArm->SpeedShakeSpeed = FMath::Lerp(1.0f, 1.5f, shakeSpeedRatio);
			springArm->FieldOfViewBias = NativeFieldOfView - v0;
			springArm->FieldOfViewProportion = (NativeFieldOfView - v0) / (v1 - v0);
		}

#pragma endregion VehicleSpringArm

		FVector2D blurCenter(0.5f, 0.5f);

		SpeedTimer += speedKPH / 75000.0f;
		SpeedTimer = FMath::Fmod(SpeedTimer, 64.0f);

		if (PostProcessSettings.WeightedBlendables.Array.Num() > 0)
		{
			if (updatePostProcess == true)
			{
				PostProcessSettings.WeightedBlendables.Array[0].Weight = 1.0f;

				// Calculate where on the screen the vehicle is to offset the speed blurring away from that.

				APlayerController* controller = (owningVehicle == nullptr) ? nullptr : Cast<APlayerController>(owningVehicle->GetController());

				if (controller != nullptr)
				{
					FVector2D position;
					FVector location = viewingVehicle->GetTargetLocation();
					FVector screenLocation;
					float effectsScale = 1.0f;

					if (controller->ProjectWorldLocationToScreenWithDistance(location, screenLocation) == true)
					{
						position.X = screenLocation.X;
						position.Y = screenLocation.Y;

						int32 x, y;

						ABaseGameMode::GetGameViewportSize(x, y, controller);

						ULocalPlayer* localPlayer = controller->GetLocalPlayer();

						if (localPlayer != nullptr &&
							localPlayer->ViewportClient != nullptr)
						{
							FSceneViewProjectionData projectionData;

							if (localPlayer->GetProjectionData(localPlayer->ViewportClient->Viewport, eSSP_FULL, projectionData))
							{
								position.X -= projectionData.GetConstrainedViewRect().Min.X;
								position.Y -= projectionData.GetConstrainedViewRect().Min.Y;

								x = projectionData.GetConstrainedViewRect().Max.X - projectionData.GetConstrainedViewRect().Min.X;
								y = projectionData.GetConstrainedViewRect().Max.Y - projectionData.GetConstrainedViewRect().Min.Y;
							}
						}

						position.X /= x;
						position.Y /= y;

						blurCenter = position;

						effectsScale = 1.0f - ((blurCenter - FVector2D(0.5f, 0.5f)).Size() * 0.5f);

						FVector2D offset = blurCenter - FVector2D(0.5f, 0.5f);

						if (offset.Size() > 0.4f)
						{
							offset.Normalize();
							offset *= 0.4f;
							blurCenter = offset + FVector2D(0.5f, 0.5f);
						}
					}

					FVector cameraSpace = GetComponentTransform().InverseTransformPositionNoScale(location);

					cameraSpace.X -= 200.0f;

					if (cameraSpace.X < 0.0f)
					{
						// Behind the camera.

						effectsScale = 1.0f;
						blurCenter = FVector2D(0.5f, 0.5f);
					}
					else if (cameraSpace.X < 200.0f)
					{
						// Very close to the camera.

						float ratio = 1.0f - (cameraSpace.X / 200.0f);

						effectsScale = FMath::Lerp(effectsScale, 1.0f, ratio);
						blurCenter = FMath::Lerp(blurCenter, FVector2D(0.5f, 0.5f), ratio);
					}

					speedStreakingAmount *= effectsScale;
					blurAmount *= effectsScale;
				}

				if (gameState->UsingVerticalSplitScreen() == true ||
					gameState->UsingHorizontalSplitScreen() == true)
				{
					speedStreakingAmount *= 0.9f;
				}

				float filmGrain = gameState->GraphicsOptions.FilmGrain;
				float distortionAmount = CameraFeed.GetDistortionAmount();

				if (distortionAmount > 0.001f)
				{
					PostProcessSettings.WeightedBlendables.Array[0].Object = ExpensiveCameraMaterial;
				}
				else
				{
					PostProcessSettings.WeightedBlendables.Array[0].Object = CheapCameraMaterial;
				}

				UMaterialInstanceDynamic* material = Cast<UMaterialInstanceDynamic>(PostProcessSettings.WeightedBlendables.Array[0].Object);

				if (material != nullptr)
				{
					// Setup the material setters if necessary.

					if (material != RealTimeSetter.Material)
					{
						SetupMaterialSetters(material, anyVehicle);
					}

					// Now update the material setters with the latest parameters.

					if (material == RealTimeSetter.Material)
					{
						float noiseScale = FilmNoiseScale;

#if !WITH_EDITOR
						if (gameState->UsingSplitScreen() == true)
						{
							if ((gameState->GeneralOptions.NumberOfLocalPlayers == 2) ||
								(gameState->GraphicsOptions.ScreenResolution.Width >= gameState->GraphicsOptions.ScreenResolution.Height && gameState->GeneralOptions.SplitScreenLayout == ESplitScreenLayout::TwoPlayerHorizontal) ||
								(gameState->GraphicsOptions.ScreenResolution.Width <= gameState->GraphicsOptions.ScreenResolution.Height && gameState->GeneralOptions.SplitScreenLayout == ESplitScreenLayout::TwoPlayerVertical))

							{
								noiseScale *= 0.5f;
							}
						}
#endif // !WITH_EDITOR

						if (gameMode != nullptr)
						{
							RealTimeSetter.Set(gameMode->GetRealTimeClock());
						}

						SpeedTimerSetter.Set(SpeedTimer);
						IonizationAmountSetter.Set(ionizationAmount);
						BlurAmountSetter.Set(blurAmount);
						SpeedStreakingAmountSetter.Set(speedStreakingAmount);
						WarningAmountSetter.Set(viewingVehicle->GetWarningAmount());
						WarningColorSetter.Set(viewingVehicle->GetWarningColour());
						NoiseScaleSetter.Set(noiseScale);
						NoiseAmountSetter.Set(FMath::Max(FilmNoiseAmount * FMath::Pow(filmGrain, 1.25f), environmentFilmNoiseAmount));
						TelevisionDistortionAmountSetter.Set(distortionAmount);
						BlurCenterSetter.Set(FLinearColor(blurCenter.X, blurCenter.Y, 0.0f, 0.0f));
						MirrorSetter.Set((gameState->IsTrackMirrored() == true) ? -1.0f : 1.0f);
					}
				}
			}
			else
			{
				PostProcessSettings.WeightedBlendables.Array[0].Weight = 0.0f;
			}
		}
	}
}

/**
* Get the owning vehicle for this camera.
***********************************************************************************/

ABaseVehicle* URaceCameraComponent::GetOwningVehicle() const
{
	const USceneComponent* sceneComp = this;

	while (sceneComp != nullptr)
	{
		ABaseVehicle* owningVehicle = Cast<ABaseVehicle>(sceneComp->GetOwner());

		if (owningVehicle != nullptr)
		{
			return owningVehicle;
		}

		sceneComp = sceneComp->GetAttachParent();
	}

	return nullptr;
}

/**
* Setup the post-process materials for the camera.
***********************************************************************************/

void URaceCameraComponent::SetupMaterials(ABaseVehicle* vehicle)
{
	if (DynamicMaterial == false)
	{
		if (vehicle == nullptr)
		{
			APlayGameMode* gameMode = APlayGameMode::Get(GetWorld());

			if (gameMode != nullptr)
			{
				vehicle = gameMode->GetVehicleForVehicleIndex(0);
			}
		}

		if (vehicle != nullptr)
		{
			while (PostProcessSettings.WeightedBlendables.Array.Num() > 0)
			{
				PostProcessSettings.RemoveBlendable(Cast<UMaterial>(PostProcessSettings.WeightedBlendables.Array[0].Object));
			}

			CheapCameraMaterial = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, vehicle->CheapCameraMaterial);
			ExpensiveCameraMaterial = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, vehicle->ExpensiveCameraMaterial);

			PostProcessSettings.AddBlendable(CheapCameraMaterial, 1.0f);

			DynamicMaterial = true;
		}
	}
}

/**
* Some static FNames for performance benefit.
***********************************************************************************/

namespace CameraParameterNames
{
	static const FName RealTimeName("RealTime");
	static const FName SpeedTimerName("SpeedTimer");
	static const FName IonizationAmountName("IonizationAmount");
	static const FName BlurAmountName("BlurAmount");
	static const FName SpeedStreakingAmountName("SpeedStreakingAmount");
	static const FName WarningAmountName("WarningAmount");
	static const FName WarningColorName("WarningColor");
	static const FName NoiseScaleName("NoiseScale");
	static const FName NoiseAmountName("NoiseAmount");
	static const FName StaticAmountName("StaticAmount");
	static const FName TelevisionDistortionAmountName("TelevisionDistortionAmount");
	static const FName BlurCenterName("BlurCenter");
	static const FName MirrorName("Mirror");
};

/**
* Setup the material setters for the camera to speed its update.
***********************************************************************************/

void URaceCameraComponent::SetupMaterialSetters(UMaterialInstanceDynamic* material, ABaseVehicle* vehicle)
{
	if (material != nullptr &&
		vehicle != nullptr)
	{
		// This code will get executed every time the material used for post-processing changes
		// so we need to ensure it's fairly optimal.

		RealTimeSetter.Setup(material, CameraParameterNames::RealTimeName);
		SpeedTimerSetter.Setup(material, CameraParameterNames::SpeedTimerName);
		IonizationAmountSetter.Setup(material, CameraParameterNames::IonizationAmountName);
		BlurAmountSetter.Setup(material, CameraParameterNames::BlurAmountName);
		SpeedStreakingAmountSetter.Setup(material, CameraParameterNames::SpeedStreakingAmountName);
		WarningAmountSetter.Setup(material, CameraParameterNames::WarningAmountName);
		WarningColorSetter.Setup(material, CameraParameterNames::WarningColorName);
		NoiseScaleSetter.Setup(material, CameraParameterNames::NoiseScaleName);
		NoiseAmountSetter.Setup(material, CameraParameterNames::NoiseAmountName);
		StaticAmountSetter.Setup(material, CameraParameterNames::StaticAmountName);
		TelevisionDistortionAmountSetter.Setup(material, CameraParameterNames::TelevisionDistortionAmountName);
		BlurCenterSetter.Setup(material, CameraParameterNames::BlurCenterName);
		MirrorSetter.Setup(material, CameraParameterNames::MirrorName, (UGlobalGameState::GetGlobalGameState(GetWorld())->IsTrackMirrored() == true) ? -1.0f : 1.0f);
	}
}

#pragma endregion VehicleCamera
