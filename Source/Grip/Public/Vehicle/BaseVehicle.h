/**
*
* Base vehicle implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* The main vehicle class, containing almost all the meat of the vehicle
* implementation, both standard and flippable.
*
***********************************************************************************/

#pragma once

#include "system/gameconfiguration.h"
#include "gameframework/pawn.h"
#include "components/stereolayercomponent.h"
#include "camera/flippablespringarmcomponent.h"
#include "camera/racecameracomponent.h"
#include "camera/cameraballactor.h"
#include "vehicle/vehiclephysicssetup.h"
#include "game/playerracestate.h"
#include "pickups/pickup.h"
#include "pickups/pickupbase.h"
#include "pickups/gatlinggun.h"
#include "pickups/shield.h"
#include "pickups/turbo.h"
#include "pickups/homingmissile.h"
#include "effects/drivingsurfacecharacteristics.h"
#include "effects/vehicleimpacteffect.h"
#include "effects/electricalbomb.h"
#include "vehicle/vehicleaudio.h"
#include "system/perlinnoise.h"
#include "system/targetable.h"
#include "system/avoidable.h"
#include "system/timeshareclock.h"
#include "gamemodes/playgamemode.h"
#include "vehicle/vehiclephysics.h"
#include "vehicle/vehiclewheel.h"
#include "vehicle/vehiclemeshcomponent.h"
#include "vehicle/vehiclehud.h"
#include "ai/playeraicontext.h"
#include "basevehicle.generated.h"

class UInputComponent;
class UWidgetComponent;
class UHUDWidget;
class ABaseVehicle;
class AAvoidanceSphere;

#if WITH_PHYSX
namespace physx
{
	class PxContactSet;
}
#endif // WITH_PHYSX

#pragma region MinimalVehicle

#pragma region VehicleLaunch

/**
* An enumeration for governing the launch ability of the vehicle.
***********************************************************************************/

enum class ELaunchStage : uint8
{
	Idle,
	Charging,
	Released,
	Discharging
};

#pragma endregion VehicleLaunch

/**
* A structure for assigning a wheel to a bone, with all its properties.
***********************************************************************************/

USTRUCT(BlueprintType)
struct FWheelAssignment
{
	GENERATED_USTRUCT_BODY()

public:

	FWheelAssignment() = default;

	FWheelAssignment(const FName& boneName, EWheelPlacement placement, float width, float radius, float restingCompression, float verticalOffset)
		: BoneName(boneName)
		, Placement(placement)
		, Width(width)
		, Radius(radius)
		, RestingCompression(restingCompression)
		, VerticalOffset(verticalOffset)
	{ }

	// The name of the bone the wheel is assigned to.
	UPROPERTY(EditAnywhere)
		FName BoneName;

	// The placement of the wheel on the vehicle.
	UPROPERTY(EditAnywhere)
		EWheelPlacement Placement;

	// The width of the wheel.
	UPROPERTY(EditAnywhere)
		float Width;

	// The radius of the wheel.
	UPROPERTY(EditAnywhere)
		float Radius;

	// The resting compression of the suspension for this wheel.
	UPROPERTY(EditAnywhere)
		float RestingCompression;

	// The vertical offset of the wheel to help determine tire edge to surface surface distance accurately.
	UPROPERTY(EditAnywhere)
		float VerticalOffset;
};

/**
* A structure for overriding the materials of a vehicle with a substitute,
* currently used for rendering the vehicle from the cockpit camera.
***********************************************************************************/

USTRUCT()
struct FMeshMaterialOverride
{
	GENERATED_USTRUCT_BODY()

public:

	FMeshMaterialOverride() = default;

	FMeshMaterialOverride(UPrimitiveComponent* component, UMaterialInterface* material)
		: Component(component)
		, Material(material)
	{ }

	// The original component.
	UPROPERTY(Transient)
		UPrimitiveComponent* Component = nullptr;

	// The original material.
	UPROPERTY(Transient)
		UMaterialInterface* Material = nullptr;
};

#pragma region PickupsAvailable

/**
* An enumeration for describing the state of a pickup slot.
***********************************************************************************/

enum class EPickupSlotState : uint8
{
	// No pickup in the slot.
	Empty,

	// Pickup in the slot, currently unactivated.
	Idle,

	// Pickup in the slot, currently active.
	Active,

	// Pickup in the slot, but used and is now being freed up through an animation.
	Used
};

/**
* An enumeration for describing the state of charging a pickup slot.
***********************************************************************************/

enum class EPickupSlotChargingState : uint8
{
	// Pickup not charging or charged.
	None,

	// Pickup is currently charging.
	Charging,

	// Pickup is now fully charged but not yet primed.
	Charged,

	// Pickup is primed and ready to go.
	Primed
};

/**
* A structure describing a pickup slot with some control functions.
***********************************************************************************/

struct FPlayerPickupSlot
{
	// Is a pickup slot currently charging?
	bool IsCharging(bool confirmed) const;

	// Is a pickup slot charged?
	bool IsCharged() const
	{ return ChargingState >= EPickupSlotChargingState::Charged; }

	// Cancel the charging of a pickup slot.
	void CancelCharging()
	{
		ChargingState = EPickupSlotChargingState::None;
		ChargeTimer = 0.0f;
		HookTimer = 0.0f;
	}

	// Get the timer used for charging the pickup slot.
	float GetChargeTimer() const
	{ return ChargeTimer; }

	// Is this pickup slot active?
	bool IsActive() const
	{ return State == EPickupSlotState::Active; }

	// Is this pickup slot empty?
	bool IsEmpty() const
	{ return State == EPickupSlotState::Empty; }

	// The type of the pickup in the slot.
	EPickupType Type = EPickupType::None;

	// The state of the pickup slot.
	EPickupSlotState State = EPickupSlotState::Empty;

	// The activation state of the pickup slot.
	EPickupActivation Activation = EPickupActivation::None;

	// The charging state of the pickup slot.
	EPickupSlotChargingState ChargingState = EPickupSlotChargingState::None;

	// General time.
	float Timer = 0.0f;

	// Time delay after becoming efficacious to use that the bot can start to think about using it.
	float EfficacyTimer = 0.0f;

	// Time the bot must use the pickup from the beginning of this time range.
	float UseAfter = 0.0f;

	// Time the bot must use the pickup before the end of this time range.
	float UseBefore = 0.0f;

	// Bot to dump this pickup after this time range whether efficacious or not.
	float DumpAfter = 0.0f;

	// Try to use the pickup automatically (it couldn't be used when the player tried to use it).
	bool AutoUse = false;

	// Will vehicles under bot control charge this pickup (selected randomly at collection).
	bool BotWillCharge = false;

	// Will vehicles under bot control use this pickup only against humans (selected randomly at collection).
	bool BotWillTargetHuman = false;

	// The count within the pickup collection order.
	int32 PickupCount = 0;

	// The charging timer between 0 and 1.
	float ChargeTimer = false;

	// The hook for charging timer.
	float HookTimer = false;

	// The pickup itself that was activated from this pickup slot.
	TWeakObjectPtr<APickupBase> Pickup;
};

#pragma endregion PickupsAvailable

#pragma region VehicleControls

/**
* A structure describing the player input to the vehicle and how to control it.
***********************************************************************************/

struct FVehicleControl
{
	// Get the braked throttle position. The more brake we apply, the less throttle we allow.
	float GetBrakedThrottle() const
	{ return ThrottleInput * (1.0f - BrakePosition); }

	// Is the steering command analog?
	bool SteeringAnalog = true;

	// The steering value will be somewhere between -1 and +1.
	float SteeringInputAnalog = 0.0f;

	// The steering value will be somewhere between -1 and +1.
	float SteeringInputDigital = 0.0f;

	// SteeringInput is often just a digital switch between -1, 0 and +1, SteeringPosition is the real position derived from that.
	float SteeringPosition = 0.0f;

	// SteeringInput is often just a digital switch between -1, 0 and +1, AntigravitySteeringPosition is the real position derived from that.
	float AntigravitySteeringPosition = 0.0f;

	// AutoSteeringPosition is used to the correct the steering where necessary (tight tunnels).
	float AutoSteeringPosition = 0.0f;

	// The thrust force value will be somewhere between -1 and +1, often at 0 or the extremes.
	float RawThrottleInput = 0.0f;

	// The thrust force value will be somewhere between -1 and +1, often at 0 or the extremes.
	float ThrottleInput = 0.0f;

	// The current braking force from the player, scaled from 0 to 1.
	float BrakeInput = 0.0f;

	// Brake is often just a digital switch between 0 and +1, BrakePosition is the real position derived from that.
	float BrakePosition = 0.0f;

	// The roll input value will be somewhere between -1 and +1.
	float AirborneRollInput = 0.0f;

	// The pitch input value will be somewhere between -1 and +1.
	float AirbornePitchInput = 0.0f;

	// The current roll position value somewhere between -1 and +1.
	float AirborneRollPosition = 0.0f;

	// The current pitch position value somewhere between -1 and +1.
	float AirbornePitchPosition = 0.0f;

	// Is airborne control currently active?
	bool AirborneControlActive = false;

	// The scale to use for airborne control.
	// This can be -1 or +1, and is used to pitch the vehicle in the most intuitive direction.
	float AirborneControlScale = 1.0f;

	// The timer used for airborne control.
	float AirborneControlTimer = 0.0f;

	// Controlling launch, for getting a good start off the line.
	// Bit 0 means throttle early, bit 1 throttle in the sweet spot, bit 2 passed the launch phase.
	int32 LaunchControl = 0;

	// The time when the handbrake was last pressed down.
	float HandbrakePressed = 0.0f;

	// Do we need to decide wheel-spin?
	bool DecideWheelSpin = true;

	// The handle for the force feedback controller.
	FDynamicForceFeedbackHandle ForceFeedbackHandle = 0;

	// The list of throttle inputs.
	FTimedFloatList ThrottleList = FTimedFloatList(1, 30);
};

#pragma endregion VehicleControls

/**
* A structure describing the elimination state of the vehicle in the Elimination
* game mode.
***********************************************************************************/

struct FVehicleElimination
{
	// Timer used for controlling the audio cue.
	float AlertTimer = 0.0f;

	// How close we are to being eliminated.
	float Ratio = 0.0f;

	// EliminationAlert sound when almost being eliminated in the Elimination game mode.
	static USoundCue* AlertSound;
};

#pragma region VehicleContactSensors

/**
* A structure to describe the wheels as a whole for a vehicle. Not only do we
* store all of the wheels themselves here, but we also have a lot of data related
* to steering and trying to determine the nearest driving surface direction from
* the wheel contact sensors, and therefore the flipped state of the vehicle.
* There's also some data regarding powered wheel simulation.
***********************************************************************************/

struct FVehicleWheels
{
	FVehicleWheels()
	{ BurnoutDirection = ((FMath::Rand() & 1) == 0) ? -1.0f : +1.0f; }

	// Do we have a nearest surface direction indicated by the wheels?
	bool HasSurfaceDirection() const
	{ return DetectedSurfaces; }

	// Do we have high confidence in the nearest surface direction indicated by the wheels?
	bool HasConfidentSurfaceDirection() const
	{ return SurfacesVincinal; }

	// Is the vehicle shortly going to need to be flipped?
	bool SoftFlipped = false;

	// Is the vehicle flipped?
	bool HardFlipped = false;

	// Have we detected surfaces from the wheels?
	bool DetectedSurfaces = false;

	// Are the detected surfaces close to the wheels?
	bool SurfacesVincinal = false;

	// State used in detecting the nearest driving surface from the wheels.
	int32 FlipDetection = 0;

	// Timer used in flipping the springs / wheel positions.
	float FlipTimer = 0.0f;

	// The steering value of the front wheels, in degrees.
	float FrontSteeringAngle = 0.0f;

	// The steering value of the back wheels, in degrees.
	float BackSteeringAngle = 0.0f;

	// The visual steering value of the front wheels, in degrees.
	float FrontVisualSteeringAngle = 0.0f;

	// The visual steering value of the back wheels, in degrees.
	float BackVisualSteeringAngle = 0.0f;

	// Should we spin the wheels when throttle is applied?
	bool SpinWheelsOnStart = false;

	// The current burnout direction.
	float BurnoutDirection = 0.0f;

	// The amount of sideways force to use in applying a burnout.
	float BurnoutForce = 0.0f;

	// A ration of angular velocity current in a burnout.
	float BurnoutPhaseOut = 0.0f;

	// The last time we were grounded, which contact sensor set where we using (0 or 1).
	int32 GroundedSensorSet = 1;

	// The number of wheels currently in contact with a surface.
	int32 NumWheelsInContact = 0;

	// Is a wheel on the rear axle currently in contact with a surface?
	bool RearWheelDown = false;

	// Is a wheel on the front axle currently in contact with a surface?
	bool FrontWheelDown = false;

	// Are all the wheels on the rear axle currently in contact with a surface?
	bool RearAxleDown = false;

	// Are all the wheels on the front axle currently in contact with a surface?
	bool FrontAxleDown = false;

	// The X position of the rear axle in local space.
	float RearAxleOffset = 0.0f;

	// The X position of the front axle in local space.
	float FrontAxleOffset = 0.0f;

	// The position of the rear axle in world space.
	FVector RearAxlePosition = FVector::ZeroVector;

	// The position of the front axle in world space.
	FVector FrontAxlePosition = FVector::ZeroVector;

	// The number of rotations per second for the wheels.
	float WheelRPS = 0.0f;
	float WheelRPSUnflipped = 0.0f;

	// A ratio used to indicate how fast the wheels are spinning.
	float SpinRatio = 0.0f;

	// Are the springs compressed hard?
	bool HardCompression = false;

	// The time since the last hard compression.
	float HardCompressionTime = 0.0f;

	// The name of the surface the vehicle is currently driving on.
	FName SurfaceName;

	// Timer used to remove glitches from skidding.
	float SkidTimer = 0.0f;

	// The target for the skid audio volume.
	float SkidAudioVolumeTarget = 0.0f;

	// The target for the spin audio volume.
	float SpinAudioVolumeTarget = 0.0f;

	// The wheels attached to the vehicle.
	TArray<FVehicleWheel> Wheels;

#pragma region VehicleSurfaceEffects

	// Timer used for coordinating surface effects.
	float SurfaceEffectsTimer = 0.0f;

#pragma endregion VehicleSurfaceEffects

};

#pragma endregion VehicleContactSensors

#pragma region VehicleDamage

/**
* A structure to describe the damage condition of a vehicle.
***********************************************************************************/

USTRUCT()
struct FVehicleDamage
{
	GENERATED_USTRUCT_BODY()

public:

	// The decrementing damage for games that don't have destructible vehicles.
	float DecrementingDamage = 0.0f;

	// The target stage of damage to head towards.
	float TargetStage = 0.0f;

	// The current stage of damage.
	float CurrentStage = 0.0f;

	// The target fire light intensity to head towards.
	float TargetLightIntensity = 0.0f;

	// Timer for emitting frequent sparks.
	float NextSparks = 0.0f;

	// An alpha value used for determining whether the vehicle is smoking.
	float SmokingAlpha = 0.0f;

	// Queue of damage to apply to the vehicle, often populated by the physics sub-step.
	TMap<int32, int32> DamageQueue;

	// The particle system used to render the damage effects.
	UPROPERTY(Transient)
		UParticleSystem* Effect = nullptr;

	// The particle system component used to render the damage effects.
	UPROPERTY(Transient)
		UParticleSystemComponent* DamageEffect = nullptr;
};

#pragma endregion VehicleDamage

#pragma region VehicleTeleport

/**
* A structure to describe the teleportation of a vehicle.
***********************************************************************************/

struct FVehicleTeleportation
{
	// Is this a forced, automatic teleport?
	bool Forced = false;

	// The current stage of the teleportation.
	int32 Action = 0;

	// The number of consecutive teleport loops.
	int32 NumLoops = 0;

	// Timer used to time the teleport input request.
	float Timer = 0.0f;

	// Timer used to delay the teleportation.
	float Countdown = 0.0f;

	// The time teleportation was last used.
	float LastVehicleClock = 0.0f;

	// The time the vehicle was last recovered via teleportation.
	float RecoveredAt = 0.0f;

	// The initial speed in KPH to teleport to.
	float InitialSpeed = 0.0f;

	// The location to teleport to.
	FVector Location = FVector::ZeroVector;

	// The rotation to teleport to.
	FRotator Rotation = FRotator::ZeroRotator;

	// The route follower used during the teleportation.
	FRouteFollower RouteFollower;
};

#pragma endregion VehicleTeleport

/**
* A small actor class for configuring and attaching canards to antigravity vehicles.
***********************************************************************************/

UCLASS(Abstract, Blueprintable, ClassGroup = Vehicle)
class GRIP_API ACanard : public AActor
{
	GENERATED_BODY()

public:

	// Construct a canard.
	ACanard();

#pragma region EditorProperties

	// The mesh used to render the canard.
	UPROPERTY(EditAnywhere, Category = Canard)
		UStaticMeshComponent* CanardMesh;

	// The front resting angle of the canard.
	UPROPERTY(EditAnywhere, Category = Canard)
		float RestingAngleFront = 25.0f;

	// The rear resting angle of the canard.
	UPROPERTY(EditAnywhere, Category = Canard)
		float RestingAngleRear = 25.0f;

	// The front maximum steering angle of the canard.
	UPROPERTY(EditAnywhere, Category = Canard)
		float MaximumSteeringAngleFront = 25.0f;

	// The rear maximum steering angle of the canard.
	UPROPERTY(EditAnywhere, Category = Canard)
		float MaximumSteeringAngleRear = 10.0f;

	// The front maximum braking angle of the canard.
	UPROPERTY(EditAnywhere, Category = Canard)
		float MaximumBrakingAngleFront = 0.0f;

	// The rear maximum braking angle of the canard.
	UPROPERTY(EditAnywhere, Category = Canard)
		float MaximumBrakingAngleRear = 25.0f;

#pragma endregion EditorProperties

};

/**
* The main, base vehicle class. This is the most important class in the whole game
* and contains almost all of the functionality exhibited by its vehicles.
***********************************************************************************/

UCLASS(Abstract, Blueprintable, ClassGroup = Vehicle)
class GRIP_API ABaseVehicle : public APawn, public ITargetableInterface, public IAvoidableInterface, public IGunHostInterface, public IMissileHostInterface
{
	GENERATED_BODY()

public:

	// Construct a base vehicle.
	ABaseVehicle();

#pragma region EditorProperties

public:

	// The vehicle mesh for this vehicle, derived from a skeletal mesh.
	UPROPERTY(EditAnywhere, Category = Vehicle)
		UVehicleMeshComponent* VehicleMesh = nullptr;

	// Spring arm that will offset the camera.
	UPROPERTY(EditAnywhere, Category = Camera)
		UFlippableSpringArmComponent* SpringArm = nullptr;

	// Camera component that will be our viewpoint.
	UPROPERTY(EditAnywhere, Category = Camera)
		URaceCameraComponent* Camera = nullptr;

	// The light used to show fire damage.
	UPROPERTY(EditAnywhere, Category = Camera)
		UPointLightComponent* DamageLight = nullptr;

	// The class to use for the camera ball when the camera is disconnected from the vehicle.
	UPROPERTY(EditAnywhere, Category = Camera)
		TSubclassOf<ACameraBallActor> CameraBallClass;

	// Are we using the antigravity model?
	UPROPERTY(EditAnywhere, Category = Vehicle)
		bool Antigravity = false;

	// The avoidance radius of the vehicle in meters.
	UPROPERTY(EditAnywhere, Category = Vehicle)
		float AvoidanceRadius = 4.0f;

	// The drag coefficient of the vehicle, or how much the surrounding air fights against its velocity.
	// Notionally this should be 0.5 * a * c * f
	// Where a = air density, c = drag coefficient and f = frontal area of the vehicle.
	// We condense all of these values into a single number here that merely limits top-speed, no need for scientific accuracy.
	// Air density and frontal area of the vehicle to the velocity direction are assumed to be constant in this case.
	// In real life this would only vary by about 10% for vehicles such as those in GRIP anyway.
	UPROPERTY(EditAnywhere, Category = "Physics Core Coefficients")
		float DragCoefficient = 0.15f;

	// The power coefficient for the vehicle. Used to tailor the natural handling of the vehicle to achieve specific, desired characteristics.
	UPROPERTY(EditAnywhere, Category = "Physics Core Coefficients")
		float PowerCoefficient = 1.0f;

	// The acceleration coefficient for the vehicle. Used to tailor the natural handling of the vehicle to achieve specific, desired characteristics.
	UPROPERTY(EditAnywhere, Category = "Physics Core Coefficients")
		float AccelerationCoefficient = 1.0f;

	// The braking coefficient for the vehicle. Used to tailor the natural handling of the vehicle to achieve specific, desired characteristics.
	UPROPERTY(EditAnywhere, Category = "Physics Core Coefficients")
		float BrakingCoefficient = 1.0f;

	// The grip coefficient for the vehicle. Used to tailor the natural handling of the vehicle to achieve specific, desired characteristics.
	UPROPERTY(EditAnywhere, Category = "Physics Core Coefficients")
		float GripCoefficient = 1.0f;

	// Scale normal gravity, > 1.0 being stronger, < 1.0 being weaker (this does not affect mass).
	UPROPERTY(EditAnywhere, Category = Physics)
		float GravityScale = 1.5f;

	// How quickly the braking input moves between positions.
	UPROPERTY(EditAnywhere, Category = Physics)
		float BrakingInputSpeed = 2.5f;

	// How much to scale the rear grip by when applying the handbrake.
	UPROPERTY(EditAnywhere, Category = Physics)
		float HandBrakeRearGripRatio = 0.85f;

	// How far forward from the rear axle to position yaw forces for donuts (on the rear axle often produces too tight a donut).
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Physics)
		float DonutOffset = 10.0f;

	// The radial force for the explosion when the vehicle is blown up.
	UPROPERTY(EditAnywhere, Category = Physics)
		URadialForceComponent* DestroyedExplosionForce = nullptr;

	// The relative force of the explosion against any peripheral vehicles.
	UPROPERTY(EditAnywhere, Category = Physics, meta = (UIMin = "0", UIMax = "100", ClampMin = "0", ClampMax = "100"))
		float VehicleExplosionForce = 1.0f;

	// How much power the thrusters have when airborne.
	UPROPERTY(EditAnywhere, Category = "Physics Power")
		float AirborneThrustersPower = 1.0f;

	// The tire engine model / characteristics to use for the vehicle.
	UPROPERTY(EditAnywhere, Category = "Physics Power")
		UVehicleEngineModel* VehicleEngineModel = nullptr;

	// The amount of longitudinal lean to apply when braking that increases with speed.
	UPROPERTY(EditAnywhere, Category = "Physics Leaning Simulation")
		float BrakingLeanScale = 0.25f;

	// The maximum amount of longitudinal lean to apply when braking.
	UPROPERTY(EditAnywhere, Category = "Physics Leaning Simulation")
		float BrakingLeanMaximum = 5.0f;

	// The amount of lean to apply in cornering that increases with speed.
	UPROPERTY(EditAnywhere, Category = "Physics Leaning Simulation")
		float CorneringLeanScale = 0.25f;

	// The maximum amount of lean to apply in cornering.
	UPROPERTY(EditAnywhere, Category = "Physics Leaning Simulation")
		float CorneringLeanMaximum = 5.0f;

	// The steering model / characteristics to use for the vehicle.
	UPROPERTY(EditAnywhere, Category = Wheels)
		USteeringModel* SteeringModel = nullptr;

	// The tire friction model / characteristics to use for the vehicle.
	UPROPERTY(EditAnywhere, Category = Wheels)
		UTireFrictionModel* TireFrictionModel = nullptr;

	// The names of the bones for the wheels.
	UPROPERTY(EditAnywhere, Category = Wheels)
		TArray<FWheelAssignment> WheelAssignments;

	// Specifies how much strength the spring has. The higher the SpringStiffness the more force the spring can push on a body with.
	UPROPERTY(EditAnywhere, Category = Suspension)
		float SpringStiffness = 60.0f;

	// Specifies how quickly the spring can absorb energy of a body. The higher the damping the less oscillation.
	UPROPERTY(EditAnywhere, Category = Suspension)
		float SpringDamping = 3.0f;

	// Accentuate the effect of the spring to make the suspension appear to have more travel than it really has.
	UPROPERTY(EditAnywhere, Category = Suspension)
		float SpringEffect = 3.0f;

	// Visually, how far should the wheel extend away from its bone (this has no effect on physics).
	UPROPERTY(EditAnywhere, Category = Suspension)
		float MaximumWheelTravel = 30.0f;

	// Enable vertical impact mitigation.
	UPROPERTY(EditAnywhere, Category = VelocityChangeMitigation)
		bool EnableVerticalImpactMitigation = true;

	// How much downward mitigation to apply vs. how much reverse velocity mitigation.
	// (larger numbers mean more speed loss)
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = VelocityChangeMitigation, meta = (EditCondition = "EnableVerticalImpactMitigation", UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
		float VerticalImpactMitigationRatio = 1.0f;

	// How much reverse velocity to apply when taking on an impact that results in upward vertical velocity changes.
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = VelocityChangeMitigation, meta = (EditCondition = "EnableVerticalImpactMitigation"))
		FRuntimeFloatCurve VerticalImpactMitigation;

	// Enable bounce impact mitigation.
	UPROPERTY(EditAnywhere, Category = VelocityChangeMitigation)
		bool EnableBounceImpactMitigation = false;

	// How much downward mitigation to apply vs. how much reverse velocity mitigation.
	// (larger numbers mean more speed loss)
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = VelocityChangeMitigation, meta = (EditCondition = "EnableBounceImpactMitigation", UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
		float BounceImpactMitigationRatio = 1.0f;

	// How much reverse velocity to apply when taking on an impact that results in upward vertical velocity changes.
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = VelocityChangeMitigation, meta = (EditCondition = "EnableBounceImpactMitigation"))
		FRuntimeFloatCurve BounceImpactMitigation;

	// The driving surface characteristics to use for the vehicle.
	UPROPERTY(EditAnywhere, Category = Effects)
		UDrivingSurfaceCharacteristics* DrivingSurfaceCharacteristics = nullptr;

	// The driving surface impact characteristics to use for the vehicle.
	UPROPERTY(EditAnywhere, Category = Effects)
		UDrivingSurfaceImpactCharacteristics* DrivingSurfaceImpactCharacteristics = nullptr;

	// Shake the camera on collision impacts.
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Effects)
		TSubclassOf<UMatineeCameraShake> ImpactCameraShake;

	// The minimum amount of normal force that must be applied to the vehicle to spawn an impact effect.
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Effects)
		float ImpactEffectNormalForceThreshold = 5000000.0f;

	// The particle system to use for the vehicle's picked up state.
	UPROPERTY(EditAnywhere, Category = Effects)
		UParticleSystemComponent* PickedUpEffect = nullptr;

	// The bones to use boost effects on.
	UPROPERTY(EditAnywhere, Category = Effects)
		TArray<FName> BoostEffectBoneNames;

	// The particle system to use for the vehicle's boost state.
	UPROPERTY(EditAnywhere, Category = Effects)
		UParticleSystem* BoostLoopEffect = nullptr;

	// The particle system to use for the vehicle's boost state.
	UPROPERTY(EditAnywhere, Category = Effects)
		UParticleSystem* BoostStopEffect = nullptr;

	// The audio to use for the vehicle.
	UPROPERTY(EditAnywhere, Category = Audio)
		UVehicleAudio* VehicleAudio = nullptr;

	// The vehicle shield.
	UPROPERTY(EditAnywhere, Category = Pickups)
		UVehicleShield* VehicleShield = nullptr;

	// The vehicle gun.
	UPROPERTY(EditAnywhere, Category = Pickups)
		UVehicleGun* VehicleGun = nullptr;

#pragma endregion EditorProperties

#pragma region AnimationProperties

public:

	// The offset of the wheel from its original bone in vehicle space.
	UPROPERTY(Transient, BlueprintReadOnly, Category = Animation)
		TArray<FVector> WheelOffsets;

	// The rotation of the wheel in vehicle space.
	UPROPERTY(Transient, BlueprintReadOnly, Category = Animation)
		TArray<FRotator> WheelRotations;

	// The offset of the vehicle from its original bone in vehicle space.
	UPROPERTY(Transient, BlueprintReadOnly, Category = Animation)
		FVector VehicleOffset = FVector::ZeroVector;

	// The rotation of the vehicle in vehicle space.
	UPROPERTY(Transient, BlueprintReadOnly, Category = Animation)
		FRotator VehicleRotation = FRotator::ZeroRotator;

#pragma endregion AnimationProperties

#pragma region APawn

public:

	// Get the current linear velocity of the vehicle.
	virtual FVector GetVelocity() const override
	{ return Physics.VelocityData.Velocity; }

protected:

	// Setup the player input.
	virtual void SetupPlayerInputComponent(UInputComponent* inputComponent) override;

	// Do some pre initialization just before the game is ready to play.
	virtual void PreInitializeComponents() override;

	// Do some post initialization just before the game is ready to play.
	virtual void PostInitializeComponents() override;

	// Do some initialization when the game is ready to play.
	virtual void BeginPlay() override;

	// Do some shutdown when the actor is being destroyed.
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

	// Do the regular update tick, in this case just after the physics has been done.
	virtual void Tick(float deltaSeconds) override;

	// Calculate camera view point, when viewing this actor.
	virtual void CalcCamera(float deltaSeconds, struct FMinimalViewInfo& outResult) override
	{ Camera->GetCameraView(deltaSeconds, outResult); }

	// Receive hit information from the collision system.
	virtual void NotifyHit(class UPrimitiveComponent* thisComponent, class AActor* other, class UPrimitiveComponent* otherComponent, bool selfMoved, FVector hitLocation, FVector hitNormal, FVector normalForce, const FHitResult& hitResult) override;

#if WITH_PHYSX
#if GRIP_ENGINE_PHYSICS_MODIFIED
	// Modify a collision contact.
	virtual bool ModifyContact(uint32 bodyIndex, AActor* other, physx::PxContactSet& contacts) override;
#endif // GRIP_ENGINE_PHYSICS_MODIFIED
#endif // WITH_PHYSX

#pragma endregion APawn

#pragma region IAvoidableInterface

public:

	// Is the attraction currently active?
	virtual bool IsAvoidanceActive() const override
	{ return true; }

	// Should vehicles brake to avoid this obstacle?
	virtual bool BrakeToAvoid() const override
	{ return true; }

	// Get the avoidance location.
	virtual FVector GetAvoidanceLocation() const override
	{ return VehicleMesh->GetComponentTransform().GetLocation(); }

	// Get the avoidance velocity in centimeters per second.
	virtual FVector GetAvoidanceVelocity() const override
	{ return Physics.VelocityData.Velocity; }

	// Get the avoidance radius from the location.
	virtual float GetAvoidanceRadius() const override
	{ return AvoidanceRadius * 100.0f; }

	// Get a direction vector that we prefer to clear the obstacle to, or ZeroVector if none.
	virtual FVector GetPreferredClearanceDirection() const override
	{ return FVector::ZeroVector; }

#pragma endregion IAvoidableInterface

#pragma region BlueprintImplementableEvents

public:

	// Indicate that thrust has just been engaged.
	UFUNCTION(BlueprintImplementableEvent, Category = "Driving Controls")
		void ThrustEngaged();

	// Indicate that thrust has just been disengaged.
	UFUNCTION(BlueprintImplementableEvent, Category = "Driving Controls")
		void ThrustDisengaged();

	// Indicate that the turbo has just been engaged.
	UFUNCTION(BlueprintImplementableEvent, Category = "Driving Controls")
		void TurboEngaged();

	// Indicate that the turbo has just been disengaged.
	UFUNCTION(BlueprintImplementableEvent, Category = "Driving Controls")
		void TurboDisengaged();

	// Indicate that we've just gone up a gear.
	UFUNCTION(BlueprintImplementableEvent, Category = "Driving Controls")
		void GearUpEngaged();

	// Indicate that we've just gone down a gear.
	UFUNCTION(BlueprintImplementableEvent, Category = "Driving Controls")
		void GearDownEngaged();

	// Set how far the shield is extended and return if block visual effects.
	UFUNCTION(BlueprintImplementableEvent, Category = "Pickups")
		bool SetShieldExtension(float extended);

	// Indicate the vehicle has just been reset, normally following being resurrected after being destroyed.
	UFUNCTION(BlueprintImplementableEvent, Category = "General")
		void GameResetVehicle();

#pragma endregion BlueprintImplementableEvents

#pragma region VehiclePhysics

public:

	// The physics properties for the vehicle.
	const FVehiclePhysics& GetPhysics() const
	{ return Physics; }

	// Get the speed of the vehicle, in centimeters per second.
	float GetSpeed() const
	{ return Physics.VelocityData.Speed; }

	// Get the speed of the vehicle, in meters per second.
	float GetSpeedMPS() const
	{ return FMathEx::CentimetersToMeters(Physics.VelocityData.Speed); }

	// Get the speed of the vehicle, in kilometers per hour.
	UFUNCTION(BlueprintCallable, Category = Vehicle)
		float GetSpeedKPH(bool reported = false) const
	{ return FMathEx::CentimetersPerSecondToKilometersPerHour(Physics.VelocityData.Speed); }

	// Get the velocity direction of the vehicle.
	const FVector& GetVelocityDirection() const
	{ return Physics.VelocityData.VelocityDirection; }

	// Get the angular velocity of the vehicle in local space.
	const FVector& GetAngularVelocity() const
	{ return Physics.VelocityData.AngularVelocity; }

	// Get the direction of the vehicle.
	FVector GetDirection() const
	{ return VehicleMesh->GetComponentQuat().GetAxisX(); }

	// Get the vehicle's facing direction.
	FVector GetFacingDirection() const
	{ return VehicleMesh->GetComponentQuat().GetAxisX(); }

	// Get the side direction of the vehicle.
	FVector GetSideDirection() const
	{ return VehicleMesh->GetComponentQuat().GetAxisY(); }

	// Get the vehicle's up direction.
	FVector GetUpDirection() const
	{ return VehicleMesh->GetComponentQuat().GetAxisZ(); }

	// Get the direction of the vehicle's velocity of facing direction if velocity is too small.
	FVector GetVelocityOrFacingDirection() const
	{ return ((Physics.VelocityData.VelocityDirection.SizeSquared() != 0.0f) ? Physics.VelocityData.VelocityDirection : GetFacingDirection()); }

	// Get the world location of the center of the vehicle.
	FVector GetCenterLocation() const
	{ return VehicleMesh->GetComponentTransform().GetLocation(); }

	// Get the bulls-eye for targeting.
	virtual FVector GetTargetBullsEye() const override
	{ return GetCenterLocation(); }

	// Is the vehicle currently with all wheels off the ground?
	bool IsAirborne(bool ignoreSkipping = false);

	// Is the vehicle currently with some but not all wheels off the ground?
	bool IsPartiallyAirborne() const
	{ return (Physics.ContactData.Airborne == false && Physics.ContactData.Grounded == false); }

	// Is the vehicle currently with all wheels on the ground?
	bool IsGrounded(float overTime = 0.0f) const
	{ return (Physics.ContactData.Grounded && (overTime == 0.0f || GetModeTime() >= overTime)); }

	// Is the vehicle currently purposefully drifting?
	bool IsDrifting() const
	{ return Physics.Drifting.Active; }

	// Has the vehicle landed after a respawn?
	bool HasRespawnLanded() const
	{ return Physics.ContactData.RespawnLanded; }

private:

	// Is the vehicle flippable and has bidirectional wheels?
	virtual bool IsFlippable()
	{ return false; }

	// Update the physics portion of the vehicle.
	void UpdatePhysics(float deltaSeconds, const FTransform& transform);

	// Is this vehicle falling?
	bool IsFalling() const
	{ return (Physics.ContactData.FallingTime > 0.666f); }

	// Is the vehicle reversing?
	bool IsReversing() const
	{ return (Control.ThrottleInput < 0.0f && FVector::DotProduct(Physics.VelocityData.VelocityDirection, Physics.Direction) < 0.25f); }

	// Get the rear end slip value, 1 being no slip and < 1 being some slip value.
	float GetDriftingRatio() const
	{ return (IsDrifting() == false) ? 0.0f : FMath::Lerp(0.0f, 1.0f, FMath::Abs(Control.SteeringPosition)); }

	// Get the amount of drifting we're doing between 0 and 1.
	float GetDriftRatio() const
	{ return (FMath::Abs(Physics.Drifting.RearDriftAngle) / TireFrictionModel->RearEndDriftAngle); }

	// Setup any additional collision structures for the vehicle.
	void SetupExtraCollision();

	// Set the actor's location and rotation. Here we just record the fact that it's changed and pass it up to our parent class.
	bool SetActorLocationAndRotation(FVector newLocation, FRotator newRotation, bool sweep, FHitResult* outSweepHitResult, ETeleportType teleport, bool resetPhysics)
	{ Physics.ResetLastLocation |= resetPhysics; return Super::SetActorLocationAndRotation(newLocation, newRotation, sweep, outSweepHitResult, teleport); }

	// Get the physics transform matrix for the vehicle.
	const FTransform& GetPhysicsTransform() const
	{ return Physics.PhysicsTransform; }

	// Do the regular physics update tick.
	void SubstepPhysics(float deltaSeconds, FBodyInstance* bodyInstance);

	// The propulsion properties for the vehicle.
	FVehiclePropulsion Propulsion;

	// The physics properties for the vehicle.
	FVehiclePhysics Physics;

	// The main body instance of the vehicle mesh.
	FBodyInstance* PhysicsBody = nullptr;

	// Hook into the physics system so that we can sub-step the vehicle dynamics with the general physics sub-stepping.
	FCalculateCustomPhysics OnCalculateCustomPhysics;

#pragma endregion VehiclePhysics

#pragma region VehicleContactSensors

public:

	// The wheel properties for the vehicle.
	const FVehicleWheels& GetWheels() const
	{ return Wheels; }

	// Is the vehicle going to need to be flipped?
	bool IsFlipped() const
	{ return Wheels.SoftFlipped; }

	// Is the vehicle currently flipped?
	bool IsFlippedAndWheelsOnGround() const
	{ return Wheels.HardFlipped; }

	// Is the vehicle currently with all wheels (more or less) on the ground?
	bool IsPracticallyGrounded(float distance = 50.0f, bool anyWheel = false);

	// Get the average distance of the wheels from the vehicle to the nearest driving surface, 0 for not near any driving surface.
	float GetSurfaceDistance(bool discountFrontWheelsWhenRaised, bool closest = false);

	// Get the direction from the vehicle to the nearest driving surface.
	FVector GetSurfaceDirection();

	// Get the direction from the vehicle to launch weapons from, often opposing the nearest surface direction.
	FVector GetLaunchDirection(bool inContact = false) const;

	// Guess the normal of the nearest driving surface.
	FVector GuessSurfaceNormal() const;

	// Do we have a valid surface contact, optionally over a period of seconds.
	bool IsSurfaceDirectionValid(float contactSeconds);

	// Get the location of the nearest driving surface to the center of the vehicle.
	FVector GetSurfaceLocation() const;

private:

	// Get the maximum of all the wheel radii.
	float GetMaxWheelRadius()
	{
		float radius = 0.0f;

		for (const FWheelAssignment& assignment : WheelAssignments)
		{
			radius = FMath::Max(radius, assignment.Radius);
		}

		return radius;
	}

	// Are we wheel spinning right now?
	bool SpinningTheWheel() const
	{ return (StandingStart == true && Wheels.SpinWheelsOnStart == true && FMath::Abs(Wheels.WheelRPS) > 1.0f && (FMath::Abs(Propulsion.PistonEngineThrottle) > KINDA_SMALL_NUMBER || IsPowerAvailable() == false)); }

	// Is the wheel currently driven by the engine? If not then it's free-wheeling.
	bool IsWheelDriven(FVehicleWheel& wheel) const
	{
		return ((VehicleEngineModel->WheelDriveModel == EWheelDriveModel::AllWheel) ||
			(VehicleEngineModel->WheelDriveModel == EWheelDriveModel::RearWheel && wheel.HasRearPlacement() == true) ||
			(VehicleEngineModel->WheelDriveModel == EWheelDriveModel::FrontWheel && wheel.HasFrontPlacement() == true));
	}

	// Is the wheel fitted with brakes.
	bool IsWheelBraked(FVehicleWheel& wheel) const
	{
		return ((TireFrictionModel->BrakeAssignment == EWheelDriveModel::AllWheel) ||
			(TireFrictionModel->BrakeAssignment == EWheelDriveModel::RearWheel && wheel.HasRearPlacement() == true) ||
			(TireFrictionModel->BrakeAssignment == EWheelDriveModel::FrontWheel && wheel.HasFrontPlacement() == true));
	}

	// Get the location of the bone for a wheel of a given index, in world space.
	FVector GetWheelBoneLocationFromIndex(int32 index)
	{ return GetWheelBoneLocation(Wheels.Wheels[index], VehicleMesh->GetComponentTransform()); }

	// Get the location of the bone for a wheel, in world space.
	// Optionally clipped on the Y axis to within the bounds of the collision shape.
	static FVector GetWheelBoneLocation(const FVehicleWheel& wheel, const FTransform& transform, bool clipToCollision = false);

	// Get the location to apply suspension forces to for a particular wheel in world space.
	FVector GetSuspensionForcesLocation(const FVehicleWheel& wheel, const FTransform& transform, float deltaSeconds);

	// Get the steering angle for a wheel index of the vehicle, in degrees.
	float GetVisualSteeringAngle(FVehicleWheel& wheel) const
	{ return (wheel.HasFrontPlacement() == true) ? Wheels.FrontVisualSteeringAngle : (wheel.HasRearPlacement() == true) ? Wheels.BackVisualSteeringAngle : 0.0f; }

	// Get the number of wheels attached to the vehicle, optionally just the wheels that have tire grip.
	int32 GetNumWheels(bool grippingOnly = false) const
	{
		if (grippingOnly == true)
		{
			int32 numWheels = 0;

			for (const FVehicleWheel& wheel : Wheels.Wheels)
			{
				if (wheel.HasCenterPlacement() == false)
				{
					numWheels++;
				}
			}

			return numWheels;
		}

		return Wheels.Wheels.Num();
	}

	// Get how much grip we should apply to a particular contact sensor at this time.
	float GetGripRatio(const FVehicleContactSensor& sensor) const;

	// Get the normal of the nearest driving surface.
	FVector GetSurfaceNormal() const;

	// Update the contact sensors.
	int32 UpdateContactSensors(float deltaSeconds, const FTransform& transform, const FVector& xdirection, const FVector& ydirection, const FVector& zdirection);

	// Get the name of the surface the vehicle is currently driving on.
	FName GetSurfaceName() const
	{ return Wheels.SurfaceName; }

	// Are we allowed to engage the throttle to the wheels? (correct race state)
	bool IsPowerAvailable() const;

	// Get the standard position of the bone for a wheel, in world space.
	// This is normally for applying grip forces, and standard bone positions
	// are used to apply symmetrical forces no-matter what the configuration.
	static FVector GetStandardWheelLocation(const FVehicleWheel& wheel, const FTransform& transform)
	{ return transform.TransformPosition(wheel.StandardBoneOffset); }

	// Get the name of a surface from its type.
	static FName GetNameFromSurfaceType(EGameSurface surfaceType);

	// The wheels / springs and associated properties for the vehicle.
	FVehicleWheels Wheels;

#pragma endregion VehicleContactSensors

#pragma region VehicleBasicForces

public:

	// Get the predicted velocity based on recorded velocity information.
	FVector GetPredictedVelocity() const;

private:

	// Arrest the vehicle until the game has started.
	void ArrestVehicle();

	// Update the power and gearing, returns true if just shifted up a gear.
	void UpdatePowerAndGearing(float deltaSeconds, const FVector& xdirection, const FVector& zdirection);

	// Get the engine power applied at this point in time if we were to use full throttle.
	float GetJetEnginePower(int32 numWheelsInContact, const FVector& xdirection);

	// Get the force of gravity to apply to the vehicle over one second.
	FVector GetGravityForce(bool totalGravity) const;

	// Get the drag force based on the velocity given and the vehicle's drag coefficient.
	FVector GetDragForceFor(FVector velocity) const;

	// Get the drag force based on the velocity of the vehicle and its drag coefficient.
	FVector GetDragForce() const
	{ return GetDragForceFor(Physics.VelocityData.Velocity); }

	// Get the rolling resistance force based on the velocity given and the vehicle's rolling resistance coefficient.
	FVector GetRollingResistanceForceFor(float speed, const FVector& velocityDirection, const FVector& xdirection) const;

	// Get the rolling resistance force based on the velocity of the vehicle and its rolling resistance coefficient.
	FVector GetRollingResistanceForce(const FVector& xdirection) const
	{ return GetRollingResistanceForceFor(Physics.VelocityData.Speed, Physics.VelocityData.VelocityDirection, xdirection); }

	// Get the down force based on the velocity of the vehicle and its down force coefficient, in meters per second.
	FVector GetDownForce();

#pragma region VehiclePickups

	// Is a pickup currently charging at all?
	bool PickupIsCharging(bool ignoreTurbos);

#pragma endregion VehiclePickups

	// Get the speed range for a single gear.
	// This is for piston engine simulation.
	float GetGearSpeedRange() const
	{ return VehicleEngineModel->GearSpeedRange; }

#pragma endregion VehicleBasicForces

#pragma region VehicleControls

public:

	// The state of control over the vehicle.
	const FVehicleControl& GetVehicleControl() const
	{ return Control; }

private:

	// Control the forwards / backwards motion, the value will be somewhere between -1 and +1, often at 0 or the extremes.
	void Throttle(float value, bool bot);

	// Control the left / right motion, the value will be somewhere between -1 and +1.
	void Steering(float value, bool analog, bool bot);

	// Engage the brake.
	void HandbrakePressed(bool bot);

	// Release the brake.
	void HandbrakeReleased(bool bot);

	// Control the forwards / backwards motion, the value will be somewhere between -1 and +1, often at 0 or the extremes.
	void Throttle(float value)
	{ Throttle(value, false); }

	// Control the left / right motion, the value will be somewhere between -1 and +1.
	void AnalogSteering(float value)
	{ Steering(value, true, false); }

	// Control the left / right motion, the value will be somewhere between -1 and +1.
	void DigitalSteering(float value)
	{ Steering(value, false, false); }

	// Engage the brake.
	void HandbrakePressed()
	{ HandbrakePressed(false); }

	// Release the brake.
	void HandbrakeReleased()
	{ HandbrakeReleased(false); }

	// Handle the use of automatic braking to assist the driver.
	float AutoBrakePosition(const FVector& xdirection) const;

	// Calculate the assisted throttle input for a player.
	float CalculateAssistedThrottleInput();

	// Interpolate the control inputs to give smooth changes to digital inputs.
	void InterpolateControlInputs(float deltaSeconds);

	// Handle the pitch control for airborne control.
	void PitchControl(float value);

	// Update the steering of the wheels.
	void UpdateSteering(float deltaSeconds, const FVector& xdirection, const FVector& ydirection, const FQuat& quaternion);

#pragma endregion VehicleControls

#pragma region VehicleGrip

private:

	// Calculate the rotations per second rate of a wheel.
	void CalculateWheelRotationRate(FVehicleWheel& wheel, const FVector& velocityDirection, float vehicleSpeed, float brakePosition, float deltaSeconds);

	// Get the lateral friction for a dot product result between normalized wheel velocity vs the wheel side vector.
	float LateralFriction(float baselineFriction, float sideSlip, FVehicleWheel& wheel) const;

	// Calculate the longitudinal grip ratio for a slip value.
	float CalculateLongitudinalGripRatioForSlip(float slip) const;

	// Get the horizontal velocity vector for a wheel, for use in slip calculations.
	static FVector GetHorizontalVelocity(const FVehicleWheel& wheel, const FTransform& transform);

	// Get the horizontal velocity vector for a wheel, for use in slip calculations.
	FVector GetHorizontalVelocity(const FVehicleWheel& wheel)
	{ const FTransform& transform = VehicleMesh->GetComponentTransform(); return GetHorizontalVelocity(wheel, transform); }

	// Get the weight acting on a wheel for this point in time, in kilograms.
	float GetWeightActingOnWheel(FVehicleWheel& wheel);

#pragma endregion VehicleGrip

#pragma region VehicleAnimation

private:

	// Apply a visual roll to add tilt to the vehicle when cornering and most of the wheels are on the ground.
	void UpdateVisualRotation(float deltaSeconds, const FVector& xdirection, const FVector& ydirection);

	// Update the animated bones.
	void UpdateAnimatedBones(float deltaSeconds, const FVector& xdirection, const FVector& ydirection);

	// When killing controlled vehicle body pitch, the pitch angle we're starting from.
	float VehiclePitchFrom = 0.0f;

	// Time accumulator used When killing controlled vehicle body pitch.
	float VehiclePitchAccumulator = 0.0f;

#pragma endregion VehicleAnimation

#pragma region VehicleLaunch

private:

	// Start charging the vehicle launch.
	UFUNCTION(BlueprintCallable, Category = Advanced)
		void LaunchChargeOn(bool ai)
	{
		if (ai == AI.BotDriver)
		{
			LaunchPressed = true;

			if (LaunchCharging == ELaunchStage::Idle)
			{
				LaunchCharging = ELaunchStage::Charging; LaunchTimer = 0.0f;
			}
		}
	}

	// Stop charging the vehicle launch and invoke it.
	UFUNCTION(BlueprintCallable, Category = Advanced)
		void LaunchChargeOff(bool ai, float overrideCharge = 0.0f)
	{
		if (ai == AI.BotDriver)
		{
			LaunchPressed = false;

			if (LaunchCharging == ELaunchStage::Charging)
			{
				LaunchCharging = ELaunchStage::Released; if (overrideCharge != 0.0f) LaunchTimer = overrideCharge;
			}
		}
	}

	// Cancel charging the vehicle launch.
	UFUNCTION(BlueprintCallable, Category = Advanced)
		void LaunchChargeCancel(bool ai)
	{
		if (ai == AI.BotDriver)
		{
			LaunchPressed = false;

			if (LaunchCharging == ELaunchStage::Charging)
			{
				LaunchCharging = ELaunchStage::Idle; LaunchTimer = 0.0f;
			}
		}
	}

public:

	// Get the charge level for vehicle launching.
	float GetLaunchChargeLevel() const
	{ return (LaunchCharging == ELaunchStage::Charging) ? LaunchTimer : 0.0f; }

	// Get the charge color for vehicle launching.
	FLinearColor GetLaunchChargeColour() const
	{ return (LaunchTimer == 1.0f) ? FLinearColor(0.0f, 1.0f, 0.0f, 1.0f) : FLinearColor(1.0f, 0.0f, 0.0f, 1.0f); }

private:

	// Update the launching of the vehicle.
	void UpdateLaunch(float deltaSeconds);

	// Start vehicle launch charging.
	void LaunchChargeInputOn()
	{ LaunchChargeOn(false); }

	// Stop vehicle launch charging, initiating the launch.
	void LaunchChargeInputOff()
	{ LaunchChargeOff(false); }

	// Cancel vehicle launch charging.
	void LaunchChargeInputCancel()
	{ LaunchChargeCancel(false); }

	// Is the launch input currently pressed?
	bool LaunchPressed = false;

	// Timer used for vehicle launching.
	float LaunchTimer = 0.0f;

	// The last time the vehicle was launched.
	float LastLaunchTime = 0.0f;

	// The current state of the vehicle launching.
	ELaunchStage LaunchCharging = ELaunchStage::Idle;

	// The surface normal that the vehicle launched from.
	FVector LaunchSurfaceNormal = FVector::ZeroVector;

#pragma endregion VehicleLaunch

#pragma region VehicleLaunchControl

private:

	// Update the launch control state for getting a boost off the start line.
	void UpdateLaunchControl();

	// Has the vehicle used launch control to boost off the start line?
	bool UsedLaunchControl() const
	{ return (Control.LaunchControl == 2 || Control.LaunchControl == 4); }

#pragma endregion VehicleLaunchControl

#pragma region VehicleDrifting

private:

	// Is the vehicle in a state where drifting is possible?
	bool CanDrift() const
	{ return (Physics.ContactData.Airborne == false && GetSpeedKPH() > 100.0f && Control.ThrottleInput > 0.5f && FMath::Abs(Control.SteeringPosition) > 0.75f); }

	// Is the bot vehicle in a state where drifting is possible?
	bool AICanDrift() const
	{ return (Physics.ContactData.Airborne == false && GetSpeedKPH() > 250.0f && Control.ThrottleInput > 0.1f && FMath::Abs(Control.SteeringPosition) > 0.5f); }

	// Start drifting.
	void StartDrifting()
	{
		if (Physics.Drifting.Active == false)
		{
			Physics.Drifting.Active = true;
			Physics.Drifting.NonDriftingTimer = Physics.Drifting.Timer;
			Physics.Drifting.Timer = 0.0f;
		}
	}

	// Update the drifting of the back end state.
	void UpdateDriftingState(float deltaSeconds);

	// Update the drifting of the back end physics.
	void UpdateDriftingPhysics(float deltaSeconds, float steeringPosition, const FVector& xdirection);

	// Is this vehicle skidding?
	bool IsSkidding(bool removeGlitches = false) const
	{ return (Antigravity == true) ? false : ((removeGlitches == true) ? (Wheels.SkidAudioVolumeTarget > 0.25f || Wheels.SkidTimer > 0.0f) : (Wheels.SkidAudioVolumeTarget > 0.25f)); }

#pragma endregion VehicleDrifting

#pragma region VehicleSurfaceEffects

public:

	// Get the color for a dust trail.
	FVector GetDustColor(bool noise);

private:

	// Get the alpha for a dust trail.
	float GetDustAlpha(FVehicleWheel& wheel, bool noise, bool spinning, bool integrateContact, bool integrateTimer);

	// Get the size for a dust trail.
	FVector GetDustSize();

	// Get the color for grit.
	FVector GetGritColor();

	// Get the amount of grit in a dust trail.
	float GetGritAmount() const;

	// Get the velocity for the grit in a dust trail.
	FVector GetGritVelocity();

	// Update the surface effects from the wheels.
	void UpdateSurfaceEffects(float deltaSeconds);

	// Spawn a new surface effect for a given wheel.
	UParticleSystemComponent* SpawnDrivingSurfaceEffect(const FVehicleWheel& wheel, UParticleSystem* particleSystem);

	// Compute a timer to co-ordinate the concurrent use of effects across vehicles.
	void ComputeSurfaceEffectsTimer();

	// Get a noise value.
	float Noise(float value) const;

	static const int32 DrivingSurfaceFullyVisible = 1;
	static const int32 DrivingSurfaceFadeOutAt = 4;
	static const int32 DrivingSurfaceMaxTime = 6;

#pragma endregion VehicleSurfaceEffects

#pragma region VehicleSurfaceImpacts

	// Update effects because of hard compression of the springs.
	void UpdateHardCompression();

	// Spawn a new surface impact effect.
	void SpawnSurfaceImpactEffect(const FVector& hitLocation, const FVector& hitNormal, const FHitResult& hitResult, const FVector& velocity, float controllerForce, bool tireImpact);

#pragma endregion VehicleSurfaceImpacts

#pragma region VehicleSlowTurningRecovery

private:

	// Update the vehicle disorientation.
	void UpdateVehicleDisorientation(float deltaSeconds);

	// The timer use for disorientation.
	float DisorientedTimer = 0.0f;

	// The yaw orbit to apply during disorientation.
	float DisorientedYaw = 0.0f;

#pragma endregion VehicleSlowTurningRecovery

#pragma region VehicleAudio

public:

	// The global volume level of this vehicle for things like engine sound effects.
	float GlobalVolume = 0.0f;

	// Ratio between 0 and 1, 0 being quiet (furthest) and 1 being (loud) closest.
	// Used in the calculation of GlobalVolume.
	float GlobalVolumeRatio = 0.0f;

private:

	// Configure the vehicles engine audio
	void SetupEngineAudio();

	// Manage the audio for skidding.
	void UpdateSkidAudio(float deltaSeconds);

	// The index of the currently used engine sound.
	int32 EngineAudioIndex;

	// The current skidding sound in use.
	TWeakObjectPtr<USoundCue> SkiddingSound;

	// The last skidding sound that was used.
	TWeakObjectPtr<USoundCue> LastSkiddingSound;

	// Is the skid audio playing?
	bool SkidAudioPlaying = false;

	// The current skid audio volume.
	float SkidAudioVolume = 0.0f;

	// The last pitch used for the gear audio.
	float LastGearPitch = 0.0f;

#pragma endregion VehicleAudio

#pragma region NavigationSplines

public:

	// Get the direction of the vehicle compared to its pursuit spline.
	int32 GetPursuitSplineDirection() const;

private:

	// The route follower to use when resurrecting the vehicle.
	FRouteFollower ResurrectionRouteFollower;

#pragma endregion NavigationSplines

#pragma region AINavigation

private:

	// Perform the AI for a vehicle.
	void UpdateAI(float deltaSeconds);

	// Reset the spline weaving to sync with the current relative vehicle position to the spline.
	void AIResetSplineWeaving()
	{ AI.ResetPursuitSplineWidthOffset = true; }

	// Reset the spline following so that it starts over.
	void AIResetSplineFollowing(bool beginPlay, bool allowDeadEnds = true, bool keepCurrentSpline = false, bool retainLapPosition = true, float minMatchingDistance = 0.0f);

	// Follow the current spline, and switch over to the next if necessary.
	void AIFollowSpline(const FVector& location, const FVector& wasHeadingTo, const FVector& movement, float movementSize, float deltaSeconds, int32 numIterations = 5, float accuracy = 1.0f);

	// Switch splines if the current one looks suspect.
	bool AICheckSplineValidity(const FVector& location, float checkCycle, bool testOnly);

	// Determine where to aim on the spline, switching splines at branches if necessary.
	// The vehicle itself will follow on a little later, as the aim point is always ahead of the vehicle.
	void AIDetermineSplineAimPoint(float ahead, float movementSize);

	// Update an offset from the center line of the current aiming spline that makes the
	// car weaves around a little on the track rather than appearing robotic.
	void AIUpdateSplineWeaving(const FVector& location);

	// Has this vehicle gone off-track somehow?
	bool IsVehicleOffTrack(bool extendedChecks);

	// Should the vehicle stay on this spline because it's being cinematically watched on it right now?
	bool StayOnThisSpline() const
	{ return (NumSplineWatchers > 0); }

	// Query parameters for a ray cast.
	FCollisionQueryParams QueryParams = FCollisionQueryParams(TEXT("VehicleSensor"), true, this);

#pragma endregion AINavigation

#pragma region VehicleTeleport

public:

	// Is the vehicle currently teleporting?
	bool IsTeleporting() const
	{ return (Teleportation.Action == 2); }

	// Get the charge level of the teleport between 0 and 1.
	float GetTeleportChargeLevel() const;

	// Get the residue level for the teleportation between 0 and 1.
	float GetTeleportResidue(float scale = GRIP_TELEPORT_SPAM_PERIOD) const;

	// Get the charge color for the teleportation.
	FLinearColor GetTeleportChargeColor() const
	{ return (Teleportation.Action == 3 || TeleportPossible() == true) ? FLinearColor(0.0f, 1.0f, 0.0f, 1.0f) : FLinearColor(1.0f, 0.0f, 0.0f, 1.0f); }

private:

	// Is teleportation currently possible?
	bool TeleportPossible() const
	{ return ((HUD.HomingMissileTime == 0.0f || HUD.HomingMissileTime > 0.333f) && (VehicleClock - Teleportation.RecoveredAt > GRIP_TELEPORT_SPAM_PERIOD) && (RaceState.RaceTime > GRIP_TELEPORT_SPAM_PERIOD) && (Teleportation.Action == 0)); }

	// Update the teleportation.
	void UpdateTeleportation(float deltaSeconds);

	// The controller input to teleport to track is down.
	void TeleportOn();

	// The controller input to teleport to track is up.
	void TeleportOff();

	// The controller input to teleport to track is down.
	void TeleportToTrackDown();

	// The controller input to teleport to track is up.
	void TeleportToTrackUp();

	// Get the destination for a teleportation from the current location / rotation.
	void GetTeleportDestination(FVector& location, FRotator& rotation, float& initialSpeed);

	// Set the teleport destination.
	void SetTeleportDestination(const FVector& location, const FRotator& rotation, float speed);

	// Teleport the vehicle back to the track.
	void Teleport(FRouteFollower& routeFollower, FVector location, FRotator rotation, float speed, float distanceAlongMasterRacingSpline, float minMatchingDistance);

	// If the vehicle is stuck then just teleport back onto the track.
	bool AITeleportIfStuck();

	// Reset the AI data on teleport.
	void AITeleportReset(const FVector& location)
	{ AI.TeleportReset(location); }

	// The teleportation data.
	FVehicleTeleportation Teleportation;

#pragma endregion VehicleTeleport

#pragma region AIVehicleControl

private:

	// Update the driving mode of the vehicle, this is the main driving coordination center.
	void AIUpdateDrivingMode(const FVector& movementPerSecond, const FVector& direction, const FVector& heading);

	// Determine if the vehicle is still in normal control and switch driving mode if not.
	void AIUpdateGeneralManeuvering(const FVector& movementPerSecond, const FVector& direction, const FVector& heading);

	// Determine if the vehicle has recovered control and switch to a new driving mode if so.
	void AIUpdateRecoveringControl(const FVector& direction, const FVector& heading);

	// Determine if the vehicle has reoriented correctly and switch to a new driving mode if so.
	void AIUpdateReversingToReorient(const FVector& movementPerSecond, const FVector& direction, const FVector& heading);

	// Determine if the vehicle has reversed away from a blockage and switch to a new driving mode if so.
	void AIUpdateReversingFromBlockage(const FVector& movementPerSecond);

	// Determine if the vehicle has launched to the correct direction and switch to a new driving mode if so.
	void AIUpdateLaunchToReorient(const FVector& direction, const FVector& heading);

	// Update the J turn maneuver and determine if the vehicle has reoriented to the correct direction and switch to a new driving mode if so.
	void AIUpdateJTurnToReorient(const FVector& direction, const FVector& heading);

	// Manage drifting around long, sweeping corners.
	void AIUpdateDrifting(const FVector& location, const FVector& direction);

	// Is the vehicle stuck and should we reverse direction to try to get out of it.
	bool AIAreWeStuck(const FVector& movementPerSecond, bool reversing);

	// Do we lost control?
	void AIHaveWeLostControl(const FVector& direction, const FVector& heading);

	// Given all the current state, update the control inputs to the vehicle to achieve the desired goals.
	void AICalculateControlInputs(const FTransform& transform, const FVector& location, const FVector& direction, const FVector& movementPerSecond, float deltaSeconds);

	//Calculate the throttle required, reverse if necessary, to achieve the desired speed. Target speed is in centimeters per second.
	float AICalculateThrottleForSpeed(const FVector& xdirection, float targetSpeed);

	// Is movement of the vehicle possible or is it stuck unable to move in the desired direction?
	bool AIMovementPossible() const;

	// Record vehicle progress, backwards and forwards, throttle settings and other data that we can use later in AI bot decision making.
	void AIRecordVehicleProgress(const FTransform& transform, const FVector& movement, const FVector& direction, float deltaSeconds);

	// Update the vehicle fishtailing.
	void AIUpdateFishTailing(float deltaSeconds);

#pragma endregion AIVehicleControl

#pragma region AIVehicleRollControl

private:

	// Given all the current state, update the airborne roll control inputs to the vehicle to achieve the desired goals.
	float AICalculateRollControlInputs(const FTransform& transform, float deltaSeconds);

	// Perform the control required to match the target roll.
	void AIPerformRollControl(float relativeRollTarget, float rollTargetTime, float& steer, ERollControlStage& rollControl) const;

#pragma endregion AIVehicleRollControl

#pragma region AIAttraction

private:

	// Keep track of targets of opportunity, deciding if any current target is still
	// valid and also picking a new target if we have no current target.
	void AIUpdateTargetsOfOpportunity(const FVector& location, const FVector& direction, const FVector& wasHeadingTo, float ahead, int32 numIterations, float accuracy, float deltaSeconds);

	// Cancel any attraction for the AI bot.
	void AICancelAttraction()
	{ if (AI.AttractedTo != nullptr) AI.AttractedTo->Attract(nullptr); AI.AttractedTo = nullptr; AI.AttractedToActor.Reset(); }

#pragma region PickupGun

	// Should this vehicle continue to follow the given vehicle?
	bool AIShouldContinueToFollow(const FVector& location, const FVector& direction, float deltaSeconds);

#pragma endregion PickupGun

#pragma endregion AIAttraction

#pragma region VehicleCatchup

public:

	// Are we using leading catchup to control this vehicle?
	bool GetUsingLeadingCatchup() const
	{ return UsingLeadingCatchup; }

	// Are we using trailing catchup to control this vehicle?
	bool GetUsingTrailingCatchup() const
	{ return UsingTrailingCatchup; }

private:

	// Update the catchup assistance state of the vehicle.
	void UpdateCatchup();

	// Are we using leading catchup to control this vehicle?
	bool UsingLeadingCatchup = false;

	// Are we using trailing catchup to control this vehicle?
	bool UsingTrailingCatchup = false;

	// The characteristics used to control vehicle catchup assistance.
	FVehicleCatchupCharacteristics CatchupCharacteristics;

#pragma endregion VehicleCatchup

#pragma region SpeedPads

public:

	// Add a temporary boost to the vehicle, for when running over speed pads and the like.
	// amount is between 0 and 1, 1 being 100% more engine power.
	// duration is in seconds.
	// direction is the world direction to apply the speed boost force.
	bool SpeedBoost(ASpeedPad* speedpad, float amount, float duration, const FVector& direction);

	// Collect the speed pads overlapping with a vehicle.
	void CollectSpeedPads();

#pragma endregion SpeedPads

#pragma region VehicleBoost

private:

	// Update the boosting of the vehicle.
	void UpdateBoost(float deltaSeconds);

	// Engage the vehicle boost.
	void BoostDown()
	{ BoostOn(false); }

	// Disengage the vehicle boost.
	void BoostUp()
	{ BoostOff(false); }

	// Set the use of boost to be on.
	void BoostOn(bool force);

	// Set the use of boost to be off.
	void BoostOff(bool force);

#pragma endregion VehicleBoost

#pragma region PickupPads

private:

	// Collect the pickups overlapping with a vehicle.
	void CollectPickups();

#pragma endregion PickupPads

#pragma region VehiclePickups

public:

	// Can AI vehicles attack this vehicle?
	bool CanBeAttacked() const
	{ return ((AI.BotVehicle == true) ? true : VehicleClock > AttackAfter); }

	// Get a scale for damage inflicted by weapons, taking into account double damage etc.
	int32 GetDamageScale() const
	{ return (RaceState.DoubleDamage > 0.0f) ? 2 : 1; }

	// Does this vehicle have a particular pickup type?
	bool HasPickup(EPickupType type, bool includeActive = true) const
	{ for (const FPlayerPickupSlot& pickup : PickupSlots) if (pickup.Type == type && (pickup.State == EPickupSlotState::Idle || (includeActive == true && pickup.State == EPickupSlotState::Active))) return true; return false; }

	// Get a pickup slot's details.
	const FPlayerPickupSlot& GetPickupSlot(int32 pickupSlot) const
	{ return PickupSlots[pickupSlot]; }

	// Release the pickup in a particular slot.
	void ReleasePickupSlot(int32 pickupSlot, bool animate = true);

	// Get the alpha for a pickup slot.
	float GetPickupSlotAlpha(int32 pickupSlot) const;

private:

	// Update the pickup slots.
	void UpdatePickupSlots(float deltaSeconds);

	// Start using a pickup.
	void BeginUsePickup(int32 pickupSlot, bool bot, bool force = false);

	// Use a pickup.
	void UsePickup(int32 pickupSlot, EPickupActivation activation, bool bot);

	// Use the pickup in slot 1.
	void UsePickup1()
	{ BeginUsePickup(0, false); }

	// Use the pickup in slot 2.
	void UsePickup2()
	{ BeginUsePickup(1, false); }

	// Use the pickup in slot 1.
	void ReleasePickup1()
	{ UsePickup(0, EPickupActivation::Released, false); }

	// Use the pickup in slot 2.
	void ReleasePickup2()
	{ UsePickup(1, EPickupActivation::Released, false); }

	// Force a particular pickup to a vehicle.
	void ForcePickup(EPickupType type, int32 pickupSlot);

	// Get the scale for a pickup slot.
	float GetPickupSlotScale(int32 pickupSlot) const;

	// Determine the selected target.
	void DetermineTargets(float deltaSeconds, const FVector& location, const FVector& direction);

	// Switch the missile target.
	void SwitchMissileTarget()
	{ if (AI.BotDriver == false) SwitchPickupTarget(-1); }

	// Switch the target for a pickup slot.
	void SwitchPickupTarget(int32 pickupSlot = -1);

	// Determine which pickup to give to a vehicle.
	EPickupType DeterminePickup(APickup* pickup);

	// The number of pickup slots.
	static const int32 NumPickups = 2;

	// Information for each of the pickup slots.
	FPlayerPickupSlot PickupSlots[NumPickups];

	// Count for the pickup collection order.
	int32 PickupCount = 0;

	// Repeat count for the last pickup given.
	int32 LastPickupRepeatCount = 0;

	// The last pickup type given to this vehicle.
	EPickupType LastPickupGiven = EPickupType::None;

	// The pseudo-randomized queued pickups for each placement.
	TArray<TArray<EPickupType>> QueuedPickups;

#pragma endregion VehiclePickups

#pragma region PickupTurbo

public:

	// Set the properties of the turbo boost.
	void SetTurboBoost(float turboBoost, float gripScale, float raiseFrontScale, float normalizedThrust)
	{
		Propulsion.Boost = turboBoost;
		Propulsion.BoostGripScale = 1.0f - (turboBoost * (1.0f - gripScale));
		Propulsion.RaiseFrontScale = raiseFrontScale;
		Propulsion.TurboThrottle = normalizedThrust;
	}

private:

	// Update the light streaks for a vehicle.
	void UpdateLightStreaks(float deltaSeconds);

	// Apply the turbo raise force when using a charged turbo pickup.
	void ApplyTurboRaiseForce(float deltaSeconds, const FTransform& transform);

	// The last alpha value used to render the vehicle's light streaks.
	float LastTurboAlpha = -1.0f;

#pragma endregion PickupTurbo

#pragma region PickupGun

public:

	// Get the orientation of the gun.
	virtual FQuat GetGunOrientation() const override;

	// Get the direction for firing a round.
	virtual FVector GetGunRoundDirection(FVector direction) const override;

	// Get the round ejection properties.
	virtual FVector EjectGunRound(int32 roundLocation, bool charged) override;

	// Apply a bullet round force.
	bool BulletRound(float strength, int32 hitPoints, int32 aggressorVehicleIndex, const FVector& position, const FVector& fromPosition, bool charged, float spinSide);

private:

	// Timer used to raise your shield after a bullet strikes your vehicle.
	float BulletHitTimer = 0.0f;

#pragma endregion PickupGun

#pragma region PickupMissile

public:

	// Get the velocity of the host.
	virtual FVector GetHostVelocity() const override
	{ return VehicleMesh->GetPhysicsLinearVelocity(); }

	// Get a false target location for a missile.
	virtual FVector GetMissileFalseTarget() const override;

	// Get the current homing missile, if any.
	TWeakObjectPtr<AHomingMissile>& GetHomingMissile()
	{ return HomingMissile; }

	// Apply a missile explosion force.
	bool MissileForce(float strength, int32 hitPoints, int32 aggressorVehicleIndex, const FVector& location, bool limitForces, bool destroyShield, FGameEvent* gameEvent);

	// Get the sustained angular pitch velocity over the last quarter second.
	float GetSustainedAngularPitch();

	// Apply a peripheral explosion force.
	static void PeripheralExplosionForce(float strength, int32 hitPoints, int32 aggressorVehicleIndex, const FVector& location, bool limitForces, FColor color, ABaseVehicle* avoid, UWorld* world, float radius);

private:

	// Fire a homing missile.
	void FireHomingMissile(int32 pickupSlot, int32 missileIndex);

	// Apply a direct explosion force.
	bool ExplosionForce(float strength, int32 hitPoints, int32 aggressorVehicleIndex, const FVector& location, bool limitForces, EPickupType source, bool destroyShield, bool applyForces, FColor color, FGameEvent* gameEvent);

	// Apply a peripheral explosion force.
	void PeripheralExplosionForce(float strength, int32 hitPoints, int32 aggressorVehicleIndex, const FVector& location, bool limitForces, FColor color);

	FName GetMissileBayName() const;

	// Update any active missiles firing from the vehicle.
	void UpdateMissiles(float deltaSeconds);

	// The missile ejection state.
	enum class EMissileEjectionState : uint8
	{
		Inactive,
		BayOpening,
		Firing1,
		Firing2
	};

	// Small structure for handling missile ejection.
	struct FMissileEjection
	{
		EMissileEjectionState State = EMissileEjectionState::Inactive;

		TArray<TWeakObjectPtr<AActor>> PickupTargets;
	};

	// Ejection state of missiles for each of the pickup slots.
	FMissileEjection EjectionState[NumPickups];

	// The last homing missile that was fired, if any.
	TWeakObjectPtr<AHomingMissile> HomingMissile;

	// Is the missile port currently in use?
	bool MissilePortInUse = false;

	// Is a missile currently incoming on the vehicle?
	bool IncomingMissile = false;

	// The time at which the vehicle last exploded.
	float LastExploded = 0.0f;

#pragma endregion PickupMissile

#pragma region PickupShield

public:

	// Is a shield currently active on the vehicle and protecting against a given position?
	bool IsShielded(const FVector& position) const;

	// Is a shield currently active on the vehicle?
	bool IsShieldActive() const;

	// Get the current shield if any.
	AShield* GetShield() const
	{ return Shield.Get(); }

	// Get the forward shield extension.
	float GetForwardShieldExtension() const
	{ return ForwardShieldExtension; }

	// Release any active shield.
	void ReleaseShield(bool permanently);

	// Remove the grip from a vehicle for a moment.
	void RemoveGripForAMoment(const FVector& impulse)
	{ Physics.ApplyImpulse += impulse; }

	// Does the player currently have a shield?
	bool HasShield() const
	{ return GRIP_POINTER_VALID(Shield); }

	// Damage the shield by a given amount.
	void DamageShield(int32 hitPoints, int32 aggressorVehicleIndex);

	// Destroy the shield.
	void DestroyShield(int32 aggressorVehicleIndex);

private:

	// The shield attached to the vehicle, if any.
	TWeakObjectPtr<AShield> Shield;

	// The rear shield extension.
	float ShieldExtension = 0.0f;

	// The forward shield extension.
	float ForwardShieldExtension = 0.0f;

	// The target value for shield extension between 0 and 1.
	float ShieldExtensionTarget = 0.0f;

	// Does the shield extension block visual effects?
	bool ShieldExtensionBlocks = false;

#pragma endregion PickupShield

#pragma region BotCombatTraining

public:

	// Should the bot raise its shield?
	bool AIShouldRaiseShield();

private:

	// Handle pickups use.
	void AIUpdatePickups(float deltaSeconds);

	// Get a weighting, between 0 and 1, of how ideally a pickup can be used.
	// 0 means cannot be used effectively at all, 1 means a very high chance of pickup efficacy.
	float GetPickupEfficacyWeighting(int32 pickupSlot, AActor*& target);

#pragma endregion BotCombatTraining

#pragma region VehicleSpringArm

public:

	// The angle that the rear-end is currently drifting at.
	float GetSpringArmYaw() const;

	// The roll angle.
	float GetSpringArmRoll() const;

	// Get the amount of shake to apply for the auto-boost.
	float GetAutoBoostShake() const
	{ return Propulsion.AutoBoostShake; }

	// Has the vehicle just smashed into something and requires the forward-facing crash-camera?
	bool HasSmashedIntoSomething(float maxKPH) const;

private:

	// Looking left.
	void LeftViewCamera();

	// Looking right.
	void RightViewCamera();

	// Looking forwards or backwards.
	void LookForwards(float val);

	// Looking left or right.
	void LookSideways(float val);

	// Ease the camera in toward the target.
	void CameraIn()
	{ CameraTarget()->SpringArm->CameraIn(); }

	// Ease the camera out away from the target.
	void CameraOut()
	{ CameraTarget()->SpringArm->CameraOut(); }

	// Looking front.
	void FrontViewCamera()
	{ CameraTarget()->SpringArm->FrontViewCamera(GameState->GeneralOptions.InstantaneousLook); }

	// Looking rear.
	void RearViewCamera()
	{ CameraTarget()->SpringArm->RearViewCamera(GameState->GeneralOptions.InstantaneousLook); }

	// Update the materials used to render the vehicle based on cockpit-camera state.
	void UpdateCockpitMaterials();

	// Indicator as to whether we're currently using cockpit-camera materials or not.
	bool UsingCockpitMaterial = false;

#pragma endregion VehicleSpringArm

#pragma region VehicleCamera

public:

	// Get the amount for the warning vignette on the camera.
	float GetWarningAmount() const
	{ return HUD.WarningAmount * HUD.WarningAlpha; }

	// Get the color for the warning vignette on the camera.
	FLinearColor GetWarningColour() const
	{
		switch (HUD.WarningSource)
		{
		case EHUDWarningSource::StandardPickup:
			return FLinearColor(1.0f, 1.0f, 0.25f, 0.0f);
		case EHUDWarningSource::HealthPickup:
			return FLinearColor(0.0f, 0.5f, 1.0f, 0.0f);
		case EHUDWarningSource::DoubleDamage:
		case EHUDWarningSource::DoubleDamagePickup:
			return FLinearColor(0.4f, 0.0f, 0.8f, 0.0f);
		case EHUDWarningSource::Elimination:
			return FLinearColor(1.0f, 0.33f, 0.0f, 0.0f);
		default:
			return FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		}
	}

#pragma endregion VehicleCamera

#pragma region CameraCinematics

public:

	// Start cinematically watching the vehicle on this spline, so stay on it if possible.
	void StartWatchingOnSpline()
	{ NumSplineWatchers++; }

	// Stop cinematically watching the vehicle on this spline.
	void StopWatchingOnSpline()
	{ NumSplineWatchers--; }

	// Is the vehicle driving in alignment with its current pursuit spline and within its bounds?
	bool IsDrivingStraightAndNarrow() const;

	// Get the camera ball for use with this vehicle.
	ACameraBallActor* GetCameraBall();

private:

	// The camera ball for blowing up the vehicle
	TWeakObjectPtr<ACameraBallActor> CameraBallActor;

	// Index for the current point camera when cycling through them authoring views.
	int32 CameraPointIndex = 0;

#pragma endregion CameraCinematics

#pragma region VehicleHUD

public:

	// Get the speed of the vehicle, in kilometers / miles per hour.
	FString GetFormattedSpeedKPH(int32 index) const;

	// Get the race time of the vehicle.
	FString GetFormattedRaceTime() const
	{ return GetFormattedTime(RaceState.RaceTime); }

	// Get the best lap time of the vehicle.
	FString GetFormattedBestLapTime() const
	{ return (RaceState.BestLapTime == 0.0f) ? TEXT("-") : GetFormattedTime(RaceState.BestLapTime); }

	// Get the lap time of the vehicle.
	FString GetFormattedLapTime() const
	{ return GetFormattedTime(RaceState.LapTime); }

	// Get the last lap time of the vehicle.
	FString GetFormattedLastLapTime() const
	{ return GetFormattedTime(RaceState.LastLapTime); }

	// Get the alpha for pickup slot 1.
	float GetPickupSlot1Alpha() const
	{
		return GetPickupSlotAlpha(0);
	}

	// Get the alpha for pickup slot 2.
	float GetPickupSlot2Alpha() const
	{
		return GetPickupSlotAlpha(1);
	}

	// Get the scale for pickup slot 1.
	float GetPickupSlot1Scale() const
	{
		return GetPickupSlotScale(0);
	}

	// Get the scale for pickup slot 2.
	float GetPickupSlot2Scale() const
	{
		return GetPickupSlotScale(1);
	}

	// Get the alpha value of the wrong way indicator.
	float GetWrongWayAlpha();

	// Has the player just completed a lap?
	UFUNCTION(BlueprintCallable, Category = Race)
		bool LapCompleted() const
	{ return RaceState.LapCompleted && (RaceState.EternalLapNumber <= GameState->GeneralOptions.NumberOfLaps); }

	// Show a status message.
	UFUNCTION(BlueprintCallable, Category = HUD)
		void ShowStatusMessage(const FStatusMessage& message, bool queue, bool inChatIfPossible) const;

	// Get the amount of auto-boost available.
	UFUNCTION(BlueprintCallable, Category = Advanced)
		float GetAutoBoost() const
	{ return Propulsion.AutoBoost; }

	// Get the auto-boost state.
	UFUNCTION(BlueprintCallable, Category = Advanced)
		EAutoBoostState GetAutoBoostState() const
	{ return Propulsion.AutoBoostState; }

	// Play a 1D client sound.
	void ClientPlaySound(USoundBase* Sound, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f) const;

	// Play the denied sound when a player tries to do something that they cannot.
	void PlayDeniedSound();

	// Shake the HUD, following an explosion or something.
	void ShakeHUD(float strength);

	// Does the vehicle have a currently selected target?
	bool HasTarget(int32 pickupSlot) const
	{ return (HUD.CurrentMissileTarget[pickupSlot] != -1); }

	// Is the target for a pickup slot a primary target?
	bool IsPrimaryTarget(int32 pickupSlot)
	{ return (HasTarget(pickupSlot) == true) ? HUD.GetCurrentMissileTarget(pickupSlot)->Primary : false; }

	// Get the fade in value for the targeting reticule.
	float TargetFadeIn(int32 pickupSlot)
	{ return (HasTarget(pickupSlot) == true) ? HUD.GetCurrentMissileTarget(pickupSlot)->TargetTimer : 0.0f; }

	// Get the missile warning amount between 0 and 1, 1 being imminent.
	float GetMissileWarningAmount() const
	{ return FMath::Min(HUD.MissileWarningAmount, 1.0f); }

	// Get the timer for respawning, or resurrecting the vehicle.
	float GetRespawnTimer() const
	{ return 0.0f; }

	// Get a formatted time for racing.
	static FString GetFormattedTime(float seconds);

	// Get the HUD for the player.
	FVehicleHUD& GetHUD()
	{ return HUD; }

private:

	// The HUD properties for the vehicle.
	FVehicleHUD HUD;

public:

	// Hookup a HUD for the player.
	void HookupPlayerHUD();

	// Unhook the HUD for the player.
	void UnhookPlayerHUD();

private:

	// Update the animation on the HUD.
	void UpdateHUDAnimation(float deltaSeconds);

#pragma endregion VehicleHUD

#pragma region VehicleAntiGravity

public:

	// Get the hovering instability for a vehicle, based on its mass.
	float GetHoveringInstability() const
	{ return (Physics.StockMass - 5000.0f) / 4000.0f; }

	// Get the amount of air power currently available to antigravity vehicles.
	float GetAirPower() const
	{ return Propulsion.AirPower; }

	// Noise function for antigravity vehicles, used for adding a low frequency hovering offset to all contact sensors in a unified way.
	FMathEx::FSineNoise HoverNoise = true;

private:

	// Cut the air power available to antigravity vehicles for a period of time.
	void CutAirPower(float forSeconds)
	{ Propulsion.AirPowerCut = FMath::Max(Propulsion.AirPowerCut, forSeconds); Propulsion.AirPower = 0.0f; }

	// Update the forwards and antigravity scaling ratios for antigravity vehicles.
	void UpdateAntigravityForwardsAndScale(float deltaSeconds, float brakePosition, float& forwardRatio, float& scaleAntigravity);

#pragma endregion VehicleAntiGravity

#pragma region ClocksAndTime

public:

	// How long has it been since the game has ended for this player?
	float GetGameEndedClock() const
	{ return (RaceState.PlayerCompletionState >= EPlayerCompletionState::Complete) ? PlayGameMode->GetRealTimeClock() - RaceState.GameFinishedAt : 0.0f; }

	// Get the clock for the vehicle since the game mode was started.
	float GetVehicleClock() const
	{ return VehicleClock; }

	// Get the general clock since the game mode was started.
	float GetClock() const
	{ return GameMode->GetClock(); }

	// Get the real-time clock since the game mode was started.
	float GetRealTimeClock() const
	{ return GameMode->GetRealTimeClock(); }

	// Get the time spent in the current mode (airborne or not).
	float GetModeTime() const
	{ return Physics.ContactData.ModeTime; }

	// Get the time spent grounded over the most recent period of time given.
	float GroundedTime(float seconds) const
	{ return Physics.ContactData.GroundedList.GetMeanValue(Physics.Timing.TickSum - seconds); }

	// Reset the timer used for controlling attack frequency.
	void ResetAttackTimer();

private:

	// The vehicle clock, ticking as per its own time dilation, especially when the Disruptor is active.
	float VehicleClock;

	// A time sharing clock for a half second period.
	FTimeShareClock Clock0p5 = FTimeShareClock(0.5f);

	// A time sharing clock for a quarter second period.
	FTimeShareClock Clock0p25 = FTimeShareClock(0.25f);

	// A time sharing clock for a tenth of a second period.
	FTimeShareClock Clock0p1 = FTimeShareClock(0.1f);

#pragma endregion ClocksAndTime

#pragma region Miscellaneous

public:

	// Give a particular pickup to a vehicle.
	UFUNCTION(BlueprintCallable, Category = Pickups)
		int32 GivePickup(EPickupType type, int32 pickupSlot = -1, bool fromTrack = false);

	// Sometimes, antigravity vehicles bounce too high on landing, but you can use this to scale it down in places.
	UFUNCTION(BlueprintCallable, Category = General)
		void SetAntigravityBounceScale(float scale)
	{ AntigravityBounceScale = scale; }

	// Is this host controlled by a local human player with a physical controller?
	UFUNCTION(BlueprintCallable, Category = General)
		bool IsHumanPlayer(bool local = true) const
	{ return (Controller != nullptr && ((local == true) ? Controller->IsLocalPlayerController() : Controller->IsPlayerController())); }

	// Kick off the cinematic camera.
	UFUNCTION(BlueprintCallable, Category = Advanced)
		void KickTheCinematics()
	{
		Camera->GetCinematicsDirector().AttachToAnyVehicle(this);
	}

	// Is an AI driver good for a launch?
	UFUNCTION(BlueprintCallable, Category = Advanced)
		bool AIVehicleGoodForLaunch(float probability, float minimumSpeedKPH) const;

	UFUNCTION(BlueprintCallable, Category = Advanced)
		void EnableClosestSplineEvaluation(bool enabled)
	{ AI.ClosestSplineEvaluationEnabled = enabled; }

	// Is the vehicle currently under AI bot control? If this flag is set, vehicle may have been human at some point, but has a bot now (end of game for example).
	UFUNCTION(BlueprintCallable, Category = System)
		bool HasAIDriver() const
	{ return AI.BotDriver; }

	// Set whether the vehicle should use an AI bot driver or not.
	UFUNCTION(BlueprintCallable, Category = System)
		void SetAIDriver(bool aiDriver, bool setVehicle = false, bool setInputMappings = false);

	// Lock the steering to spline direction?
	UFUNCTION(BlueprintCallable, Category = AI)
		void SteeringToSplineDirection(bool locked, bool avoidStaticObjects);

	// Spawn an appropriately scaled particle system on the vehicle.
	UFUNCTION(BlueprintCallable, Category = System)
		UParticleSystemComponent* SpawnParticleSystem(UParticleSystem* emitterTemplate, FName attachPointName, FVector location, FRotator rotation, EAttachLocation::Type locationType, float scale = 1.0f, bool autoDestroy = true);

	// Is the vehicle current using cockpit-camera view?
	UFUNCTION(BlueprintCallable, Category = "General")
		bool IsCockpitView() const
	{
		return (IsCinematicCameraActive() == true) ? false : SpringArm->IsCockpitView();
	}

	// Cycle through the camera points on the vehicle.
	UFUNCTION(BlueprintCallable, Category = System)
		void CycleCameraPoint();

	// Begin teleportation.
	UFUNCTION(BlueprintCallable, Category = Teleport)
		void BeginTeleport();

	// Get the target vehicle for the camera.
	ABaseVehicle* CameraTarget();

	// Get the unique index for the vehicle.
	virtual int32 GetVehicleIndex() const override
	{ return VehicleIndex; }

	// Disqualify this player from the game event.
	void Disqualify();

	// Is the player disqualifed from the game event?
	bool IsDisqualified() const
	{ return RaceState.PlayerCompletionState == EPlayerCompletionState::Disqualified; }

	// Get the progress through the game event, from 0 to 1.
	float GetEventProgress();

	// Get the race state for the player.
	FPlayerRaceState& GetRaceState()
	{ return RaceState; }

	// Get the AI context for the player.
	FVehicleAI& GetAI()
	{ return AI; }

	// Are all of the pickup slots filled?
	bool ArePickupSlotsFilled() const
	{ return ((PickupSlots[0].State != EPickupSlotState::Empty) && (PickupSlots[1].State != EPickupSlotState::Empty)); }

	// Is the vehicle currently destroyed.
	bool IsVehicleDestroyed(bool ignoreSuspended = true) const
	{ return (RaceState.PlayerHidden == true && (ignoreSuspended == true || RaceState.PlayerCompletionState != EPlayerCompletionState::Suspended)); }

	// Is the vehicle going the wrong way around the track?
	bool IsGoingTheWrongWay() const;

	// Should the vehicle turn left to head in the correct direction?
	bool ShouldTurnLeft() const;

	// Should the vehicle turn right to head in the correct direction?
	bool ShouldTurnRight() const;

	// Use human player audio?
	virtual bool UseHumanPlayerAudio() const override
	{ return IsHumanPlayer(); }

	// Does this vehicle belong to a human player?
	int32 DetermineLocalPlayerIndex();

	// The state of elimination for the vehicle.
	FVehicleElimination& GetVehicleElimination()
	{ return Elimination; }

	// Add points to the player's total if the player's game hasn't ended.
	bool AddPoints(int32 numPoints, bool visualize, ABaseVehicle* fromVehicle = nullptr, const FVector& worldLocation = FVector::ZeroVector);

	// Shakes the Gamepad controller.
	void ShakeController(float strength, float duration, bool smallLeft, bool smallRight, bool largeLeft, bool largeRight, TEnumAsByte<EDynamicForceFeedbackAction::Type> action);

	// Is a vehicle good for a smacking? i.e. it doesn't upset the game in general to attack them now.
	bool IsGoodForSmacking() const
	{ return (AI.LockSteeringToSplineDirection == false); }

	// Get the target heading for the vehicle, roughly what direction it should be heading in for this part of the track.
	FVector GetTargetHeading() const;

	// Get the name of the player, optionally shortened or full.
	const FString& GetPlayerName(bool shortened, bool full = true);

	// If this flag is set, the vehicle started off with/has always been an AI - it's never been human controlled.
	bool IsAIVehicle() const
	{ return AI.BotVehicle; }

	// Get the bounding extent of the vehicle.
	FVector GetBoundingExtent() const
	{ return BoundingExtent; }

	// Perform some initialization on the vehicle post spawn.
	void PostSpawn(int32 vehicleIndex, bool isLocalPlayer, bool bot);

	// Are we still accounting for this vehicle in this game event?
	bool IsAccountingClosed() const
	{ return RaceState.IsAccountingClosed(); }

	// Is the player current using a turbo boost?
	bool IsUsingTurbo(bool isCharged = false) const
	{ for (const FPlayerPickupSlot& pickup : PickupSlots) if (pickup.Type == EPickupType::TurboBoost && pickup.State == EPickupSlotState::Active) return (isCharged == false || pickup.IsCharged()); return false; }

#pragma region PickupGun

	// Is the player current using a Gatling gun?
	bool IsUsingGatlingGun(bool isCharged = false) const
	{ for (const FPlayerPickupSlot& pickup : PickupSlots) if (pickup.Type == EPickupType::GatlingGun && pickup.State == EPickupSlotState::Active) return (isCharged == false || pickup.IsCharged()); return false; }

#pragma endregion PickupGun

	// Is the player currently using double damage?
	bool IsUsingDoubleDamage() const
	{ for (const FPlayerPickupSlot& pickup : PickupSlots) if (pickup.State == EPickupSlotState::Active && RaceState.DoubleDamage > 0.0f) return true; return false; }

	// Shake the camera.
	bool ShakeCamera(float strength);

	// Is cinematic camera currently active?
	bool IsCinematicCameraActive(bool orGameEnded = true) const
	{ return (Camera->GetCinematicsDirector().IsActive() == true || (orGameEnded == true && RaceState.PlayerCompletionState >= EPlayerCompletionState::Complete)); }

	// Get the position within the current gear that the emulated piston engine is at, between 0 and 1.
	float GearPosition() const
	{ return Propulsion.CurrentGearPosition; }

	// The unique index number of the vehicle.
	int32 VehicleIndex = 0;

	// The index of the local player, used to relate controllers to players, or -1 if not a local player (AI bot for example).
	// This doesn't always mirror Cast<APlayerController>(GetController())->GetLocalPlayer()->GetControllerId() for local players.
	// LocalPlayerIndex will always be 0 for the primary player.
	int32 LocalPlayerIndex = INDEX_NONE;

	// Local copy of Cast<APlayerController>(GetController())->GetLocalPlayer()->GetControllerId() for local players.
	int32 ControllerID = INDEX_NONE;

	// Sometimes, antigravity vehicles bounce too high on landing and this scales it down in places.
	float AntigravityBounceScale = 1.0f;

	// The index of the RootDummy bone.
	int32 RootDummyBoneIndex = INDEX_NONE;

	// The clipping box around the vehicle of the camera mounted on the spring arm.
	FBox CameraClipBox;

	// Intersection query parameters for wheel contact sensors.
	FCollisionQueryParams ContactSensorQueryParams = FCollisionQueryParams(TEXT("ContactSensor"), true, this);

	// The amount to scale attached effects by to have them appear at a consistent scale across vehicles.
	// Normally this remains at FVector::OneVector.
	FVector AttachedEffectsScale = FVector::OneVector;

	// Noise function for generating random chaos to the game.
	FPerlinNoise PerlinNoise;

	// The hover distance of the wheel in cm. #TODO refactor
	float HoverDistance = 0.0f;

	// The cheap material to use for the race camera.
	static UMaterialInterface* CheapCameraMaterial;

	// The expensive material to use for the race camera.
	static UMaterialInterface* ExpensiveCameraMaterial;

	// How long it takes to hook a pickup slot into charging.
	static float PickupHookTime;

private:

	// Handle the update of the idle locking, ensuring the vehicle stays still at very
	// low speed rather than subtly sliding around.
	void UpdateIdleLock();

	// Complete the post spawn sequence.
	void CompletePostSpawn();

	// Has the post spawn sequence started?
	bool PostSpawnStarted = false;

	// Has the post spawn sequence completed?
	bool PostSpawnComplete = false;

	// The AI context for controlling this vehicle.
	FVehicleAI AI;

	// The state of control over the vehicle.
	FVehicleControl Control;

	// The elimination properties for the vehicle.
	FVehicleElimination Elimination;

	// The race state of the player controlling this vehicle.
	FPlayerRaceState RaceState = FPlayerRaceState(this);

	// The name of the player for the vehicle.
	FString PlayerName;

	// The short name of the player for the vehicle.
	FString ShortPlayerName;

	// Are the player names valid?
	bool PlayerNamesValid = false;

	// Is the vehicle to accelerate from a standing start?
	bool StandingStart = false;

	// If the vehicle is to accelerate from a standing start, is this a restart?
	bool StandingRestart = false;

	// The number of watches of this vehicle on its current spline.
	int32 NumSplineWatchers = 0;

	// The VehicleClock value that it's OK for AI bots to attack this vehicle after.
	// This is to prevent bots hammering a single player repeatedly.
	float AttackAfter = 0.0f;

	// The bounding extent of the entire vehicle.
	FVector BoundingExtent = FVector::OneVector;

	// Array of collision contact points from the last frame.
	TArray<FVector> ContactPoints[2];

	// Array of collision contact forces from the last frame.
	TArray<FVector> ContactForces[2];

	// The number of default wheels when no wheels are detected.
	static const int32 NumDefaultWheels = 4;

	// Have pickup probabilities been initialized for this game mode?
	static bool ProbabilitiesInitialized;

#pragma endregion Miscellaneous

#pragma region TransientProperties

private:

	// The box component used for vehicle / vehicle collision.
	UPROPERTY(Transient)
		UBoxComponent* VehicleCollision = nullptr;

	// Naked pointer to the world for performance reasons.
	UPROPERTY(Transient)
		UWorld* World = nullptr;

	// Naked pointer to game state for performance reasons.
	UPROPERTY(Transient)
		UGlobalGameState* GameState = nullptr;

	// Naked pointer to play game mode for performance reasons.
	UPROPERTY(Transient)
		ABaseGameMode* GameMode = nullptr;

	// Naked pointer to play game mode for performance reasons.
	UPROPERTY(Transient)
		APlayGameMode* PlayGameMode = nullptr;

	// Audio components for the jet engine sound.
	UPROPERTY(Transient)
		TArray<UAudioComponent*> JetEngineAudio;

	// Audio components for the piston engine sound.
	UPROPERTY(Transient)
		TArray<UAudioComponent*> PistonEngineAudio;

	// Audio component for the gear shift sound.
	UPROPERTY(Transient)
		UAudioComponent* GearShiftAudio = nullptr;

	// Audio component for the engine boost sound.
	UPROPERTY(Transient)
		UAudioComponent* EngineBoostAudio = nullptr;

	// Audio component for the skidding sound.
	UPROPERTY(Transient)
		UAudioComponent* SkiddingAudio = nullptr;

	// The ghost material we use to render the vehicle in cockpit-camera.
	UPROPERTY(Transient)
		UMaterialInstanceDynamic* OurGhostMaterial = nullptr;

	// The base materials this vehicle uses.
	UPROPERTY(Transient)
		TArray<FMeshMaterialOverride> BaseMaterials;

	// The list of canards attached to this vehicle.
	UPROPERTY(Transient)
		TArray<ACanard*> Canards;

	// The list of light streaks attached to this vehicle.
	UPROPERTY(Transient)
		TArray<ULightStreakComponent*> LightStreaks;

	// The list of turbo particle systems attached to this vehicle.
	UPROPERTY(Transient)
		TArray<UParticleSystemComponent*> TurboParticleSystems;

	// The out effect for teleporting the vehicle.
	UPROPERTY(Transient)
		UParticleSystemComponent* ResetEffectOut = nullptr;

	// The in effect for teleporting the vehicle.
	UPROPERTY(Transient)
		UParticleSystemComponent* ResetEffectIn = nullptr;

	// The sound for teleporting the vehicle.
	UPROPERTY(Transient)
		UAudioComponent* TeleportAudio = nullptr;

	// The sound for charging a pickup.
	UPROPERTY(Transient)
		UAudioComponent* PickupChargingSoundComponent = nullptr;

	// The sound to use for the charged shield impact.
	UPROPERTY(Transient)
		USoundBase* ShieldChargedImpactSound = nullptr;

	// The widget to use for the HUD.
	UPROPERTY(Transient)
		UHUDWidget* HUDWidget = nullptr;

	// The particle systems to use for the vehicle's boost state.
	UPROPERTY(Transient)
		TArray<UParticleSystemComponent*> BoostEffectComponents;

#pragma endregion TransientProperties

#pragma region BlueprintAssets

private:

	static TSubclassOf<AGatlingGun> Level1GatlingGunBlueprint;
	static TSubclassOf<AGatlingGun> Level2GatlingGunBlueprint;
	static TSubclassOf<AHomingMissile> Level1MissileBlueprint;
	static TSubclassOf<AHomingMissile> Level2MissileBlueprint;
	static TSubclassOf<AShield> Level1ShieldBlueprint;
	static TSubclassOf<AShield> Level2ShieldBlueprint;
	static TSubclassOf<ATurbo> Level1TurboBlueprint;
	static TSubclassOf<ATurbo> Level2TurboBlueprint;
	static TSubclassOf<AElectricalBomb> DestroyedElectricalBomb;
	static UParticleSystem* DestroyedParticleSystem;
	static UParticleSystem* ResetEffectBlueprint;
	static UParticleSystem* LaunchEffectBlueprint;
	static UParticleSystem* HardImpactEffect;
	static UParticleSystem* DamageEffect;
	static UParticleSystem* DamageSparks;
	static UMaterialInterface* CockpitGhostMaterial;
	static USoundCue* TeleportSound;
	static USoundCue* LaunchSound;
	static USoundCue* DestroyedSound;

#pragma endregion BlueprintAssets

#pragma region FriendClasses

	friend class ADebugAIHUD;
	friend class ADebugPickupsHUD;
	friend class ADebugVehicleHUD;
	friend class ADebugCatchupHUD;
	friend class ADebugRaceCameraHUD;

#pragma endregion FriendClasses

};

#pragma endregion MinimalVehicle
