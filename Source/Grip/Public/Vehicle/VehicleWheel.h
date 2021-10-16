/**
*
* Wheel implementation, use for wheels attached to vehicles.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* All of the data required to manage a wheel attached to a vehicle.
*
***********************************************************************************/

#pragma once

#include "system/gameconfiguration.h"
#include "vehicle/vehiclephysicssetup.h"

#pragma region VehicleContactSensors

#include "vehicle/vehiclecontactsensor.h"
#include "effects/drivingsurfacecharacteristics.h"

#pragma region VehicleSurfaceEffects

/**
* A wheel driving surface visual effect.
***********************************************************************************/

struct FWheelDrivingSurface
{
	TWeakObjectPtr<UParticleSystemComponent> Surface;

	// The timer used to co-ordinate the effect.
	float Timer = 0.0f;

	// The fade time to transition in / out.
	float FadeTime = 0.0f;

	// The alpha value for the coating on the tire.
	float CoatingAlpha = 0.0f;

	// Is the effect associated with skidding?
	bool Skidding = false;

	// Is the effect associated with wheel-spinning?
	bool Spinning = false;

	// Is the effect associated with launching?
	bool Launched = false;

	// Is the effect mandatory?
	bool Mandatory = false;
};

/**
* A combination of two wheel driving surface visual effects, so that we can
* transition between them.
***********************************************************************************/

struct FWheelDrivingSurfaces
{
	// Destroy the particle system components for the surfaces.
	void DestroyComponents();

	// Setup the last component from the current one, ready to transition in a new one.
	void SetupLastComponent(float fadeOutTime, bool destroy);

	// Destroy the last component, called whenever it's clearly faded out.
	void DestroyLastComponent();

	// Discard a component, letting it die naturally one it has completed its visual effect.
	static void DiscardComponent(FWheelDrivingSurface& surface);

	// Two surfaces, one current and one transitioning out.
	// Surfaces transition from index 0 to 1 as new surfaces are encountered.
	FWheelDrivingSurface Surfaces[2];
};

#pragma endregion VehicleSurfaceEffects

/**
* A structure used to implement a vehicle wheel, or canards for antigravity
* vehicles. The bolting of contact sensors to the vehicle is established through
* this structure, and the steering is also performed here too.
***********************************************************************************/

struct FVehicleWheel
{
public:

#pragma region VehicleAntiGravity

	// Construct a wheel.
	FVehicleWheel(FName boneName, const FVector& boneOffset, const FVector& standardBoneOffset, const FVector& suspensionForcesOffset, EWheelPlacement placement, float width, float radius, float canardRestingAngle, float canardSteeringAngle, float canardBrakeAngle)
		: BoneName(boneName)
		, BoneOffset(boneOffset)
		, StandardBoneOffset(standardBoneOffset)
		, SuspensionForcesOffset(suspensionForcesOffset)
		, Placement(placement)
		, Width(width)
		, Radius(radius)
		, CanardRestingAngle(canardRestingAngle)
		, CanardSteeringAngle(canardSteeringAngle)
		, CanardBrakeAngle(canardBrakeAngle)
	{ }

#pragma endregion VehicleAntiGravity

	// Has the wheel got front placement on the vehicle?
	bool HasFrontPlacement() const
	{ return (Placement == EWheelPlacement::Front); }

	// Has the wheel got rear placement on the vehicle?
	bool HasRearPlacement() const
	{ return (Placement == EWheelPlacement::Rear); }

	// Has the wheel got center placement on the vehicle?
	bool HasCenterPlacement() const
	{ return (Placement == EWheelPlacement::Center); }

	// Get the rotation of the wheel from a parent rotation.
	FQuat GetSteeringTransform(const FQuat& parentRotation, bool scaledSteering) const
	{ return (scaledSteering == true) ? parentRotation * ScaledSteering : parentRotation * Steering; }

	// Set the transform on the wheel to match the parent vehicle and a given steering rotation.
	void SetSteeringTransform(const FQuat& parentRotation, const FRotator& steering, const FRotator& scaledSteering)
	{ Steering = steering.Quaternion(); ScaledSteering = scaledSteering.Quaternion(); Transform = parentRotation * Steering; }

	// Get the unflipped rotations per seconds.
	float GetUnflippedRPS() const
	{ return (RPSFlipped == true) ? -RPS : RPS; }

	// Get the currently active sensor.
	FVehicleContactSensor& GetActiveSensor()
	{ return Sensors[SensorIndex]; }

	// Get the currently active sensor.
	const FVehicleContactSensor& GetActiveSensor() const
	{ return Sensors[SensorIndex]; }

	// Is the wheel in near contact with the ground?
	float IsInNearContact(float wheelRadius) const
	{ return (IsInContact == true) ? 1.0f : 1.0f - FMath::Min(GetActiveSensor().GetSurfaceDistance() / (wheelRadius * 4.0f), 1.0f); }

	// Compare this wheel with a bone name, used by TArray::FindByKey.
	bool operator == (const FName& boneName) const
	{ return BoneName == boneName; }

#pragma region VehicleSurfaceEffects

	// The world or free components used primarily as trails, normally rear wheels only.
	FWheelDrivingSurfaces SurfaceComponents;

	// The local or fixed components wrapped around a wheel, normally all wheels.
	FWheelDrivingSurfaces FixedSurfaceComponents;

	// The mesh associated with the tire.
	TWeakObjectPtr<UStaticMeshComponent> TireMesh;

#pragma endregion VehicleSurfaceEffects

private:

	// The name of the bone to which the wheel is attached on the vehicle.
	FName BoneName = NAME_None;

	// The offset from the root bone in vehicle space that the sensor is mounted at for suspension forces.
	// This has to computed dynamically as it depends on the current physics tick race
	FVector BoneOffset = FVector::ZeroVector;

	// The offset from the root bone in vehicle space that the standard wheel is mounted at for symmetrical, balanced physics.
	FVector StandardBoneOffset = FVector::ZeroVector;

	// The offset from the root bone in vehicle space that the suspension forces are applied at.
	FVector SuspensionForcesOffset = FVector::ZeroVector;

	// The rotations per second of this wheel.
	// This is the visible, flipped state, so it's not easily understood with respect to the driving direction.
	float RPS = 0.0f;

	// Is the rotations per second flipped?
	bool RPSFlipped = false;

	// The longitudinal slip value for the tire.
	float LongitudinalSlip = 0.0f;

	// The location of the wheel point in world space.
	FVector Location = FVector::ZeroVector;

	// The velocity of the wheel in world space.
	FVector Velocity = FVector::ZeroVector;

	// The lateral force to apply to the wheel.
	float LateralForceStrength = 0.0f;

	// The average lateral force applied to the wheel over the last two frames.
	float TwoFrameLateralForceStrength = 0.0f;

	// The lateral force vector to apply to the wheel.
	FVector LateralForceVector = FVector::ZeroVector;

	// The local steering rotator for this wheel.
	FQuat Steering = FQuat::Identity;

	// The local scaled steering rotator for this wheel.
	FQuat ScaledSteering = FQuat::Identity;

	// The transform for the wheel, from local to world space.
	FQuat Transform = FQuat::Identity;

	// The last surface the wheel contacted.
	EGameSurface LastSurfaceContact = EGameSurface::Default;

	// Is the wheel in surface contact?
	bool IsInContact = false;

	// The time since it switch from being in contact vs. not in contact and vice versa.
	float ModeTime = 0.0f;

	// The placement of the wheel, front, rear or center.
	EWheelPlacement Placement;

	// The width of the wheel.
	float Width;

	// The radius of the wheel.
	float Radius;

#pragma region VehicleAntiGravity

	// If this is a canard, then the angle at which it rests before firing up the engine.
	float CanardRestingAngle = 0.0f;

	// If this is a canard, then the maximum steering angle.
	float CanardSteeringAngle = 0.0f;

	// If this is a canard, then the maximum brake angle.
	float CanardBrakeAngle = 0.0f;

#pragma endregion VehicleAntiGravity

	// The two sensors attached to both surfaces of this wheel.
	// The first points "up", and the second, "down".
	FVehicleContactSensor Sensors[2];

	// The current sensor for the wheel, can swap between two different sensors for the upper and lower halves of the vehicle.
	int32 SensorIndex = 0;

	friend class ABaseVehicle;
	friend class ADebugVehicleHUD;
	friend class ADebugRaceCameraHUD;
};

#pragma endregion VehicleContactSensors
