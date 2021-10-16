/**
*
* Speed pad implementation.
*
* Original author: Nicky van de Groep.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Speed pads push the vehicles forwards at higher speed when they traverse over
* them. They inherit from the IAttractableInterface interface and so the AI bot
* code will automatically detect them and head towards them where appropriate.
*
***********************************************************************************/

#include "pickups/speedpad.h"
#include "vehicle/flippablevehicle.h"
#include "gamemodes/playgamemode.h"

/**
* Sets default values for this actor's properties.
***********************************************************************************/

ASpeedPad::ASpeedPad()
{
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));

	SetRootComponent(CollisionBox);

	CollisionBox->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	CollisionBox->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	GRIP_ATTACH(PadMesh, RootComponent, NAME_None);

	CollectedAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("CollectedSound"));
	GRIP_ATTACH(CollectedAudio, RootComponent, NAME_None);

	CollectedEffect = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("CollectedEffect"));

	CollectedEffect->bAutoDestroy = false;
	CollectedEffect->bAutoActivate = false;
	CollectedEffect->SetHiddenInGame(true);

	GRIP_ATTACH(CollectedEffect, RootComponent, NAME_None);

	CollectedEffect->SetWorldScale3D(FVector::OneVector);
	CollectedEffect->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
}

#pragma region SpeedPads

/**
* Do some post initialization just before the game is ready to play.
***********************************************************************************/

void ASpeedPad::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Setup the collected effect.

	CollectedEffect->SetTemplate(CollectedVisual);

	// Setup all the data required by the attractable interface.

	FRotator rotation = GetActorRotation();

	AttractionLocation = GetActorLocation() + rotation.RotateVector(FVector(0.0f, 0.0f, 100.0f));
	AttractionDirection = rotation.RotateVector(FVector(-1.0f, 0.0f, 0.0f));
	AttractionDistanceRangeCms = FMathEx::MetersToCentimeters(AttractionDistanceRange);

	// Calculate the direction vector that is used to send the vehicles in the correct direction.

	FacingDirection = AttractionDirection * -1.0f;

	APlayGameMode* gameMode = APlayGameMode::Get(this);

	if (gameMode != nullptr)
	{
		GRIP_ADD_TO_GAME_MODE_LIST_FROM(SpeedPads, gameMode);

		gameMode->AddAttractable(this);
	}

	// Fix for bad data in some levels.

	CollisionBox->SetCollisionObjectType(ECC_WorldStatic);
}

/**
* Do some shutdown when the actor is being destroyed.
***********************************************************************************/

void ASpeedPad::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	APlayGameMode* gameMode = APlayGameMode::Get(this);

	if (gameMode != nullptr)
	{
		GRIP_REMOVE_FROM_GAME_MODE_LIST_FROM(SpeedPads, gameMode);

		gameMode->RemoveAttractable(this);
	}

	Super::EndPlay(endPlayReason);
}

/**
* Event for when the speed pad is collected by a vehicle.
***********************************************************************************/

void ASpeedPad::OnSpeedPadCollected(ABaseVehicle* vehicle)
{
	// Scale the boost from the speed pad by the vehicle's direction alignment
	// with that of the speed pad - the more parallel the more boost given.

	FVector vehicleFacing = vehicle->GetVelocityOrFacingDirection();
	float degrees = FMathEx::DotProductToDegrees(FVector::DotProduct(vehicleFacing, FacingDirection));
	float scale = 1.0f - FMathEx::GetRatio(degrees, 30.0f, 45.0f);

	if (scale > KINDA_SMALL_NUMBER)
	{
		// If we have any boost from the alignment, then try to apply it to the vehicle.

		if (vehicle->SpeedBoost(this, Power * scale, Duration, FacingDirection) == true)
		{
			// If the vehicle says this speed pad is OK then play the audio and visual effects
			// for collecting a speed pad.

			if (CollectedAudio != nullptr)
			{
				CollectedAudio->SetSound(vehicle->IsHumanPlayer() ? CollectedSoundPlayer : CollectedSoundNonPlayer);
				CollectedAudio->Play();
			}

			if (CollectedEffect != nullptr)
			{
				CollectedEffect->Activate(true);
				CollectedEffect->SetHiddenInGame(false);
			}
		}
	}
}

#pragma endregion SpeedPads
