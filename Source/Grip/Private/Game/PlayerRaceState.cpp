/**
*
* The race state for a player.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Management of the state of a player driving a vehicle in the game. This normally
* handles the event progress and scoring. Applicable to both humans and bots.
*
***********************************************************************************/

#include "game/playerracestate.h"
#include "game/globalgamestate.h"
#include "vehicle/flippablevehicle.h"
#include "ui/hudwidget.h"
#include "gamemodes/playgamemode.h"

/**
* Do the regular update tick.
***********************************************************************************/

void FPlayerRaceState::Tick(float deltaSeconds, APlayGameMode* gameMode, UGlobalGameState* gameState)
{
	LapCompleted = false;

	if (gameMode != nullptr)
	{
		if (gameMode->PastGameSequenceStart() == true)
		{
			if (PlayerCompletionState < EPlayerCompletionState::Complete)
			{
				// Update the race time for the player.

				float frameTime = gameMode->GetRealTimeGameClock() - RaceTime;

				// We always use real-time for lap and race times, and don't rely on deltaSeconds.

				LapTime += frameTime;
				RaceTime += frameTime;

				// Do the normal end-of-game detection checks for the player and signal completion if met.

				if (gameState->IsGameModeRace() == true)
				{
					if (gameMode->NoOpponentsLeft() == true &&
						gameMode->GetNumOpponents() > 1)
					{
						PlayerComplete(true, true, false);
					}
				}

				if (gameState->IsGameModeRace() == true)
				{
					// We're in a race-type scenario so let's do that particular management.

#pragma region GameModeElimination

					float eliminationRatio = 0.0f;
					FVehicleElimination& elimination = PlayerVehicle->GetVehicleElimination();

					// Manage the EliminationAlert sound and redding screen.

					if (gameState->GamePlaySetup.DrivingMode == EDrivingMode::Elimination)
					{
						elimination.AlertTimer -= deltaSeconds;
						elimination.AlertTimer = FMath::Max(elimination.AlertTimer, 0.0f);

						// Check if we are in last position.

						if (gameMode->GetNumOpponentsLeft() - 1 == RacePosition)
						{
							// Don't play sound for the AI.

							if (PlayerVehicle->IsHumanPlayer() == true &&
								PlayerVehicle->IsCinematicCameraActive() == false)
							{
								float minCooldown = 0.15f; // In seconds
								float maxCooldown = 1.5f; // In seconds

								eliminationRatio = gameMode->GetEliminationRatio();

								if (eliminationRatio != 0.0f &&
									elimination.AlertTimer <= 0.0f)
								{
									PlayerVehicle->GetHUD().Warning(EHUDWarningSource::Elimination, 1.0f, 1.0f);

									elimination.AlertTimer = FMath::Lerp(maxCooldown, minCooldown, FMath::Sin(eliminationRatio * PI * 0.5f));

									// Play the sound.

									PlayerVehicle->ClientPlaySound(elimination.AlertSound);
								}
							}
						}
						else
						{
							// If we are not last set timer on 0.

							elimination.AlertTimer = 0.0f;
						}

						// Game stops when all opponents are destroyed.

						if (gameMode->NoOpponentsLeft() == true &&
							gameMode->GetNumOpponents() > 1)
						{
							RacePosition = gameMode->GetNumOpponentsLeft() - 1;

							PlayerComplete(true, true, false);
						}

						float ratio = FMathEx::GetSmoothingRatio(0.95f, deltaSeconds);

						elimination.Ratio = FMath::Lerp(eliminationRatio, elimination.Ratio, ratio);
					}

#pragma endregion GameModeElimination

					// We're in a lap-based kind of game mode, so let's handle that here.

					UpdateCheckpoints(false);
				}

				if (PlayerCompletionState >= EPlayerCompletionState::Complete)
				{
					GameFinishedAt = gameMode->GetRealTimeClock();
				}
			}
			else
			{
				if (gameState->IsGameModeRace() == true)
				{
					// We're in a lap-based kind of game mode the update the checkpoints even if the game
					// has finished for this player as the cinematic camera relies on the progress information.

					UpdateCheckpoints(false);
				}
			}
		}
	}
}

/**
* Update the checkpoints for this player race state to determine their progress
* around the track.
***********************************************************************************/

void FPlayerRaceState::UpdateCheckpoints(bool ignoreCheckpointSize)
{

#pragma region VehicleRaceDistance

	APlayGameMode* gameMode = APlayGameMode::Get(PlayerVehicle->GetWorld());
	UGlobalGameState* gameState = UGlobalGameState::GetGlobalGameState(PlayerVehicle);
	float masterRacingSplineLength = gameMode->MasterRacingSplineLength;

	if (gameState->IsGameModeRace() == true)
	{
		if (LastCheckpoint == -1)
		{
			// Handle setting up the checkpoints for the first time.

			if (gameMode->Checkpoints.Num() > 0)
			{
				NextCheckpoint = 0;
				LastCheckpoint = gameMode->Checkpoints.Num() - 1;
			}
		}

		if (NextCheckpoint >= 0)
		{
			// DistanceAlongMasterRacingSpline may be greater or less than LastDistanceAlongMasterRacingSpline if
			// the vehicle is moving backwards or has teleported. We need to handle movement in either direction,
			// with teleporting just behaving like a very fast movement over a single frame. Teleporting will set
			// ignoreCheckpointSize to true as it's likely it'll legitimately miss the checkpoint window.

			float halfMasterRacingSplineLength = masterRacingSplineLength * 0.5f;
			bool masterRacingSplinePresent = (gameMode->MasterRacingSpline != nullptr);
			bool crossedSplineStart = (FMath::Abs(LastDistanceAlongMasterRacingSpline - DistanceAlongMasterRacingSpline) > halfMasterRacingSplineLength);
			int32 lastCheckPoint = LastCheckpoint;
			FVector location = PlayerVehicle->GetActorLocation();

			do
			{
				// Have we crossed the next checkpoint, effectively going forwards?

				int32 crossed0 = gameMode->Checkpoints[NextCheckpoint]->Crossed(LastDistanceAlongMasterRacingSpline, DistanceAlongMasterRacingSpline, masterRacingSplineLength, crossedSplineStart, PlayerVehicle->GetAI().LastLocation, location, ignoreCheckpointSize);

				// Have we crossed the last checkpoint, effectively going backwards?

				int32 crossed1 = gameMode->Checkpoints[LastCheckpoint]->Crossed(LastDistanceAlongMasterRacingSpline, DistanceAlongMasterRacingSpline, masterRacingSplineLength, crossedSplineStart, PlayerVehicle->GetAI().LastLocation, location, ignoreCheckpointSize);

				if (crossed0 > 0)
				{
					// The player has crossed a checkpoint the right way, so traverse forwards to the next one.

					CheckpointsReached++;
					LastCheckpoint = NextCheckpoint++;
					NextCheckpoint %= gameMode->Checkpoints.Num();

					if (LastCheckpoint == 0)
					{
						// So we need a checkpoint that specifically marks the end of the course, which isn't the
						// initial checkpoint.

						EternalLapNumber++;
						LapDistance = 0.0f;

						if (EternalLapNumber > 0 &&
							EternalLapNumber > MaxLapNumber)
						{
							if (gameState->GamePlaySetup.DrivingMode != EDrivingMode::Elimination)
							{
								// Signal that a lap was just completed, the HUD will do something with this very shortly.

								LapCompleted = true;
							}

							LastLapTime = LapTime;

							// Update the best lap time if we're just beat it.

							if (BestLapTime == 0.0f ||
								BestLapTime > LastLapTime)
							{
								BestLapTime = LastLapTime;
							}

							// Detect the end of game by checking the number of laps for this race.

							if (EternalLapNumber == gameState->GeneralOptions.NumberOfLaps &&
								(gameState->GamePlaySetup.DrivingMode == EDrivingMode::Race))
							{
								// Complete the game for this player if that was the last lap.

								PlayerComplete(true, false, false);
							}
							else
							{
								// Otherwise reset the lap time for the new lap.

								LapTime = 0.0f;
							}
						}

						MaxLapNumber = FMath::Max(MaxLapNumber, EternalLapNumber);
					}
				}
				else if (crossed1 < 0)
				{
					// The player has crossed a checkpoint the wrong way, so traverse backwards to the previous one.

					CheckpointsReached--;
					NextCheckpoint = LastCheckpoint;

					if (--LastCheckpoint < 0)
					{
						EternalLapNumber--;

						LastCheckpoint = gameMode->Checkpoints.Num() - 1;
						LapDistance = masterRacingSplineLength;
					}
				}
				else
				{
					break;
				}

				// while loop to catch large jumps in position due to teleporting and wind through all
				// checkpoints that may have been crossed because of that. Don't jump more than 1 lap
				// forwards or backwards though, no matter how large the jump in position is. It's
				// highly unlikely we'll cross more than one checkpoint in a frame in any event.
			}
			while (lastCheckPoint != LastCheckpoint);

			if (EternalLapNumber >= 0)
			{
				if (masterRacingSplinePresent == true)
				{
					float thisLapDistance = gameMode->MasterRacingSplineDistanceToLapDistance(DistanceAlongMasterRacingSpline);

					if (LapDistance > thisLapDistance)
					{
						// If we're further behind from where we were, then just take that.

						LapDistance = thisLapDistance;
					}
					else if ((thisLapDistance - LapDistance) > halfMasterRacingSplineLength)
					{
						// If we've jumped more than half a track in length, it basically means we've
						// crossed the start line backwards so set this lap distance to 0.

						LapDistance = 0.0f;
					}
					else
					{
						// We've moved forwards but not massively so we're OK, just take that forward
						// movement.

						LapDistance = thisLapDistance;
					}

					// Cap the lap distance to a maximum of the next checkpoint because DistanceAlongMasterRacingSpline
					// is measured straight from their player's spline distance, which doesn't account for any checkpointing
					// done until now. If the player somehow got ahead of the checkpoint without legitimately passing it,
					// then we need to clamp the lap distance to that next checkpoint distance.

					float maxLapDistance = gameMode->MasterRacingSplineDistanceToLapDistance(gameMode->Checkpoints[NextCheckpoint]->DistanceAlongMasterRacingSpline);

					// maxLapDistance is now between 0 and masterRacingSplineLength. But quickly do a math error check to
					// ensure we didn't go into negative territory, and if so, correct it. Remember, the first checkpoint
					// is also the last checkpoint, so it needs to reflect that here by setting it to the length of the
					// master racing spline.

					if (NextCheckpoint == 0 ||
						FMath::Abs(maxLapDistance) < KINDA_SMALL_NUMBER)
					{
						maxLapDistance = masterRacingSplineLength;
					}

					LapDistance = FMath::Min(LapDistance, maxLapDistance);

					// Establish the number of laps done for the vehicle.

					EternalRaceDistance = EternalLapNumber * masterRacingSplineLength;

					// Add this lap distance into the total number of completed laps.

					EternalRaceDistance += LapDistance;
				}
			}
		}
	}

	if (PlayerCompletionState < EPlayerCompletionState::Complete)
	{
		// If the game isn't complete then copy the eternal variables to the in-game variables.

		LapNumber = EternalLapNumber;
		RaceDistance = EternalRaceDistance;
	}

#pragma endregion VehicleRaceDistance

}

/**
* Complete the event for the player.
***********************************************************************************/

void FPlayerRaceState::PlayerComplete(bool setCompletionStatus, bool gameComplete, bool estimateRaceTime, EPlayerCompletionState completionState)
{
	bool disqualified = PlayerCompletionState == EPlayerCompletionState::Disqualified;

	if (PlayerCompletionState != EPlayerCompletionState::Complete &&
		PlayerCompletionState != EPlayerCompletionState::Abandoned)
	{
		if (estimateRaceTime == true)
		{
			UE_LOG(GripLog, Log, TEXT("FPlayerRaceState::PlayerComplete estimating event result for %s"), *PlayerVehicle->GetPlayerName(false, false));
		}

		APlayGameMode* gameMode = APlayGameMode::Get(PlayerVehicle);
		UGlobalGameState* gameState = UGlobalGameState::GetGlobalGameState(PlayerVehicle);

		if (setCompletionStatus == true &&
			disqualified == false)
		{
			PlayerCompletionState = completionState;
		}

		if (gameState->GamePlaySetup.DrivingMode != EDrivingMode::Elimination)
		{
			RacePosition = gameMode->CollectFinishingRacePosition();
		}

		if (estimateRaceTime == true)
		{
			float gameClock = gameMode->GetRealTimeGameClock();

			if (gameState->IsGameModeLapBased() == true)
			{
				float progress = PlayerVehicle->GetEventProgress();

				if (progress > KINDA_SMALL_NUMBER)
				{
					RaceTime /= progress;
				}

				RaceTime = FMath::Max(RaceTime, gameClock);
			}
			else if (gameState->GamePlaySetup.DrivingMode == EDrivingMode::Elimination)
			{
				int32 index = (gameMode->GetNumOpponents() - RacePosition) + 1;

				RaceTime = (index * GRIP_ELIMINATION_SECONDS) + FMath::FRandRange(-0.2f, 0.2f);
			}
		}
	}
}

/**
* Add points to the player's total if the player's game hasn't ended.
***********************************************************************************/

bool FPlayerRaceState::AddPoints(int32 numPoints)
{
	if (IsAccountingClosed() == false)
	{
		// Only register points if the game is in-play. If accounting is closed
		// then we're too late for points now.

		NumInGamePoints += numPoints;
		NumTotalPoints += numPoints;

		return true;
	}

	return false;
}
