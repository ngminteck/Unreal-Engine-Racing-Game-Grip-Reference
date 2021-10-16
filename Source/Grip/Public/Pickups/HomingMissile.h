/**
*
* Scorpion homing missile implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Homing missile pickup type, one of the pickups used by vehicles in the game.
*
***********************************************************************************/

#pragma once

#include "system/gameconfiguration.h"
#include "pickupbase.h"
#include "components/staticmeshcomponent.h"
#include "physicsengine/radialforcecomponent.h"
#include "pickups/missilemovementcomponent.h"
#include "effects/lightstreakcomponent.h"
#include "homingmissile.generated.h"

struct FPlayerPickupSlot;

/**
* Boilerplate class for the MissileHostInterface.
***********************************************************************************/

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UMissileHostInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

/**
* Interface class for the MissileHostInterface.
***********************************************************************************/

class IMissileHostInterface
{
	GENERATED_IINTERFACE_BODY()

public:

	// Use human player audio?
	virtual bool UseHumanPlayerAudio() const = 0;

	// Get the unique index for the vehicle host.
	virtual int32 GetVehicleIndex() const
	{ return -1; }

#pragma region PickupMissile

	// Get the velocity of the host.
	virtual FVector GetHostVelocity() const = 0;

	// Get a false target location for a missile.
	virtual FVector GetMissileFalseTarget() const = 0;

#pragma endregion PickupMissile

};

/**
* Legacy class derived from UParticleSystemComponent, no longer extends it.
***********************************************************************************/

UCLASS(ClassGroup = Rendering)
class GRIP_API UGripTrailParticleSystemComponent : public UParticleSystemComponent
{
	GENERATED_BODY()
};

/**
* The homing missile pickup actor.
***********************************************************************************/

UCLASS(Abstract, ClassGroup = Pickups)
class GRIP_API AHomingMissile : public APickupBase
{
	GENERATED_BODY()

public:

	// Construct a homing missile.
	AHomingMissile();

	// The amount of variance of the angle of ejection, for untargeted missiles.
	UPROPERTY(EditAnywhere, Category = Missile, meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
		float AngleVariance = 0.1f;

	// Rocket duration in seconds.
	UPROPERTY(EditAnywhere, Category = Missile)
		float RocketDuration = 10.0f;

	// The relative force of the explosion against the target vehicle.
	UPROPERTY(EditAnywhere, Category = Missile, meta = (UIMin = "0", UIMax = "100", ClampMin = "0", ClampMax = "100"))
		float VehicleExplosionForce = 1.0f;

	// The hit points associated with this weapon.
	UPROPERTY(EditAnywhere, Category = Missile, meta = (UIMin = "0", UIMax = "250", ClampMin = "0", ClampMax = "250"))
		int32 HitPoints = 40;

	// Should we lose lock with the target when the target is behind the missile?
	UPROPERTY(EditAnywhere, Category = Missile)
		bool LoseLockOnRear = true;

	// The distance for the proximity fuse.
	UPROPERTY(EditAnywhere, Category = Missile)
		float ProximityFuse = 1000.0f;

	// Sound cue for the eject sound.
	UPROPERTY(EditAnywhere, Category = Missile)
		USoundCue* EjectSound = nullptr;

	// Sound cue for the eject sound. Non-Player variant.
	UPROPERTY(EditAnywhere, Category = Missile)
		USoundCue* EjectSoundNonPlayer = nullptr;

	// Sound cue for the ignition sound.
	UPROPERTY(EditAnywhere, Category = Missile)
		USoundCue* IgnitionSound = nullptr;

	// Sound cue for the ignition sound. Non-Player variant.
	UPROPERTY(EditAnywhere, Category = Missile)
		USoundCue* IgnitionSoundNonPlayer = nullptr;

	// Sound cue for the rocket sound.
	UPROPERTY(EditAnywhere, Category = Missile)
		USoundCue* RocketSound = nullptr;

	// Sound cue for the explosion sound.
	UPROPERTY(EditAnywhere, Category = Missile)
		USoundCue* ExplosionSound = nullptr;

	// Sound cue for the explosion sound.
	UPROPERTY(EditAnywhere, Category = Missile)
		USoundCue* ExplosionSoundNonPlayer = nullptr;

	// The particle system to use for airborne the missile explosion.
	UPROPERTY(EditAnywhere, Category = Missile)
		UParticleSystem* ExplosionVisual = nullptr;

	// The radial force for the explosion.
	UPROPERTY(EditAnywhere, Category = Missile)
		URadialForceComponent* ExplosionForce = nullptr;

	// The mesh to use as a missile.
	UPROPERTY(EditAnywhere, Category = Missile)
		UStaticMeshComponent* MissileMesh = nullptr;

	// The particle system to use for rocket trail.
	UPROPERTY(EditAnywhere, Category = Missile)
		UGripTrailParticleSystemComponent* RocketTrail = nullptr;

	// The light streak to use for rocket trail.
	UPROPERTY(EditAnywhere, Category = Missile)
		ULightStreakComponent* RocketLightStreak = nullptr;

	// The light to use for rocket trail.
	UPROPERTY(EditAnywhere, Category = Missile)
		UPointLightComponent* RocketLight = nullptr;

	// The movement used for the missile.
	UPROPERTY(EditAnywhere, Category = Missile)
		UMissileMovementComponent* MissileMovement = nullptr;

	// The target actor that the missile is aiming for right now.
	UPROPERTY(Transient)
		AActor* Target = nullptr;

	// Audio component for the rocket sound.
	UPROPERTY(Transient)
		UAudioComponent* RocketAudio = nullptr;

#pragma region PickupMissile

	// Activate the pickup.
	virtual void ActivatePickup(ABaseVehicle* launchVehicle, int32 pickupSlot, EPickupActivation activation, bool charged) override;

	// Get the launch platform for the missile, this may or may not be a vehicle.
	AActor* GetLaunchPlatform() const
	{ return LaunchPlatform.Get(); }

	// Called when the missile is moved at all.
	virtual bool OnMove();

	// Ignite the missile.
	virtual void Ignite();

	// Explode the missile.
	virtual void Explode(AActor* hitActor, const FHitResult* hitResult);

	// Set the initial impulse for the missile.
	void SetInitialImpulse(const FVector& impulse) const
	{ MissileMovement->Velocity = impulse; }

	// Set the launcher velocity for the missile.
	void SetLauncherVelocity(const FVector& velocity) const
	{ MissileMovement->SetInheritedVelocity(velocity); }

	// Get the current smoke trail color.
	FVector GetSmokeColor();

	// Get the current smoke trail alpha.
	float GetSmokeAlpha();

	// Get the current smoke trail size.
	FVector GetSmokeSize();

	// Show a HUD indicator?
	bool ShowHUDIndicator() const
	{ return LaunchVehicleIsValid() == true && (CurrentState != EState::Exploding || Timer < 2.0f); }

	// Has the target been hit?
	bool HUDTargetHit() const
	{ return TargetHit; }

	// Has the missile exploded?
	bool HasExploded() const
	{ return (CurrentState == EState::Exploding); }

	// Attach to a launch platform, like a defense turret.
	void AttachLaunchPlatform(AActor* launchPlatform);

	// Setup a target for the missile to aim for.
	void SetTarget(AActor* target, bool missTarget = false)
	{ Target = target; MissTarget = missTarget; }

	// Manually launch the missile, normally from a defense turret.
	void Launch(const FVector& location, const FVector& velocity);

	// Is the missile currently homing?
	bool IsHoming() const
	{ return (CurrentState == EState::Flight); }

	// Is the target within reach?
	// (about to be hit and good for cinematic effect)
	bool IsTargetWithinReach() const
	{ return TargetWithinReach; }

	// Setup a false target for the missile to aim for in the absence of a real target.
	void SetupFalseTarget();

	// Get the time in seconds before impacting target (assuming straight terminal phase and constant speed).
	float GetTimeToTarget() const;

	// Is the missile likely to hit the target?
	bool IsLikelyToHitTarget() const;

	// Is the missile targeting a particular actor?
	bool IsTargeting(AActor* actor) const
	{ return (Target == actor && MissileMovement->HasLostLock() == false); }

	// Select a target to aim for.
	static bool SelectTarget(AActor* launchPlatform, FPlayerPickupSlot* launchPickup, AActor*& existingTarget, TArray<TWeakObjectPtr<AActor>>& targetList, float& weight, int32 maxTargets, bool speculative);

	// Get the target location for a particular target.
	static FVector GetTargetLocationFor(AActor* target, const FVector& targetOffset);

#pragma region BotCombatTraining

	// Is this launch vehicle in a good condition to launch a missile?
	static bool GoodLaunchCondition(ABaseVehicle* launchVehicle);

	// Get a weighting, between 0 and 1, of how ideally a pickup can be used, optionally against a particular vehicle.
	// 0 means cannot be used effectively at all, 1 means a very high chance of pickup efficacy.
	static float EfficacyWeighting(ABaseVehicle* launchVehicle, FPlayerPickupSlot* launchPickup, ABaseVehicle* againstVehicle = nullptr);

#pragma endregion BotCombatTraining

	// Offset the targeting of the missile to cause a deliberate miss.
	FVector HomingTargetOffset = FVector::ZeroVector;

protected:

	// Do some post initialization just before the game is ready to play.
	virtual void PostInitializeComponents() override;

	// Do some shutdown when the actor is being destroyed.
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

	// Do the regular update tick.
	virtual void Tick(float deltaSeconds) override;

private:

	enum class EState : uint8
	{
		Ejecting,
		Flight,
		Exploding
	};

	// Set the initial torque for the missile.
	void SetInitialTorque(FRotator rotator, float roll, bool constrainUp);

	// Is the missile in terminal range of the target?
	bool IsInTerminalRange(AActor* target, float distance = -1.0f, float seconds = 1.0f) const;

	// Record that this missile is imminently incoming on its target.
	bool RecordIncoming();

	// The launch platform for the missile, this may or may not be a vehicle.
	TWeakObjectPtr<AActor> LaunchPlatform;

	// The missile host interface for the launch platform.
	IMissileHostInterface* MissileHost = nullptr;

	// Has the target been hit?
	bool TargetHit = false;

	// Timer.
	float Timer = 0.0f;

	// Time to die at, if any.
	float DieAt = 0.0f;

	// The current state of the missile.
	EState CurrentState = EState::Ejecting;

	// Should we actually try to miss the target?
	bool MissTarget = false;

	// Is the target within reach?
	// (about to be hit and good for cinematic effect)
	bool TargetWithinReach = false;

	// The natural intensity of the rocket motor.
	float RocketIntensity = 0.0f;

	// The last location of the missile, used for measuring distance.
	FVector LastLocation = FVector::ZeroVector;
	FVector LastSubLocation = FVector::ZeroVector;

	// Has the missile entered the lethal range of the target?
	bool InRangeOfTarget = false;

	// Collision query for target visibility.
	FCollisionQueryParams MissileToTargetQueryParams = FCollisionQueryParams("MissileTerrainSensor", true);

	// Noise function for animating opacity in the missile trail.
	FMathEx::FSineNoise OpacityNoise = FMathEx::FSineNoise(false);

	// Noise function for animating brightness in the missile trail.
	FMathEx::FSineNoise BrightnessNoise = FMathEx::FSineNoise(true);

	// Noise function for animating size in the missile trail.
	FMathEx::FSineNoise SizeNoise = FMathEx::FSineNoise(false);

	// The natural size of the flare on the rocket motor.
	float FlareSize = 0.0f;

	// The natural aspect ratio of the flare on the rocket motor.
	float FlareAspectRatio = 0.0f;

	// The amount of time before ignition.
	float IgnitionTime = 0.0f;

	// Constrain the upward motion of the missile ejection on launch?
	bool ConstrainUp = false;

	// Some random drift if not homing towards a target.
	FVector2D RandomDrift = FVector2D(0.0f, 0.0f);

#pragma endregion PickupMissile

	friend class ADebugMissileHUD;
};
