/**
*
* Catchup debugging HUD.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
***********************************************************************************/

#pragma once

#include "system/gameconfiguration.h"
#include "debughud.h"
#include "debugcatchuphud.generated.h"

/**
* The debugging HUD for catchup assistance.
***********************************************************************************/

UCLASS()
class ADebugCatchupHUD : public ADebugHUD
{
	GENERATED_BODY()

public:

#pragma region VehicleCatchup

	// Draw the HUD.
	virtual void DrawHUD() override;

#pragma endregion VehicleCatchup

};
