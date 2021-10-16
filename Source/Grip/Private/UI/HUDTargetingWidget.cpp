/**
*
* HUD targeting widgets implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Widget used specifically to draw symbology related to targeting onto the HUD.
*
***********************************************************************************/

#include "ui/hudtargetingwidget.h"
#include "blueprint/userwidget.h"
#include "blueprint/widgetblueprintlibrary.h"

/**
* Draw the primary homing symbology.
***********************************************************************************/

void UHUDTargetingWidgetComponent::DrawPrimaryHoming(const UHUDTargetingWidgetComponent* component, const FPaintContext& paintContext, USlateBrushAsset* slateBrush, float globalOpacity)
{

#pragma region VehicleHUD

	ABaseVehicle* targetVehicle = component->GetTargetVehicle();

	if (targetVehicle != nullptr)
	{
		FMinimalViewInfo desiredView;

		targetVehicle->Camera->GetCameraViewNoPostProcessing(0.0f, desiredView);

#pragma region PickupMissile

		for (AHomingMissile* missile : component->PlayGameMode->Missiles)
		{
			if (GRIP_OBJECT_VALID(missile) == true &&
				GRIP_OBJECT_VALID(missile->Target) == true &&
				missile->ShowHUDIndicator() == true &&
				missile->GetLaunchVehicle() == targetVehicle)
			{
				FVector2D screenPosition;
				FVector2D size = FVector2D(32.0f, 32.0f);
				ITargetableInterface* target = Cast<ITargetableInterface>(missile->Target);

				if (target != nullptr &&
					component->PlayGameMode->ProjectWorldLocationToWidgetPosition(targetVehicle, target->GetTargetBullsEye(), screenPosition, &desiredView) == true)
				{
					FLinearColor color = FLinearColor(1.0f, 0.0f, 0.0f, globalOpacity);

					screenPosition -= size * 0.5f;

					if (missile->HasExploded() == false)
					{
						if (component->PlayGameMode->GetFlashingOpacity() < 0.01f)
						{
							continue;
						}

						color = FLinearColor(0.0f, 1.0f, 0.0f, globalOpacity);
					}

					if (missile->HUDTargetHit() == true)
					{
						color = FLinearColor(0.0f, 1.0f, 0.0f, globalOpacity);
					}

					UWidgetBlueprintLibrary::DrawBox(const_cast<FPaintContext&>(paintContext), screenPosition, size, slateBrush, color);
				}
			}
		}

#pragma endregion PickupMissile

	}

#pragma endregion VehicleHUD

}

/**
* Draw the primary tracking symbology.
***********************************************************************************/

void UHUDTargetingWidgetComponent::DrawPrimaryTracking(const UHUDTargetingWidgetComponent* component, const FPaintContext& paintContext, USlateBrushAsset* slateBrush, USlateBrushAsset* slateBrushSecondary, float globalOpacity)
{

#pragma region VehicleHUD

	ABaseVehicle* targetVehicle = component->GetTargetVehicle();

	if (GRIP_OBJECT_VALID(targetVehicle) == true)
	{
		FMinimalViewInfo desiredView;

		targetVehicle->Camera->GetCameraViewNoPostProcessing(0.0f, desiredView);

		for (int32 pass = 0; pass < 2; pass++)
		{
			for (int32 pickupSlot = 0; pickupSlot < 2; pickupSlot++)
			{
				if (targetVehicle->HasTarget(pickupSlot) == true)
				{
					FVector2D screenPosition;
					float alpha = targetVehicle->TargetFadeIn(pickupSlot);
					FVector2D size = component->GetTargetSizeFromOpacity(alpha, 64.0f);

					if (component->PlayGameMode->ProjectWorldLocationToWidgetPosition(targetVehicle, targetVehicle->GetHUD().TargetLocation[pickupSlot], screenPosition, &desiredView) == true)
					{
						float lineScale = 1.0f;
						FLinearColor color = FLinearColor(0.0f, 1.0f, 0.0f, alpha * globalOpacity);

						if (alpha < 0.99f)
						{
							color = FLinearColor(1.0f, 1.0f, 1.0f, alpha * globalOpacity);
						}

						if (targetVehicle->IsPrimaryTarget(pickupSlot) == false)
						{
							color.A = 0.5f;
							size *= 0.666f;
							lineScale *= 0.666f;
						}

						if (pass == 0)
						{
							UWidgetBlueprintLibrary::DrawBox(const_cast<FPaintContext&>(paintContext), screenPosition - (size * 0.5f), size, slateBrush, color);
						}
						else
						{
							bool inBoth = (targetVehicle->HasTarget(pickupSlot ^ 1) && targetVehicle->GetHUD().GetCurrentMissileTargetActor(pickupSlot) == targetVehicle->GetHUD().GetCurrentMissileTargetActor(pickupSlot ^ 1));

							float lineWidth = 12.0f;
							float linelength = 48.0f;
							FVector2D lineSize = FVector2D(lineWidth, linelength);
							FVector2D lineSize2 = lineSize * 0.5f;

							if (inBoth == true)
							{
								if (pickupSlot == 0)
								{
									UWidgetBlueprintLibrary::DrawBox(const_cast<FPaintContext&>(paintContext), screenPosition - lineSize2, lineSize, slateBrushSecondary, color);
									UWidgetBlueprintLibrary::DrawBox(const_cast<FPaintContext&>(paintContext), screenPosition - FVector2D(+12.0f * lineScale, 0.0f) - lineSize2 * lineScale, lineSize * lineScale, slateBrushSecondary, color);
									UWidgetBlueprintLibrary::DrawBox(const_cast<FPaintContext&>(paintContext), screenPosition - FVector2D(-12.0f * lineScale, 0.0f) - lineSize2 * lineScale, lineSize * lineScale, slateBrushSecondary, color);
								}
							}
							else if (pickupSlot == 0)
							{
								UWidgetBlueprintLibrary::DrawBox(const_cast<FPaintContext&>(paintContext), screenPosition - lineSize2 * lineScale, lineSize * lineScale, slateBrushSecondary, color);
							}
							else
							{
								UWidgetBlueprintLibrary::DrawBox(const_cast<FPaintContext&>(paintContext), screenPosition - FVector2D(+6.0f * lineScale, 0.0f) - lineSize2 * lineScale, lineSize * lineScale, slateBrushSecondary, color);
								UWidgetBlueprintLibrary::DrawBox(const_cast<FPaintContext&>(paintContext), screenPosition - FVector2D(-6.0f * lineScale, 0.0f) - lineSize2 * lineScale, lineSize * lineScale, slateBrushSecondary, color);
							}
						}
					}
				}
			}
		}
	}

#pragma endregion VehicleHUD

}

/**
* Draw the secondary tracking symbology.
***********************************************************************************/

void UHUDTargetingWidgetComponent::DrawSecondaryTracking(const UHUDTargetingWidgetComponent* component, const FPaintContext& paintContext, USlateBrushAsset* slateBrush, USlateBrushAsset* slateBrushSecondary, float globalOpacity)
{

#pragma region VehicleHUD

	ABaseVehicle* targetVehicle = component->GetTargetVehicle();

	if (GRIP_OBJECT_VALID(targetVehicle) == true)
	{
		FMinimalViewInfo desiredView;

		targetVehicle->Camera->GetCameraViewNoPostProcessing(0.0f, desiredView);

		for (int32 pickupSlot = 0; pickupSlot < 2; pickupSlot++)
		{
			AActor* targetted = targetVehicle->GetHUD().GetCurrentMissileTargetActor(pickupSlot);

			for (FHUDTarget& missile : targetVehicle->GetHUD().PickupTargets[pickupSlot])
			{
				if (missile.Target.Get() != targetted)
				{
					FVector2D screenPosition;
					float alpha = missile.TargetTimer;
					FVector2D size = component->GetTargetSizeFromOpacity(alpha, 32.0f);
					ITargetableInterface* target = Cast<ITargetableInterface>(missile.Target.Get());

					if (target != nullptr &&
						component->PlayGameMode->ProjectWorldLocationToWidgetPosition(targetVehicle, target->GetTargetBullsEye(), screenPosition, &desiredView) == true)
					{
						screenPosition -= size * 0.5f;

						FLinearColor color = FLinearColor(1.0f, 1.0f, 1.0f, alpha * globalOpacity);

						UWidgetBlueprintLibrary::DrawBox(const_cast<FPaintContext&>(paintContext), screenPosition, size, (missile.Primary == true) ? slateBrush : slateBrushSecondary, color);
					}
				}
			}
		}
	}

#pragma endregion VehicleHUD

}

/**
* Draw the threat symbology.
***********************************************************************************/

void UHUDTargetingWidgetComponent::DrawThreats(const UHUDTargetingWidgetComponent* component, const FPaintContext& paintContext, USlateBrushAsset* slateBrush, float globalOpacity)
{

#pragma region VehicleHUD

	ABaseVehicle* targetVehicle = component->GetTargetVehicle();

	if (GRIP_OBJECT_VALID(targetVehicle) == true)
	{
		FMinimalViewInfo desiredView;

		targetVehicle->Camera->GetCameraViewNoPostProcessing(0.0f, desiredView);

		for (FHUDTarget& missile : targetVehicle->GetHUD().ThreatTargets)
		{
			FVector2D screenPosition;
			float alpha = missile.TargetTimer;
			FVector2D size = component->GetTargetSizeFromOpacity(alpha, 30.0f);
			AActor* target = missile.Target.Get();

			if (target != nullptr &&
				component->PlayGameMode->ProjectWorldLocationToWidgetPosition(targetVehicle, target->GetActorLocation(), screenPosition, &desiredView) == true)
			{
				screenPosition -= size * 0.5f;

				FLinearColor color = FLinearColor(1.0f, 0.0f, 0.0f, alpha * globalOpacity);

				UWidgetBlueprintLibrary::DrawBox(const_cast<FPaintContext&>(paintContext), screenPosition, size, slateBrush, color);
			}
		}
	}

#pragma endregion VehicleHUD

}
