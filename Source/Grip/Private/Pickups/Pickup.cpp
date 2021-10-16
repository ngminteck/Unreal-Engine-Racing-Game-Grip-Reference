/**
*
* Pickup pad implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Pickup pads for vehicles to collect pickups from.
*
***********************************************************************************/

#include "pickups/pickup.h"
#include "vehicle/flippablevehicle.h"
#include "game/globalgamestate.h"
#include "ai/pursuitsplineactor.h"
#include "system/worldfilter.h"
#include "gamemodes/playgamemode.h"

/**
* Construct a pickup effect.
***********************************************************************************/

APickupEffect::APickupEffect()
{
	IdleEffect = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("IdleEffect"));

	IdleEffect->bAutoDestroy = false;
	IdleEffect->bAutoActivate = false;
	IdleEffect->SetHiddenInGame(true);

	SetRootComponent(IdleEffect);

	PickedUpEffect = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("PickedUpEffect"));

	PickedUpEffect->bAutoDestroy = false;
	PickedUpEffect->bAutoActivate = false;
	PickedUpEffect->SetHiddenInGame(true);
}

/**
* Construct a pickup.
***********************************************************************************/

APickup::APickup()
{
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));

	SetRootComponent(CollisionSphere);

	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	CollisionSphere->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	GRIP_ATTACH(PadMesh, RootComponent, NAME_None);

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	PrimaryActorTick.TickGroup = TG_DuringPhysics;
}

#pragma region PickupPads

/**
* Set the location and scale of the pickup effect.
***********************************************************************************/

void APickupEffect::SetLocationAndScale(USceneComponent* component, const FVector& location, float scale) const
{
	GRIP_ATTACH(IdleEffect, component, NAME_None);
	IdleEffect->SetRelativeLocation(location);
	IdleEffect->SetRelativeRotation(FRotator::ZeroRotator);
	IdleEffect->SetWorldScale3D(FVector(scale, scale, scale));

	IdleEffect->SetActive(true);
	IdleEffect->SetHiddenInGame(false);

	GRIP_ATTACH(PickedUpEffect, component, NAME_None);
	PickedUpEffect->SetRelativeLocation(location);
	PickedUpEffect->SetRelativeRotation(FRotator::ZeroRotator);
	PickedUpEffect->SetWorldScale3D(FVector(scale, scale, scale));
}

/**
* Handle the visual effects for a pickup collection.
***********************************************************************************/

void APickupEffect::OnPickupPadCollected()
{
	if (GRIP_OBJECT_VALID(IdleEffect) == true)
	{
		IdleEffect->DestroyComponent();
		IdleEffect = nullptr;
	}

	if (GRIP_OBJECT_VALID(PickedUpEffect) == true)
	{
		PickedUpEffect->SetActive(true);
		PickedUpEffect->SetHiddenInGame(false);
	}
}

/**
* Do some post initialization just before the game is ready to play.
***********************************************************************************/

void APickup::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	FVector location = GetActorLocation();
	FRotator rotation = GetActorRotation();

	if (SnapToSurface == true &&
		DetermineSurfacePosition(location, rotation, CollisionSphere->GetScaledSphereRadius(), this) == true)
	{
		RootComponent->SetMobility(EComponentMobility::Movable);

		SetActorLocation(location);
		SetActorRotation(rotation);

		RootComponent->SetMobility(EComponentMobility::Static);
	}

	AttractionLocation = location + rotation.RotateVector(FVector(0.0f, 0.0f, 100.0f));

	// Identify the attraction direction for the pickup by examining the nearest
	// pursuit spline and its nearest direction.

	if (APlayGameMode::Get(this) != nullptr)
	{
		float distanceAway = 0.0f;
		float distanceAlong = 0.0f;

		APursuitSplineActor::FindNearestPursuitSpline(location, FVector::ZeroVector, GetWorld(), NearestPursuitSpline, distanceAway, distanceAlong, EPursuitSplineType::General, false, false, true, true);

		if (GRIP_POINTER_VALID(NearestPursuitSpline) == true)
		{
			AttractionDirection = NearestPursuitSpline->GetWorldDirectionAtDistanceAlongSpline(FMath::Clamp(distanceAlong, 1.0f, NearestPursuitSpline->GetSplineLength() - 1.0f)) * -1.0f;
		}
	}

	AttractionDistanceRangeCms = FMathEx::MetersToCentimeters(AttractionDistanceRange);

	CollectedAudio = NewObject<UAudioComponent>(this, TEXT("CollectedSound"));
	GRIP_ATTACH(CollectedAudio, RootComponent, NAME_None);
	CollectedAudio->RegisterComponent();

	SpawnedAudio = NewObject<UAudioComponent>(this, TEXT("SpawnedSound"));
	GRIP_ATTACH(SpawnedAudio, RootComponent, NAME_None);
	SpawnedAudio->RegisterComponent();

	// Fix for bad data in some levels.

	CollisionSphere->SetCollisionObjectType(ECC_WorldStatic);
}

/**
* Do some initialization when the game is ready to play.
***********************************************************************************/

void APickup::BeginPlay()
{
	Super::BeginPlay();

	if (Spawn() == true)
	{
		GRIP_ADD_TO_GAME_MODE_LIST(PickupPads);

		APlayGameMode::Get(this)->AddAttractable(this);
	}
}

/**
* Do some shutdown when the actor is being destroyed.
***********************************************************************************/

void APickup::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	GRIP_REMOVE_FROM_GAME_MODE_LIST(PickupPads);

	APlayGameMode::Get(this)->RemoveAttractable(this);

	Super::EndPlay(endPlayReason);
}

/**
* Do the regular update tick.
***********************************************************************************/

void APickup::Tick(float deltaSeconds)
{
	Super::Tick(deltaSeconds);

	if (CurrentState == EState::Collected)
	{
		Timer += deltaSeconds;

		if (Timer >= DelayTime &&
			Class != EPickupClass::Collectible)
		{
			Spawn();

			if (GRIP_OBJECT_VALID(SpawnedAudio) == true)
			{
				SpawnedAudio->SetSound(SpawnedSound);
				SpawnedAudio->Play();
			}
		}
	}
	else if (CurrentState == EState::Spawning)
	{
		Timer += deltaSeconds;

		if (Timer >= SpawnTime)
		{
			CurrentState = EState::Uncollected;
		}
	}
}

/**
* Event for when the pickup is collected.
***********************************************************************************/

void APickup::OnPickupPadCollected(ABaseVehicle* vehicle)
{
	if (CurrentState == EState::Uncollected)
	{
		// Handle the general pickup picked-up event.

		Timer = 0.0f;
		CurrentState = EState::Collected;

		if (GRIP_OBJECT_VALID(CollectedAudio) == true)
		{
			CollectedAudio->SetSound(vehicle->IsHumanPlayer() ? CollectedSound : CollectedSoundNonPlayer);
			CollectedAudio->Play();
		}

		if (GRIP_OBJECT_VALID(PickupEffect) == true)
		{
			PickupEffect->OnPickupPadCollected();

			if (GRIP_OBJECT_VALID(vehicle->PickedUpEffect) == true)
			{
				vehicle->PickedUpEffect->SetActive(true);
				vehicle->PickedUpEffect->SetHiddenInGame(false);
				vehicle->PickedUpEffect->SetOwnerNoSee(vehicle->IsCockpitView());
			}
		}

		Attract(nullptr);
	}
}

/**
* Spawn a new pickup from the pad.
***********************************************************************************/

bool APickup::Spawn()
{
	UGlobalGameState* gameState = UGlobalGameState::GetGlobalGameState(this);
	bool spawn = gameState->ArePickupsActive();

	if (spawn == true &&
		FWorldFilter::IsValid(this, gameState) == true)
	{
		// If we already have a pickup effect then kill it off.

		if (GRIP_OBJECT_VALID(PickupEffect) == true)
		{
			PickupEffect->Destroy();
			PickupEffect = nullptr;
		}

		// Spawn a new pickup effect and set it up.

		FActorSpawnParameters spawnParams;

		spawnParams.Owner = this;
		spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		PickupEffect = GetWorld()->SpawnActor<APickupEffect>(Effect, PadMesh->GetComponentLocation(), PadMesh->GetComponentRotation(), spawnParams);

		PickupEffect->SetLocationAndScale(PadMesh, FVector(0.0f, 0.0f, SurfaceOffset), Scale);

		Timer = 0.0f;
		CurrentState = EState::Spawning;

		return true;
	}

	return false;
}

/**
* How long to wait after becoming efficacious to use should a pickup be used by a bot?
***********************************************************************************/

float APickup::GetEfficacyDelayBeforeUse(EPickupType type, AActor* worldContextObject)
{
	APlayGameMode* gameMode = APlayGameMode::Get(worldContextObject);
	UGlobalGameState* gameState = UGlobalGameState::GetGlobalGameState(worldContextObject);
	FDifficultyCharacteristics& difficulty = gameMode->GetDifficultyCharacteristics();

	if (gameState->IsGameModeRace() == true)
	{
		switch (type)
		{
		case EPickupType::Shield:
			// Don't react too quickly, depending on defense responsiveness.
			return 0.5f + ((1.0f - difficulty.PickupUseCharacteristics.Race.DefenseResponsiveness) * 5.0f);

		case EPickupType::HomingMissile:
			// Ensure the vehicle is good for launching missiles by waiting a little.
			return 0.25f;

		case EPickupType::GatlingGun:
			// Ensure the gun is aligned non-transiently by waiting a little.
			return 1.0f;

		default:
			return 0.0f;
		}
	}

	return 0.0f;
}

#pragma endregion PickupPads
