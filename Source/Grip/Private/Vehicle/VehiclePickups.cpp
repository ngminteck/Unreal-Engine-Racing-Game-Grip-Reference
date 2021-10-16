/**
*
* Vehicle pickups implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Handle all of the pickups-related activity for the vehicle, mostly related to the
* two pickup slots that each vehicle has for two different pickups.
*
***********************************************************************************/

#include "vehicle/flippablevehicle.h"
#include "gamemodes/playgamemode.h"
#include "game/globalgamestate.h"
#include "pickups/speedpad.h"
#include "pickups/gatlinggun.h"
#include "pickups/turbo.h"
#include "pickups/shield.h"
#include "components/widgetcomponent.h"

DEFINE_LOG_CATEGORY_STATIC(GripLogPickups, Warning, All);

/**
* Give a particular pickup to a vehicle.
***********************************************************************************/

int32 ABaseVehicle::GivePickup(EPickupType type, int32 pickupSlot, bool fromTrack)
{

#pragma region VehiclePickups

	if (type == EPickupType::None)
	{
		return -1;
	}

	if (pickupSlot >= 0 &&
		PickupSlots[pickupSlot].Type == type &&
		PickupSlots[pickupSlot].State == EPickupSlotState::Idle)
	{
		// We already know about this so don't bother doing anything more.
		// This is normally for network play so we don't repeat ourselves.

		return pickupSlot;
	}

	if (pickupSlot < 0)
	{
		for (pickupSlot = 0; pickupSlot < NumPickups; pickupSlot++)
		{
			if (PickupSlots[pickupSlot].State == EPickupSlotState::Empty)
			{
				break;
			}
		}
	}

	FDifficultyCharacteristics& difficulty = PlayGameMode->GetDifficultyCharacteristics();
	FPickupUseCharacteristics& useCharacteristics = difficulty.PickupUseCharacteristics.Race;

	float useDelay = useCharacteristics.PickupUseAfter + FMath::RandRange(-useCharacteristics.PickupUseAfter * 0.25f, useCharacteristics.PickupUseAfter * 0.25f);
	float useBefore = useCharacteristics.PickupUseBefore + FMath::RandRange(-useCharacteristics.PickupUseBefore * 0.25f, useCharacteristics.PickupUseBefore * 0.25f);
	float dumpAfter = useCharacteristics.PickupDumpAfter + FMath::RandRange(-useCharacteristics.PickupDumpAfter * 0.25f, useCharacteristics.PickupDumpAfter * 0.25f);

	if (useBefore < KINDA_SMALL_NUMBER)
	{
		useBefore = 0.0f;
	}
	else if (useBefore <= useDelay)
	{
		useBefore = useDelay + 5.0f;
	}

	// Always dump shields and Gatling guns if we can't use them in a reasonable time-frame.
	// And no enforced delay for shields.

	if (dumpAfter < KINDA_SMALL_NUMBER)
	{
		switch (type)
		{
		default:
			dumpAfter = 0.0f;
			break;

		case EPickupType::Shield:
			useBefore = 0.0f;
			// Deliberate lack of break;

		case EPickupType::GatlingGun:
			dumpAfter = useBefore * 2.0f;
			break;
		}
	}

	if (dumpAfter != 0.0f &&
		dumpAfter < useBefore)
	{
		dumpAfter = useBefore;
	}

	if (pickupSlot < NumPickups)
	{
		FPlayerPickupSlot& playerPickupSlot = PickupSlots[pickupSlot];

		playerPickupSlot.State = EPickupSlotState::Idle;
		playerPickupSlot.Activation = EPickupActivation::None;
		playerPickupSlot.Type = type;
		playerPickupSlot.Timer = 0.0f;
		playerPickupSlot.EfficacyTimer = 0.0f;
		playerPickupSlot.UseAfter = useDelay;
		playerPickupSlot.UseBefore = useBefore;
		playerPickupSlot.DumpAfter = dumpAfter;
		playerPickupSlot.PickupCount = PickupCount++;
		playerPickupSlot.AutoUse = false;
		playerPickupSlot.BotWillCharge = false;
		playerPickupSlot.BotWillTargetHuman = false;

		if (HasAIDriver() == true)
		{
			int32 difficultyLevel = GameState->GetDifficultyLevel();

			switch (difficultyLevel)
			{
			case 1:
				playerPickupSlot.BotWillCharge = (FMath::Rand() % 7) == 0;
				break;
			case 2:
				playerPickupSlot.BotWillCharge = (FMath::Rand() % 3) == 0;
				break;
			case 3:
				playerPickupSlot.BotWillCharge = (FMath::Rand() % 2) == 0;
				break;
			case 0:
				break;
			}

			if (type == EPickupType::TurboBoost)
			{
				// Too difficult for bots to handle.

				playerPickupSlot.BotWillCharge = false;
			}

			if (IsAIVehicle() == true &&
				RaceState.PlayerCompletionState < EPlayerCompletionState::Complete)
			{
				float bias = useCharacteristics.HumanAttackBias;

				if (bias > KINDA_SMALL_NUMBER)
				{
					float p0 = (float)PlayGameMode->GetNumOpponents(true) / (float)PlayGameMode->GetNumOpponents();

					playerPickupSlot.BotWillTargetHuman = FMath::FRand() < FMath::Lerp(p0, 1.0f, bias);
				}
			}
		}

		PlayGameMode->SetPickupLastUsed(type);
	}
	else
	{
		return -1;
	}

	return pickupSlot;

#pragma endregion VehiclePickups

}

/**
* Is a pickup slot currently charging?
***********************************************************************************/

bool FPlayerPickupSlot::IsCharging(bool confirmed) const
{
	return ChargingState == EPickupSlotChargingState::Charging && (confirmed == false || HookTimer >= ABaseVehicle::PickupHookTime);
}

#pragma region SpeedPads

/**
* Collect the speed pads overlapping with a vehicle.
***********************************************************************************/

void ABaseVehicle::CollectSpeedPads()
{
	if (GRIP_OBJECT_VALID(VehicleCollision) == true)
	{
		// Determine which speed pad actors are currently overlapping with this
		// vehicle's collision shell.

		TSet<AActor*> collectedActors;

		VehicleCollision->GetOverlappingActors(collectedActors, ASpeedPad::StaticClass());

		if (collectedActors.Num() > 0)
		{
			// If we have any overlapping speed pads then find the closest one to the vehicle.

			float minDistance = 0.0f;
			AActor* closestSpeedpad = nullptr;
			FVector location = GetActorLocation();

			for (AActor* actor : collectedActors)
			{
				float distance = (actor->GetActorLocation() - location).SizeSquared();

				if (minDistance > distance ||
					closestSpeedpad == nullptr)
				{
					minDistance = distance;
					closestSpeedpad = actor;
				}
			}

			// Collect the closest speed pad from this vehicle.

			(Cast<ASpeedPad>(closestSpeedpad))->OnSpeedPadCollected(this);
		}
	}
}

/**
* Add a temporary boost to the vehicle, for when running over speed pads and the
* like.
*
* amount is between 0 and 1, 1 being 100% more engine power.
* duration is in seconds.
* direction is the world direction to apply the speed boost force.
*
***********************************************************************************/

bool ABaseVehicle::SpeedBoost(ASpeedPad* speedpad, float amount, float duration, const FVector& direction)
{
	FVector thisLocation = speedpad->GetActorLocation();
	FVector thisDirection = speedpad->GetActorRotation().Vector();

	for (FSpeedpadVehicleBoost& boost : Propulsion.SpeedPadBoosts)
	{
		if (speedpad == boost.SpeedPad)
		{
			// Reject the speed pad given as we're already boosting from it.

			return false;
		}

		// Block the speed pad if we're already going over one that is more or less
		// horizontally aligned with the speed pad given. This is to prevent one
		// vehicle hogging a couple of pads in a line across the track when there are
		// other players that need them too. This is a real game-play fix and not
		// something I would have thought we'd need to do, but the players think so.

		FVector location = boost.SpeedPad->GetActorLocation();
		FRotator rotation = boost.SpeedPad->GetActorRotation();
		float radius = boost.SpeedPad->CollisionBox->GetScaledBoxExtent().Size();
		FVector difference = location - thisLocation;
		float distance = difference.Size();

		// Are these speed pads close to one another?

		if (distance < radius * 2.0f)
		{
			difference.Normalize();

			// Are these speed pads broadly facing the same direction?

			if (FVector::DotProduct(rotation.Vector(), thisDirection) > 0.8f)
			{
				// Are these speed pads horizontally aligned?

				if (FMath::Abs(FVector::DotProduct(rotation.Vector(), difference)) < 0.1f)
				{
					return false;
				}
			}
		}
	}

	Propulsion.SpeedPadBoosts.Emplace(FSpeedpadVehicleBoost(speedpad, amount, duration, direction));

	return true;
}

#pragma endregion SpeedPads

#pragma region PickupPads

/**
* Collect the pickups overlapping with a vehicle.
***********************************************************************************/

void ABaseVehicle::CollectPickups()
{
	if (GRIP_OBJECT_VALID(VehicleCollision) == true)
	{
		TSet<AActor*> collectedActors;

		VehicleCollision->GetOverlappingActors(collectedActors, APickup::StaticClass());

		for (AActor* actor : collectedActors)
		{
			APickup* pickup = Cast<APickup>(actor);

			if (pickup->IsCollectible() == true)
			{
				if (pickup->Class == EPickupClass::Pickup)
				{

#pragma region VehiclePickups

					for (int32 slotIndex = 0; slotIndex < NumPickups; slotIndex++)
					{
						if (PickupSlots[slotIndex].State == EPickupSlotState::Empty)
						{
							pickup->OnPickupPadCollected(this);

							EPickupType pickupType = DeterminePickup(pickup);

							if (pickupType != EPickupType::None)
							{
								GivePickup(pickupType, slotIndex, true);

								HUD.Warning(EHUDWarningSource::StandardPickup, 1.0f, 0.666f);
							}

							break;
						}
					}

#pragma endregion VehiclePickups

				}
				else if (pickup->Class == EPickupClass::Health)
				{
					if (RaceState.HitPoints != RaceState.MaxHitPoints)
					{
						pickup->OnPickupPadCollected(this);

						RaceState.HitPoints += (RaceState.MaxHitPoints >> 2);
						RaceState.HitPoints = FMath::Min(RaceState.HitPoints, RaceState.MaxHitPoints);

						HUD.Warning(EHUDWarningSource::HealthPickup, 1.0f, 0.666f);
					}
				}
				else if (pickup->Class == EPickupClass::DoubleDamage)
				{
					if (RaceState.DoubleDamage == 0.0f)
					{
						pickup->OnPickupPadCollected(this);

						RaceState.DoubleDamage = GRIP_DOUBLE_DAMAGE_SECONDS;

						HUD.Warning(EHUDWarningSource::DoubleDamagePickup, 1.0f, 0.666f);
					}
				}
				else if (pickup->Class == EPickupClass::Collectible)
				{
					pickup->OnPickupPadCollected(this);
				}
			}
		}
	}
}

#pragma endregion PickupPads

#pragma region VehiclePickups

/**
* Update the pickup slots.
***********************************************************************************/

void ABaseVehicle::UpdatePickupSlots(float deltaSeconds)
{
	bool charging = false;

	for (int32 i = 0; i < NumPickups; i++)
	{
		PickupSlots[i].Timer += deltaSeconds;

		if (PickupSlots[i].AutoUse == true)
		{
			UsePickup(i, EPickupActivation::Released, AI.BotDriver);
		}

		if (AI.BotDriver == true &&
			PickupSlots[i].BotWillTargetHuman == true)
		{
			// Make sure we have some humans left to hit.

			bool haveHumans = false;

			GRIP_GAME_MODE_LIST_FROM(GetVehicles(), vehicles, PlayGameMode);

			for (ABaseVehicle* vehicle : vehicles)
			{
				if (vehicle->IsAIVehicle() == false &&
					vehicle->IsVehicleDestroyed() == false)
				{
					haveHumans = true;
					break;
				}
			}

			if (haveHumans == false)
			{
				PickupSlots[i].BotWillTargetHuman = false;
			}
		}

		float rate = 0.333f;

		if (PickupSlots[i].IsCharged() == false)
		{
			if (PickupSlots[i].IsCharging(false) == true &&
				PickupSlots[i].HookTimer < PickupHookTime)
			{
				PickupSlots[i].HookTimer += deltaSeconds;

				if (PickupSlots[i].HookTimer >= PickupHookTime)
				{
					if (PickupSlots[i ^ 1].State == EPickupSlotState::Idle &&
						PickupSlots[i ^ 1].Type != EPickupType::None &&
						PickupSlots[i ^ 1].Timer > 0.0f)
					{
						if (IsHumanPlayer() == true &&
							IsCinematicCameraActive() == false)
						{
							PickupChargingSoundComponent = UGameplayStatics::SpawnSound2D(this, HUD.PickupChargingSound);
						}
					}
					else
					{
						PickupSlots[i].CancelCharging();

						if (IsHumanPlayer() == true &&
							IsCinematicCameraActive() == false)
						{
							ClientPlaySound(HUD.PickupNotChargeableSound);
						}
					}

					if (AI.BotDriver == true &&
						PickupSlots[i].IsCharging(false) == true)
					{
						// Release the charging ready for future firing if an AI driver.

						UsePickup(i, EPickupActivation::Released, true);
					}
				}
			}

			charging |= PickupSlots[i].IsCharging(false);

			if (PickupSlots[i].IsCharging(true) == true &&
				PickupSlots[i].ChargeTimer != 1.0f)
			{
				PickupSlots[i].ChargeTimer += deltaSeconds * rate;

				if (PickupSlots[i].ChargeTimer >= 1.0f)
				{
					PickupSlots[i].ChargingState = EPickupSlotChargingState::Charged;
					PickupSlots[i].ChargeTimer = 1.0f;

					ReleasePickupSlot(i ^ 1, false);

					if (IsHumanPlayer() == true &&
						IsCinematicCameraActive() == false)
					{
						ClientPlaySound(HUD.PickupChargedSound);
					}
				}
			}
		}

		if (PickupSlots[i].State == EPickupSlotState::Used)
		{
			// Note that pickups can't be collected if the slot isn't empty, so your change here
			// will allow AI or remote vehicles to collect pickups more swiftly after use than
			// the local human players.

			if (GetPickupSlotAlpha(i) < 0.001f)
			{
				PickupSlots[i].State = EPickupSlotState::Empty;
				PickupSlots[i].Type = EPickupType::None;
				PickupSlots[i].ChargingState = EPickupSlotChargingState::None;
				PickupSlots[i].ChargeTimer = 0.0f;
			}
		}
	}

	if (charging == false &&
		PickupChargingSoundComponent != nullptr &&
		PickupChargingSoundComponent->IsPlaying() == true)
	{
		PickupChargingSoundComponent->Stop();
	}
}

/**
* Start using a pickup.
***********************************************************************************/

void ABaseVehicle::BeginUsePickup(int32 pickupSlot, bool bot, bool force)
{
	if ((pickupSlot >= 0) &&
		(force == true || (PlayGameMode != nullptr && PlayGameMode->PastGameSequenceStart() == true)))
	{
		if (bot != AI.BotDriver ||
			IsVehicleDestroyed() == true)
		{
			return;
		}

		FPlayerPickupSlot& playerPickupSlot = PickupSlots[pickupSlot];

		if (playerPickupSlot.State == EPickupSlotState::Idle &&
			playerPickupSlot.Type != EPickupType::None &&
			playerPickupSlot.Timer > 0.0f)
		{
			if (playerPickupSlot.ChargingState < EPickupSlotChargingState::Charged)
			{
				playerPickupSlot.ChargeTimer = 0.0f;
				playerPickupSlot.HookTimer = 0.0f;

				if (playerPickupSlot.ChargingState == EPickupSlotChargingState::Charging)
				{
					playerPickupSlot.ChargingState = EPickupSlotChargingState::None;
				}
				else
				{
					playerPickupSlot.ChargingState = EPickupSlotChargingState::Charging;
				}
			}

			UsePickup(pickupSlot, EPickupActivation::Pressed, bot);
		}
	}
}

/**
* Use a pickup.
***********************************************************************************/

void ABaseVehicle::UsePickup(int32 pickupSlot, EPickupActivation activation, bool bot)
{
	if (bot != AI.BotDriver)
	{
		// Don't allow players to control AI vehicles.

		return;
	}

	FPlayerPickupSlot& playerPickupSlot = PickupSlots[pickupSlot];

	if (IsVehicleDestroyed() == true)
	{
		// Cancel the charging if we're dead.

		if (playerPickupSlot.IsCharging(false) == true)
		{
			playerPickupSlot.CancelCharging();

			return;
		}
	}

	if (PickupSlots[pickupSlot ^ 1].IsCharging(false) == true)
	{
		// Cancel charging of the other pickup if we're trying to use this one.

		PickupSlots[pickupSlot ^ 1].CancelCharging();
	}

	bool slotIdle = (playerPickupSlot.State == EPickupSlotState::Idle);
	bool slotReady = slotIdle == true && playerPickupSlot.Type != EPickupType::None && (playerPickupSlot.Timer > 0.0f);
	bool prime = activation == EPickupActivation::Pressed && slotReady == true;
	bool release = activation != EPickupActivation::Pressed && ((playerPickupSlot.Activation == EPickupActivation::None && slotReady == true) || playerPickupSlot.Activation == EPickupActivation::Pressed);

	if (prime == true)
	{
		if (playerPickupSlot.IsCharged() == false &&
			playerPickupSlot.HookTimer >= PickupHookTime &&
			playerPickupSlot.Activation == EPickupActivation::None)
		{
			playerPickupSlot.CancelCharging();
		}

		if (playerPickupSlot.ChargingState == EPickupSlotChargingState::Charged)
		{
			// Prime the pickup if it's been charged.

			playerPickupSlot.ChargingState = EPickupSlotChargingState::Primed;
		}

		playerPickupSlot.Activation = EPickupActivation::Pressed;
	}
	else if (release == true)
	{
		switch (playerPickupSlot.ChargingState)
		{
		case EPickupSlotChargingState::Charging:
			if (playerPickupSlot.HookTimer >= PickupHookTime)
			{
				// Do nothing if we're into the charging sequence now.

				return;
			}

			// Otherwise just fall through and use the pickup as normal, uncharged.

			break;

		case EPickupSlotChargingState::Charged:
			// It's charged, but not primed, so do nothing.
			return;

		case EPickupSlotChargingState::Primed:
			// It's charged and primed, so fall through and use the pickup.
			break;
		}

		if (playerPickupSlot.IsCharged() == false)
		{
			playerPickupSlot.CancelCharging();
		}

#pragma region PickupTurbo

		FActorSpawnParameters spawnParams;

		spawnParams.Owner = this;
		spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		switch (playerPickupSlot.Type)
		{
		case EPickupType::TurboBoost:
		{
			if (release == true)
			{
				if (GetSpeedKPH() > 100.0f &&
					IsUsingTurbo() == false &&
					Control.ThrottleInput >= 0.5f)
				{
					ATurbo* turbo = nullptr;

					if (playerPickupSlot.IsCharged() == true)
					{
						if (Level2TurboBlueprint != nullptr)
						{
							turbo = GetWorld()->SpawnActor<ATurbo>(Level2TurboBlueprint, VehicleMesh->GetComponentLocation(), VehicleMesh->GetComponentRotation(), spawnParams);
						}
					}
					else
					{
						if (Level1TurboBlueprint != nullptr)
						{
							turbo = GetWorld()->SpawnActor<ATurbo>(Level1TurboBlueprint, VehicleMesh->GetComponentLocation(), VehicleMesh->GetComponentRotation(), spawnParams);
						}
					}

					if (GRIP_OBJECT_VALID(turbo) == true)
					{
						playerPickupSlot.Activation = activation;

						turbo->ActivatePickup(this, pickupSlot, activation, playerPickupSlot.IsCharged());

						Propulsion.RaiseFrontAchieved = 0.0f;

						playerPickupSlot.Pickup = turbo;
						playerPickupSlot.Timer = 0.0f;
						playerPickupSlot.State = EPickupSlotState::Active;

						FGameEvent gameEvent;

						gameEvent.LaunchVehicleIndex = VehicleIndex;
						gameEvent.TargetVehicleIndex = -1;
						gameEvent.PickupUsed = playerPickupSlot.Type;
						gameEvent.PickupUsedWasCharged = playerPickupSlot.IsCharged();
						gameEvent.EventType = EGameEventType::Used;

						PlayGameMode->AddGameEvent(gameEvent);
					}
				}
				else
				{
					PlayDeniedSound();
				}
			}
		}
		break;

#pragma region PickupGun

		case EPickupType::GatlingGun:
		{
			if (release == true)
			{
				if (IsUsingGatlingGun() == false)
				{
					AGatlingGun* gatlingGun = nullptr;

					if (playerPickupSlot.IsCharged() == true)
					{
						if (Level2GatlingGunBlueprint != nullptr)
						{
							gatlingGun = GetWorld()->SpawnActor<AGatlingGun>(Level2GatlingGunBlueprint, VehicleMesh->GetComponentLocation(), VehicleMesh->GetComponentRotation(), spawnParams);
						}
					}
					else
					{
						if (Level1GatlingGunBlueprint != nullptr)
						{
							gatlingGun = GetWorld()->SpawnActor<AGatlingGun>(Level1GatlingGunBlueprint, VehicleMesh->GetComponentLocation(), VehicleMesh->GetComponentRotation(), spawnParams);
						}
					}

					if (GRIP_OBJECT_VALID(gatlingGun) == true)
					{
						playerPickupSlot.Activation = activation;

						gatlingGun->ActivatePickup(this, pickupSlot, activation, playerPickupSlot.IsCharged());

						playerPickupSlot.Pickup = gatlingGun;
						playerPickupSlot.Timer = 0.0f;
						playerPickupSlot.State = EPickupSlotState::Active;

						FGameEvent gameEvent;
						ABaseVehicle* targettedVehicle = Cast<ABaseVehicle>(gatlingGun->Target.Get());

						gameEvent.LaunchVehicleIndex = VehicleIndex;
						gameEvent.TargetVehicleIndex = (targettedVehicle != nullptr) ? targettedVehicle->VehicleIndex : -1;
						gameEvent.PickupUsed = playerPickupSlot.Type;
						gameEvent.PickupUsedWasCharged = playerPickupSlot.IsCharged();
						gameEvent.EventType = EGameEventType::Used;

						PlayGameMode->AddGameEvent(gameEvent);
					}
				}
				else
				{
					PlayDeniedSound();
				}
			}
		}
		break;

#pragma endregion PickupGun

#pragma region PickupMissile

		case EPickupType::HomingMissile:
		{
			FMissileEjection& ejectionState = EjectionState[pickupSlot];

			if (release == true &&
				MissilePortInUse == false &&
				ejectionState.State == EMissileEjectionState::Inactive)
			{
				MissilePortInUse = true;

				// Develop the list of missile targets.

				float weight = 0.0f;
				AActor* lastTarget = HUD.GetCurrentMissileTargetActor(pickupSlot);
				int32 numTargets = (playerPickupSlot.IsCharged() == true) ? 2 : 1;

				HUD.CurrentMissileTarget[pickupSlot] = -1;

				ejectionState.PickupTargets.Empty();

				AHomingMissile::SelectTarget(this, &playerPickupSlot, lastTarget, ejectionState.PickupTargets, weight, numTargets, AI.BotDriver);

				playerPickupSlot.Activation = activation;

				playerPickupSlot.Pickup.Reset();
				playerPickupSlot.Timer = 0.0f;
				playerPickupSlot.State = EPickupSlotState::Active;

				ejectionState.State = EMissileEjectionState::BayOpening;

				FGameEvent gameEvent;

				gameEvent.LaunchVehicleIndex = VehicleIndex;
				gameEvent.TargetVehicleIndex = -1;
				gameEvent.PickupUsed = playerPickupSlot.Type;
				gameEvent.PickupUsedWasCharged = playerPickupSlot.IsCharged();
				gameEvent.EventType = EGameEventType::Preparing;

				PlayGameMode->AddGameEvent(gameEvent);
			}
		}
		break;

#pragma endregion PickupMissile

#pragma region PickupShield

		case EPickupType::Shield:
		{
			if (release == true)
			{
				if (GRIP_POINTER_VALID(Shield) == false)
				{
					if (playerPickupSlot.IsCharged() == true)
					{
						if (Level2ShieldBlueprint != nullptr)
						{
							Shield = GetWorld()->SpawnActor<AShield>(Level2ShieldBlueprint, VehicleMesh->GetComponentLocation(), VehicleMesh->GetComponentRotation(), spawnParams);
						}
					}
					else
					{
						if (Level1ShieldBlueprint != nullptr)
						{
							Shield = GetWorld()->SpawnActor<AShield>(Level1ShieldBlueprint, VehicleMesh->GetComponentLocation(), VehicleMesh->GetComponentRotation(), spawnParams);
						}
					}

					if (GRIP_POINTER_VALID(Shield) == true)
					{
						playerPickupSlot.Activation = activation;

						Shield->ActivatePickup(this, pickupSlot, activation, playerPickupSlot.IsCharged());

						playerPickupSlot.Pickup = Shield;
						playerPickupSlot.Timer = 0.0f;
						playerPickupSlot.State = EPickupSlotState::Active;

						FGameEvent gameEvent;

						gameEvent.LaunchVehicleIndex = VehicleIndex;
						gameEvent.TargetVehicleIndex = -1;
						gameEvent.PickupUsed = playerPickupSlot.Type;
						gameEvent.PickupUsedWasCharged = playerPickupSlot.IsCharged();
						gameEvent.EventType = EGameEventType::Used;

						PlayGameMode->AddGameEvent(gameEvent);
					}
				}
				else
				{
					PlayDeniedSound();
				}
			}
		}
		break;

#pragma endregion PickupShield

		default:
			break;
		}

#pragma endregion PickupTurbo

	}
}

/**
* Determine which pickup to give to a vehicle.
***********************************************************************************/

EPickupType ABaseVehicle::DeterminePickup(APickup* pickup)
{
	if (pickup->GivePickup == EPickupType::None)
	{
		return pickup->GivePickup;
	}

	EPickupType pickupType = EPickupType::None;

	if (pickup->GivePickup >= EPickupType::Num)
	{
		pickup->GivePickup = EPickupType::Random;
	}

	if (pickup->GivePickup != EPickupType::Random)
	{
		if (pickup->GivePickup < EPickupType::Num)
		{
			pickupType = pickup->GivePickup;
		}
		else
		{
			pickup->GivePickup = EPickupType::Random;
		}
	}

	if (pickup->GivePickup == EPickupType::Random)
	{
		// Dimensions are rough race position (0 - 2 (0 is winning and 2 is losing)), probability.

		const int32 raceSections = 3;

		static float raceProbabilities[raceSections][(int32)EPickupType::Num];

		if (ProbabilitiesInitialized == false)
		{
			// ProbabilitiesInitialized is set to false when each vehicle calls BeginPlay.
			// So this initialization is guaranteed to happen at the start of every race.

			ProbabilitiesInitialized = true;

			FPickupAssignmentRatios* raceRow = nullptr;
			FDifficultyCharacteristics& difficulty = PlayGameMode->GetDifficultyCharacteristics();

			for (int32 section = 0; section < raceSections; section++)
			{
				switch (section)
				{
				default:
					raceRow = &difficulty.PickupAssignmentRatios.Race.Leading;
					break;
				case 1:
					raceRow = &difficulty.PickupAssignmentRatios.Race.Central;
					break;
				case 2:
					raceRow = &difficulty.PickupAssignmentRatios.Race.Trailing;
					break;
				}

				// Zero and set the probabilities from the play game mode blueprint.

				for (int32 i = 0; i < (int32)EPickupType::Num; i++)
				{
					raceProbabilities[section][i] = 0.0f;
				}

#pragma region PickupShield

				raceProbabilities[section][(int32)EPickupType::Shield] = raceRow->Shield;

#pragma endregion PickupShield

#pragma region PickupTurbo

				raceProbabilities[section][(int32)EPickupType::TurboBoost] = raceRow->TurboBoost;

#pragma endregion PickupTurbo

#pragma region PickupMissile

				raceProbabilities[section][(int32)EPickupType::HomingMissile] = raceRow->HomingMissile;

#pragma endregion PickupMissile

#pragma region PickupGun

				raceProbabilities[section][(int32)EPickupType::GatlingGun] = raceRow->GatlingGun;

#pragma endregion PickupGun

			}
		}

		bool valid = false;
		int32 attempts = 0;

		// Use the missile for now.

		pickupType = EPickupType::HomingMissile;

		int32 positionIndex = (PlayGameMode != nullptr) ? PlayGameMode->GetPlayerRacePickupIndex(this) : 0;
		float* probabilities = &raceProbabilities[positionIndex][0];

		// Ensure we have a pickup array for each of the race sections.

		while (QueuedPickups.Num() < raceSections)
		{
			QueuedPickups.Emplace(TArray<EPickupType>());
		}

		TArray<EPickupType>& queuedPickups = QueuedPickups[positionIndex];

		// Attempts check just to ensure we don't get stiffed by the criteria tying us up in knots.

		while (valid == false && attempts++ < 100)
		{
			if (queuedPickups.Num() == 0)
			{
				// If we have no pickups in our array for this position index, then fill it up
				// ready for use. This can happen multiple times in an event as the array is
				// drained when you collect a pickup from it.

				TArray<EPickupType> orderedPickups;

				// So first fill the array with a while bunch of pickups according how often
				// we've been told they're to be collected by the play game mode blueprint.

				for (int32 i = 0; i < (int32)EPickupType::Num; i++)
				{
					int32 numChances = FMath::CeilToInt(probabilities[i]);

					for (int32 j = 0; j < numChances; j++)
					{
						orderedPickups.Emplace((EPickupType)i);
					}
				}

				// Shuffle the ordered list of pickups to randomize them and place them
				// into queuedPickups.

				queuedPickups.Reserve(orderedPickups.Num());

				while (orderedPickups.Num() > 0)
				{
					int32 index = FMath::Rand() % orderedPickups.Num();

					queuedPickups.Emplace(orderedPickups[index]);

					orderedPickups.RemoveAt(index, 1, false);
				}
			}

			// Collect the pickup from the end of the queue.

			pickupType = queuedPickups[queuedPickups.Num() - 1];
			queuedPickups.RemoveAt(queuedPickups.Num() - 1, 1, false);

			if (VehicleIndex == 0)
			{
				UE_LOG(GripLogPickups, Log, TEXT("Attempting to give pickup %d for position index %d"), (int32)pickupType, positionIndex);
			}

			switch (pickupType)
			{
			default:
				valid = true;
				break;

			case EPickupType::Shield:
				// Can't collect a pickup if you already have one or this vehicle somehow isn't setup for a shield.
				valid = (HasPickup(pickupType) == false && VehicleShield != nullptr);
				break;

			case EPickupType::GatlingGun:
				// Can't collect a pickup if this vehicle somehow isn't setup for a gun.
				valid = (VehicleGun != nullptr);
				break;
			}

			if (valid == false &&
				VehicleIndex == 0)
			{
				UE_LOG(GripLogPickups, Log, TEXT("Rejected pickup %d because it wasn't valid right now"), (int32)pickupType);
			}

			if (valid == true)
			{
				int32 maxRepeat = 2;
				int32 maxPresent = 0;
				float minSeconds = 0.0f;

				FDifficultyCharacteristics& difficulty = PlayGameMode->GetDifficultyCharacteristics();
				FPickupAssignmentMaximums& table = difficulty.PickupAssignmentRatios.Race.PickupMaximums;

				switch (pickupType)
				{
				case EPickupType::Shield:
					maxRepeat = table.ShieldMaxRepeat;
					maxPresent = table.ShieldMaxPresent;
					minSeconds = table.ShieldMinSeconds;
					break;
				case EPickupType::TurboBoost:
					maxRepeat = table.TurboBoostMaxRepeat;
					maxPresent = table.TurboBoostMaxPresent;
					minSeconds = table.TurboBoostMinSeconds;
					break;
				case EPickupType::HomingMissile:
					maxRepeat = table.HomingMissileMaxRepeat;
					maxPresent = table.HomingMissileMaxPresent;
					minSeconds = table.HomingMissileMinSeconds;
					break;
				case EPickupType::GatlingGun:
					maxRepeat = table.GatlingGunMaxRepeat;
					maxPresent = table.GatlingGunMaxPresent;
					minSeconds = table.GatlingGunMinSeconds;
					break;
				}

				if (minSeconds != 0.0f &&
					PlayGameMode->GetRealTimeGameClock() - PlayGameMode->PickupLastUsed(pickupType) < minSeconds)
				{
					// Don't give a pickup if one has already been used within the time-frame we've been given for minimum reuse.

					if (VehicleIndex == 0)
					{
						UE_LOG(GripLogPickups, Log, TEXT("Rejected pickup %d because it's not been long enough since the last one"), (int32)pickupType);
					}

					valid = false;
				}

				if (valid == true &&
					maxPresent > 0 &&
					PlayGameMode->NumPickupsPresent(pickupType) >= maxPresent)
				{
					// Don't give a pickup if we've already got too many of them in the world.

					if (VehicleIndex == 0)
					{
						UE_LOG(GripLogPickups, Log, TEXT("Rejected pickup %d because there is already too many present"), (int32)pickupType);
					}

					valid = false;
				}

				if (valid == true &&
					LastPickupGiven == pickupType)
				{
					int32 repeated = LastPickupRepeatCount;

					// This takes into account the "other" slot if we keep filling / using one particular slot.

					for (FPlayerPickupSlot& slot : PickupSlots)
					{
						if (slot.Type == pickupType &&
							slot.State != EPickupSlotState::Empty &&
							slot.PickupCount < PickupCount - maxRepeat)
						{
							repeated++;
						}
					}

					if (repeated >= maxRepeat)
					{
						if (VehicleIndex == 0)
						{
							UE_LOG(GripLogPickups, Log, TEXT("Rejected pickup %d because it's been repeated too many times"), (int32)pickupType);
						}

						valid = false;
					}
				}
			}
		}

		if (valid == false)
		{
			// If we somehow couldn't determine a valid pickup then just give a turbo boost by default.

			pickupType = EPickupType::TurboBoost;
		}

		if (VehicleIndex == 0)
		{
			UE_LOG(GripLogPickups, Log, TEXT("Given pickup %d"), (int32)pickupType);
		}
	}

	if (LastPickupGiven == pickupType)
	{
		LastPickupRepeatCount++;
	}
	else
	{
		LastPickupGiven = pickupType;
		LastPickupRepeatCount = 1;
	}

	return pickupType;
}

/**
* Force a particular pickup to a vehicle.
***********************************************************************************/

void ABaseVehicle::ForcePickup(EPickupType type, int32 pickupSlot)
{
	PickupSlots[pickupSlot].State = EPickupSlotState::Idle;
	PickupSlots[pickupSlot].Activation = EPickupActivation::None;
	PickupSlots[pickupSlot].Type = type;
	PickupSlots[pickupSlot].AutoUse = false;
}

/**
* Determine the targets.
***********************************************************************************/

void ABaseVehicle::DetermineTargets(float deltaSeconds, const FVector& location, const FVector& direction)
{
	if (AI.BotVehicle == false)
	{
		for (int32 pickupSlot = 0; pickupSlot < 2; pickupSlot++)
		{
			// If the current target has disappeared from the scene then forget about it.

			if (HUD.CurrentMissileTargetIsValid(pickupSlot) == false)
			{
				HUD.CurrentMissileTarget[pickupSlot] = -1;
			}

			TArray<TWeakObjectPtr<AActor>> targets;

			AActor* missileTarget = HUD.GetCurrentMissileTargetActor(pickupSlot);

			HUD.CurrentMissileTarget[pickupSlot] = -1;

			if (PickupSlots[pickupSlot].State == EPickupSlotState::Idle)
			{
				float weight = 0.0f;

#pragma region PickupMissile

				if (PickupSlots[pickupSlot].Type == EPickupType::HomingMissile)
				{
					AActor* newTarget = missileTarget;

					AHomingMissile::SelectTarget(this, nullptr, newTarget, targets, weight, 4, AI.BotDriver);
				}

#pragma endregion PickupMissile

#pragma region PickupGun

				if (PickupSlots[pickupSlot].Type == EPickupType::GatlingGun)
				{
					AGatlingGun* gun = Level1GatlingGunBlueprint->GetDefaultObject<AGatlingGun>();
					AActor* newTarget = AGatlingGun::SelectTarget(this, nullptr, gun->AutoAiming, weight, false);

					if (newTarget != nullptr)
					{
						targets.Emplace(newTarget);
					}
				}

#pragma endregion PickupGun

			}

			if (missileTarget == nullptr &&
				targets.Num() > 0)
			{
				missileTarget = targets[0].Get();
			}

			HUD.SwitchTargetTimer -= deltaSeconds * 10.0f;
			HUD.SwitchTargetTimer = FMath::Max(0.0f, HUD.SwitchTargetTimer);

			if (targets.Num() == 0)
			{
				HUD.PickupTargets[pickupSlot].Empty();
			}
			else
			{
				// Update all of the missile targets.

				// Remove old targets.

				for (int32 i = 0; i < HUD.PickupTargets[pickupSlot].Num(); i++)
				{
					bool found = false;

					for (int32 j = 0; j < targets.Num(); j++)
					{
						if (HUD.PickupTargets[pickupSlot][i].Target.Get() == targets[j].Get())
						{
							found = true;
							break;
						}
					}

					if (found == false)
					{
						HUD.PickupTargets[pickupSlot].RemoveAt(i--, 1, false);
					}
				}

				// Add new targets.

				for (int32 j = 0; j < targets.Num(); j++)
				{
					if (GRIP_OBJECT_VALID(targets[j]) == true)
					{
						bool found = false;

						for (int32 i = 0; i < HUD.PickupTargets[pickupSlot].Num(); i++)
						{
							if (HUD.PickupTargets[pickupSlot][i].Target.Get() == targets[j].Get())
							{
								found = true;
								break;
							}
						}

						if (found == false)
						{
							ABaseVehicle* vehicle = Cast<ABaseVehicle>(targets[j].Get());

							HUD.PickupTargets[pickupSlot].Emplace(FHUDTarget(targets[j], vehicle != nullptr));
						}
					}
				}

				// Sort the targets by address.

				HUD.PickupTargets[pickupSlot].Sort([] (const FHUDTarget& object1, const FHUDTarget& object2)
					{
						return (uint64)object1.Target.Get() < (uint64)object2.Target.Get();
					});

				for (int32 i = 0; i < HUD.PickupTargets[pickupSlot].Num(); i++)
				{

#pragma region PickupMissile

					if (HUD.PickupTargets[pickupSlot][i].Target.Get() == missileTarget)
					{
						HUD.CurrentMissileTarget[pickupSlot] = i;
						HUD.TargetLocation[pickupSlot] = AHomingMissile::GetTargetLocationFor(missileTarget, FVector::ZeroVector);

						if (HUD.SwitchTargetTimer != 0.0f)
						{
							if (GRIP_POINTER_VALID(HUD.LastTarget[pickupSlot]))
							{
								HUD.LastTargetLocation[pickupSlot] = AHomingMissile::GetTargetLocationFor(HUD.LastTarget[pickupSlot].Get(), FVector::ZeroVector);
							}

							HUD.TargetLocation[pickupSlot] = FMath::Lerp(HUD.TargetLocation[pickupSlot], HUD.LastTargetLocation[pickupSlot], HUD.SwitchTargetTimer);
						}
					}

#pragma endregion PickupMissile

					HUD.PickupTargets[pickupSlot][i].TargetTimer += deltaSeconds;
					HUD.PickupTargets[pickupSlot][i].TargetTimer = FMath::Min(1.0f, HUD.PickupTargets[pickupSlot][i].TargetTimer);
				}
			}

			bool findThreats = true;

			if (findThreats == true)
			{
				// Update all of the mine targets.

				targets.Empty(16);

				// Remove old targets.

				for (int32 i = 0; i < HUD.ThreatTargets.Num(); i++)
				{
					bool found = false;

					for (int32 j = 0; j < targets.Num(); j++)
					{
						if (HUD.ThreatTargets[i].Target.Get() == targets[j].Get())
						{
							found = true;
							break;
						}
					}

					if (found == false)
					{
						HUD.ThreatTargets.RemoveAt(i--, 1, false);
					}
				}

				// Add new targets.

				for (int32 j = 0; j < targets.Num(); j++)
				{
					bool found = false;

					for (int32 i = 0; i < HUD.ThreatTargets.Num(); i++)
					{
						if (HUD.ThreatTargets[i].Target.Get() == targets[j].Get())
						{
							found = true;
							break;
						}
					}

					if (found == false)
					{
						HUD.ThreatTargets.Emplace(FHUDTarget(targets[j], false));
					}
				}

				for (int32 i = 0; i < HUD.ThreatTargets.Num(); i++)
				{
					HUD.ThreatTargets[i].TargetTimer += deltaSeconds;
					HUD.ThreatTargets[i].TargetTimer = FMath::Min(1.0f, HUD.ThreatTargets[i].TargetTimer);
				}
			}
		}

		// If we have two pickups of the same type, ensure the second pickup isn't targeting the same target
		// as the first where possible.

#pragma region PickupMissile

		if (PickupSlots[0].Type == PickupSlots[1].Type &&
			HUD.PickupTargets[0].IsValidIndex(HUD.CurrentMissileTarget[0]) == true &&
			HUD.PickupTargets[1].IsValidIndex(HUD.CurrentMissileTarget[1]) == true &&
			HUD.PickupTargets[0][HUD.CurrentMissileTarget[0]].Target.Get() == HUD.PickupTargets[1][HUD.CurrentMissileTarget[1]].Target.Get())
		{
			int32 pickupSlot = 1;

			if (HUD.PickupTargets[pickupSlot].Num() > 1)
			{
				HUD.LastTarget[pickupSlot] = HUD.GetCurrentMissileTargetActor(pickupSlot);
				HUD.LastTargetLocation[pickupSlot] = AHomingMissile::GetTargetLocationFor(HUD.LastTarget[pickupSlot].Get(), FVector::ZeroVector);

				HUD.CurrentMissileTarget[pickupSlot] = (HUD.CurrentMissileTarget[pickupSlot] + 1) % HUD.PickupTargets[pickupSlot].Num();
				HUD.SwitchTargetTimer = (HUD.LastTarget[pickupSlot].IsValid() == true) ? 1.0f : 0.0f;
			}
		}

#pragma endregion PickupMissile

	}
}

/**
* Get the alpha for a pickup slot.
***********************************************************************************/

float ABaseVehicle::GetPickupSlotAlpha(int32 pickupSlot) const
{
	switch (PickupSlots[pickupSlot].State)
	{
	case EPickupSlotState::Active:
		return ((FMath::FloorToInt(GetRealTimeClock() * 4.0f) & 1) ? 0.2f : 1.0f);

	case EPickupSlotState::Empty:
		return 0.0f;

	case EPickupSlotState::Idle:
		return FMath::Min(PickupSlots[pickupSlot].Timer * 3.0f, 1.0f);

	case EPickupSlotState::Used:
		return 1.0f - FMath::Min(PickupSlots[pickupSlot].Timer * 1.0f, 1.0f);
	}

	return 0.0f;
}

/**
* Get the scale for a pickup slot.
***********************************************************************************/

float ABaseVehicle::GetPickupSlotScale(int32 pickupSlot) const
{
	switch (PickupSlots[pickupSlot].State)
	{
	case EPickupSlotState::Idle:
	{
		float timer = FMath::Min(PickupSlots[pickupSlot].Timer * 2.5f, 1.0f);

		timer = FMath::Sqrt(timer);
		timer = FMath::Sqrt(timer);

		return 1.0f + FMath::Sin(timer * PI) * 0.75f;
	}
	case EPickupSlotState::Used:
		return FMath::Cos((FMath::Min(PickupSlots[pickupSlot].Timer * 1.0f, 1.0f)));
	default:
		return 1.0f;
	}
}

/**
* Release the pickup in a particular slot.
***********************************************************************************/

void ABaseVehicle::ReleasePickupSlot(int32 pickupSlot, bool animate)
{
	FPlayerPickupSlot& playerPickupSlot = PickupSlots[pickupSlot];

	if (playerPickupSlot.State == EPickupSlotState::Active)
	{

#pragma region PickupMissile

		if (playerPickupSlot.Type == EPickupType::HomingMissile)
		{
			MissilePortInUse = false;
			EjectionState[pickupSlot].State = EMissileEjectionState::Inactive;
		}

#pragma endregion PickupMissile

	}

	if (playerPickupSlot.State != EPickupSlotState::Used &&
		playerPickupSlot.State != EPickupSlotState::Empty)
	{
		playerPickupSlot.Timer = 0.0f;
		playerPickupSlot.EfficacyTimer = 0.0f;
		playerPickupSlot.State = (animate == true) ? EPickupSlotState::Used : EPickupSlotState::Empty;

		if (animate == false)
		{
			playerPickupSlot.ChargingState = EPickupSlotChargingState::None;
			playerPickupSlot.ChargeTimer = 0.0f;
		}

		playerPickupSlot.HookTimer = 0.0f;
		playerPickupSlot.Pickup.Reset();

		if (animate == false)
		{
			playerPickupSlot.Type = EPickupType::None;
		}
	}
}

/**
* Switch the target for a pickup slot.
***********************************************************************************/

void ABaseVehicle::SwitchPickupTarget(int32 pickupSlot)
{

#pragma region PickupMissile

	int32 start = pickupSlot;
	int32 end = pickupSlot;

	if (pickupSlot == -1)
	{
		start = 0;
		end = NumPickups - 1;
	}

	for (pickupSlot = start; pickupSlot <= end; pickupSlot++)
	{
		if (HUD.PickupTargets[pickupSlot].Num() > 1)
		{
			HUD.LastTarget[pickupSlot] = HUD.GetCurrentMissileTargetActor(pickupSlot);
			HUD.LastTargetLocation[pickupSlot] = AHomingMissile::GetTargetLocationFor(HUD.LastTarget[pickupSlot].Get(), FVector::ZeroVector);

			HUD.CurrentMissileTarget[pickupSlot] = (HUD.CurrentMissileTarget[pickupSlot] + 1) % HUD.PickupTargets[pickupSlot].Num();
			HUD.SwitchTargetTimer = (HUD.LastTarget[pickupSlot].IsValid() == true) ? 1.0f : 0.0f;

			break;
		}
	}

#pragma endregion PickupMissile

}

/**
* Is a pickup currently charging at all?
***********************************************************************************/

bool ABaseVehicle::PickupIsCharging(bool ignoreTurbos)
{
	for (FPlayerPickupSlot& pickup : PickupSlots)
	{
		if (pickup.IsCharging(true) == true)
		{
			if (ignoreTurbos == false ||
				pickup.Type != EPickupType::TurboBoost)
			{
				return true;
			}
		}
	}

	return false;
}

#pragma endregion VehiclePickups

#pragma region PickupShield

/**
* Is a shield currently active on the vehicle?
***********************************************************************************/

bool ABaseVehicle::IsShieldActive() const
{
	return GRIP_POINTER_VALID(Shield) && Shield->IsActive();
}

/**
* Is a shield currently active on the vehicle and protecting against a given
* position?
***********************************************************************************/

bool ABaseVehicle::IsShielded(const FVector& position) const
{
	if (GRIP_POINTER_VALID(Shield) == true)
	{
		if (Shield->RearOnly == true)
		{
			FVector difference = VehicleMesh->GetComponentTransform().InverseTransformVector(position - GetCenterLocation());

			return (difference.X < 0.0f);
		}

		return true;
	}

	return false;
}

/**
* Release any active shield.
***********************************************************************************/

void ABaseVehicle::ReleaseShield(bool permanently)
{
	if (GRIP_POINTER_VALID(Shield) == true)
	{
		Shield->DestroyShield();
		Shield.Reset();
	}
}

/**
* Damage the shield by a given amount.
***********************************************************************************/

void ABaseVehicle::DamageShield(int32 hitPoints, int32 aggressorVehicleIndex)
{
	if (IsShieldActive() == true)
	{
		Shield->Impact(hitPoints);

		if (Shield->IsDestroyed() == true)
		{
			if (hitPoints >= 10)
			{
				ABaseVehicle* vehicle = PlayGameMode->GetVehicleForVehicleIndex(aggressorVehicleIndex);

				if (vehicle != nullptr)
				{
					int32 numPoints = 100;

					if (vehicle->AddPoints(numPoints, true, this, GetActorLocation()) == true)
					{
						vehicle->ShowStatusMessage(FStatusMessage(PlayGameMode->GetXPMessage(EPickupType::Shield, numPoints)), true, false);
					}
				}
			}

			ReleaseShield(true);
		}
		else
		{
			if (hitPoints >= 10)
			{
				ABaseVehicle* vehicle = PlayGameMode->GetVehicleForVehicleIndex(aggressorVehicleIndex);

				if (vehicle != nullptr)
				{
					vehicle->AddPoints(100, true, this, GetActorLocation());
				}

				AddPoints(100, false, nullptr, GetActorLocation());
			}
		}
	}
}

/**
* Destroy the shield.
***********************************************************************************/

void ABaseVehicle::DestroyShield(int32 aggressorVehicleIndex)
{
	if (GRIP_POINTER_VALID(Shield))
	{
		ReleaseShield(true);

		ABaseVehicle* vehicle = PlayGameMode->GetVehicleForVehicleIndex(aggressorVehicleIndex);

		int32 numPoints = 100;

		if (vehicle->AddPoints(numPoints, true, this, GetActorLocation()) == true)
		{
			vehicle->ShowStatusMessage(FStatusMessage(PlayGameMode->GetXPMessage(EPickupType::Shield, numPoints)), true, false);
		}
	}
}

#pragma endregion PickupShield

#pragma region PickupGun

/**
* Apply a bullet round force.
***********************************************************************************/

bool ABaseVehicle::BulletRound(float strength, int32 hitPoints, int32 aggressorVehicleIndex, const FVector& position, const FVector& fromPosition, bool charged, float spinSide)
{
	VehicleMesh->IdleUnlock();

#pragma region PickupShield

	if (IsShieldActive() == true &&
		IsShielded(fromPosition) == true)
	{
		DamageShield(hitPoints, aggressorVehicleIndex);

		return false;
	}

#pragma endregion PickupShield

	float massScale = Physics.CurrentMass / 5000.0f;

	BulletHitTimer = APickup::GetEfficacyDelayBeforeUse(EPickupType::Shield, this);

	strength *= 2.25f;

	// Lift the vehicle up in the air a bit and push it sideways a little also.

	FVector direction(0.0f, 1000000.0f * strength, 0.0f);

	if (FMath::RandBool() == true)
	{
		direction *= -1.0f;
	}

	if (IsGrounded() == true)
	{
		direction.Z = 5000000.0f * FMath::Min(0.25f, strength) * ((IsFlipped() == true) ? -1.0f : 1.0f);
	}

	const FTransform& transform = VehicleMesh->GetComponentTransform();

	direction = transform.TransformVector(direction);

	VehicleMesh->AddImpulse(direction * massScale * 2.0f);

	// If we're going greater than 100 kph then slow the vehicle down a bit.

	if (GetSpeedKPH() > 100.0f)
	{
		// More stopping power for charged hits.

		if (charged == true)
		{
			VehicleMesh->AddImpulse(GetVelocityOrFacingDirection() * massScale * -1500000.0f);
		}
		else
		{
			VehicleMesh->AddImpulse(GetVelocityOrFacingDirection() * massScale * -1150000.0f);
		}
	}

	// Now spin it around a bit.

	if (charged == true &&
		(FMath::Rand() & 3) != 0)
	{
		// For charged bullets, let 3 out of 4 rounds all hit on one side to promote a strong spin.

		// Just add some random left/right angular velocity (Z is yaw), and a little pitch (Y is pitch).

		direction = FVector(0.0f, FMath::FRandRange(-0.15f, 0.15f), FMath::FRandRange(0.1f, 0.15f) * spinSide);
	}
	else
	{
		// Just add some random left/right angular velocity (Z is yaw), and a little pitch (Y is pitch).

		direction = FVector(0.0f, FMath::FRandRange(-0.25f, 0.25f), FMath::FRandRange(-0.25f, 0.25f));

		if (IsAirborne() == false)
		{
			direction *= FMathEx::GetRatio(FMath::Abs(FVector::DotProduct(GetVelocityOrFacingDirection(), GetFacingDirection())), 0.5f, 1.0f);
		}
	}

	// Bring it into world space before applying it to the angular velocity.

	direction = transform.TransformVector(direction);
	direction.Normalize();
	direction *= 75.0f * strength;

	if (IsGrounded() == false)
	{
		direction *= 0.5f;
	}

	// Note this is not the best way of inducing the angular velocity. For example, if the vehicle was
	// spinning wildly before being hit, this change in velocity to a set value could well stop the
	// vehicle spinning so badly and help it out. It's much better to add a torque to the vehicle
	// instead if you can figure out the math for how strong the torque should be to bring about
	// the desired physical behavior.

	VehicleMesh->SetPhysicsAngularVelocityInDegrees(direction, true);

	return true;
}

/**
* Get the orientation of the gun.
***********************************************************************************/

FQuat ABaseVehicle::GetGunOrientation() const
{
	FVector forward = GetFacingDirection();
	FVector up = GetLaunchDirection();
	FQuat quaternion = FQuat::Identity;

	FMathEx::GetQuaternionFromForwardUp(forward, up, quaternion);

	return quaternion;
}

/**
* Get the direction for firing a round.
***********************************************************************************/

FVector ABaseVehicle::GetGunRoundDirection(FVector direction) const
{
	if (IsGrounded() == true)
	{
		// Aim the machine gun along the ground rather than where the car is pointing
		// as this may well be tilting up and down while it's driving.

		FVector up = GetSurfaceNormal();
		FVector newDirection = direction - (up * FVector::DotProduct(direction, up));

		newDirection.Normalize();

		if (FVector::DotProduct(direction, newDirection) > 0.9f)
		{
			direction = newDirection;
		}
	}

	return direction;
}

/**
* Get the round ejection properties.
***********************************************************************************/

FVector ABaseVehicle::EjectGunRound(int32 roundLocation, bool charged)
{
	// Spawn the muzzle flash particle system.

	FName muzzleLocation = ((roundLocation == 0) ? "MachineGun_L" : "MachineGun_R");

	UParticleSystemComponent* muzzleFlash = SpawnParticleSystem(VehicleGun->MuzzleFlashEffect, muzzleLocation, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, (charged == true) ? 2.0f : 1.0f);

	muzzleFlash->SetOwnerNoSee(IsCockpitView());

	// Spawn the shell ejection particle system.

	FName shellLocation = ((roundLocation == 0) ? "ShellEject_TL" : "ShellEject_TR");
	FRotator rotation = FRotator(FMath::FRandRange(-10.0, 10.0f), 0.0f, FMath::FRandRange(-15.0, 15.0f)) + GetActorRotation();
	FVector velocity = rotation.RotateVector(FVector(0.0, 0.0f, 6.5f * 100.0f));

	if (IsFlipped() == true)
	{
		shellLocation = (roundLocation == 0) ? "ShellEject_BL" : "ShellEject_BR";
		velocity *= -1.0f;
	}

	velocity += GetVelocity() * 0.9f;

	UParticleSystemComponent* shellEjection = SpawnParticleSystem(VehicleGun->ShellEjectEffect, shellLocation, FVector::ZeroVector, FRotator(0.0f, -90.0f, 0.0f), EAttachLocation::KeepRelativeOffset, 1.0f);

	shellEjection->SetVectorParameter(FName("ShellVelocity"), velocity);

	// Spawn the round firing sound.

	UGameplayStatics::SpawnSoundAttached((IsHumanPlayer() == true) ? VehicleGun->RoundSound : VehicleGun->RoundSoundNonPlayer, VehicleMesh);

	// Return the world location of the muzzle flash.

	return VehicleMesh->GetBoneLocation(muzzleLocation);
}

#pragma endregion PickupGun

#pragma region PickupMissile

/**
* Apply a direct explosion force.
***********************************************************************************/

bool ABaseVehicle::ExplosionForce(float strength, int32 hitPoints, int32 aggressorVehicleIndex, const FVector& location, bool limitForces, EPickupType source, bool destroyShield, bool applyForces, FColor color, FGameEvent* gameEvent)
{
	VehicleMesh->IdleUnlock();

	ResetAttackTimer();

	ShakeHUD(1.5f);
	ShakeCamera(2.5f);

#pragma region PickupShield

	if (destroyShield == true &&
		GRIP_POINTER_VALID(Shield) == true &&
		Shield->IsCharged() == false)
	{
		DestroyShield(aggressorVehicleIndex);
	}

	if (IsShieldActive() == true &&
		IsShielded(location) == true)
	{
		Camera->Shock(true);

		DamageShield(hitPoints, aggressorVehicleIndex);

		if (applyForces == true)
		{
			FGameEvent thisEvent;

			thisEvent.LaunchVehicleIndex = VehicleIndex;
			thisEvent.TargetVehicleIndex = aggressorVehicleIndex;
			thisEvent.PickupUsed = EPickupType::Shield;
			thisEvent.EventType = EGameEventType::Blocked;

			PlayGameMode->AddGameEvent(thisEvent);
		}

		return false;
	}
	else

#pragma endregion PickupShield

	{
		Camera->Shock(false, 1.0f);

		if (applyForces == true)
		{
			if (gameEvent != nullptr)
			{
				PlayGameMode->AddGameEvent(*gameEvent);
			}
		}

		float massScale = Physics.CurrentMass / 5000.0f;
		FVector difference = GetActorLocation() - location;
		const FTransform& transform = VehicleMesh->GetComponentTransform();
		bool isSecondary = ((VehicleClock - LastExploded) < 3.0f && IsAirborne() == true);

		// General explosion force.

		difference.Normalize();

		FVector direction = (difference * 20000000.0f * strength);
		direction = transform.InverseTransformVector(direction);
		direction.Z = 0.0f;

		if (limitForces == true)
		{
			// Not so much side-spin.

			direction.Y *= 0.1f;
		}

		direction = transform.TransformVector(direction);

		FVector side = FVector(0.0f, FMathEx::UnitSign(FVector::DotProduct(difference, transform.GetUnitAxis(EAxis::Y))), 0.0f);

		if (IsPracticallyGrounded() == true)
		{
			// Specific upward force just to loosen tire grip.

			direction += GetLaunchDirection() * 7500000.0f * strength;
		}

		// Some random sideways force.

		if (limitForces == true)
		{
			direction *= 0.5f;
			side *= 2000.0f * strength;
		}
		else
		{
			side *= 15000.0f * strength;
		}

		side = transform.TransformVector(side);
		side *= massScale;
		direction *= massScale;

		if (applyForces == true &&
			PlayGameMode->PastGameSequenceStart() == true)
		{
			if (isSecondary == true)
			{
				if (FVector::DotProduct(direction, GetLaunchDirection()) < 0.0f)
				{
					direction *= -1.0f;
				}
			}

			if (FVector::DistSquared(Wheels.RearAxlePosition, location) < FVector::DistSquared(Wheels.FrontAxlePosition, location))
			{
				if (isSecondary == true)
				{
					VehicleMesh->AddImpulseAtLocation(direction * 0.5f, Wheels.RearAxlePosition);
				}
				else
				{
					VehicleMesh->AddImpulseAtLocation(direction, Wheels.RearAxlePosition);
					VehicleMesh->AddImpulseAtLocation(side, Wheels.RearAxlePosition + ((Wheels.RearAxlePosition - Wheels.FrontAxlePosition) * 100.0f));
				}
			}
			else
			{
				if (isSecondary == true)
				{
					VehicleMesh->AddImpulseAtLocation(direction * 0.5f, Wheels.FrontAxlePosition);
				}
				else
				{
					// Handle the reduction of explosion forces if the missile was in front of the vehicle when it exploded
					// and the player is apparently braking to evade it.

					float acceleration = (AI.Speed.DifferenceFromPerSecond(VehicleClock - 0.75f, VehicleClock, GetSpeedMPS() * 100.0f) / 100.0f);

					if (acceleration < -25.0f)
					{
						side *= 0.25f;
						direction *= 0.25f;
					}

					VehicleMesh->AddImpulseAtLocation(direction, Wheels.FrontAxlePosition);
					VehicleMesh->AddImpulseAtLocation(side, Wheels.FrontAxlePosition + ((Wheels.FrontAxlePosition - Wheels.RearAxlePosition) * 100.0f));
				}
			}

#pragma region VehicleAntiGravity

			CutAirPower(1.0f);

#pragma endregion VehicleAntiGravity

			LastExploded = VehicleClock;
		}

		return true;
	}
}

/**
* Apply a peripheral explosion force.
***********************************************************************************/

void ABaseVehicle::PeripheralExplosionForce(float strength, int32 hitPoints, int32 aggressorVehicleIndex, const FVector& location, bool limitForces, FColor color)
{
	if (strength > KINDA_SMALL_NUMBER)
	{
		VehicleMesh->IdleUnlock();

		ShakeHUD(1.25f);
		ShakeCamera(1.75f);

#pragma region PickupShield

		if (IsShieldActive() == true &&
			IsShielded(location) == true)
		{
			DamageShield(hitPoints, aggressorVehicleIndex);
		}
		else

#pragma endregion PickupShield

		{
			float massScale = Physics.CurrentMass / 5000.0f;
			FVector difference = GetActorLocation() - location;
			const FTransform& transform = VehicleMesh->GetComponentTransform();

			// General explosion force.

			difference.Normalize();

			FVector direction = (difference * 20000000.0f * strength);
			direction = transform.InverseTransformVector(direction);
			direction.Z = 0.0f;

			if (limitForces == true)
			{
				// Not so much side-spin.

				direction.Y *= 0.1f;
			}

			direction = transform.TransformVector(direction);

			FVector side = FVector(0.0f, FMathEx::UnitSign(FVector::DotProduct(difference, transform.GetUnitAxis(EAxis::Y))), 0.0f);

			if (IsPracticallyGrounded() == false)
			{
				// Specific upward force just to loosen tire grip.

				direction += GetLaunchDirection() * 4000000.0f * strength;
			}

			// Some random sideways force.

			if (limitForces == true)
			{
				direction *= 0.5f;
				side *= 2000.0f * strength;
			}
			else
			{
				side *= 5000.0f * strength;
			}

			side = transform.TransformVector(side);
			side *= massScale;
			direction *= massScale;

			if (PlayGameMode->PastGameSequenceStart() == true)
			{
				if (FVector::DistSquared(Wheels.RearAxlePosition, location) < FVector::DistSquared(Wheels.FrontAxlePosition, location))
				{
					VehicleMesh->AddImpulseAtLocation(direction, Wheels.RearAxlePosition);
					VehicleMesh->AddImpulseAtLocation(side, Wheels.RearAxlePosition + ((Wheels.RearAxlePosition - Wheels.FrontAxlePosition) * 100.0f));
				}
				else
				{
					VehicleMesh->AddImpulseAtLocation(direction, Wheels.FrontAxlePosition);
					VehicleMesh->AddImpulseAtLocation(side, Wheels.FrontAxlePosition + ((Wheels.FrontAxlePosition - Wheels.RearAxlePosition) * 100.0f));
				}
			}
		}
	}
}

/**
* Apply a peripheral explosion force.
***********************************************************************************/

void ABaseVehicle::PeripheralExplosionForce(float strength, int32 hitPoints, int32 aggressorVehicleIndex, const FVector& location, bool limitForces, FColor color, ABaseVehicle* avoid, UWorld* world, float radius)
{
	GRIP_GAME_MODE_LIST_FOR(GetVehicles(), vehicles, world);

	for (ABaseVehicle* vehicle : vehicles)
	{
		if (vehicle != avoid)
		{
			FVector targetPosition = vehicle->GetCenterLocation();
			FVector difference = targetPosition - location;
			float distance = difference.Size();

			if (distance < radius * 2.0f)
			{
				if (distance < radius)
				{
					float ratio = (distance / radius);

					ratio = FMath::Cos(ratio * PI * 0.5f);

					float thisStrength = strength;

					if (aggressorVehicleIndex == vehicle->VehicleIndex)
					{
						thisStrength *= 0.25f;
					}

					vehicle->PeripheralExplosionForce(thisStrength * ratio, hitPoints * ratio, aggressorVehicleIndex, location, limitForces, color);
				}
				else
				{
					vehicle->ShakeHUD(1.0f);
					vehicle->ShakeCamera(1.0f);
				}
			}
		}
	}
}

/**
* Apply a missile explosion force.
***********************************************************************************/

bool ABaseVehicle::MissileForce(float strength, int32 hitPoints, int32 aggressorVehicleIndex, const FVector& location, bool limitForces, bool destroyShield, FGameEvent* gameEvent)
{
	return ExplosionForce(strength, hitPoints, aggressorVehicleIndex, location, limitForces, EPickupType::HomingMissile, destroyShield, true, FColor(255, 64, 0), gameEvent);
}

/**
* Get a false target location for a missile.
***********************************************************************************/

FVector ABaseVehicle::GetMissileFalseTarget() const
{
	FVector up = GetLaunchDirection();
	FVector forward = GetFacingDirection();
	FVector position = (forward * 100.0f * 5000.0f) + (up * 100.0f * 500.0f);

	return position + GetActorLocation();
}

/**
* Update any active missiles firing from the vehicle.
***********************************************************************************/

void ABaseVehicle::UpdateMissiles(float deltaSeconds)
{
	// Handle the ejection of homing missiles.

	static float scorpionTimes[] = { 0.15f, 0.7f };

	for (int32 i = 0; i < NumPickups; i++)
	{
		if (EjectionState[i].State != EMissileEjectionState::Inactive)
		{
			switch (EjectionState[i].State)
			{
			case EMissileEjectionState::BayOpening:
				if (PickupSlots[i].Timer > scorpionTimes[0])
				{
					FireHomingMissile(i, 0);
					EjectionState[i].State = (PickupSlots[i].IsCharged() == true) ? EMissileEjectionState::Firing1 : EMissileEjectionState::Firing2;
				}
				break;

			case EMissileEjectionState::Firing1:
				if (PickupSlots[i].Timer > scorpionTimes[1])
				{
					FireHomingMissile(i, 1);
					EjectionState[i].State = EMissileEjectionState::Firing2;
				}
				break;

			case EMissileEjectionState::Firing2:
				ReleasePickupSlot(i);
				break;
			}
		}
	}

	// Now handle the homing missile audio visual indicator.

	HUD.HomingMissileTime = 0.0f;

	if (IsHumanPlayer() == true &&
		IsCinematicCameraActive() == false)
	{
		float clipDistance = 100.0f * 100.0f;
		float maxDistance = 1000.0f * 100.0f;
		float minDistance = maxDistance;
		AActor* viewVehicle = GetController()->GetViewTarget();

		if (viewVehicle == nullptr)
		{
			viewVehicle = this;
		}

		FVector location = viewVehicle->GetActorLocation();

		GRIP_GAME_MODE_LIST_FROM(Missiles, missiles, PlayGameMode);

		for (AHomingMissile* missile : missiles)
		{
			if (missile->Target == viewVehicle &&
				missile->IsHoming() == true)
			{
				float distance = (missile->GetActorLocation() - location).Size();

				if (minDistance > distance)
				{
					minDistance = distance;
				}
			}
		}

		if (minDistance < maxDistance)
		{
			minDistance = FMath::Clamp(minDistance, clipDistance, maxDistance);

			HUD.HomingMissileTime = minDistance / maxDistance;
			HUD.HomingMissileTimer -= deltaSeconds;

			if (HUD.HomingMissileTimer <= 0.0f)
			{
				HUD.MissileWarningTimer = 1.0f;
				HUD.HomingMissileTimer = HUD.HomingMissileTime;

				float pitch = 1.0f;

				ClientPlaySound(HUD.HomingMissileIndicatorSound, 1.0f, pitch);
			}
		}

		// Handle all of the HUD warnings, which display a vignette of a specific color
		// representing what it is that it's warning us about.

		float warningAmount = 0.0f;
		float warningDecrement = 1.0f;

		if (HUD.WarningSource == EHUDWarningSource::Elimination)
		{
			warningDecrement = 4.0f;
		}
		else if (HUD.WarningSource == EHUDWarningSource::DoubleDamage)
		{
			warningDecrement = 2.0f;
		}

		HUD.WarningTimer -= deltaSeconds * warningDecrement;

		if (HUD.WarningTimer <= 0.0f)
		{
			HUD.WarningTimer = 0.0f;
			HUD.WarningSource = EHUDWarningSource::None;
		}

		if (HUD.WarningTimer > 0.0f)
		{
			float clock = FMath::Frac(1.0f - HUD.WarningTimer);

			warningAmount = (clock < 0.5f) ? 1.0f - (clock * 2.0f) : 0.0f;
		}

		float ratio = FMathEx::GetSmoothingRatio(0.666f, deltaSeconds);

		HUD.WarningAmount = (HUD.WarningAmount * ratio) + (warningAmount * (1.0f - ratio));

		warningAmount = 0.0f;

		HUD.MissileWarningTimer -= deltaSeconds * 4.0f;
		HUD.MissileWarningTimer = FMath::Max(HUD.MissileWarningTimer, 0.0f);

		if (HUD.MissileWarningTimer > 0.0f)
		{
			float clock = FMath::Frac(1.0f - HUD.MissileWarningTimer);

			warningAmount = ((clock < 0.5f) ? 1.0f - clock * 2.0f : 0.0f) * 2.0f;
		}

		HUD.MissileWarningAmount = (HUD.MissileWarningAmount * ratio) + (warningAmount * (1.0f - ratio));
	}
}

/**
* Get the bone name of the missile bay to use in the vehicle's current condition.
***********************************************************************************/

FName ABaseVehicle::GetMissileBayName() const
{
	return ((IsFlipped() == false) ? FName("MissileBay_T_Eject") : FName("MissileBay_B_Eject"));
}

/**
* Fire a homing missile.
***********************************************************************************/

void ABaseVehicle::FireHomingMissile(int32 pickupSlot, int32 missileIndex)
{
	UWorld* world = GetWorld();

	if (world != nullptr)
	{
		FActorSpawnParameters spawnParams;

		spawnParams.Owner = this;
		spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		FVector location = VehicleMesh->GetBoneLocation(GetMissileBayName());
		FVector direction = location - GetCenterLocation();

		direction.X = 0.0f;
		direction.Z = 0.0f;
		direction.Normalize();

		FVector launchLocation = location + (direction * 20.0f);
		FRotator launchRotation = GetActorRotation();
		EPickupType type = PickupSlots[pickupSlot].Type;
		bool charged = PickupSlots[pickupSlot].IsCharged();
		AHomingMissile* missile = nullptr;

		if (type == EPickupType::HomingMissile)
		{
			if (charged == false)
			{
				missile = world->SpawnActor<AHomingMissile>(Level1MissileBlueprint, launchLocation, launchRotation, spawnParams);
			}
			else
			{
				missile = world->SpawnActor<AHomingMissile>(Level2MissileBlueprint, launchLocation, launchRotation, spawnParams);
			}
		}
		else
		{
			return;
		}

		if (missile != nullptr)
		{
			HomingMissile = missile;

			AActor* target = (EjectionState[pickupSlot].PickupTargets.Num() > 0) ? EjectionState[pickupSlot].PickupTargets[missileIndex % EjectionState[pickupSlot].PickupTargets.Num()].Get() : nullptr;

			missile->SetTarget(target);
			missile->ActivatePickup(this, pickupSlot, EPickupActivation::Released, charged);

			if (missile->Target != nullptr)
			{
				FGameEvent gameEvent;
				ABaseVehicle* targetVehicle = Cast<ABaseVehicle>(missile->Target);

				gameEvent.LaunchVehicleIndex = VehicleIndex;
				gameEvent.TargetVehicleIndex = (targetVehicle != nullptr) ? targetVehicle->VehicleIndex : -1;
				gameEvent.PickupUsed = type;
				gameEvent.PickupUsedWasCharged = missile->IsCharged();
				gameEvent.EventType = EGameEventType::Used;

				PlayGameMode->AddGameEvent(gameEvent);
			}
		}
	}
}

/**
* Get the sustained angular pitch velocity over the last quarter second.
***********************************************************************************/

float ABaseVehicle::GetSustainedAngularPitch()
{
	float sustained = Physics.AngularPitchList.GetMeanValue(Physics.Timing.TickSum - 0.25f);

	if (IsGrounded() == true)
	{
		float angVelocity = -Physics.VelocityData.AngularVelocity.Y;
		float scale = angVelocity / sustained;

		scale = FMath::Clamp(scale, 1.0f, 1.25f);

		return sustained * scale;
	}

	if (IsPracticallyGrounded() == true)
	{
		return sustained;
	}

	return 0.0f;
}

#pragma endregion PickupMissile

#pragma region BotCombatTraining

/**
* Get a weighting, between 0 and 1, of how ideally a pickup can be used. 0 means
* cannot be used effectively at all, 1 means a very high chance of pickup efficacy.
***********************************************************************************/

float ABaseVehicle::GetPickupEfficacyWeighting(int32 pickupSlot, AActor*& target)
{
	float result = 0.0f;

	target = nullptr;

	switch (PickupSlots[pickupSlot].Type)
	{

#pragma region PickupShield

	case EPickupType::Shield:
		result = AShield::EfficacyWeighting(this);
		break;

#pragma endregion PickupShield

#pragma region PickupTurbo

	case EPickupType::TurboBoost:
		result = ATurbo::EfficacyWeighting(this);
		break;

#pragma endregion PickupTurbo

#pragma region PickupMissile

	case EPickupType::HomingMissile:
		result = AHomingMissile::EfficacyWeighting(this, &PickupSlots[pickupSlot], Cast<ABaseVehicle>(HUD.GetCurrentMissileTargetActor(pickupSlot)));
		break;

#pragma endregion PickupMissile

#pragma region PickupGun

	case EPickupType::GatlingGun:
		result = AGatlingGun::EfficacyWeighting(this, &PickupSlots[pickupSlot], nullptr, target, Level1GatlingGunBlueprint->GetDefaultObject<AGatlingGun>());
		break;

#pragma endregion PickupGun

	default:
		break;
	}

	return result;
}

#pragma endregion BotCombatTraining
