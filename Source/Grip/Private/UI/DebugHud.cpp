/**
*
* Debugging HUD for Metal in Motion.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
***********************************************************************************/

#include "UI/DebugHUD.h"
#include "Vehicle/BaseVehicle.h"
#include "AI/PursuitSplineComponent.h"
#include "UObject/ConstructorHelpers.h"

/**
* Construct the debugging HUD, mainly establishing a font to use for display.
***********************************************************************************/

ADebugHUD::ADebugHUD()
{
	static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("/Engine/EngineFonts/Roboto"));

	MainFont = Font.Object;
}

/**
* Add a route spline with a given distance and length to the HUD for rendering.
***********************************************************************************/

void ADebugHUD::AddRouteSpline(UPursuitSplineComponent* spline, float distance, float length, bool randomColor)
{

#pragma region AINavigation

#if !UE_BUILD_SHIPPING
	APawn* owningPawn = GetOwningPawn();
	ABaseVehicle* vehicle = Cast<ABaseVehicle>(owningPawn);
	float from = spline->ClampDistance(distance - 20.0f * 100.0f);
	float to = spline->ClampDistance(distance + length);

	if (spline == vehicle->GetAI().RouteFollower.ThisSpline &&
		spline != vehicle->GetAI().RouteFollower.NextSpline)
	{
		to = FMath::Min(to, vehicle->GetAI().RouteFollower.ThisSwitchDistance);
	}

	FMinimalViewInfo desiredView;

	vehicle->Camera->GetCameraViewNoPostProcessing(0.0f, desiredView);

	bool exit = false;
	float splineLength = spline->GetSplineLength();
	float lengthLeft = (to > from) ? to - from : (splineLength - from) + to;
	TArray<int32> indices;
	TArray<FVector> vertices;

	do
	{
		if (lengthLeft <= 0.0f)
		{
			from += lengthLeft;
			exit = true;
		}

		FVector location = spline->GetWorldLocationAtDistanceAlongSpline(from);
		FVector direction = spline->GetDirectionAtDistanceAlongSpline(from, ESplineCoordinateSpace::World);
		FVector tangent = FVector::CrossProduct(direction, desiredView.Location - location); tangent.Normalize();

		vertices.Emplace(location - (tangent * 25.0f));
		vertices.Emplace(location + (tangent * 25.0f));

		from = spline->ClampDistance(from + 5.0f * 100.0f);
		lengthLeft -= 5.0f * 100.0f;
	}
	while (exit == false);

	for (int32 i = 0; i < vertices.Num() - 2; i += 2)
	{
		indices.Emplace(i + 2);
		indices.Emplace(i + 3);
		indices.Emplace(i + 1);
		indices.Emplace(i + 2);
		indices.Emplace(i + 1);
		indices.Emplace(i + 0);
	}

	FColor color = FColor(0, 255, 0, 255);

	if (spline->Type == EPursuitSplineType::MissileAssistance)
	{
		color = FColor(0, 0, 255, 255);
	}
	else if (randomColor == true)
	{
		color = FColor::MakeRandomColor();
	}

	color.A = 128;

	DrawDebugMesh(owningPawn->GetWorld(), vertices, indices, color);

	for (FRouteChoice& choice : spline->RouteChoices)
	{
		if (choice.DecisionDistance >= distance - length &&
			choice.DecisionDistance <= to)
		{
			for (FSplineLink& link : choice.SplineLinks)
			{
				AddRouteSpline(link.Spline.Get(), link.NextDistance, length - (choice.DecisionDistance - distance), (link.Spline != vehicle->GetAI().RouteFollower.NextSpline));
			}
		}
	}
#endif // !UE_BUILD_SHIPPING

#pragma endregion AINavigation

}
