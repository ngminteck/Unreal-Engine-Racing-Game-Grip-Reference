/**
*
* Pursuit spline actors.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Spline actors have functions for querying nearest splines for a given point in
* space. Generally, the is just one spline component attached to a spline actor.
* It also has support for enabling, always selecting and never selecting splines
* for bots at run-time, along with other, Editor-related functions.
*
***********************************************************************************/

#pragma once

#include "system/gameconfiguration.h"
#include "ai/pursuitsplinecomponent.h"
#include "ai/advancedsplineactor.h"
#include "pursuitsplineactor.generated.h"

/**
* Class for an pursuit spline actor, normally containing a single spline component.
***********************************************************************************/

UCLASS(ClassGroup = Navigation)
class GRIP_API APursuitSplineActor : public AAdvancedSplineActor
{
	GENERATED_BODY()

public:

	// Construct pursuit spline.
	APursuitSplineActor();

	// The point data specific to the pursuit spline.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Pursuit)
		TArray<FPursuitPointData> PointData;

	// The point extended data specific to the pursuit spline.
	UPROPERTY()
		TArray<FPursuitPointExtendedData> PointExtendedData;

	// Is this pursuit spline currently selected in the Editor?
	UPROPERTY(Transient, BlueprintReadOnly, Category = Pursuit)
		bool Selected;

	// Synchronize the pursuit point data with the points on the parent spline.
	UFUNCTION(BlueprintCallable, Category = Spline)
		bool SynchronisePointData();

	// Always select the path with the given name given the choice.
	UFUNCTION(BlueprintCallable, Category = Spline)
		static void AlwaysSelectPursuitPath(FString routeName, FString actorName, UObject* worldContextObject);

	// Never select the path with the given name given the choice.
	UFUNCTION(BlueprintCallable, Category = Spline)
		static void NeverSelectPursuitPath(FString routeName, FString actorName, UObject* worldContextObject);

	// Enable / disable the spline with the given name / route.
	UFUNCTION(BlueprintCallable, Category = Spline)
		static void EnablePursuitPath(FString routeName, FString actorName, bool enabled, UObject* worldContextObject);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Default")
		void UpdateVisualisation();

#pragma region NavigationSplines

	// Determine any splines that this actor has which can link onto the given spline.
	bool EstablishPursuitSplineLinks(UPursuitSplineComponent* spline) const;

	// Calculate the extended point data by examining the scene around the spline.
	bool Build(bool fromMenu);

#pragma region AINavigation

	// Find the nearest spline for a point in world space.
	// If you opt to matchMasterDistanceAlong then you need to provide that distance in distanceAlong.
	static bool FindNearestPursuitSpline(const FVector& location, const FVector& direction, UWorld* world, TWeakObjectPtr<UPursuitSplineComponent>& pursuitSpline, float& distanceAway, float& distanceAlong, EPursuitSplineType type, bool visibleOnly, bool matchMasterDistanceAlong, bool allowDeadStarts, bool allowDeadEnds, float minMatchingDistance = 0.0f);

#pragma endregion AINavigation

	// Distance between which we should consider spline links part of the same junction for route switching.
	static const float MinDistanceForSplineLinksSquared;

protected:

	// Do some shutdown when the actor is being destroyed.
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

private:

#if WITH_EDITORONLY_DATA
	// When an object has been selected in the Editor, handle the selected state of the pursuit spline mesh component.
	void OnObjectSelected(UObject* object);
#endif

#pragma endregion NavigationSplines

};
