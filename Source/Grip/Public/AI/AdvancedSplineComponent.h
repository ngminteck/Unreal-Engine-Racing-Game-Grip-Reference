/**
*
* Advanced spline components.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Spline components with extended functionality over USplineComponent but not really
* much in the way of new properties. It performs some extended geometry analysis
* on splines, including GetNearestDistance which returns the nearest position on a
* spline for a given position in space.
*
***********************************************************************************/

#pragma once

#include "system/gameconfiguration.h"
#include "components/splinecomponent.h"
#include "advancedsplinecomponent.generated.h"

#pragma region NavigationSplines

/**
* Structure for describing a section of a spline, used in an array within a spline.
***********************************************************************************/

struct FSplineSection
{
public:

	FSplineSection(const FSplineSection& section)
		: StartDistance(section.StartDistance)
		, EndDistance(section.EndDistance)
	{ }

	FSplineSection(float startDistance, float endDistance)
		: StartDistance(startDistance)
		, EndDistance(endDistance)
	{ }

	// The start distance along the spline of the section.
	float StartDistance = 0.0f;

	// The end distance along the spline of the section.
	float EndDistance = 0.0f;
};

#pragma endregion NavigationSplines

/**
* Class for an advanced spline component, normally one per actor.
***********************************************************************************/

UCLASS(ClassGroup = Navigation, meta = (BlueprintSpawnableComponent))
class GRIP_API UAdvancedSplineComponent : public USplineComponent
{
	GENERATED_BODY()

public:

	// Construct an advanced spline component.
	UAdvancedSplineComponent();

	// What is the name of the route this spline it linked to?
	UPROPERTY(EditAnywhere, Category = AdvancedSpline)
		FString RouteName;

	// Is the spline currently enabled?
	UPROPERTY(EditAnywhere, Category = AdvancedSpline)
		bool Enabled = true;

	// Get the index after the given point clamped to the number of points on the spline.
	UFUNCTION(BlueprintCallable, Category = AdvancedSpline)
		int32 ClampedNextIndex(int32 index) const
	{ return (index + 1) % GetNumberOfSplinePoints(); }

	// Draw a box for debugging purposes.
	UFUNCTION(BlueprintCallable, Category = AdvancedSpline)
		void DrawBox(FBox const& Box, FColor const& Color)
	{ DrawDebugSolidBox(GetWorld(), Box, Color); }

	// The parent actor's name, for debugging.
	FString ActorName;

#pragma region NavigationSplines

public:

	// Post initialize the component.
	virtual void PostInitialize();

	// Clamp a distance to the length of the spline, accounting for whether it's open or closed.
	float ClampDistance(float distance) const
	{
		float length = GetSplineLength();
		if (distance < 0.0f) distance = (IsClosedLoop() == true) ? length - FMath::Fmod(-distance, length) : 0.0f;
		else if (distance > length) distance = (IsClosedLoop() == true) ? FMath::Fmod(distance, length) : length;
		return distance;
	}

	// Find the nearest distance along a spline to a given world location.
	// The fewer iterations and samples you use the faster it will be, but also the less
	// accurate it will be. Conversely, the smaller the difference between startDistance
	// and endDistance the more accurate the result will be.
	float GetNearestDistance(FVector location, float startDistance = 0.0f, float endDistance = 0.0f, int32 numIterations = 4, int32 numSamples = 50, float earlyExitDistance = 10.0f) const;

	// Find the nearest distance along a spline to a given plane location and direction.
	// The fewer iterations and samples you use the faster it will be, but also the less
	// accurate it will be. Conversely, the smaller the difference between startDistance
	// and endDistance the more accurate the result will be.
	float GetNearestDistance(FVector planeLocation, FVector planeDirection, float startDistance = 0.0f, float endDistance = 0.0f, int32 numIterations = 4, int32 numSamples = 50, float earlyExitDistance = 10.0f) const;

	// Get the distance between two points on a spline (accounting for looped splines).
	float GetDistanceDifference(float distance0, float distance1, float length = 0.0f, bool signedDifference = false) const;

	// Clamp a distance along the spline to its length if it's not looped, or wrapped within its length if looped.
	float ClampDistanceAgainstLength(float distance, float length) const;

	// Get which side a world location is on with respect to its nearest point along the spline center-line.
	float GetSide(float distance, const FVector& fromLocation) const;

	// Get the direction on a spline for a given distance along it.
	FVector GetDirection(float distance) const
	{ return GetDirectionAtDistanceAlongSpline(distance, ESplineCoordinateSpace::World); }

	// Get the relative direction on a spline for a given distance along it compared to the facing direction of another object.
	int32 GetRelativeDirectionAtDistanceAlongSpline(float distance, const FVector& direction) const
	{ return (FVector::DotProduct(direction, GetDirectionAtDistanceAlongSpline(distance, ESplineCoordinateSpace::World)) >= 0.0f) ? 1 : -1; }

	// Get the number of samples required to achieve a given accuracy with the given range and number of iterations.
	int32 GetNumSamplesForRange(float range, int32 numIterations = 0, float accuracy = 1.0f, int32 minimum = 4) const
	{ if (numIterations <= 0) numIterations = 5; return FMath::Max(minimum, FMath::CeilToInt(FMath::Pow((FMath::Max(1.0f, range) / accuracy), 1.0f / numIterations) * 2.0f)); }

	// Convert a location from world space to spline space.
	FVector WorldSpaceToSplineSpace(FVector worldLocation, float distance, bool fullLocation) const
	{ if (fullLocation == true) return GetQuaternionAtDistanceAlongSpline(distance, ESplineCoordinateSpace::World).UnrotateVector(worldLocation - GetWorldLocationAtDistanceAlongSpline(distance)); else return GetQuaternionAtDistanceAlongSpline(distance, ESplineCoordinateSpace::World).UnrotateVector(worldLocation); }

	// Convert a location from spline space to world space.
	FVector SplineSpaceToWorldSpace(FVector splineLocation, float distance, bool fullLocation) const
	{ if (fullLocation == true) return GetQuaternionAtDistanceAlongSpline(distance, ESplineCoordinateSpace::World).RotateVector(splineLocation) + GetWorldLocationAtDistanceAlongSpline(distance); else return GetQuaternionAtDistanceAlongSpline(distance, ESplineCoordinateSpace::World).RotateVector(splineLocation); }

protected:

	// Calculate the sections of the spline.
	virtual void CalculateSections();

	// The distance at which extended points are laid out along a spline.
	static const int32 ExtendedPointMeters = 10;

#pragma region AIVehicleControl

public:

	// Get the curvature of the spline in degrees over distance (in withRespectTo space).
	virtual FRotator GetCurvatureOverDistance(float distance, float& overDistance, int32 direction, const FQuat& withRespectTo, bool absolute) const;

#pragma endregion AIVehicleControl

#pragma region CameraCinematics

public:

	// Get the surface sections of the spline.
	virtual TArray<FSplineSection> GetSurfaceSections() const
	{ return TArray<FSplineSection>(); }

	// Get the surface break property of the spline over distance.
	virtual bool GetSurfaceBreakOverDistance(float distance, float& overDistance, int32 direction) const
	{ return false; }

	// Get the grounded property of the spline over distance.
	virtual bool GetGroundedOverDistance(float distance, float& overDistance, int32 direction) const
	{ return true; }

	// Get the clearances of the spline.
	virtual TArray<float> GetClearancesFromSurface() const
	{ return TArray<float>(); }

	// Get the distance into between a start and end point.
	float GetDistanceInto(float distance, float start, float end) const;

	// Get the distance left between a start and end point.
	float GetDistanceLeft(float distance, float start, float end) const;

	// The sections where there's a relative straight for cinematic camera purposes.
	TArray<FSplineSection> StraightSections;

	// The sections where there's a relative straight and is exterior for cinematic drone camera purposes.
	TArray<FSplineSection> DroneSections;

#pragma endregion CameraCinematics

#pragma endregion NavigationSplines

};
