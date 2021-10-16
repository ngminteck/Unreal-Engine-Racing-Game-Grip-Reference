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

#pragma once

#include "system/gameconfiguration.h"
#include "camera/cameracomponent.h"
#include "camera/cinematicsdirector.h"
#include "system/mathhelpers.h"
#include "advancedcameracomponent.generated.h"

/**
* An advanced camera component for adding helper functionality to generalized camera
* work.
***********************************************************************************/

UCLASS(ClassGroup = Camera, meta = (BlueprintSpawnableComponent))
class GRIP_API UAdvancedCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:

	// Construct an advanced camera component.
	UAdvancedCameraComponent();

	// Is the location to receive smoothing?
	UPROPERTY(EditAnywhere, Category = AdvancedCameraSettings)
		bool SmoothLocation = false;

	// Controls how quickly camera reaches target location.
	UPROPERTY(EditAnywhere, Category = AdvancedCameraSettings, meta = (EditCondition = "SmoothLocation"))
		float LocationLagRatio = 0.9f;

	// Is the rotation to receive smoothing?
	UPROPERTY(EditAnywhere, Category = AdvancedCameraSettings)
		bool SmoothRotation = false;

	// Controls how quickly camera reaches target yaw angle.
	UPROPERTY(EditAnywhere, Category = AdvancedCameraSettings, meta = (EditCondition = "SmoothRotation"))
		float YawLagRatio = 0.9f;

	// Controls how quickly camera reaches target pitch angle.
	UPROPERTY(EditAnywhere, Category = AdvancedCameraSettings, meta = (EditCondition = "SmoothRotation"))
		float PitchLagRatio = 0.9f;

	// Controls how quickly camera reaches roll angle.
	UPROPERTY(EditAnywhere, Category = AdvancedCameraSettings, meta = (EditCondition = "SmoothRotation"))
		float RollLagRatio = 0.9f;

	// Is the mouse rotation to receive smoothing?
	UPROPERTY(EditAnywhere, Category = AdvancedCameraSettings)
		bool SmoothMouseRotation = true;

	// Is the roll axis locked?
	UPROPERTY(EditAnywhere, Category = AdvancedCameraSettings)
		bool LockRollAxis = false;

	// Returns camera's point of view, more quickly if we don't required post processing information.
	void GetCameraViewNoPostProcessing(float deltaSeconds, FMinimalViewInfo& desiredView)
	{
		float temp = PostProcessBlendWeight;
		PostProcessBlendWeight = 0.0f;
		GetCameraView(deltaSeconds, desiredView);
		PostProcessBlendWeight = temp;
	}

#pragma region VehicleCamera

public:

	// Fade camera effects over to custom control.
	void FadeEffectsToCustomControl(float transitionDuration = 1.0f, float transitionEasing = 2.0f);

	// Fade camera effects over to native control.
	void FadeEffectsToNativeControl(float transitionDuration = 1.0f, float transitionEasing = 2.0f);

	// Switch camera effects over to custom control.
	void SwitchEffectsToCustomControl();

	// Switch camera effects over to native control.
	void SwitchEffectsToNativeControl();

	// Fade camera rotation over to mouse control.
	void FadeRotationToMouseControl(float transitionDuration = 1.0f, float transitionEasing = 2.0f, bool inheritCurrentRotation = true);

	// Fade camera rotation over to custom control.
	void FadeRotationToCustomControl(float transitionDuration = 1.0f, float transitionEasing = 2.0f);

	// Fade camera rotation over to native control.
	void FadeRotationToNativeControl(float transitionDuration = 1.0f, float transitionEasing = 2.0f);

	// Switch camera rotation over to mouse control.
	void SwitchRotationToMouseControl(bool inheritCurrentRotation = true);

	// Switch camera rotation over to custom control.
	void SwitchRotationToCustomControl();

	// Switch camera rotation over to native control.
	void SwitchRotationToNativeControl();

	// Switch camera location over to mouse control.
	void SwitchLocationToMouseControl(bool inheritCurrentLocation = true);

	// Switch camera location over to custom control.
	void SwitchLocationToCustomControl();

	// Switch camera location over to native control.
	void SwitchLocationToNativeControl();

	// Fade camera field of view over to custom control.
	void FadeFieldOfViewToCustomControl(float transitionDuration = 1.0f, float transitionEasing = 2.0f);

	// Fade camera field of view over to native control.
	void FadeFieldOfViewToNativeControl(float transitionDuration = 1.0f, float transitionEasing = 2.0f);

	// Switch camera field of view over to custom control.
	void SwitchFieldOfViewToCustomControl();

	// Switch camera field of view over to native control.
	void SwitchFieldOfViewToNativeControl();

	// Have the custom rotation look at a particular location.
	void LookAtCustomLocation(FVector location);

	// Have the custom rotation look at a particular actor.
	void LookAtCustomActor(AActor* actor);

	// Have the custom rotation reset back to none.
	void LookAtCustomNone();

	// The custom field of view.
	float CustomFieldOfView = 90.0f;

	// The custom effects amount.
	float CustomEffectsAmount = 0.0f;

	// The custom location.
	FVector CustomLocation = FVector::ZeroVector;

	// The custom rotation.
	FRotator CustomRotation = FRotator::ZeroRotator;

	// Do the regular update tick.
	virtual void TickComponent(float deltaSeconds, enum ELevelTick tickType, FActorComponentTickFunction* thisTickFunction) override;

	// Returns camera's point of view.
	virtual void GetCameraView(float deltaSeconds, FMinimalViewInfo& desiredView) override;

	// Restore the relative transform once the camera has been used.
	void RestoreRelativeTransform();

	// Get the smoothed rotation for a given duration, always taking the shortest path if pre-normalized.
	FRotator GetSmoothedRotation(FRotator previous, FRotator next, float deltaSeconds) const
	{ return FMathEx::GetSmoothedRotation(previous, next, deltaSeconds, YawLagRatio, PitchLagRatio, RollLagRatio); }

	// Set whether a component (and its children) can be seen by their owner.
	void SetOwnerNoSee(UPrimitiveComponent* component, bool nosee) const;

	// Set whether a component (and its children) can be seen only by their owner.
	void SetOnlyOwnerSee(UPrimitiveComponent* component, bool nosee) const;

	// Calculate the field of view to view an object of a given radius so that it covers a given proportion of the screen.
	static float GetFieldOfViewForRadius(const FVector& cameraLocation, const FVector& actorLocation, float radius, float screenProportion);

	// Calculate the distance to an object of a given radius using a given field of view so that it covers a given proportion of the screen.
	static float GetDistanceForRadius(float radius, float screenProportion, float fov);

	// Calculate an adjusted FOV taking into account the viewport for the local player.
	static float GetAdjustedFOV(APlayerController* controller, float degrees);

	// Update the native location and rotation from the component's world location and rotation.
	void UpdateFromComponent()
	{
		NativeLocation = GetComponentLocation();
		NativeRotation = GetComponentRotation();
	}

	// Get the native world location.
	const FVector& GetNativeLocation() const
	{ return NativeLocation; }

	// Get the native field of view.
	float GetNativeFieldOfView() const
	{ return NativeFieldOfView; }

	// Get the current mouse rotation.
	const FRotator& GetCurrentMouseRotation() const
	{ return CurrentMouseRotation; }

	// Reset the mouse rotation back to zero.
	void ResetMouseRotation()
	{ MouseRotation = CurrentMouseRotation = FRotator::ZeroRotator; }

protected:

	// The faders we can use for sources of different camera properties.
	enum ECameraFader
	{
		FaderNative,
		FaderCustom,
		FaderMouse,
		NumFaders
	};

	// The different targets we can focus the camera upon.
	enum class ECameraTarget : uint8
	{
		TargetLocation,
		TargetActor,
		TargetNone
	};

	// Set an effect fader to a particular amount and set the others accordingly.
	void SetEffectsFader(int32 faderIndex, float amount);

	// Set a rotation fader to a particular amount and set the others accordingly.
	void SetRotationFader(int32 faderIndex, float amount);

	// Set a field of view fader to a particular amount and set the others accordingly.
	void SetFieldOfViewFader(int32 faderIndex, float amount);

	// Get the amount of native effects to use.
	float GetNativeEffectsAmount()
	{ return FMathEx::EaseInOut(EffectSources[FaderNative], EffectsFaderEasing) + CustomEffectsAmount * FMathEx::EaseInOut(EffectSources[FaderCustom], EffectsFaderEasing); }

	// The native field of view.
	float NativeFieldOfView = 90.0f;

	// The native location.
	FVector NativeLocation = FVector::ZeroVector;

	// The mouse location.
	FVector MouseLocation = FVector::ZeroVector;

	// The native rotation.
	FRotator NativeRotation = FRotator::ZeroRotator;

	// The mouse rotation.
	FRotator MouseRotation = FRotator::ZeroRotator;

	// The mouse rotation.
	FRotator CurrentMouseRotation = FRotator::ZeroRotator;

	// The fader we are using for effects.
	ECameraFader EffectsFaderIndex = FaderNative;

	// The effects for each of the sources.
	float EffectSources[NumFaders];

	// Delta time value to use when fading between effect sources.
	float EffectsFaderDelta = 1.0f;

	// The easing value to use when fading between effect sources.
	float EffectsFaderEasing = 1.0f;

	// Which source we are using for location (no fading / smoothing performed for obvious reasons).
	ECameraFader LocationIndex = FaderNative;

	// Which custom target to focus the camera upon.
	ECameraTarget LookAtCustomTarget = ECameraTarget::TargetNone;

	// The custom target location for focusing the camera upon.
	FVector CustomTargetLocation = FVector::ZeroVector;

	// The custom target actor for focusing the camera upon.
	TWeakObjectPtr<AActor> CustomTargetActor;

	// The fader we are using for rotation.
	ECameraFader RotationFaderIndex = FaderNative;

	// The rotations for each of the sources.
	float RotationSources[NumFaders];

	// Delta time value to use when fading between rotation sources.
	float RotationFaderDelta = 1.0f;

	// The easing value to use when fading between rotation sources.
	float RotationFaderEasing = 1.0f;

	// The fader we are using for field-of-view.
	int32 FieldOfViewFaderIndex = FaderNative;

	// The field-of-view values for each of the sources.
	float FieldOfViewSources[NumFaders];

	// Delta time value to use when fading between field-of-view sources.
	float FieldOfViewFaderDelta = 1.0f;

	// The easing value to use when fading between field-of-view sources.
	float FieldOfViewFaderEasing = 1.0f;

	// Which actor are we currently viewing?
	TWeakObjectPtr<AActor> ViewingActor;

	// Has the cached relative transform been set?
	bool RelativeTransformSet = false;

	// Cached relative transform so we can reset it easily.
	FTransform RelativeTransform;

	// The location to smooth to.
	FVector SmoothedLocation = FVector::ZeroVector;

	// The rotation to smooth to.
	FRotator SmoothedRotation = FRotator::ZeroRotator;

	// The last view given before the game was paused.
	FMinimalViewInfo LastView;

#pragma endregion VehicleCamera

#pragma region CameraCinematics

public:

	// Cinematic manager for this camera.
	FCinematicsDirector& GetCinematicsDirector()
	{ return CinematicsDirector; }

private:

	// Cinematics director for this camera.
	FCinematicsDirector CinematicsDirector = FCinematicsDirector(this);

	friend class FCinematicsDirector;

#pragma endregion CameraCinematics

};
