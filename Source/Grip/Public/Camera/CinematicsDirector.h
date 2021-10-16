/**
*
* Cinematics director.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* All of the enums, structures and classes used for managing the cinematic camera,
* itself a director in coordinating the viewing of the action, encapsulated in the
* FCinematicsDirector class.
*
* The code that drives the cinematic camera sequencing at the end of a race and
* during the attract mode for the game.
*
***********************************************************************************/

#pragma once

#include "system/gameconfiguration.h"
#include "system/commontypes.h"
#include "camera/camerapointcomponent.h"

#pragma region CameraCinematics

#include "camera/statictrackcamera.h"
#include "gamemodes/playgamemode.h"

class ABaseVehicle;
class UAdvancedSplineComponent;
class UAdvancedCameraComponent;

/**
* Start transition for a spline camera.
***********************************************************************************/

enum class ECameraStartTransition : uint8
{
	Random,
	None,
	Lower,
	SpeedUp,
	SlowUp,
	Rotate,
	CrossoverForwards,
	CrossoverBackwards
};

/**
* End transition for a spline camera.
***********************************************************************************/

enum class ECameraEndTransition : uint8
{
	Random,
	None,
	Raise,
	SpeedUp,
	SlowUp
};

/**
* View direction for a spline camera.
***********************************************************************************/

enum class ECameraViewDirection : uint8
{
	Random,
	Forwards,
	Backwards,
	Sideways,
	Overhead,
	Crossover
};

/**
* Structure for managing a dynamic field of view for a camera.
***********************************************************************************/

struct FDynamicFOV
{
public:

	// The fixed field of view of the camera if not using dynamic zoom.
	float FieldOfView = 60.0f;

	// What kind of proportion of the screen should the target fill?
	float TargetScreenProportion = 0.4f;

	// Should the camera dynamically zoom to the target?
	bool DynamicZoom = false;

	// The min field of view of the camera if using dynamic zoom.
	float MinFieldOfView = 20.0f;

	// The max field of view of the camera if using dynamic zoom.
	float MaxFieldOfView = 60.0f;

	// The target field-of-view.
	float TargetFieldOfView = 0.0f;

	// The rate of change between the current and target field-of-view.
	float FieldOfViewChangeRate = 0.0f;

	// The time at which the field-of-view was last changed.
	float LastFieldOfViewChangeTime = 0.0f;

	// FOV difference break when zooming in.
	float FieldOfViewBreakIn = 5.0f;

	// FOV difference break when zooming out.
	float FieldOfViewBreakOut = 5.0f;
};

/**
* Structure for a camera following a spline to view the action.
***********************************************************************************/

struct FSplineCamera
{
public:

	// Construct an FSplineCamera.
	FSplineCamera(float startDistance, float endDistance)
		: StartDistance(startDistance)
		, EndDistance(endDistance)
		, VisibilityQueryParams("CameraVisibilityClipping", true)
	{
		for (auto& value : OffsetFromGround) value = 0.0f;
		for (auto& value : LastSplineOffset) value = FVector::ZeroVector;
	}

	// Do the regular update tick.
	void Tick(float deltaSeconds, bool modeReset);

	// Is the spline camera currently in use?
	bool IsInUse() const
	{ return (Spline != nullptr && Target != nullptr); }

	// Get the current location of the spline camera.
	FVector GetLocation() const
	{ return WorldLocation; }

	// Get the current rotation of the spline camera.
	FRotator GetRotation(bool locked);

	// Get the angle difference between where the camera is looking and where the target is.
	float GetAngleToTarget() const;

	// Get the distance along a spline for a given vehicle location.
	float GetSplineDistance(float distanceAlongSpline, const FVector& vehicleLocation, float offset, float deltaSeconds, FVector& newLocation);

	// Get the world location for given point along the spline taking into account smoothing.
	void GetSplineWorldLocation(const FVector& vehicleLocation, float splineDistanceOffset, float deltaSeconds, bool reset, FVector& currentLocation, FVector& targetLocation);

	// Is the spline camera easing out of its viewing session?
	bool IsEasingOut() const
	{ return EasingDirection == -1; }

	// Is the viewing of the action from this particular spline camera currently OK for being interrupted?
	bool IsInterruptable() const
	{ return Clock > (1.0f / EasingDelta) + 3.0f && ViewDirection != ECameraViewDirection::Crossover && (ViewDirection == ECameraViewDirection::Overhead || EasingDirection != -1); }

	// Set the end time for viewing from a spline camera.
	void SetEndTime(float secondsFromNow, float timeScale = 1.0f);

	// Get the amount of time left for the spline camera's viewing session.
	float GetTimeLeft() const
	{ return FMath::Max(0.0f, EndClock - Clock); }

	// The spline to move along.
	TWeakObjectPtr<UAdvancedSplineComponent> Spline;

	// The target object to follow.
	TWeakObjectPtr<ABaseVehicle> Target;

	// The distance along the spline that the shot should start after.
	float StartDistance = 0.0f;

	// The distance along the spline that the shot should end before.
	float EndDistance = 0.0f;

	// The height above ground to keep the camera at.
	float HeightAboveGround = 4.0f * 100.0f;

	// The range of motion to keep eyes on the target.
	float AngleRange = 170.0f;

	// The distance to keep from the target along the spline.
	float LongitudinalDistanceFromTarget = 10.0f * 100.0f;

	// 1 = forwards towards the target, -1 = backwards towards the target, 0 = alongside target.
	ECameraViewDirection ViewDirection = ECameraViewDirection::Random;

	// The transition to apply at the start of the shot.
	ECameraStartTransition StartTransition = ECameraStartTransition::Random;

	// The transition to apply at the end of the shot.
	ECameraEndTransition EndTransition = ECameraEndTransition::Random;

	// Should the camera dynamically roll with yaw tracking to target?
	bool RollingYawTracking = false;

	// The current distance along the spline, currently the same as TargetDistanceAlongSpline below.
	float DistanceAlongSpline = 0.0f;

	// The current distance offset above the normal spline height, used for transitioning in and out.
	float DistanceAboveSpline = 0.0f;

	// The current distance along the spline, like DistanceAlongSpline, but smoothed.
	float ProjectedDistanceAlongSpline = 0.0f;

	// The current view-target distance along the spline - this will have some natural jitter due to the algorithm.
	float TargetDistanceAlongSpline = 0.0f;

	// The time that the target has been hidden from the camera for.
	float TargetHiddenTime = 0.0f;

	// 1 = ease in, -1 = ease out.
	int32 EasingDirection = 1;

	// The time used for easing distance offsets, between 0 and 1.
	float DistanceOffsetTime = 0.0f;

	// The start distance offset.
	float StartDistanceOffset = 0.0f;

	// The end distance offset.
	float EndDistanceOffset = 0.0f;

	// The current offset to apply to the distance along the spline, used for transitioning in and out.
	float CurrentDistanceOffset = 0.0f;

	// The world location for the camera.
	FVector WorldLocation;

	// The last rotation for the camera's movement direction.
	FRotator LastRotation;

	// The offset in spline space of the spline camera location for the last frame.
	// This is normalized for the first couple of entries at least.
	FVector LastSplineOffset[4];

	// The current clock for the spline.
	float Clock = 0.0f;

	// The end clock at which time the spline is done with.
	float EndClock = 8.0f;

	// Delta time to use for easing in / out the camera to the spline.
	float EasingDelta = 0.25f;

	// The last clamped ground-offset value to avoid sharp "vertical" changes.
	// First one is actual position including transitioning height while the second one doesn't have that.
	float OffsetFromGround[2];

	// The last used spline direction, used in dampening direction changes from frame to frame.
	FVector SplineDirection = FVector::ZeroVector;

	// Oscillator used for speed shake.
	FMathEx::FOscillator SpeedShakeX = FMathEx::FOscillator(3.0f, 12.0f, false);

	// Oscillator used for speed shake.
	FMathEx::FOscillator SpeedShakeY = FMathEx::FOscillator(29.0f, 10.0f, false);

	// Oscillators used for tracking.
	FMathEx::FOscillator TrackingOffset1 = FMathEx::FOscillator(23.0f, 4.0f, false);
	FMathEx::FOscillator TrackingOffset2 = FMathEx::FOscillator(29.0f, 2.0f, false);

	// Query parameters used for visibility testing.
	FCollisionQueryParams VisibilityQueryParams;

	// The dynamic field of view for target tracking.
	FDynamicFOV DynamicFOV;

	// The transition time duration to end the viewing sequence.
	float EndTransitionTime = 3.0f;

	// The direction the camera in pointing in local spline space, used when the camera rotation is locked.
	FVector LocalDirection = FVector::ZeroVector;

	// The world location of the viewing target of the camera.
	FVector LastTargetLocation = FVector::ZeroVector;
};

#pragma endregion CameraCinematics

/**
* Class for managing the cinematic camera, a director in coordinating the viewing
* of the action.
***********************************************************************************/

class FCinematicsDirector
{

#pragma region CameraCinematics

public:

	// Construct an FCinematicsDirector.
	FCinematicsDirector(UAdvancedCameraComponent* camera);

	// Do the regular update tick.
	void Tick(float deltaSeconds);

	// Is the director currently active?
	bool IsActive() const
	{ return (CinematicCameraMode != ECinematicCameraMode::Off); }

	// Attach the cinematic camera manager to a specific vehicle.
	void AttachToVehicle(ABaseVehicle* vehicle);

	// Attach the cinematic camera manager to a all vehicles.
	void AttachToAnyVehicle(ABaseVehicle* firstVehicle = nullptr);

	// Are we currently attached to this or any vehicle?
	bool IsAttachedToVehicle(ABaseVehicle* vehicle = nullptr) const
	{ return (vehicle == nullptr) ? (GRIP_POINTER_VALID(AttachedToVehicle) == true || Vehicles.Num() > 0) : (AttachedToVehicle.Get() == vehicle); }

	// Are we currently viewing this or any vehicle?
	bool IsViewingVehicle(ABaseVehicle* vehicle = nullptr) const
	{ return (vehicle == nullptr) ? (GRIP_POINTER_VALID(CurrentVehicle) == true || Vehicles.Num() > 0) : (CurrentVehicle.Get() == vehicle); }

	// Cycle to the next vehicle in the list and stay on it.
	void CycleVehicle();

	// Get the vehicle that we're currently focusing upon (if any).
	ABaseVehicle* GetCurrentVehicle() const
	{ return CurrentVehicle.Get(); }

	// Get the current camera point being used by the director.
	UCameraPointComponent* GetCurrentCameraPoint() const
	{ return CurrentCameraPoint; }

	// Set the current camera point being used by the director.
	void UseCameraPoint(UCameraPointComponent* cameraPoint)
	{ CurrentCameraPoint = cameraPoint; CurrentCameraPoint->Reset(); }

	// Returns camera's point of view.
	bool GetCameraView(float deltaSeconds, FMinimalViewInfo& desiredView);

	// Is the director currently using a spline camera?
	bool UsingSplineCamera() const
	{ return (IsActive() == true && (CinematicCameraMode == ECinematicCameraMode::SplineFollowingVehicle || CinematicCameraMode == ECinematicCameraMode::SplineFollowingVictimVehicle)); }

	// Is the director currently using a point camera?
	bool UsingCameraPointCamera(bool includeTracking) const
	{
		if (includeTracking == true)
			return (IsActive() == true && CinematicCameraMode >= ECinematicCameraMode::CameraPointVehicle && CinematicCameraMode <= ECinematicCameraMode::CameraPointVehicleToVehicle);
		else
			return (IsActive() == true && (CinematicCameraMode == ECinematicCameraMode::CameraPointVehicle || CinematicCameraMode == ECinematicCameraMode::CameraPointVehicleToGun));
	}

	// Is the director currently using a custom override?
	bool UsingCustomOverride() const
	{ return (IsActive() == true && CinematicCameraMode == ECinematicCameraMode::CustomOverride); }

	// Set whether spirit camera is in use.
	void UseSpiritCamera(bool use, bool fromImpact, const FVector& velocity);

	// Set the custom location, rotation and field-of-view of the director.
	void UseCustomOverride(bool use, const FVector& location, const FRotator& rotation, float fieldOfView);

	// Get the distance between the camera and its target.
	float GetFocalDistance() const;

	// Does the current camera view require this vehicle's spring-arm to be active?
	bool RequiresActiveSpringArm(ABaseVehicle* vehicle);

	// Switch the current cinematic camera mode.
	void SwitchMode(ECinematicCameraMode mode);

	// Update a dynamic field of view to keep a target in screen proportion bounds within the guidelines that we're given.
	static void UpdateDynamicFieldOfView(float deltaSeconds, bool allowInChanges, bool allowOutChanges, AActor* cameraTarget, const FVector& location, FDynamicFOV& dynamicFOV, bool timeSlowed);

	// Some constants for managing camera work.
	static const int32 MinCameraDuration = 2;
	static const int32 MinSplineCameraDurationIncoming = 5;
	static const int32 MinSplineCameraDuration = 8;
	static const int32 MaxSplineCameraDuration = 11;

private:

	// Queue a vehicle for showing.
	void QueueVehicle();

	// Queue all vehicles ready for showing.
	void QueueVehicles();

	// Queue the cameras for the current vehicle for showing.
	void QueueCamerasForVehicle();

	// Reset the vehicle time.
	void ResetVehicleTime()
	{ VehicleTimer = 0.0f; VehicleDuration = FMath::FRandRange(20.0f, 30.0f); }

	// Reset the camera time.
	void ResetCameraTime()
	{ CameraModeTimer = 0.0f; CameraShotTimer = 0.0f; CameraDuration = FMath::FRandRange(5.0f, 8.0f); }

	// Identify a potential camera point.
	void IdentifyCameraPoint(bool switchVehicle);

	// Identify a potential static camera.
	bool IdentifyStaticCamera();

	// Identify a potential spline target.
	bool IdentifySplineTarget(bool impactEventsOnly);

	// Identify a potential vehicle event visible from a nearby vehicle.
	bool IdentifyVehicleEvent();

	// Identify a potential impact event visible from a nearby vehicle.
	bool IdentifyImpactEventFromVehicle();

	// Hookup missile impact event visible from a nearby vehicle.
	bool HookupMissileImpactFromVehicle(AHomingMissile* missile, ABaseVehicle* forVehicle, float maxImpactTime);

	// Identify a potential weapon event.
	bool IdentifyWeaponEvent(bool highValue = false);

	// Identify a potential weapon launch from the currently observed vehicle.
	bool IdentifyWeaponLaunches();

	// Identify a potential impact event.
	float IdentifyImpactEvent(ABaseVehicle* vehicle, TWeakObjectPtr<AActor>& impactingActor, float maxImpactTime, bool missilesOnly = false) const;

	// Find a good forward facing camera point on a given vehicle.
	UCameraPointComponent* FindForeFacingCameraPoint(ABaseVehicle* vehicle) const;

	// Is this vehicle smoothly controlled and good for cinematic work?
	bool IsVehicleSmoothlyControlled(ABaseVehicle* vehicle)
	{ return true; }

	// Can we slow time at this point?
	bool CanSlowTime(bool atAnyPoint) const;

	// Create a stock camera point on a vehicle to be used for a viewing platform.
	void CreateStockPointCamera();

	// Switch to a camera point attached to a vehicle.
	void SwitchToVehicleCameraPoint();

	// Identify camera action and switch the current camera view if found.
	bool IdentifyCameraAction(bool allowVehicleTrackingCamera, bool highPriority, bool highValue = false);

	// Get the world target location for the camera at a given location.
	FVector GetCameraTargetLocation(const FVector& fromLocation) const;

	// The owner of the camera to use for this director.
	// This shouldn't disappear, but is still a slightly worrying naked pointer.
	AActor* Owner = nullptr;

	// The camera to use for this director.
	// This shouldn't disappear, but is still a slightly worrying naked pointer.
	UAdvancedCameraComponent* Camera = nullptr;

	// The current camera target, if any.
	TWeakObjectPtr<AActor> CameraTarget;

	// The last camera target, if any.
	TWeakObjectPtr<AActor> LastCameraTarget;

	// The currently focused vehicle, if any.
	TWeakObjectPtr<ABaseVehicle> CurrentVehicle;

	// The currently attached vehicle, if any.
	TWeakObjectPtr<ABaseVehicle> AttachedToVehicle;

	// The rotation for the camera's movement direction.
	FRotator ViewRotation;

	// The last rotation for the camera's movement direction.
	FRotator LastRotation;

	// The time the current camera mode has been used for.
	float CameraModeTimer = 0.0f;

	// The time the current camera shot has been used for.
	float CameraShotTimer = 0.0f;

	// The duration to use the current camera mode for.
	float CameraDuration = 0.0f;

	// The time the current vehicle has been used for.
	float VehicleTimer = 0.0f;

	// The duration to use the current vehicle subject for.
	float VehicleDuration = 0.0f;

	// The current index of the vehicle to use.
	int32 VehicleIndex = 0;

	// The current index of the camera to use within a vehicle.
	int32 CameraIndex = 0;

	// The actor about to impact the currently viewing vehicle.
	TWeakObjectPtr<AActor> ImpactingActor;

	// Has time been slowed for the current view.
	bool TimeSlowed = false;

	// Has the current weapon event been concluded?
	bool WeaponEventConcluded = false;

	// The last time a particular view was in use.
	float LastViewTimes[(int32)ECinematicCameraMode::Num];

	// The clock on the last frame.
	float LastClock = 0.0f;

	// A yaw adjustment for when cameras are offset somehow, like when viewing a track camera and it gets hit by a vehicle and knocked sideways.
	float AdjustedYaw = 0.0f;

	// The field of view to use for the spirit camera.
	float SpiritCameraFOV = 0.0f;

	// The current camera shot mode of the director.
	ECinematicCameraMode CinematicCameraMode = ECinematicCameraMode::Off;

	// The spline camera currently in use.
	FSplineCamera SplineCamera = FSplineCamera(0.0f, 0.0f);

	// The static camera currently in use.
	TWeakObjectPtr<AStaticTrackCamera> StaticCamera;

	// Are we letting the player manually cycle vehicles?
	bool CyclingVehicles = false;

	// The time of the last overhead view.
	float LastOverheadView = 0.0f;

	// Counter for allowing selection of wide FOV static track cameras.
	int32 StaticCameraCount = 0;

	// The index numbers of the vehicles to use for camera work.
	TArray<int32> Vehicles;

	// The camera points that can be used for the current vehicle.
	TArray<UCameraPointComponent*> VehicleCameras;

	// How long has the camera target been hidden from view for?
	float TargetHiddenTime = 0.0f;

	// Query parameters used for visibility testing.
	FCollisionQueryParams VisibilityQueryParams;

	// The dynamic FOV properties.
	FDynamicFOV DynamicFOV;

	friend class ADebugRaceCameraHUD;

#pragma endregion CameraCinematics

	// A stock camera point to use on a vehicle that can be used for cinematic work. Reassigned between vehicles when required.
	UPROPERTY(Transient)
		UCameraPointComponent* StockCameraPoint = nullptr;

	// The current camera point, if any.
	UPROPERTY(Transient)
		UCameraPointComponent* CurrentCameraPoint = nullptr;
};
