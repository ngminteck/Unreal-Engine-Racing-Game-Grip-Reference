/**
*
* Advanced spline actors.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Spline actors have functions for querying nearest splines for a given point in
* space. Generally, the is just one spline component attached to a spline actor.
*
***********************************************************************************/

#pragma once

#include "system/gameconfiguration.h"
#include "ai/advancedsplinecomponent.h"
#include "advancedsplineactor.generated.h"

/**
* Class for an advanced spline actor, normally containing a single spline component.
***********************************************************************************/

UCLASS(ClassGroup = Navigation)
class GRIP_API AAdvancedSplineActor : public AActor
{
	GENERATED_BODY()

public:

	// Is the spline visualization currently enabled?
	UPROPERTY(EditAnywhere, Category = Rendering)
		bool VisualisationEnabled = true;

#pragma region NavigationSplines

	// Find the nearest spline component attached to this actor to a world space location.
	bool FindNearestSpline(const FVector& location, UAdvancedSplineComponent*& nearestSpline, float& distance) const;

	// Find the nearest spline for a point in world space.
	static bool FindNearestSpline(const FVector& location, UWorld* world, UAdvancedSplineComponent*& nearestSpline, float& distanceAway, float& distanceAlong);

	// Find the nearest spline for a point in world space.
	static bool FindNearestSplines(const FVector& location, UWorld* world, TArray<TWeakObjectPtr<UAdvancedSplineComponent>>& nearestSplines, TArray<float>& splineDistances);

protected:

	// Do some post initialization just before the game is ready to play.
	virtual void PostInitializeComponents() override;

#pragma endregion NavigationSplines

};
