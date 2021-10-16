/**
*
* Static track camera implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Track cameras are placed around the track to show views of the race after it has
* finished when in cinematic camera mode, or when in attract mode from the main
* menu.
*
***********************************************************************************/

#include "camera/statictrackcamera.h"
#include "ai/pursuitsplineactor.h"
#include "system/worldfilter.h"
#include "vehicle/flippablevehicle.h"
#include "gamemodes/playgamemode.h"

/**
* Construct a AStaticTrackCamera.
***********************************************************************************/

AStaticTrackCamera::AStaticTrackCamera()
{
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->bConstrainAspectRatio = false;
	Camera->AspectRatio = 1.777778f;
	Camera->PostProcessBlendWeight = 1.0f;
	Camera->SetFieldOfView(30.0f);
	Camera->SetMobility(EComponentMobility::Static);

	SetRootComponent(Camera);

#pragma region CameraCinematics

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetBoxExtent(FVector(25.0f, 25.0f, 25.0f));
	CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &AStaticTrackCamera::OnVehicleHit);

	CollisionBox->SetCollisionProfileName("StaticCamera");
	CollisionBox->SetSimulatePhysics(false);
	CollisionBox->SetGenerateOverlapEvents(true);

	CollisionBox->ShapeColor = FColor::White;
	CollisionBox->SetHiddenInGame(true);
	CollisionBox->SetMobility(EComponentMobility::Static);

	GRIP_ATTACH(CollisionBox, Camera, NAME_None);

	{
		static ConstructorHelpers::FObjectFinder<USoundCue> asset(TEXT("SoundCue'/Game/Audio/Sounds/Impacts/A_ImpactMetalAudio_Cue.A_ImpactMetalAudio_Cue'"));
		ImpactSound = asset.Object;
	}

#pragma endregion CameraCinematics

}

/**
* Respond to a vehicle hitting the camera, often by throwing it off its mount and
* onto the track.
***********************************************************************************/

void AStaticTrackCamera::OnVehicleHit(class UPrimitiveComponent* hitComponent, class AActor* otherActor, class UPrimitiveComponent* otherComponent, int32 otherBodyIndex, bool fromSweep, const FHitResult& sweepResult)
{

#pragma region CameraCinematics

	if (otherActor != nullptr &&
		CameraHit == false &&
		Indestructible == false)
	{
		ABaseVehicle* vehicle = Cast<ABaseVehicle>(otherActor);
		bool valid = vehicle == nullptr;

		if (vehicle != nullptr)
		{
			valid = (FVector::DotProduct(vehicle->GetVelocityOrFacingDirection(), GetActorTransform().GetUnitAxis(EAxis::X)) < 0.0f);

			CameraHitVelocity = vehicle->GetVelocity() * 0.5f;
		}

		if (valid == true)
		{
			CameraHit = true;
			ImpactLocation = otherActor->GetActorLocation();
			AdjustedYaw = FMath::FRandRange(10.0f, 20.0f) * ((FMath::Rand() & 1) ? 1.0f : -1.0f);
		}
	}

#pragma endregion CameraCinematics

}

#pragma region CameraCinematics

/**
* Do some initialization when the game is ready to play.
***********************************************************************************/

void AStaticTrackCamera::BeginPlay()
{
	APlayGameMode* gameMode = APlayGameMode::Get(this);

	if (gameMode != nullptr &&
		GRIP_POINTER_VALID(gameMode->MasterRacingSpline) == true)
	{
		HookupDelay = FMath::Min(HookupDelay, 2.0f);

		int32 numIterations = 5;
		FVector actorLocation = GetActorLocation();

		DistanceAlongMasterRacingSpline = gameMode->MasterRacingSpline->GetNearestDistance(actorLocation, 0.0f, 0.0f, numIterations, 100);

		float distanceAway = 0.0f;
		float distanceAlong = 0.0f;
		UAdvancedSplineComponent* nearestSpline = nullptr;

		AAdvancedSplineActor::FindNearestSpline(actorLocation, GetWorld(), nearestSpline, distanceAway, distanceAlong);

		if (nearestSpline != nullptr)
		{
			UWorld* world = GetWorld();
			UGlobalGameState* gameState = UGlobalGameState::GetGlobalGameState(world);
			FVector direction = nearestSpline->GetWorldDirectionAtDistanceAlongSpline(FMath::Clamp(distanceAlong, 1.0f, nearestSpline->GetSplineLength() - 1.0f));
			FVector location = nearestSpline->GetLocationAtDistanceAlongSpline(distanceAlong, ESplineCoordinateSpace::World);
			float difference = FVector::DotProduct(GetActorRotation().Vector(), direction);
			float baseDifference = (actorLocation - location).Size();

			AngleVsTrack = FMathEx::DotProductToDegrees(difference);

			if (LinkToClosestPursuitSpline == true)
			{
				LinkedPursuitSpline = Cast<UPursuitSplineComponent>(nearestSpline);
			}

			for (TActorIterator<APursuitSplineActor> actorItr(world); actorItr; ++actorItr)
			{
				if (FWorldFilter::IsValid(*actorItr, gameState) == true)
				{
					TArray<UActorComponent*> splines;

					(*actorItr)->GetComponents(UPursuitSplineComponent::StaticClass(), splines);

					for (UActorComponent* component : splines)
					{
						UPursuitSplineComponent* splineComponent = Cast<UPursuitSplineComponent>(component);
						float distance = splineComponent->GetNearestDistance(actorLocation, 0.0f, 0.0f, numIterations, 100);
						FVector location1 = splineComponent->GetLocationAtDistanceAlongSpline(distance, ESplineCoordinateSpace::World);

						difference = (location1 - location).Size();

						if (difference < 50.0f * 100.0f ||
							difference < baseDifference * 2.0f)
						{
							LinkedPursuitSplines.Emplace(splineComponent);
						}
					}
				}
			}
		}
	}
}

/**
* Do some shutdown when the actor is being destroyed.
***********************************************************************************/

void AStaticTrackCamera::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	Super::EndPlay(endPlayReason);

	GRIP_REMOVE_FROM_GAME_MODE_LIST(TrackCameras);
}

/**
* Has this static camera just been hit by a vehicle?
***********************************************************************************/

bool AStaticTrackCamera::HasCameraJustBeenHit()
{
	bool result = CameraHit == true && CameraHitReported == false;

	CameraHitReported = CameraHit;

	if (result == true)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, ImpactLocation, 3.5f);
	}

	return result;
}

#pragma endregion CameraCinematics
