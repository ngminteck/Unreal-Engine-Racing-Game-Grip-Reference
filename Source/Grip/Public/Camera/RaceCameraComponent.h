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

#pragma once

#include "system/gameconfiguration.h"
#include "camera/advancedcameracomponent.h"
#include "racecameracomponent.generated.h"

class ABaseVehicle;

#pragma region VehicleCamera

/**
* Structure used for tampering with the camera feed, causing distortion.
***********************************************************************************/

struct FCameraFeedTamperer
{
public:

	// Initiate some tampering.
	void Initiate(float duration, float delay, bool includeStatic = true);

	// Update the camera feed tamperer for a particular vehicle.
	bool Update(float deltaSeconds, ABaseVehicle* vehicle);

	// Is tampering active?
	bool IsActive() const
	{ return Clock < TamperingDuration; }

	// Get the amount of distortion currently being applied to the camera feed tamperer.
	float GetDistortionAmount() const;

	// Reset the tampering.
	void Reset()
	{ Clock = TamperingDuration = 0.0f; }

	// Delay to apply before tampering takes place.
	float Delay = 0.0f;

	// The clock for the tampering.
	float Clock = 0.0f;

	// The clock for the current mode, the mode will quickly flicker between tampering and not tampering.
	float ModeClock = 0.0f;

	// The duration of the current mode.
	float ModeClockDuration = 0.0f;

	// How long to apply tampering for.
	float TamperingDuration = 0.0f;

	// Apply tampering or not.
	bool TamperMode = false;

	// Include static interference with the tampering.
	bool IncludeStatic = true;
};

#pragma endregion VehicleCamera

/**
* A race camera component for adding vehicle-specific enhancements.
***********************************************************************************/

UCLASS(ClassGroup = Camera, hideCategories = Transform, meta = (BlueprintSpawnableComponent))
class GRIP_API URaceCameraComponent : public UAdvancedCameraComponent
{
	GENERATED_BODY()

public:

	// Construct a race camera component.
	URaceCameraComponent();

	// The amount of field of view to use in degrees, vs speed in kilometers per hour.
	UPROPERTY(EditAnywhere, Category = RaceCameraSettings)
		FRuntimeFloatCurve FieldOfViewVsSpeed;

	// The amount of radial speed blur to apply, vs speed in kilometers per hour.
	UPROPERTY(EditAnywhere, Category = RaceCameraSettings)
		FRuntimeFloatCurve RadialSpeedBlurVsSpeed;

	// The amount of fiery ionization to apply, vs speed in kilometers per hour.
	UPROPERTY(EditAnywhere, Category = RaceCameraSettings)
		FRuntimeFloatCurve IonisationVsSpeed;

	// The amount of speed streaking to apply, vs speed in kilometers per hour.
	UPROPERTY(EditAnywhere, Category = RaceCameraSettings)
		FRuntimeFloatCurve StreaksVsSpeed;

	// How much to scale film noise by, affecting its granularity.
	UPROPERTY(EditAnywhere, Category = RaceCameraSettings, meta = (UIMin = "100", UIMax = "10000", ClampMin = "100", ClampMax = "10000"))
		float FilmNoiseScale = 666.0f;

	// The amount of film noise to apply.
	UPROPERTY(EditAnywhere, Category = RaceCameraSettings, meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
		float FilmNoiseAmount = 0.0f;

#pragma region VehicleCamera

public:

	// Shock the camera for a few seconds, often due to nearby explosion.
	void Shock(bool shielded, float scale = 1.0f)
	{ ShockTimer = ShockTime * scale; ShockShielded = shielded; }

	// Helper class to tamper with the camera feed as a result of damage.
	FCameraFeedTamperer CameraFeed;

protected:

	// Do some initialization when the game is ready to play.
	virtual void BeginPlay() override;

	// Do some shutdown when the component is being destroyed.
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

	// Do the regular update tick.
	virtual void TickComponent(float deltaSeconds, enum ELevelTick tickType, FActorComponentTickFunction* thisTickFunction) override;

private:

	// Get the owning vehicle for this camera.
	ABaseVehicle* GetOwningVehicle() const;

	// Setup the post-process materials for the camera.
	void SetupMaterials(ABaseVehicle* vehicle);

	// Setup the material setters for the camera to speed its update.
	void SetupMaterialSetters(UMaterialInstanceDynamic* material, ABaseVehicle* vehicle);

	// How long to shock the camera for.
	static const int32 ShockTime = 3;

	// Timer used to animate effects related to camera speed.
	float SpeedTimer = 0.0f;

	// Timer associated with camera shock.
	float ShockTimer = 0.0f;

	// Was the shock shielded by the parent vehicle?
	bool ShockShielded = false;

	// Has a dynamic material been set for custom camera post-processing?
	bool DynamicMaterial = false;

	// Material setters for the camera to speed its update.
	FMathEx::FMaterialScalarParameterSetter RealTimeSetter;
	FMathEx::FMaterialScalarParameterSetter SpeedTimerSetter;
	FMathEx::FMaterialScalarParameterSetter IonizationAmountSetter;
	FMathEx::FMaterialScalarParameterSetter BlurAmountSetter;
	FMathEx::FMaterialScalarParameterSetter SpeedStreakingAmountSetter;
	FMathEx::FMaterialScalarParameterSetter WarningAmountSetter;
	FMathEx::FMaterialVectorParameterSetter WarningColorSetter;
	FMathEx::FMaterialScalarParameterSetter NoiseScaleSetter;
	FMathEx::FMaterialScalarParameterSetter NoiseAmountSetter;
	FMathEx::FMaterialScalarParameterSetter StaticAmountSetter;
	FMathEx::FMaterialScalarParameterSetter TelevisionDistortionAmountSetter;
	FMathEx::FMaterialVectorParameterSetter BlurCenterSetter;
	FMathEx::FMaterialScalarParameterSetter MirrorSetter;

	friend class ADebugRaceCameraHUD;

#pragma endregion VehicleCamera

	// The cheap, minimal material to use for the race camera.
	UPROPERTY(Transient)
		UMaterialInstanceDynamic* CheapCameraMaterial = nullptr;

	// The expensive, maximal material to use for the race camera.
	UPROPERTY(Transient)
		UMaterialInstanceDynamic* ExpensiveCameraMaterial = nullptr;
};
