/**
*
* Play game mode implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* The play game mode to use for the game, specifically for playing a level and
* is the C++ game mode used in GRIP, with a blueprint wrapping it for actual use.
*
***********************************************************************************/

#include "gamemodes/playgamemode.h"
#include "ai/pursuitsplineactor.h"
#include "vehicle/basevehicle.h"
#include "game/globalgamestate.h"
#include "system/worldfilter.h"
#include "blueprint/userwidget.h"
#include "components/panelwidget.h"
#include "components/textblock.h"
#include "blueprint/widgetblueprintlibrary.h"
#include "blueprint/widgetlayoutlibrary.h"
#include "components/canvaspanelslot.h"
#include "components/image.h"
#include "camera/statictrackcamera.h"
#include "ui/hudwidget.h"

/**
* APlayGameMode statics.
***********************************************************************************/

// The type of widget to use for the single screen UI.
TSubclassOf<USingleHUDWidget> APlayGameMode::SingleScreenWidgetClass = nullptr;

/**
* Construct a play game mode.
***********************************************************************************/

APlayGameMode::APlayGameMode()
{

#pragma region VehicleHUD

	{
		static ConstructorHelpers::FObjectFinder<UClass> asset(TEXT("'/Game/UserInterface/HUD/WBP_SingleHUDWidget.WBP_SingleHUDWidget_C'"));
		SingleScreenWidgetClass = (UClass*)asset.Object;
	}

#pragma endregion VehicleHUD

	// We need all of the players to be ticked before the game state so that we can
	// calculate race position effectively.

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	// Ensure that random is random.

	FMath::RandInit((int32)FDateTime::Now().ToUnixTimestamp() + (uint64)(this));

#pragma region VehiclePickups

	while (NumPickupTypes.Num() < (int32)EPickupType::Num)
	{
		NumPickupTypes.Emplace(0);
		LastUsedPickupTypes.Emplace(0.0f);
	}

#pragma endregion VehiclePickups

}

/**
* Get the vehicle for a vehicle index.
***********************************************************************************/

ABaseVehicle* APlayGameMode::GetVehicleForVehicleIndex(int32 vehicleIndex) const
{
	if (vehicleIndex >= 0)
	{
		for (ABaseVehicle* vehicle : Vehicles)
		{
			if (vehicle->VehicleIndex == vehicleIndex)
			{
				return vehicle;
			}
		}
	}

	return nullptr;
}

/**
* Do some post initialization just before the game is ready to play.
***********************************************************************************/

void APlayGameMode::PostInitializeComponents()
{
	UE_LOG(GripLog, Log, TEXT("APlayGameMode::PostInitializeComponents"));

	Super::PostInitializeComponents();

#if UE_BUILD_SHIPPING
	if (GameStateOverrides != nullptr)
	{
		GameStateOverrides->OverrideGrid = false;
	}
#endif // UE_BUILD_SHIPPING

#if !WITH_EDITOR
	HUDClass = nullptr;
#endif // !WITH_EDITOR

	if (GetWorld() != nullptr && GetWorld()->GetGameViewport() != nullptr)
	{
		GetWorld()->GetGameViewport()->SetForceDisableSplitscreen(false);
	}

	// Iterate through the navigation layers present in the level and record their names.

	TArray<FName> navigationLayers;

	for (TActorIterator<APursuitSplineActor> actorItr0(GetWorld()); actorItr0; ++actorItr0)
	{
		for (const FName& layer : (*actorItr0)->Layers)
		{
			if (layer.ToString().EndsWith("Navigation") == true)
			{
				if (navigationLayers.Contains(layer) == false)
				{
					navigationLayers.Emplace(layer);
				}
			}
		}
	}

	if (GlobalGameState != nullptr)
	{
		const bool inTransition = IsValid(GetWorld()) && GetWorld()->IsInSeamlessTravel();

		// Pick a valid navigation layer name to use.

		if (navigationLayers.Contains(FName(*GlobalGameState->TransientGameState.NavigationLayer)) == false && inTransition == false)
		{
			GlobalGameState->TransientGameState.NavigationLayer = TEXT("");

			if (navigationLayers.Num() > 0)
			{
				if (navigationLayers.Contains("ForwardNavigation") == true)
				{
					GlobalGameState->TransientGameState.NavigationLayer = "ForwardNavigation";
				}
				else
				{
					GlobalGameState->TransientGameState.NavigationLayer = navigationLayers[0].ToString();
				}
			}
		}

		// Now iterate the actors, destroying those that are not compatible with the
		// current navigation layer.

		for (TActorIterator<AActor> actorItr(GetWorld()); actorItr; ++actorItr)
		{
			FWorldFilter::IsValid(*actorItr, GlobalGameState);
		}

#pragma region VehicleSurfaceEffects

		// Find the driving surface properties for this level and store them away in the game play setup.

		for (TActorIterator<ADrivingSurfaceProperties> actorItr(GetWorld()); actorItr; ++actorItr)
		{
			if (FWorldFilter::IsValid(*actorItr, GlobalGameState) == true)
			{
				ADrivingSurfaceProperties* properties = *actorItr;

				GlobalGameState->TransientGameState.MapBrightness = properties->AmbientBrightness;
				GlobalGameState->TransientGameState.MapSurfaceColor = FVector(properties->SurfaceColor.R, properties->SurfaceColor.G, properties->SurfaceColor.B);
				GlobalGameState->TransientGameState.MapLightingColor = FVector(properties->LightColor.R, properties->LightColor.G, properties->LightColor.B);

				break;
			}
		}

#pragma endregion VehicleSurfaceEffects

	}
}

/**
* Calculate the maximum number of players.
***********************************************************************************/

int32 APlayGameMode::CalculateMaxPlayers() const
{
	int32 maxPlayers = FMath::Min(GlobalGameState->GeneralOptions.NumberOfPlayers, Startpoints.Num());

	if (GameStateOverrides != nullptr &&
		GameStateOverrides->OverrideGrid == true)
	{
		maxPlayers = FMath::Min(maxPlayers, GameStateOverrides->Grid.Num() + GlobalGameState->TransientGameState.NumberOfLocalPlayers);
		maxPlayers = FMath::Min(maxPlayers, Startpoints.Num());
	}

#if WITH_EDITOR

	// If we're not starting on the normal start line, then only create one player.

	for (TActorIterator<APlayerStartPIE> itr(GetWorld()); itr; ++itr)
	{
		maxPlayers = FMath::Min(maxPlayers, 1);
		break;
	}

#endif // WITH_EDITOR

	return maxPlayers;
}

/**
* Do some initialization when the game is ready to play.
***********************************************************************************/

void APlayGameMode::BeginPlay()
{
	UE_LOG(GripLog, Log, TEXT("APlayGameMode::BeginPlay"));

	Super::BeginPlay();

	// Create a new single screen widget and add it to the viewport. This is what will
	// contain all of the HUDs for each player - there is more than one in split-screen
	// games. It ordinarily contains the pause menu and other full-screen elements too,
	// but are missing from this stripped implementation.

	if (SingleScreenWidgetClass != nullptr)
	{
		SingleScreenWidget = NewObject<USingleHUDWidget>(this, SingleScreenWidgetClass);

		if (SingleScreenWidget != nullptr)
		{
			SingleScreenWidget->AddToViewport(1);
		}
	}

	StartLineDropTime = 6;
	StartLineCountFrom = StartLineDropTime;
	StartLineCountTo = StartLineCountFrom + 3;

	UWorld* world = GetWorld();

	for (TActorIterator<AActor> actorItr(world); actorItr; ++actorItr)
	{
#if GRIP_FIX_REVERB_FADE_TIMES
		if (FWorldFilter::IsValid(*actorItr, GlobalGameState) == true)
		{
			if ((*actorItr)->IsA<AAudioVolume>() == true)
			{
				// Hack to stop glitching audio by setting the reverb fade time to zero.

				AAudioVolume* volume = Cast<AAudioVolume>(*actorItr);

				if (volume != nullptr)
				{
					FReverbSettings settings = volume->GetReverbSettings();
					settings.FadeTime = 0.0f;
					volume->SetReverbSettings(settings);
				}
			}
		}
#else // GRIP_FIX_REVERB_FADE_TIMES
		FWorldFilter::IsValid(*actorItr, GlobalGameState);
#endif // GRIP_FIX_REVERB_FADE_TIMES
	}

	ChangeTimeDilation(1.0f, 0.0f);

	// Setup some good defaults for the game setup if not already set.

	if (GlobalGameState->GamePlaySetup.GameType == EGameType::SinglePlayerEvent)
	{
		GlobalGameState->GamePlaySetup.DrivingMode = EDrivingMode::Race;
	}

	if (GlobalGameState->GamePlaySetup.DrivingMode == EDrivingMode::None)
	{
		GlobalGameState->GamePlaySetup.DrivingMode = EDrivingMode::Race;
	}

	if (GlobalGameState->GeneralOptions.NumberOfLaps == 0)
	{
		GlobalGameState->GeneralOptions.NumberOfLaps = 4;
	}

#pragma region NavigationSplines

	// Record all of the pursuit splines in the level.

	DeterminePursuitSplines();

#pragma endregion NavigationSplines

#pragma region VehicleRaceDistance

	// Record all of the checkpoints in the level.

	Checkpoints.Empty();

	for (TActorIterator<ATrackCheckpoint> actorItr(world); actorItr; ++actorItr)
	{
		if (FWorldFilter::IsValid(*actorItr, GlobalGameState) == true)
		{
			Checkpoints.Emplace(*actorItr);
		}
	}

#pragma endregion VehicleRaceDistance

	// Find a master racing spline against which we can measure race distance.

	if (MasterRacingSpline.IsValid() == false)
	{
		MasterRacingSpline = DetermineMasterRacingSpline(FName(*GlobalGameState->TransientGameState.NavigationLayer), world, GlobalGameState);
	}

	// Now determine the length of that master racing spline.

	if (MasterRacingSpline.IsValid() == true)
	{
		MasterRacingSplineLength = MasterRacingSpline->GetSplineLength();
	}

	// Do some conditioning on all the pursuit splines so that we have accurate data
	// to work with, especially regarding race distance.

	BuildPursuitSplines(false, FName(*GlobalGameState->TransientGameState.NavigationLayer), world, GlobalGameState, MasterRacingSpline.Get());
	EstablishPursuitSplineLinks(false, FName(*GlobalGameState->TransientGameState.NavigationLayer), world, GlobalGameState, MasterRacingSpline.Get());

#pragma region VehicleRaceDistance

	// Link each of the checkpoints to the master racing spline.

	int32 numCheckpoints = Checkpoints.Num();

	if (numCheckpoints > 0)
	{
		Checkpoints.StableSort([] (const ATrackCheckpoint& object1, const ATrackCheckpoint& object2)
			{
				return object1.Order < object2.Order;
			});

		// Ensure that all of the start points are behind the first track checkpoint.

		for (APlayerStart* startPoint : Startpoints)
		{
			if (startPoint->IsA<APlayerStartPIE>() == true)
			{
				UnknownPlayerStart = true;
			}
			else
			{
				ensureAlways(FVector::DotProduct(Checkpoints[0]->GetActorRotation().Vector(), startPoint->GetActorLocation() - Checkpoints[0]->GetActorLocation()) > 0.0f);
			}
		}

		if (MasterRacingSpline != nullptr)
		{
			for (int32 i = 0; i < numCheckpoints; i++)
			{
				Checkpoints[i]->DistanceAlongMasterRacingSpline = MasterRacingSpline->GetNearestDistance(Checkpoints[i]->GetActorLocation(), 0.0f, 0.0f, 10, 50);
			}

			MasterRacingSplineStartDistance = Checkpoints[0]->DistanceAlongMasterRacingSpline;
		}
	}

#pragma endregion VehicleRaceDistance

	int32 index = 0;

	Vehicles.Empty();

	// Setup all the vehicles that have already been created in the menu UI
	// (all local players normally).

	for (TActorIterator<ABaseVehicle> actorItr(world); actorItr; ++actorItr)
	{
		ABaseVehicle* vehicle = *actorItr;

		if (Vehicles.Num() == 0)
		{
			ViewingPawn = vehicle;
		}

		vehicle->PostSpawn(index++, true, false);

#pragma region CameraCinematics

		if (vehicle->IsHumanPlayer() == true)
		{
			APlayerController* controller = Cast<APlayerController>(vehicle->GetController());

			if (controller != nullptr)
			{
				controller->PlayerCameraManager->SetManualCameraFade(1.0f, FLinearColor(0.0f, 0.0f, 0.0f), true);
			}
		}

#pragma endregion CameraCinematics

	}

#pragma region AIVehicleControl

	// Now setup all the remaining bot vehicles.

	TArray<int32> bots;

	int32 maxPlayers = CalculateMaxPlayers();
	int32 startIndex = Vehicles.Num();

	for (int32 i = 0; i < maxPlayers - startIndex; i++)
	{
		bots.Emplace(-1);
	}

	// OK, so we have a list of bots that is relevant to the current game setup.

	for (int32 i = Vehicles.Num(); i < maxPlayers; i++)
	{
		APlayerStart* startPoint = Cast<APlayerStart>(ChoosePlayerStartProperly(nullptr, maxPlayers));

		if (startPoint != nullptr)
		{
			FRotator rotation = startPoint->GetActorRotation();
			FVector offset = rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f));
			FVector location = startPoint->GetActorLocation() + offset;
			TSubclassOf<ABaseVehicle> vehicleBlueprint;

			// Right now we can only use what it specified in the play game mode blueprint in terms
			// of which bot vehicles to create. Normally there would be a sophisticated system in
			// place for assigning appropriate bot characters through game progression.

			if (GameStateOverrides != nullptr &&
				GameStateOverrides->OverrideGrid == true)
			{
				if (GameStateOverrides->Grid.Num() > i - startIndex)
				{
					vehicleBlueprint = GameStateOverrides->Grid[i - startIndex];
				}
			}
			else
			{
				// If the grid is not overridden, then use the blueprint that the player is using.

				vehicleBlueprint = Vehicles[0]->GetClass();
			}

			ABaseVehicle* vehicle = Cast<ABaseVehicle>(UGameplayStatics::BeginDeferredActorSpawnFromClass(this, vehicleBlueprint, FTransform(rotation, location), ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn));

			if (vehicle != nullptr)
			{
				vehicle->PostSpawn(index++, false, true);

				UGameplayStatics::FinishSpawningActor(vehicle, FTransform(rotation, location));
			}
		}
	}

	// Now setup the AI bots for their revving and burnouts on the start line.

	for (ABaseVehicle* vehicle : Vehicles)
	{
		if (vehicle->IsAIVehicle() == true)
		{
			if (vehicle->Antigravity == false)
			{
				vehicle->GetAI().WillRevOnStartLine = FMath::FRand() <= 0.5f;
			}
		}
	}

#pragma endregion AIVehicleControl

#pragma region VehicleHUD

	if (GlobalGameState->IsGameModeRace() == true)
	{
		for (ABaseVehicle* vehicle : Vehicles)
		{
			URaceCameraComponent* camera = vehicle->Camera;

			camera->SmoothLocation = false;
			camera->SmoothRotation = false;

			camera->SwitchLocationToCustomControl();
		}
	}

#pragma endregion VehicleHUD

	GameSequence = EGameSequence::Initialise;

	// Record all of the frictional actors in the level.

	for (TActorIterator<AActor> actorItr(world); actorItr; ++actorItr)
	{
		AActor* actor = *actorItr;

		if (actor->GetClass()->GetName().StartsWith("StartingGateBP") == true)
		{
			FrictionalActors.Emplace(actor);
		}
		else if (FWorldFilter::IsValid(actor, GlobalGameState) == true)
		{
			for (const FName& layer : actor->Layers)
			{
				if (layer == TEXT("LimitVehicleLaunching"))
				{
					FrictionalActors.Emplace(actor);
					break;
				}
			}
		}
	}

#pragma region CameraCinematics

	// Record all of the track cameras in the level.

	for (TActorIterator<AStaticTrackCamera> actorItr(world); actorItr; ++actorItr)
	{
		if (FWorldFilter::IsValid(*actorItr, GlobalGameState) == true)
		{
			TrackCameras.Emplace(*actorItr);
		}
	}

#pragma endregion CameraCinematics

	LastOptionsResetTime = GetClock();
}

/**
* Do some shutdown when the actor is being destroyed.
***********************************************************************************/

void APlayGameMode::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	UE_LOG(GripLog, Log, TEXT("APlayGameMode::EndPlay"));

	if (SingleScreenWidget != nullptr)
	{
		SingleScreenWidget->RemoveFromViewport();
		SingleScreenWidget = nullptr;
	}

	// Ensure time dilation is switched off here.

	ChangeTimeDilation(1.0f, 0.0f);

	Super::EndPlay(endPlayReason);
}

/**
* Determine the vehicles that are currently present in the level.
***********************************************************************************/

void APlayGameMode::DetermineVehicles()
{
	Vehicles.Empty();

	for (TActorIterator<ABaseVehicle> actorItr(GetWorld()); actorItr; ++actorItr)
	{
		Vehicles.Emplace(*actorItr);
	}

	// Sort the vehicles by vehicle index, not strictly necessary, but this could
	// help to avoid bugs when referencing vehicles later.

	Vehicles.Sort([] (const ABaseVehicle& object1, const ABaseVehicle& object2)
		{
			return object1.GetVehicleIndex() < object2.GetVehicleIndex();
		});
}

/**
* Determine the pursuit splines that are currently present in the level.
***********************************************************************************/

void APlayGameMode::DeterminePursuitSplines()
{
	PursuitSplines.Empty();

	for (TActorIterator<APursuitSplineActor> actorItr(GetWorld()); actorItr; ++actorItr)
	{
		if (FWorldFilter::IsValid(*actorItr, GlobalGameState) == true)
		{
			PursuitSplines.Emplace(*actorItr);
		}
	}
}

/**
* Determine the master racing spline.
***********************************************************************************/

UPursuitSplineComponent* APlayGameMode::DetermineMasterRacingSpline(const FName& navigationLayer, UWorld* world, UGlobalGameState* gameState)
{
	// Go through every spline in the world to find a master or master racing spline.

	for (TActorIterator<APursuitSplineActor> actorItr0(world); actorItr0; ++actorItr0)
	{
		if ((gameState != nullptr && FWorldFilter::IsValid(*actorItr0, gameState) == true) ||
			(gameState == nullptr && FWorldFilter::IsValid(*actorItr0, navigationLayer) == true))
		{
			TArray<UActorComponent*> splines;

			(*actorItr0)->GetComponents(UPursuitSplineComponent::StaticClass(), splines);

			for (UActorComponent* component : splines)
			{
				UPursuitSplineComponent* spline = Cast<UPursuitSplineComponent>(component);

				if (spline->GetNumberOfSplinePoints() > 1)
				{
					if (spline->IsClosedLoop() == true)
					{
						// The first looped spline becomes the master racing spline.
						// There should only ever be one looped spline on a track (for each navigation layer).

						return spline;
					}
				}
			}
		}
	}

	return nullptr;
}

/**
* Build all of the pursuit splines.
***********************************************************************************/

void APlayGameMode::BuildPursuitSplines(bool check, const FName& navigationLayer, UWorld* world, UGlobalGameState* gameState, UPursuitSplineComponent* masterRacingSpline)
{

#pragma region NavigationSplines

	if (check == false)
	{
		UE_LOG(GripLog, Log, TEXT("APlayGameMode::BuildPursuitSplines"));
	}

	// Build all of the pursuit splines.

	for (TActorIterator<APursuitSplineActor> actorItr(world); actorItr; ++actorItr)
	{
		if ((gameState != nullptr && FWorldFilter::IsValid(*actorItr, gameState) == true) ||
			(gameState == nullptr && FWorldFilter::IsValid(*actorItr, navigationLayer) == true))
		{
			TArray<UActorComponent*> splines;

			(*actorItr)->GetComponents(UPursuitSplineComponent::StaticClass(), splines);

			for (UActorComponent* component : splines)
			{
				UPursuitSplineComponent* spline = Cast<UPursuitSplineComponent>(component);

				if (check == false)
				{
					spline->Build(false, false, false);
				}
			}
		}
	}

#pragma endregion NavigationSplines

}

/**
* Establish all of the links between pursuit splines.
***********************************************************************************/

void APlayGameMode::EstablishPursuitSplineLinks(bool check, const FName& navigationLayer, UWorld* world, UGlobalGameState* gameState, UPursuitSplineComponent* masterRacingSpline)
{

#pragma region NavigationSplines

	TArray<APursuitSplineActor*> validSplines;

	// Go through every spline in the world to find a master or master racing spline while also
	// building a list of valid splines.

	for (TActorIterator<APursuitSplineActor> actorItr0(world); actorItr0; ++actorItr0)
	{
		if ((gameState != nullptr && FWorldFilter::IsValid(*actorItr0, gameState) == true) ||
			(gameState == nullptr && FWorldFilter::IsValid(*actorItr0, navigationLayer) == true))
		{
			bool useSpline = false;
			TArray<UActorComponent*> splines;

			(*actorItr0)->GetComponents(UPursuitSplineComponent::StaticClass(), splines);

			for (UActorComponent* component : splines)
			{
				UPursuitSplineComponent* spline = Cast<UPursuitSplineComponent>(component);

				spline->ClearSplineLinks();

				if (spline->GetNumberOfSplinePoints() > 1)
				{
					useSpline = true;
				}
			}

			if (useSpline == true)
			{
				validSplines.Emplace(*actorItr0);
			}
		}
	}

	validSplines.StableSort([] (const APursuitSplineActor& object1, const APursuitSplineActor& object2)
		{
			return object1.GetName() < object2.GetName();
		});

	// Now go through every spline in the world and establish their links.

	for (APursuitSplineActor* validSpline0 : validSplines)
	{
		TArray<UActorComponent*> splines;

		validSpline0->GetComponents(UPursuitSplineComponent::StaticClass(), splines);

		for (UActorComponent* component : splines)
		{
			UPursuitSplineComponent* spline = Cast<UPursuitSplineComponent>(component);

			for (APursuitSplineActor* validSpline1 : validSplines)
			{
				validSpline1->EstablishPursuitSplineLinks(spline);
			}
		}
	}

	if (check == true)
	{
		for (APursuitSplineActor* validSpline0 : validSplines)
		{
			TArray<UActorComponent*> splines;

			validSpline0->GetComponents(UPursuitSplineComponent::StaticClass(), splines);

			for (UActorComponent* component : splines)
			{
				UPursuitSplineComponent* spline = Cast<UPursuitSplineComponent>(component);

				if (spline->DeadStart == true)
				{
					UE_LOG(GripLogPursuitSplines, Warning, TEXT("Pursuit spline %s is a dead-start"), *spline->ActorName);
				}

				if (spline->DeadEnd == true)
				{
					UE_LOG(GripLogPursuitSplines, Warning, TEXT("Pursuit spline %s is a dead-end"), *spline->ActorName);
				}

				if (spline->SplineLinks.Num() == 0)
				{
					UE_LOG(GripLogPursuitSplines, Warning, TEXT("Pursuit spline %s has no links"), *spline->ActorName);
				}
				else
				{
					UE_LOG(GripLogPursuitSplines, Log, TEXT("Pursuit spline %s of length %d has the following links:"), *spline->ActorName, (int32)spline->GetSplineLength());
				}

				for (FSplineLink& spline2 : spline->SplineLinks)
				{
					UE_LOG(GripLogPursuitSplines, Log, TEXT("  %s (%s) %d on this, %d on next"), *spline2.Spline->ActorName, ((spline2.ForwardLink == true) ? TEXT("forward link") : TEXT("passive link")), (int32)spline2.ThisDistance, (int32)spline2.NextDistance);
				}
			}
		}
	}

	float minDistance = 10.0f * 100.0f;

	for (APursuitSplineActor* validSpline0 : validSplines)
	{
		TArray<UActorComponent*> splines;

		validSpline0->GetComponents(UPursuitSplineComponent::StaticClass(), splines);

		for (UActorComponent* component : splines)
		{
			UPursuitSplineComponent* spline = Cast<UPursuitSplineComponent>(component);

			// So now the spline is fully loaded with all the splines linked to it. We now need to go
			// through and aggregate the links into branch points where a decision needs to be made
			// by the AI driver as to which path to take.

			TArray<FSplineLink> links;

			for (FSplineLink link : spline->SplineLinks)
			{
				if (link.LinkIsRouteChoice() == true)
				{
					links.Emplace(link);
				}
			}

			spline->RouteChoices.Empty();

			while (links.Num() > 0)
			{
				FRouteChoice choice;
				FSplineLink link = links[0];

				choice.DecisionDistance = link.ThisDistance;
				choice.SplineLinks.Emplace(link);

				links.RemoveAt(0);

				for (int32 i = 0; i < links.Num(); i++)
				{
					FSplineLink nextLink = links[i];

					if (FMath::Abs(nextLink.ThisDistance - link.ThisDistance) < minDistance)
					{
						choice.SplineLinks.Emplace(nextLink);
						choice.DecisionDistance = FMath::Min(choice.DecisionDistance, nextLink.ThisDistance);
						links.RemoveAt(i--);
					}
				}

				// We don't want to make a route change the moment you get onto a new spline as this
				// is probably just in the positioning CEP that we use and was probably already part
				// of the route choice to get onto this spline from the previous spline.

				if (choice.DecisionDistance > minDistance)
				{
					spline->RouteChoices.Emplace(choice);
				}
			}
		}
	}

	// Go through every spline in the world and compute the extended point data.

	if (masterRacingSpline != nullptr)
	{
		// Calculate the master racing spline distances by branching forwards from the master racing spline
		// onto all of it's connected splines.

		float masterRacingSplineLength = masterRacingSpline->GetSplineLength();

		masterRacingSpline->CalculateMasterSplineDistances(masterRacingSpline, masterRacingSplineLength, 0.0f, 0, check);

		// Now go through every spline in the world and check that we've master spline distances.

		for (APursuitSplineActor* validSpline0 : validSplines)
		{
			TArray<UActorComponent*> splineComponents;

			validSpline0->GetComponents(UPursuitSplineComponent::StaticClass(), splineComponents);

			for (UActorComponent* component : splineComponents)
			{
				UPursuitSplineComponent* splineComponent = Cast<UPursuitSplineComponent>(component);

				// If this is a closed spline but hasn't any master spline distances then calculate them now.
				// This will also calculate distances for any branches extending from the closed splines.

				if (splineComponent->IsClosedLoop() == true &&
					splineComponent->HasMasterSplineDistances() == false)
				{
					float distance = masterRacingSpline->GetNearestDistance(splineComponent->GetWorldLocationAtDistanceAlongSpline(0.0f));

					splineComponent->CalculateMasterSplineDistances(masterRacingSpline, masterRacingSplineLength, distance, 0, check);
				}
			}
		}

		for (int32 degreesOfSeparation = 0; degreesOfSeparation < 4; degreesOfSeparation++)
		{
			// degreesOfSeparation
			// 0 = directly connected at both ends
			// 1 = directly connected at least one end, and the other connected through one degree of separation
			// 2 = indirectly connected at both ends through one degree of separation
			// 3 = fall-back computation

			bool result = false;

			do
			{
				result = false;

				for (APursuitSplineActor* validSpline0 : validSplines)
				{
					TArray<UActorComponent*> splineComponents;

					validSpline0->GetComponents(UPursuitSplineComponent::StaticClass(), splineComponents);

					for (UActorComponent* component : splineComponents)
					{
						UPursuitSplineComponent* splineComponent = Cast<UPursuitSplineComponent>(component);

						// If this is not a closed spline but hasn't any master spline distances then calculate them now.
						// This will also calculate distances for any branches extending from the closed splines.

						if (splineComponent->IsClosedLoop() == false &&
							splineComponent->HasMasterSplineDistances() == false)
						{
							float distance = masterRacingSpline->GetNearestDistance(splineComponent->GetWorldLocationAtDistanceAlongSpline(0.0f));

							result |= splineComponent->CalculateMasterSplineDistances(masterRacingSpline, masterRacingSplineLength, distance, degreesOfSeparation, check);
						}
					}
				}
			}
			while (result == true);
		}

		for (APursuitSplineActor* validSpline0 : validSplines)
		{
			TArray<UActorComponent*> splineComponents;

			validSpline0->GetComponents(UPursuitSplineComponent::StaticClass(), splineComponents);

			for (UActorComponent* component : splineComponents)
			{
				UPursuitSplineComponent* splineComponent = Cast<UPursuitSplineComponent>(component);

				if (splineComponent->Type == EPursuitSplineType::General)
				{
					// Check for splines that weren't linked up at all.

					if (check == true)
					{
						if (splineComponent->HasMasterSplineDistances() == false)
						{
							UE_LOG(GripLogPursuitSplines, Warning, TEXT("Pursuit spline %s may not be connected up properly (ignore for non-Race maps)."), *splineComponent->ActorName);

							if (splineComponent->DeadStart == true)
							{
								UE_LOG(GripLogPursuitSplines, Warning, TEXT("It appears to be a dead start."));
							}
							else
							{
								UE_LOG(GripLogPursuitSplines, Warning, TEXT("It doesn't appear to be a dead start."));
							}
						}
					}
				}

				if (splineComponent->HasMasterSplineDistances() == false)
				{
					float distance = masterRacingSpline->GetNearestDistance(splineComponent->GetWorldLocationAtDistanceAlongSpline(0.0f));

					splineComponent->CalculateMasterSplineDistances(masterRacingSpline, masterRacingSplineLength, distance, 3, check);
				}
			}
		}

		int32 attempts = 0;
		bool recalibrated = false;

		do
		{
			recalibrated = false;

			for (APursuitSplineActor* validSpline0 : validSplines)
			{
				TArray<UActorComponent*> splineComponents;

				validSpline0->GetComponents(UPursuitSplineComponent::StaticClass(), splineComponents);

				for (UActorComponent* component : splineComponents)
				{
					UPursuitSplineComponent* splineComponent = Cast<UPursuitSplineComponent>(component);

					recalibrated |= splineComponent->CalculateMasterSplineDistances(masterRacingSpline, masterRacingSplineLength, 0.0f, 2, check, 1, attempts);
				}
			}
		}
		while ((recalibrated == true || attempts == 0) && (attempts++ < 10));
	}

#pragma endregion NavigationSplines

}

/**
* Do the regular update tick, post update work for this actor, guaranteed to execute
* after other regular actor ticks.
***********************************************************************************/

void APlayGameMode::Tick(float deltaSeconds)
{
	float clock = Clock;

	Super::Tick(deltaSeconds);

	FrameTimes.AddValue(GetRealTimeClock(), deltaSeconds);

	if (clock == 0.0f)
	{
		LastOptionsResetTime = clock;

#pragma region CameraCinematics

		for (TActorIterator<ABaseVehicle> actorItr(GetWorld()); actorItr; ++actorItr)
		{
			ABaseVehicle* vehicle = *actorItr;

			if (vehicle->IsHumanPlayer() == true)
			{
				APlayerController* controller = Cast<APlayerController>(vehicle->GetController());

				if (controller != nullptr)
				{
					controller->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 3.0f, FLinearColor(0.0f, 0.0f, 0.0f), true, true);
				}
			}
		}

#pragma endregion CameraCinematics

	}

	// Handle the update of each game sequence by calling the appropriate function.

	switch (GameSequence)
	{
	case EGameSequence::Initialise:
		GameSequence = EGameSequence::Start;

		// We purposefully don't break here to do the Start immediately.

	case EGameSequence::Start:
		UpdateRaceStartLine();
		UpdateRacePositions(deltaSeconds);
		break;

	case EGameSequence::Play:
		UpdateRacePositions(deltaSeconds);
		UpdateUILoading();
		break;

	case EGameSequence::End:
		UpdateRacePositions(deltaSeconds);
		UpdateUILoading();
		break;
	}

#pragma region VehicleAudio

	UpdateVehicleVolumes(deltaSeconds);

#pragma endregion VehicleAudio

}

/**
* Upload the loading of the main UI.
***********************************************************************************/

void APlayGameMode::UpdateUILoading()
{
	if (GameSequence == EGameSequence::End)
	{
		QuitGame();
	}
}

/**
* Restart the game.
***********************************************************************************/

void APlayGameMode::RestartGame()
{
	UE_LOG(GripLog, Log, TEXT("APlayGameMode::RestartGame"));

	Super::RestartGame();
}

/**
* Quit the game.
***********************************************************************************/

void APlayGameMode::QuitGame(bool force)
{
}

/**
* Calculate the race positions for each of the vehicles.
***********************************************************************************/

void APlayGameMode::UpdateRacePositions(float deltaSeconds)
{

#pragma region VehicleRaceDistance

	if (GameFinishedAt == 0.0f &&
		GameSequence == EGameSequence::Play)
	{
		// The game hasn't ended yet and is ostensibly still in play. Mark it as finished by default now
		// and have further code in this function correct it back to unfinished when appropriate.

		GameFinishedAt = GetRealTimeClock();
	}

	if (GameSequence >= EGameSequence::Play)
	{
		CalculateRanksAndScoring();
	}

	// Calculate the mean race distance of the human players in the race.

	int32 i = 0;
	int32 numHumans = 0;
	int32 firstRacePosition = 0;
	float meanHumanDistance = 0.0f;

	TArray<FPlayerRaceState*, TInlineAllocator<16>> raceStates;

	for (ABaseVehicle* vehicle : Vehicles)
	{
		if (vehicle->GetRaceState().PlayerCompletionState < EPlayerCompletionState::Complete)
		{
			raceStates.Emplace(&vehicle->GetRaceState());
		}
		else if (vehicle->GetRaceState().PlayerCompletionState == EPlayerCompletionState::Complete)
		{
			firstRacePosition = FMath::Max(firstRacePosition, vehicle->GetRaceState().RacePosition + 1);
		}

		if (vehicle->IsAIVehicle() == false)
		{
			numHumans++;
			meanHumanDistance += vehicle->GetRaceState().EternalRaceDistance;
		}

		if (GameSequence == EGameSequence::Play &&
			vehicle->GetRaceState().PlayerCompletionState < EPlayerCompletionState::Complete)
		{
			// If the game is still being played and this vehicle hasn't finished yet.

			if (vehicle->IsAIVehicle() == false)
			{
				// If this vehicle is human or we need to wait for all AI bots to finish too,
				// then signal the game as unfinished.

				GameFinishedAt = 0.0f;
			}
		}
	}

	if (numHumans > 0)
	{
		meanHumanDistance /= numHumans;
	}

	// Detect if the race has finished (GameFinishedAt will be non-zero) and switch
	// to the end game sequence if so.

	if (GameFinishedAt != 0.0f &&
		GameSequence == EGameSequence::Play)
	{
		GameSequence = EGameSequence::End;
	}

	if (raceStates.Num() > 0)
	{
		// Calculate the race position for each player.

		raceStates.StableSort([] (const FPlayerRaceState& object1, const FPlayerRaceState& object2)
			{
				if (object1.RaceDistance == object2.RaceDistance) return object1.PlayerVehicle->VehicleIndex < object2.PlayerVehicle->VehicleIndex; else return object1.RaceDistance > object2.RaceDistance;
			});

		for (i = 0; i < raceStates.Num(); i++)
		{
			if ((raceStates[i]->PlayerCompletionState < EPlayerCompletionState::Complete) &&
				(raceStates[i]->RaceDistance != 0.0f || GlobalGameState->GamePlaySetup.DrivingMode == EDrivingMode::Elimination))
			{
				raceStates[i]->RacePosition = FMath::Min(firstRacePosition++, GRIP_MAX_PLAYERS - 1);
			}
		}
	}

	raceStates.Empty(GRIP_MAX_PLAYERS);

	if (GameSequence >= EGameSequence::Play)
	{
		for (ABaseVehicle* vehicle : Vehicles)
		{
			raceStates.Emplace(&vehicle->GetRaceState());
		}

#pragma region VehicleCatchup

		if (raceStates.Num() > 0)
		{
			// Now calculate the auto-catchup assistance.

			raceStates.StableSort([] (const FPlayerRaceState& object1, const FPlayerRaceState& object2)
				{
					return object1.EternalRaceDistance > object2.EternalRaceDistance;
				});

			FVehicleCatchupCharacteristics& characteristics = GetDifficultyCharacteristics().VehicleCatchupCharacteristics;

			// Pick the median race distance for all of the players in the race.

			float median = raceStates[raceStates.Num() >> 1]->EternalRaceDistance;

			if (numHumans == 0)
			{
				meanHumanDistance = median;
			}

			float centerOffset = characteristics.CentreOffset;
			ABaseVehicle* player = GetVehicleForVehicleIndex(0);

			if (GameHasEnded() == true)
			{
				LastLapRatio = FMath::Max(LastLapRatio - (deltaSeconds * 0.1f), 0.0f);
			}
			else
			{
				LastLapRatio = 0.0f;

				if (player != nullptr)
				{
					if (GlobalGameState->IsGameModeLapBased() == true)
					{
						int32 lastLap = GlobalGameState->GeneralOptions.NumberOfLaps - 1;
						float eventProgress = FMath::Min(player->GetRaceState().RaceDistance / (MasterRacingSplineLength * GlobalGameState->GeneralOptions.NumberOfLaps), 1.0f);

						if ((eventProgress * GlobalGameState->GeneralOptions.NumberOfLaps) > lastLap)
						{
							LastLapRatio = (eventProgress * GlobalGameState->GeneralOptions.NumberOfLaps) - lastLap;
						}
					}
				}
			}

			if (centerOffset > 0.0f)
			{
#if 0
				float centerOffsetScale = 1.0f - LastLapRatio;
				float centerCycle = FMath::Pow((FMath::Cos(GetRealTimeGameClock() * CatchupCyclingSpeed * PI * 2.0) * 0.5f) + 0.5f, 0.75f);
#else
				float centerOffsetScale = 1.0f;
				float centerCycle = 1.0f;
#endif

				centerOffsetScale = FMath::Min(centerOffsetScale, centerCycle);

				centerOffset = FMath::Lerp(FMath::Min(centerOffset, 100.0f), centerOffset, centerOffsetScale);
			}

			if (numHumans > 0)
			{
				meanHumanDistance = FMath::Max(meanHumanDistance + (centerOffset * 100.0f), 0.0f);
			}

			float minDistance = -1.0f;
			float maxDistance = -1.0f;
			float distanceSpread = characteristics.DistanceSpread * 0.5f;

			for (ABaseVehicle* vehicle : Vehicles)
			{
				FPlayerRaceState& raceState = vehicle->GetRaceState();
				bool usingLeadingCatchup = vehicle->GetUsingLeadingCatchup();
				bool usingTrailingCatchup = vehicle->GetUsingTrailingCatchup();

				if (minDistance < 0.0f)
				{
					minDistance = maxDistance = raceState.EternalRaceDistance;
				}
				else
				{
					minDistance = FMath::Min(minDistance, raceState.EternalRaceDistance);
					maxDistance = FMath::Max(maxDistance, raceState.EternalRaceDistance);
				}

				raceState.StockCatchupRatioUnbounded = FMathEx::CentimetersToMeters(raceState.EternalRaceDistance - median) / distanceSpread;

				float delay = characteristics.SpeedChangeDelay * 3.0f;
				float distanceTarget = (vehicle->HasAIDriver() == true) ? meanHumanDistance : median;
				float distance = FMathEx::CentimetersToMeters(raceState.EternalRaceDistance - distanceTarget);

				// Distance is distance of this car from the middle of the pack in meters.
				// Positive figures mean leading and negative trailing.

				distance = FMath::Clamp(distance, -distanceSpread, distanceSpread);

				// We factor the drag of the vehicle for now, so initial and low-speed
				// handling isn't affected, just the top speed will vary. We vary it by
				// around 20% in either direction to slow you down or speed you up accordingly.

				raceState.RaceCatchupRatio = distance / distanceSpread;

				if (raceState.RaceCatchupRatio > raceState.DragCatchupRatio)
				{
					// If we're slowing up because we're progressing through the pack then make
					// the delay spread out longer so this vehicle has a chance to get ahead.
					// This will then induce a rolling effect as vehicles overtake and then
					// fall back and create a kind of natural cycle while injecting excitement
					// into the game.

					delay *= 2.5f;
				}

				// Slowly drift from one ratio to the next, providing nice overlap in
				// vehicle positioning.

				raceState.DragCatchupRatio = FMathEx::GravitateToTarget(raceState.DragCatchupRatio, raceState.RaceCatchupRatio, (1.0f / delay) * deltaSeconds);

				// Calculate the drag scale for the vehicle based on its new drag catchup ratio.

				float normalized = FMathEx::NegativePow(raceState.DragCatchupRatio, 0.5f);

				if (normalized < 0.0f)
				{
					// If we're behind.

					normalized *= ((vehicle->HasAIDriver() == true) ? characteristics.DragScaleAtRearNonHumans : characteristics.DragScaleAtRearHumans);
				}
				else
				{
					// If we're in front.

					normalized *= ((vehicle->HasAIDriver() == true) ? characteristics.DragScaleAtFrontNonHumans : characteristics.DragScaleAtFrontHumans);
				}

				raceState.DragScale = 1.0f;

				if (normalized > 0.0f)
				{
					if (usingLeadingCatchup == true)
					{
						raceState.DragScale += normalized;
					}
				}
				else
				{
					if (usingTrailingCatchup == true)
					{
						raceState.DragScale += normalized;
					}
				}

				// Now consider the relative position of this vehicle to the other humans
				// in the game.

				distance = FMathEx::CentimetersToMeters(raceState.EternalRaceDistance - meanHumanDistance);

				// Distance is distance of this car from the middle of the human pack in meters.
				// Positive figures mean leading and negative trailing.

				distance = FMath::Clamp(distance, -distanceSpread, distanceSpread);

				// Now calculate the boost catchup ratio.

				distanceSpread = 250.0f;
				distance = FMathEx::CentimetersToMeters(raceState.EternalRaceDistance - distanceTarget);
				distance = FMath::Clamp(distance, -distanceSpread, distanceSpread);

				raceState.BoostCatchupRatio = distance / distanceSpread;
			}
		}

#pragma endregion VehicleCatchup

		if (PastGameSequenceStart() == true)
		{
			EliminationTimer += deltaSeconds;

			if (EliminationTimer >= GRIP_ELIMINATION_SECONDS)
			{
				EliminationTimer = 0.0f;

				if (GetNumOpponentsLeft() > 1 &&
					GlobalGameState->GamePlaySetup.DrivingMode == EDrivingMode::Elimination)
				{
					// Obtain the last player in the race.

					int32 maxPosition = -1;
					ABaseVehicle* rearmostVehicle = nullptr;

					for (ABaseVehicle* vehicle : Vehicles)
					{
						if (vehicle->IsVehicleDestroyed() == false)
						{
							if (maxPosition < vehicle->GetRaceState().RacePosition)
							{
								maxPosition = vehicle->GetRaceState().RacePosition;
								rearmostVehicle = vehicle;
							}
						}
					}

					if (maxPosition > 0 &&
						rearmostVehicle != nullptr)
					{
					}
				}
			}
		}
	}

#pragma endregion VehicleRaceDistance

}

/**
* Get a local player's vehicle.
***********************************************************************************/

ABaseVehicle* APlayGameMode::GetPlayerVehicle(int32 localPlayerIndex) const
{
	APlayerController* controller = UGameplayStatics::GetPlayerController(this, localPlayerIndex);

	if (controller != nullptr)
	{
		return Cast<ABaseVehicle>(controller->GetPawn());
	}
	else
	{
		return nullptr;
	}
}

/**
* Get the vehicle that is the current camera target.
***********************************************************************************/

ABaseVehicle* APlayGameMode::CameraTarget(int32 localPlayerIndex)
{
	ABaseVehicle* player = GetPlayerVehicle(localPlayerIndex);

	if (player != nullptr &&
		localPlayerIndex == 0)
	{
		AController* controller = player->GetController();

		if (controller != nullptr)
		{
			AActor* target = controller->GetViewTarget();
			ABaseVehicle* vehicle = Cast<ABaseVehicle>(target);

			if (vehicle != nullptr)
			{
				return vehicle;
			}
		}
	}

	return player;
}

/**
* Quick function for grabbing the children of a panel.
***********************************************************************************/

void APlayGameMode::GetAllWidgetsForParent(TArray<UWidget*>& widgets, UPanelWidget* panel)
{
	int32 numChildren = panel->GetChildrenCount();

	for (int32 i = 0; i < numChildren; i++)
	{
		widgets.Emplace(panel->GetChildAt(i));
	}
}

#pragma region VehicleHUD

/**
* Small structure used for name tag sorting.
***********************************************************************************/

struct FNameTagSorter
{
	int32 Index;

	FVector2D ScreenPosition;

	float Depth;

	float Opacity;
};

/**
* Find a name tag structure for a given index.
***********************************************************************************/

static FNameTagSorter* FindNameTagForIndex(TArray<FNameTagSorter>& nameTags, int32 index)
{
	for (FNameTagSorter& nameTag : nameTags)
	{
		if (nameTag.Index == index)
		{
			return &nameTag;
		}
	}

	return nullptr;
}

/**
* Get the alpha value for a player tag.
***********************************************************************************/

static float GetPlayerTagAlphaValue(float distance, bool arenaMode)
{
	float opacity = 1.0f;
	float visMinDist = 1.0f * 100.0f;
	float visMaxDist = visMinDist + (10.0f * 100.0f);

	if (distance < visMinDist)
	{
		opacity = 0.0f;
	}
	else if (distance < visMaxDist)
	{
		opacity = (distance - visMinDist) / (visMaxDist - visMinDist);
	}

	if (arenaMode == false)
	{
		float visFarDist = visMaxDist + (400.0f * 100.0f);

		if (distance > visFarDist)
		{
			opacity = 0.0f;
		}
		else if (distance > visMaxDist)
		{
			opacity = 1.0f - ((distance - visMaxDist) / (visFarDist - visMaxDist));
			opacity = FMath::Pow(opacity, 0.5f);
		}
	}

	return opacity;
}

#pragma endregion VehicleHUD

/**
* Update the player tags on the HUD.
***********************************************************************************/

void APlayGameMode::UpdatePlayerTags(APawn* owningPawn, UPanelWidget* parent)
{

#pragma region VehicleHUD

	if (parent != nullptr &&
		owningPawn != nullptr)
	{
		TArray<UWidget*> widgets;
		GetAllWidgetsForParent(widgets, parent);

		ABaseVehicle* vehicle = Cast<ABaseVehicle>(owningPawn);
		FVector ownerLocation = owningPawn->GetActorLocation();

		bool showTags = GlobalGameState->GeneralOptions.ShowPlayerNameTags != EShowPlayerNameTags::None;
		bool showAllTags = GlobalGameState->GeneralOptions.ShowPlayerNameTags == EShowPlayerNameTags::All;
		bool showNoTags = PastGameSequenceStart() == false;

		FMinimalViewInfo desiredView;

		vehicle->Camera->GetCameraViewNoPostProcessing(0.0f, desiredView);

		static const FName kArenaName("ArenaPipper");

		// Find all of the visual components for the name tags and calculate their screen position
		// and initial opacity.

		int32 arenaIndex = 0;
		int32 playerIndex = 0;
		bool arenaMode = GlobalGameState->IsGameModeArena();

		TArray<FNameTagSorter> nameTags;

		for (UWidget* widget : widgets)
		{
			ANSICHAR ansiName[NAME_SIZE];
			FName name = widget->GetFName();
			name.GetDisplayNameEntry()->GetAnsiName(ansiName);

			if (strncmp(ansiName, "ArenaPipper", 11) == 0)
			{
				// Handle the pipper arrow for a player.

				int32 vehicleIndex = arenaIndex++;

				if (showNoTags == false &&
					Vehicles.Num() > vehicleIndex &&
					Vehicles[vehicleIndex]->IsVehicleDestroyed() == false)
				{
					if (vehicle == nullptr ||
						vehicle->GetRaceState().PlayerCompletionState == EPlayerCompletionState::Incomplete)
					{
						if (showAllTags == true)
						{
							FVector2D position;
							FVector location = Vehicles[vehicleIndex]->GetTargetLocation();

							location.Z += 200.0f;

							if (ProjectWorldLocationToWidgetPosition(owningPawn, location, position, &desiredView) == true)
							{
								FNameTagSorter nameTag;

								float distance = (location - ownerLocation).Size();

								nameTag.Index = vehicleIndex;
								nameTag.Opacity = (showTags == true && vehicle != Vehicles[vehicleIndex]) ? GetPlayerTagAlphaValue(distance, arenaMode) : 0.0f;
								nameTag.ScreenPosition = position;
								nameTag.Depth = distance;

								nameTags.Emplace(nameTag);

								UCanvasPanelSlot* canvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(widget);

								canvasSlot->SetPosition(position);
							}
						}
					}
				}
			}
		}

		// Sort the name tags by depth.

		nameTags.Sort([this] (const FNameTagSorter& object1, const FNameTagSorter& object2)
			{
				return object1.Depth < object2.Depth;
			});

		// Now declutter overlapping tags by fading out those that are overlapping and furthest away.

		for (int32 i = 0; i < nameTags.Num(); i++)
		{
			for (int32 j = i + 1; j < nameTags.Num(); j++)
			{
				FNameTagSorter& n0 = nameTags[i];

				if (n0.Opacity > KINDA_SMALL_NUMBER)
				{
					FNameTagSorter& n1 = nameTags[j];
					FVector2D difference = n1.ScreenPosition - n0.ScreenPosition;
					float width = 100.0f;
					float height = 20.0f;

					float ox = 1.0f - FMath::Min(FMath::Max(FMath::Abs(difference.X) - width, 0.0f) / width, 1.0f);
					float oy = 1.0f - FMath::Min(FMath::Max(FMath::Abs(difference.Y) - height, 0.0f) / height, 1.0f);
					float o = ox * oy;

					n1.Opacity *= 1.0f - (o * n0.Opacity);
				}
			}
		}

		// Update the visual components associated with the name tags.

		arenaIndex = 0;
		playerIndex = 0;

		for (UWidget* widget : widgets)
		{
			ANSICHAR ansiName[NAME_SIZE];
			FName name = widget->GetFName();
			name.GetDisplayNameEntry()->GetAnsiName(ansiName);

			if (strncmp(ansiName, "ArenaPipper", 11) == 0)
			{
				// Handle the pipper arrow for a player.

				ESlateVisibility visible = ESlateVisibility::Collapsed;
				int32 vehicleIndex = arenaIndex++;
				FNameTagSorter* nameTag = FindNameTagForIndex(nameTags, vehicleIndex);

				if (nameTag != nullptr &&
					nameTag->Opacity > 0.0f)
				{
					ABaseVehicle* tagVehicle = Vehicles[vehicleIndex];
					const FString& playerName = tagVehicle->GetPlayerName(true, true);

					FLinearColor color;

					float opacity = nameTag->Opacity;

					if (vehicle->IsUsingDoubleDamage() == true)
					{
						color = FLinearColor(0.4f, 0.0f, 0.8f, opacity);
					}
					else
					{
						color = FLinearColor(1.0f, 1.0f, 1.0f, opacity);
					}

					if (color.A < 0.01f)
					{
						color.A = 0.0f;
					}

					Cast<UImage>(widget)->SetColorAndOpacity(color);

					if (color.A != 0.0f)
					{
						UWidgetLayoutLibrary::SlotAsCanvasSlot(widget)->SetPosition(nameTag->ScreenPosition);

						visible = ESlateVisibility::HitTestInvisible;
					}
				}

				widget->SetVisibility(visible);
			}
			else if (strncmp(ansiName, "PlayerName", 10) == 0)
			{
				// Handle the name rendering for a player.

				ESlateVisibility visible = ESlateVisibility::Collapsed;
				int32 vehicleIndex = playerIndex++;
				FNameTagSorter* nameTag = FindNameTagForIndex(nameTags, vehicleIndex);

				if (nameTag != nullptr &&
					nameTag->Opacity > 0.0f)
				{
					ABaseVehicle* tagVehicle = Vehicles[vehicleIndex];
					UTextBlock* textBlock = Cast<UTextBlock>(widget);

					FLinearColor color;

					float opacity = nameTag->Opacity;

					if (vehicle->IsUsingDoubleDamage() == true)
					{
						color = FLinearColor(0.4f, 0.0f, 0.8f, opacity);
					}
					else
					{
						color = FLinearColor(1.0f, 1.0f, 1.0f, opacity);
					}

					if (color.A < 0.01f)
					{
						color.A = 0.0f;
					}

					textBlock->SetColorAndOpacity(FSlateColor(color));

					if (color.A != 0.0f)
					{
						FFormatNamedArguments arguments;

						arguments.Emplace(TEXT("PlayerName"), FText::FromString(tagVehicle->GetPlayerName(true, true)));
						arguments.Emplace(TEXT("Distance"), FText::AsNumber((int32)FMathEx::CentimetersToMeters(nameTag->Depth)));

						textBlock->SetText(FText::Format(NSLOCTEXT("GripHUD", "PlayerDistance", "{PlayerName}\r\n{Distance} m"), arguments));

						UWidgetLayoutLibrary::SlotAsCanvasSlot(widget)->SetPosition(nameTag->ScreenPosition);

						visible = ESlateVisibility::HitTestInvisible;
					}
				}

				widget->SetVisibility(visible);
			}
		}
	}

#pragma endregion VehicleHUD

}

/**
* The default ChoosePlayerStart is broken in the engine, so we override it here to
* allocate player starts serially to vehicles.
***********************************************************************************/

AActor* APlayGameMode::ChoosePlayerStartProperly(AController* player, int32 maxPlayers)
{
	if (ResetPlayerStarts == true)
	{
		Startpoints.Empty();
		UnusedStartpoints.Empty();
		ResetPlayerStarts = false;
	}

	FString startName = "";

	UWorld* world = GetWorld();

	if (Startpoints.Num() == 0)
	{
		for (TActorIterator<APlayerStart> itr(world); itr; ++itr)
		{
			APlayerStart* playerStart = *itr;

			if (FWorldFilter::IsValid(playerStart, GlobalGameState) == true)
			{
				if (playerStart->IsA<APlayerStartPIE>() == false)
				{
					Startpoints.Emplace(playerStart);
					UnusedStartpoints.Emplace(playerStart);
				}
			}
		}

#pragma region NavigationSplines

		// Now sort the start points by main spline distance.

		if (GlobalGameState != nullptr &&
			MasterRacingSpline.IsValid() == false)
		{
			MasterRacingSpline = DetermineMasterRacingSpline(FName(*GlobalGameState->TransientGameState.NavigationLayer), world, GlobalGameState);
		}

		if (MasterRacingSpline.IsValid() == true)
		{
			UnusedStartpoints.Sort([this] (const APlayerStart& object1, const APlayerStart& object2)
				{
					FVector l1 = object1.GetActorLocation();
					FVector l2 = object2.GetActorLocation();

					float d1 = MasterRacingSpline->GetNearestDistance(l1);
					float d2 = MasterRacingSpline->GetNearestDistance(l2);

					return (d2 < d1);
				});
		}

#pragma endregion NavigationSplines

		for (TActorIterator<APlayerStart> itr(world); itr; ++itr)
		{
			APlayerStart* playerStart = *itr;

			if (FWorldFilter::IsValid(playerStart, GlobalGameState) == true)
			{
				if (playerStart->IsA<APlayerStartPIE>() == true)
				{
					Startpoints.Insert(playerStart, 0);
					UnusedStartpoints.Insert(playerStart, 0);
				}
			}
		}
	}

	if (maxPlayers == 0)
	{
		maxPlayers = CalculateMaxPlayers();
	}

	if (UnusedStartpoints.Num() > 0)
	{
		if (startName.Len() > 0)
		{
			for (int32 i = 0; i < UnusedStartpoints.Num(); i++)
			{
				if (UnusedStartpoints[i]->GetName() == startName)
				{
					APlayerStart* result = UnusedStartpoints[i];

					UnusedStartpoints.RemoveAt(i);

					return result;
				}
			}
		}

		int32 index = FMath::Rand() % FMath::Max(1, FMath::Min(UnusedStartpoints.Num(), maxPlayers - (Startpoints.Num() - UnusedStartpoints.Num())));

		if (UnusedStartpoints[0]->IsA<APlayerStartPIE>())
		{
			index = 0;
		}

		APlayerStart* result = UnusedStartpoints[index];

		UnusedStartpoints.RemoveAt(index);

		return result;
	}

	return nullptr;
}

/**
* Record an event that has just occurred within the game.
***********************************************************************************/

void APlayGameMode::AddGameEvent(FGameEvent& gameEvent)
{
	// Process the event.

	gameEvent.Time = GetRealTimeClock();

#pragma region VehicleHUD

	if (gameEvent.LaunchVehicleIndex >= 0)
	{
		ABaseVehicle* vehicle = GetVehicleForVehicleIndex(gameEvent.LaunchVehicleIndex);
		ABaseVehicle* target = GetVehicleForVehicleIndex(gameEvent.TargetVehicleIndex);
		FVector location = (target != nullptr) ? target->GetActorLocation() : FVector::ZeroVector;

		if (gameEvent.EventType == EGameEventType::Impacted)
		{
			switch (gameEvent.PickupUsed)
			{
			case EPickupType::HomingMissile:
			{
				int32 numPoints = 100;

				if (vehicle->AddPoints(numPoints, (target != nullptr), target, location) == true)
				{
					vehicle->ShowStatusMessage(FStatusMessage(GetXPMessage(gameEvent.PickupUsed, numPoints)), true, false);
				}
			}
			break;
			}
		}
	}

#pragma endregion VehicleHUD

	// Record the event.

	GameEvents.Emplace(gameEvent);
}

/**
* Convert a master racing spline distance to a lap distance.
***********************************************************************************/

float APlayGameMode::MasterRacingSplineDistanceToLapDistance(float distance)
{
	// Only if we've crossed the start line should be consider where in are in the lap.

	if (distance <= MasterRacingSplineStartDistance)
	{
		// If we're before the start line in the master racing spline.

		return distance + (MasterRacingSplineLength - MasterRacingSplineStartDistance);
	}
	else
	{
		// If we're after the start line in the master racing spline.

		return distance - MasterRacingSplineStartDistance;
	}
}

/**
* Project a point in world space for use on the HUD.
***********************************************************************************/

bool APlayGameMode::ProjectWorldLocationToWidgetPosition(APawn* pawn, FVector worldLocation, FVector2D& screenPosition, FMinimalViewInfo* cachedView)
{

#pragma region VehicleHUD

	APlayerController* controller = Cast<APlayerController>(pawn->GetController());

	if (controller != nullptr)
	{
		FVector screenLocation;

		if (controller->ProjectWorldLocationToScreenWithDistance(worldLocation, screenLocation) == true)
		{
			screenLocation.X = FMath::RoundToInt(screenLocation.X);
			screenLocation.Y = FMath::RoundToInt(screenLocation.Y);

			ULocalPlayer* localPlayer = controller->GetLocalPlayer();

			if (localPlayer != nullptr &&
				localPlayer->ViewportClient != nullptr)
			{
				FSceneViewProjectionData projectionData;

				if (localPlayer->GetProjectionData(localPlayer->ViewportClient->Viewport, eSSP_FULL, projectionData))
				{
					screenLocation.X -= projectionData.GetConstrainedViewRect().Min.X;
					screenLocation.Y -= projectionData.GetConstrainedViewRect().Min.Y;

					// If invalid position.

					if (screenLocation.X < (-projectionData.GetConstrainedViewRect().Min.X) || (screenLocation.X > projectionData.GetConstrainedViewRect().Max.X))
					{
						return false;
					}
				}
			}

			// Get the application / DPI scale.

			float scale = UWidgetLayoutLibrary::GetViewportScale(controller);

			// Apply inverse DPI scale so that the widget ends up in the expected position.

			screenLocation *= FMath::Pow(scale, -1.0f);

			// screenLocation is now in general screen space offset from the top-right corner for the
			// viewport. It takes nothing about the widget's positioning into account, or its size.
			// It assumes the widget covers the entire viewport.

			ABaseVehicle* vehicle = Cast<ABaseVehicle>(pawn);

			if (vehicle != nullptr)
			{
				FVehicleHUD& hud = vehicle->GetHUD();

				if (GlobalGameState->IsTrackMirrored() == true)
				{
					screenLocation.X -= hud.WidgetPositionSize.X * 0.5f;
					screenLocation.X *= -1.0f;
					screenLocation.X += hud.WidgetPositionSize.X * 0.5f;
				}

				FVector2D distorted = FVector2D(screenLocation.X / hud.WidgetPositionSize.X, screenLocation.Y / hud.WidgetPositionSize.Y);

				screenLocation.X = distorted.X * hud.WidgetPositionSize.X * hud.WidgetPositionScale.X;
				screenLocation.Y = distorted.Y * hud.WidgetPositionSize.Y * hud.WidgetPositionScale.Y;
			}

			screenPosition.X = screenLocation.X;
			screenPosition.Y = screenLocation.Y;

			return true;
		}

		return false;
	}

#pragma endregion VehicleHUD

	return false;
}

/**
* Get the difficulty characteristics for a given level, or the current level if -1
* is passed.
***********************************************************************************/

FDifficultyCharacteristics& APlayGameMode::GetDifficultyCharacteristics(int32 level)
{
	if (level < 0)
	{
		level = GlobalGameState->GetDifficultyLevel();
	}

	switch (level)
	{
	default:
		return DifficultyEasy;
	case 1:
		return DifficultyMed;
	case 2:
		return DifficultyHard;
	case 3:
		return DifficultyNeo;
	}
}

/**
* Set the graphics options into the system.
***********************************************************************************/

void APlayGameMode::SetGraphicsOptions(bool initialization)
{
	Super::SetGraphicsOptions(initialization);
}

/**
* Get a random player start point.
***********************************************************************************/

APlayerStart* APlayGameMode::GetRandomPlayerStart() const
{
	if (Startpoints.Num() > 0)
	{
		return Startpoints[FMath::Rand() % Startpoints.Num()];
	}

	return nullptr;
}

/**
* Have all the players finished the event.
***********************************************************************************/

bool APlayGameMode::HaveAllPlayersFinished() const
{
	for (ABaseVehicle* vehicle : Vehicles)
	{
		if (vehicle->GetRaceState().PlayerCompletionState < EPlayerCompletionState::Complete)
		{
			return false;
		}
	}

	return true;
}

/**
* Get the ratio of completion for the current event, 1 being fully complete.
***********************************************************************************/

float APlayGameMode::GetEventProgress()
{

#pragma region NavigationSplines

	switch (GlobalGameState->GamePlaySetup.DrivingMode)
	{
	case EDrivingMode::Race:
	{
		float minProgress = 1.0f;
		TArray<ABaseVehicle*>& vehicles = GetVehicles();

		for (ABaseVehicle* vehicle : vehicles)
		{
			if (vehicle->IsAIVehicle() == false &&
				vehicle->IsVehicleDestroyed() == false)
			{
				minProgress = FMath::Min(minProgress, vehicle->GetEventProgress());
			}
		}

		return minProgress;
	}
	break;

	case EDrivingMode::Elimination:
	{
		TArray<ABaseVehicle*>& vehicles = GetVehicles();

		for (ABaseVehicle* vehicle : vehicles)
		{
			if (vehicle->IsAIVehicle() == false &&
				vehicle->IsVehicleDestroyed() == false)
			{
				float totalTime = GRIP_ELIMINATION_SECONDS * GetNumOpponents();
				float gameTime = GetRealTimeGameClock();

				return FMath::Min(gameTime / totalTime, 1.0f);
			}
		}

		return 1.0f;
	}
	break;
	}

#pragma endregion NavigationSplines

	return 0.0f;
}

/**
* Get the number of players dead or alive in the game.
***********************************************************************************/

int32 APlayGameMode::GetNumOpponents(bool humansOnly)
{
	if (humansOnly == true)
	{
		int32 numHumans = 0;

		for (ABaseVehicle* vehicle : Vehicles)
		{
			if (vehicle->IsAIVehicle() == false)
			{
				numHumans++;
			}
		}

		return numHumans;
	}
	else
	{
		return Vehicles.Num();
	}
}

/**
* Update the race start line stuff, mostly the camera at this point.
***********************************************************************************/

void APlayGameMode::UpdateRaceStartLine()
{
	if (GameSequence == EGameSequence::Start)
	{

#pragma region VehicleHUD

		// Allow the player to cut short the camera drop by hitting the throttle.

		for (ABaseVehicle* vehicle : Vehicles)
		{
			float throttle = vehicle->GetVehicleControl().ThrottleInput;

			if (vehicle->HasAIDriver() == false &&
				Clock < StartLineDropTime - 1.5f &&
				FMath::Abs(throttle) > 0.25f)
			{
				Clock = StartLineDropTime - 1.5f;
				break;
			}
		}

		bool cameraDropping = Clock <= StartLineDropTime;

		if (cameraDropping == true)
		{
			if (StartCameraDropped == false)
			{
				// Drop the camera down onto the vehicle.

				for (ABaseVehicle* vehicle : Vehicles)
				{
					URaceCameraComponent* camera = vehicle->Camera;
					float ratio = FMathEx::EaseInOut(Clock / StartLineDropTime, 2.5f);
					FVector to = camera->GetNativeLocation();
					FVector from = camera->GetNativeLocation() + camera->GetComponentRotation().RotateVector(FVector(0.0f, 0.0f, 250.0f));

					camera->CustomLocation = FMath::Lerp(from, to, ratio);
				}
			}
		}
		else
		{
			if (StartCameraDropped == false)
			{
				StartCameraDropped = true;

				for (ABaseVehicle* vehicle : Vehicles)
				{
					URaceCameraComponent* camera = vehicle->Camera;

					camera->CustomLocation = camera->GetNativeLocation();

					camera->SwitchLocationToNativeControl();
				}
			}
		}

#pragma endregion VehicleHUD

		bool startingGame = Clock > StartLineCountTo;

		if (startingGame == true)
		{
			// Kick off the game as we're done with the start line intro.

			GameSequence = EGameSequence::Play;

			RealTimeGameClockTicking = true;
		}
}
}

/**
* Get the countdown time for the race.
***********************************************************************************/

FText APlayGameMode::GetCountDownTime() const
{

#pragma region VehicleHUD

	if (GameSequence == EGameSequence::Start)
	{
		if (Clock >= StartLineCountFrom &&
			Clock < StartLineCountTo)
		{
			return FText::AsNumber(StartLineCountTo - FMath::FloorToInt(Clock));
		}
	}
	else if (GameSequence == EGameSequence::Play)
	{
		if (GetRealTimeGameClock() < 2.0f)
		{
			return NSLOCTEXT("GripScoreboard", "Go", "GO!");
		}
	}

#pragma endregion VehicleHUD

	return FText::FromString("");
}

/**
* Get the countdown opacity for the text at the start of a race.
***********************************************************************************/

float APlayGameMode::GetCountdownOpacity() const
{

#pragma region VehicleHUD

	if (GameSequence == EGameSequence::Start)
	{
		return 1.0f;
	}
	else if (GameSequence == EGameSequence::Play)
	{
		if (GetRealTimeGameClock() < 2.0f)
		{
			return 1.0f - (GetRealTimeGameClock() / 2.0f);
		}
	}

#pragma endregion VehicleHUD

	return 0.0f;
}

/**
* Get the time left before the game starts.
***********************************************************************************/

float APlayGameMode::GetPreStartTime() const
{
	if (GameSequence <= EGameSequence::Start)
	{
		return StartLineCountTo - Clock;
	}

	return 0.0f;
}

/**
* Get the scale of the HUD.
***********************************************************************************/

float APlayGameMode::GetHUDScale() const
{

#pragma region VehicleHUD

	UGlobalGameState* gameState = UGlobalGameState::GetGlobalGameState(this);

	if (GameSequence == EGameSequence::Start)
	{
		float zoomTime = 0.5f;
		float startTime = 0;

		if (Clock < startTime)
		{
			return 0.0f;
		}
		else if ((Clock - startTime) < zoomTime)
		{
			return (Clock - startTime) / zoomTime;
		}
		else
		{
			return 1.0f;
		}
	}
	else
	{
		return 1.0f;
	}

#pragma endregion VehicleHUD

}

#pragma region VehiclePhysicsTweaks

#if GRIP_ANTI_SKYWARD_LAUNCH

/**
* Should an actor actively limit the collision response when a vehicle collides
* with it?
***********************************************************************************/

bool APlayGameMode::ShouldActorLimitCollisionResponse(AActor* actor)
{
	if (actor == LastFrictionalActorCheck.Get())
	{
		return LastFrictionalActorCheckResult;
	}

	LastFrictionalActorCheck = actor;
	LastFrictionalActorCheckResult = FrictionalActors.Contains(actor);

	return LastFrictionalActorCheckResult;
}

#endif // GRIP_ANTI_SKYWARD_LAUNCH

#pragma endregion VehiclePhysicsTweaks

#pragma region VehicleAudio

/**
* Increase the sound volume of vehicles that are close to the local player.
* This will be capped at a max overall volume to keep things from getting drowned
* out.
***********************************************************************************/

void APlayGameMode::UpdateVehicleVolumes(float deltaSeconds)
{
	WatchedVehicles.Reset();

	// Get a list of local player camera locations.

	TArray<FVector, TInlineAllocator<16>> localPositions;

	for (ABaseVehicle* vehicle : Vehicles)
	{
		if (vehicle->LocalPlayerIndex >= 0)
		{
			FMinimalViewInfo desiredView;

			vehicle->Camera->GetCameraViewNoPostProcessing(0.0f, desiredView);

			localPositions.Emplace(desiredView.Location);

			ABaseVehicle* target = vehicle->CameraTarget();

			if (WatchedVehicles.Contains(target) == false)
			{
				WatchedVehicles.Emplace(target);
			}
		}
	}

#if !UE_BUILD_SHIPPING
	// If this isn't a shipping build, and our pawn is a spectator pawn, then override
	// the camera locations with just one, single location.

	APlayerController* controller = UGameplayStatics::GetPlayerController(GetWorld(), 0);

	if (controller != nullptr)
	{
		APawn* pawn = controller->GetPawn();

		if (IsValid(pawn) == true &&
			pawn->IsA<ASpectatorPawn>() == true)
		{
			localPositions.Empty();
			WatchedVehicles.Empty();

			localPositions.Emplace(pawn->GetActorLocation());
		}
	}
#endif // !UE_BUILD_SHIPPING

	if (localPositions.Num() > 0)
	{
		TArray<ABaseVehicle*, TInlineAllocator<16>> volumeVehicles;

		for (ABaseVehicle* vehicle : Vehicles)
		{
			if (vehicle->IsVehicleDestroyed() == false)
			{
				// Find the shortest distance to one of the player cameras.

				vehicle->GlobalVolumeRatio = 0.0f;

				for (FVector& location : localPositions)
				{
					// Normalize the distance of the vehicle between the min and max volume distances.

					float size = (vehicle->GetActorLocation() - location).Size();
					float volume = 1.0f - FMathEx::GetRatio(size, MinVehicleVolumeDistance, MaxVehicleVolumeDistance);

					vehicle->GlobalVolumeRatio = FMath::Max(vehicle->GlobalVolumeRatio, volume);
				}

				volumeVehicles.Emplace(vehicle);
			}
		}

		// For each vehicle, GlobalVolumeRatio is now the normalized linear proximity to the nearest listener
		// 1 being within MinVehicleVolumeDistance and 0 being MaxVehicleVolumeDistance or further away.

		int32 numVehicles = volumeVehicles.Num();

		if (numVehicles > 0)
		{
			// Sort the vehicles based on distance to camera, closest and therefore loudest first.

			volumeVehicles.Sort([this] (const ABaseVehicle& object1, const ABaseVehicle& object2)
				{
					return object1.GlobalVolumeRatio > object2.GlobalVolumeRatio;
				});

			// Fit the vehicles to the range of the vehicles.

			float min = volumeVehicles[volumeVehicles.Num() - 1]->GlobalVolumeRatio;
			float max = volumeVehicles[0]->GlobalVolumeRatio;
			float switchRatio = FMathEx::GetRatio(numVehicles / MaxGlobalVolume, 1.0f, 2.0f);

			for (ABaseVehicle* vehicle : volumeVehicles)
			{
				if (min != max)
				{
					vehicle->GlobalVolumeRatio = FMath::Lerp(1.0f, (vehicle->GlobalVolumeRatio - min) / (max - min), switchRatio);
				}
				else
				{
					vehicle->GlobalVolumeRatio = 1.0f;
				}
			}

			// Apply a bell curve to that fitting, so volume is biased more to the closest vehicles.

			float sum = 0.0f;
			float watchedSum = 0.0f;

			for (ABaseVehicle* vehicle : volumeVehicles)
			{
				if (WatchedVehicles.Contains(vehicle) == true)
				{
					// A watched vehicle is always top volume.

					vehicle->GlobalVolumeRatio = 1.0f;

					watchedSum += vehicle->GlobalVolumeRatio;
				}
				else
				{
					// Apply a bell curve to the volume ratio here.

					vehicle->GlobalVolumeRatio = FMath::Sin(vehicle->GlobalVolumeRatio * PI * 0.5f);
					vehicle->GlobalVolumeRatio *= vehicle->GlobalVolumeRatio;
					vehicle->GlobalVolumeRatio *= vehicle->GlobalVolumeRatio;

					sum += vehicle->GlobalVolumeRatio;
				}
			}

			// Normalize the unwatched vehicle volumes to fit the available volume space.

			float maxGlobalVolume = MaxGlobalVolume - watchedSum;

			if (sum > 0.0f)
			{
				for (ABaseVehicle* vehicle : volumeVehicles)
				{
					if (WatchedVehicles.Contains(vehicle) == false)
					{
						vehicle->GlobalVolumeRatio = FMath::Min(1.0f, (vehicle->GlobalVolumeRatio / sum) * maxGlobalVolume);
					}
				}
			}

			// Adjust the volume level of all vehicles to these new normalized values.
			// Do this swiftly but not instantaneously.

			float ratio = FMathEx::GetSmoothingRatio(0.9f, deltaSeconds);

			for (ABaseVehicle* vehicle : volumeVehicles)
			{
				vehicle->GlobalVolume = FMath::Lerp(vehicle->GlobalVolumeRatio, vehicle->GlobalVolume, ratio);
			}
		}
	}
}

#pragma endregion VehicleAudio

#pragma region VehicleRaceDistance

/**
* Calculate the rank and scoring for each vehicle.
***********************************************************************************/

void APlayGameMode::CalculateRanksAndScoring()
{
	TArray<FPlayerRaceState*, TInlineAllocator<16>> raceStates;

	// Calculate the scoring for each vehicle from which rank will be determined.

	for (ABaseVehicle* vehicle : Vehicles)
	{
		if (vehicle->GetRaceState().PlayerCompletionState != EPlayerCompletionState::Disqualified)
		{
			vehicle->GetRaceState().NumTotalPoints = 0;

			raceStates.Emplace(&vehicle->GetRaceState());
		}
		else
		{
			// No points for disqualified vehicles.

			vehicle->GetRaceState().NumTotalPoints = 0;
		}
	}

	// Sort the race states according to total points.

	// So in networked code, whenever the server talks to us it'll give us a list of game results
	// that it knows about and will also modify the RaceRank and NumTotalPoints of each vehicle
	// in those results so generally we'll be in agreement at this point with regard to sorting
	// - except when there are multiple vehicles sharing the same NumTotalPoints.

	if (raceStates.Num() > 0)
	{
		// In non-networked games, secondarily order on player name when NumTotalPoints is equal.

		raceStates.StableSort([] (const FPlayerRaceState& object1, const FPlayerRaceState& object2)
			{
				return (object1.PlayerVehicle->GetPlayerName(false, true).Compare(object2.PlayerVehicle->GetPlayerName(false, true)) < 0);
			});

		for (int32 i = 0; i < raceStates.Num(); i++)
		{
			raceStates[i]->RaceRank = i;
		}

		raceStates.StableSort([] (const FPlayerRaceState& object1, const FPlayerRaceState& object2)
			{
				return (object1.NumTotalPoints == object2.NumTotalPoints) ? object1.RaceRank < object2.RaceRank : object1.NumTotalPoints > object2.NumTotalPoints;
			});

		for (int32 i = 0; i < raceStates.Num(); i++)
		{
			raceStates[i]->RaceRank = i;
		}
	}
}

#pragma endregion VehicleRaceDistance

#pragma region VehiclePickups

/**
* Get the relative pickup index between 0 and 2 for a particular vehicle.
* 0 is winning and 2 is losing.
***********************************************************************************/

int32 APlayGameMode::GetPlayerRacePickupIndex(ABaseVehicle* vehicle)
{
	if (GlobalGameState->IsGameModeRace() == true)
	{
		int32 position = FMath::Max(0, vehicle->GetRaceState().RacePosition);
		int32 opponentsLeft = GetNumOpponentsLeft();

		if (opponentsLeft >= 3)
		{
			// We have at least 3 players, so they fit into metric of 3 areas of pickup determination.

			return FMath::FloorToInt(((float)position / (float)opponentsLeft) * 2.999f);
		}
		else if (opponentsLeft > 1)
		{
			// We've few players, so work on distance now.

			if (position > 0)
			{
				float firstPlayerDistance = 0.0f;

				for (ABaseVehicle* firstVehicle : Vehicles)
				{
					if (firstVehicle->GetRaceState().RacePosition == 0)
					{
						firstPlayerDistance = firstVehicle->GetRaceState().EternalRaceDistance;
						break;
					}
				}

				float distance = firstPlayerDistance - vehicle->GetRaceState().EternalRaceDistance;

				if (distance > 250.0f * 100.0f)
				{
					return 2;
				}
				else if (distance > 150.0f * 100.0f)
				{
					return 1;
				}
			}
		}
	}

	return 0;
}

/**
* Should a vehicle be fighting another vehicle or just try to catchup with the
* humans?
*
* -1 means no, 0 means yes, +1 means hell yeah!
*
* This generated from the playgamemode weapon use data.
***********************************************************************************/

float APlayGameMode::VehicleShouldFightVehicle(ABaseVehicle* aggressor, ABaseVehicle* victim)
{
	// Handle the simple cases first.

	if (aggressor == nullptr)
	{
		return 0.0f;
	}

	if (aggressor->HasAIDriver() == false)
	{
		// This is a human player, let them do what they want.

		return 0.0f;
	}

	FWeaponCatchupCharacteristics& weapons = GetDifficultyCharacteristics().WeaponCatchupCharacteristics;
	FVehicleCatchupCharacteristics& vehicles = GetDifficultyCharacteristics().VehicleCatchupCharacteristics;
	float halfSpread = vehicles.DistanceSpread * 0.5f;
	float catchupRatio = 0.0f, min = 0.0f, max = 0.0f;

	if (victim != nullptr)
	{
		// We're asked to consider another vehicle to fight with.

		if (victim->HasAIDriver() == true)
		{
			// It's an AI player, so we should fight them if they're ahead of the human players.

			// The catchup ratio of this vehicle compared to the mean human distance. -1 = max speedup and 1 = max slowdown.

			catchupRatio = victim->GetRaceState().StockCatchupRatioUnbounded;

			min = -weapons.TrailingDistanceNonHumans /* 100 */ / halfSpread;
			max = weapons.LeadingDistanceNonHumans /* 250 */ / halfSpread;
		}
		else
		{
			// It's a human player, so we should fight them if they're not losing. So consider
			// distance from the median of the pack.

			// The ratio of catchup to be applied to the vehicle. -1 = max speedup and 1 = max slowdown.

			catchupRatio = victim->GetRaceState().StockCatchupRatioUnbounded;

			min = -weapons.TrailingDistanceHumans /* 100 */ / halfSpread;
			max = weapons.LeadingDistanceHumans /* 100 */ / halfSpread;
		}
	}
	else
	{
		// Just fighting in general, we should fight if we're ahead of the human players.

		// The catchup ratio of this vehicle compared to the mean human distance. -1 = max speedup and 1 = max slowdown.

		catchupRatio = aggressor->GetRaceState().StockCatchupRatioUnbounded;

		min = -weapons.TrailingDistance / halfSpread;
		max = weapons.LeadingDistance / halfSpread;
	}

	float result = 0.0f;

	if (catchupRatio < 0.0f)
	{
		// Target vehicle is trailing. If they're really trailing then much less likely to fight.

		result = FMath::Max(-1.0f, -(catchupRatio / FMath::Max(-1.0f, min)));
	}
	else
	{
		// Target vehicle is leading. If they're really leading then much more likely to fight.

		result = FMath::Min(+1.0f, (catchupRatio / FMath::Min(+1.0f, max)));
	}

	// Add a curve to the result, in order to increase the aggression of the aggressor
	// above linear.

	result = (FMath::Pow((result * 0.5f) + 0.5f, 0.5f) - 0.5f) * 2.0f;

	return result;
}

/**
* Should a pickup be used?
*
* aggressionRatio from VehicleShouldFightVehicle, -1 to 1 meaning using weapons,
* 1 use as soon as possible, -1 meaning don't use any time soon.
***********************************************************************************/

bool APlayGameMode::ShouldUsePickup(bool isBot, const FPlayerPickupSlot* pickup, float aggressionRatio) const
{
	if (isBot == true)
	{
		if (aggressionRatio < 0.0f)
		{
			// The vehicle we want to attack is trailing, so don't attack it until we've run out of time.
			// This should never be the case for human players.

			return (pickup->Timer >= pickup->UseBefore);
		}
		else
		{
			// The vehicle is leading so use the pickup more quickly the more aggressive we are.
			// Human players will be between 0 and 1, with 0 being normal and 1 being a special target.

			return (pickup->Timer >= FMath::Lerp(pickup->UseBefore, pickup->UseAfter, aggressionRatio));
		}
	}
	else
	{
		return aggressionRatio > -1.0f;
	}
}

/**
* Should an offensive pickup be used?
*
* weight is 0 for perfect target and 1 for worst-case target, < 0 means don't
* target ever.
* aggressionRatio from VehicleShouldFightVehicle, -1 to 1 meaning using weapons,
* 1 use as soon as possible, -1 meaning don't use any time soon.
***********************************************************************************/

float APlayGameMode::ScaleOffensivePickupWeight(bool isBot, float weight, const FPlayerPickupSlot* pickup, float aggressionRatio) const
{
	if (aggressionRatio == -1.0f)
	{
		// This aggressionRatio means do not fight.

		weight = -1.0f;
	}

	if (weight >= 0.0f)
	{
		if (isBot == false)
		{
			// Triple the chances of use if this is a target which a human player really wants to hit.
			// aggressionRatio is always 0 to 1 for human players.

			return FMath::Lerp(weight, weight * 0.333f, aggressionRatio);
		}
		else if (pickup != nullptr)
		{
			// Void the weight if the bot isn't ready for this target due to pickup use rules considering aggression.

			return (ShouldUsePickup(isBot, pickup, aggressionRatio) == true) ? weight : 1.0f;
		}
	}

	return weight;
}

/**
* Should a defensive pickup be used?
*
* weight is 0 for perfect defensive posture and 1 for worst-case posture.
* aggressionRatio from VehicleShouldFightVehicle, 0 to 1 meaning using pickups,
* 1 use as soon as possible.
***********************************************************************************/

float APlayGameMode::ScaleDefensivePickupWeight(bool isBot, float weight, const FPlayerPickupSlot* pickup, float aggressionRatio) const
{
	if (weight >= 0.0f && pickup != nullptr)
	{
		return 1.0f - ((1.0f - weight) * ((ShouldUsePickup(isBot, pickup, aggressionRatio) == true) ? 1.0f : 0.0f));
	}

	return weight;
}

/**
* Get the number of pickups currently present for a given pickup type.
***********************************************************************************/

int32 APlayGameMode::NumPickupsPresent(EPickupType pickupType)
{
	int32 numPickups = NumPickupTypes[(int32)pickupType];

	for (ABaseVehicle* vehicle : Vehicles)
	{
		if (vehicle->IsVehicleDestroyed(false) == false)
		{
			if (vehicle->HasPickup(pickupType, false) == true)
			{
				numPickups++;
			}
		}
	}

	return numPickups;
}

#pragma endregion VehiclePickups
