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

#include "ai/advancedsplineactor.h"
#include "system/worldfilter.h"
#include "game/globalgamestate.h"

#pragma region NavigationSplines

/**
* Do some post initialization just before the game is ready to play.
***********************************************************************************/

void AAdvancedSplineActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	TArray<UActorComponent*> splines;

	GetComponents(UAdvancedSplineComponent::StaticClass(), splines);

	for (UActorComponent* component : splines)
	{
		UAdvancedSplineComponent* thisSpline = Cast<UAdvancedSplineComponent>(component);

		thisSpline->PostInitialize();
	}
}

/**
* Find the nearest spline component attached to this actor to a world space
* location.
***********************************************************************************/

bool AAdvancedSplineActor::FindNearestSpline(const FVector& location, UAdvancedSplineComponent*& nearestSpline, float& distance) const
{
	float minDistance = 0.0f;
	TArray<UActorComponent*> splines;

	GetComponents(UAdvancedSplineComponent::StaticClass(), splines);

	nearestSpline = nullptr;

	for (UActorComponent* component : splines)
	{
		UAdvancedSplineComponent* thisSpline = Cast<UAdvancedSplineComponent>(component);

		if (thisSpline->Enabled == true &&
			thisSpline->GetNumberOfSplinePoints() > 1)
		{
			int32 numIterations = 5;
			float thisDistance = thisSpline->GetNearestDistance(location, 0.0f, 0.0f, numIterations, 100);

			if (nearestSpline == nullptr ||
				minDistance > thisDistance)
			{
				minDistance = thisDistance;
				nearestSpline = thisSpline;
			}
		}
	}

	distance = minDistance;

	return (nearestSpline != nullptr);
}

/**
* Find the nearest pursuit spline to a world space location.
***********************************************************************************/

bool AAdvancedSplineActor::FindNearestSpline(const FVector& location, UWorld* world, UAdvancedSplineComponent*& nearestSpline, float& distanceAway, float& distanceAlong)
{
	UGlobalGameState* gameState = UGlobalGameState::GetGlobalGameState(world);

	distanceAway = -1.0f;
	nearestSpline = nullptr;

	for (TActorIterator<AAdvancedSplineActor> actorItr(world); actorItr; ++actorItr)
	{
		if (FWorldFilter::IsValid(*actorItr, gameState) == true)
		{
			float distance = 0.0f;
			AAdvancedSplineActor* paths = *actorItr;
			UAdvancedSplineComponent* spline = nullptr;

			if (paths->FindNearestSpline(location, spline, distance) == true)
			{
				FVector splineLocation = spline->GetWorldLocationAtDistanceAlongSpline(distance);
				FVector difference = location - splineLocation;
				float away = difference.SizeSquared();

				if (nearestSpline == nullptr ||
					distanceAway > away)
				{
					distanceAway = away;
					distanceAlong = distance;
					nearestSpline = spline;
				}
			}
		}
	}

	if (distanceAway > 0.0f)
	{
		distanceAway = FMath::Sqrt(distanceAway);
	}

	return (nearestSpline != nullptr);
}

/**
* Structure for describing a distance along a spline.
***********************************************************************************/

struct FSplineDistance3
{
public:

	FSplineDistance3(UAdvancedSplineComponent* spline, float distance, float away)
		: Spline(spline)
		, Distance(distance)
		, Away(away)
	{ }

	// The spline.
	UAdvancedSplineComponent* Spline = nullptr;

	// The distance along the spline.
	float Distance = 0.0f;

	// The distance away from the spline.
	float Away = 0.0f;
};

/**
* Find the nearest spline for a point in world space.
***********************************************************************************/

bool AAdvancedSplineActor::FindNearestSplines(const FVector& location, UWorld* world, TArray<TWeakObjectPtr<UAdvancedSplineComponent>>& nearestSplines, TArray<float>& splineDistances)
{
	UGlobalGameState* gameState = UGlobalGameState::GetGlobalGameState(world);

	TArray<FSplineDistance3> splineDistancesAway;

	nearestSplines.Reset();
	splineDistances.Reset();

	for (TActorIterator<AAdvancedSplineActor> actorItr(world); actorItr; ++actorItr)
	{
		if (FWorldFilter::IsValid(*actorItr, gameState) == true)
		{
			float distance = 0.0f;
			AAdvancedSplineActor* paths = *actorItr;
			UAdvancedSplineComponent* spline = nullptr;

			if (paths->FindNearestSpline(location, spline, distance) == true)
			{
				FVector splineLocation = spline->GetWorldLocationAtDistanceAlongSpline(distance);
				FVector difference = location - splineLocation;
				float away = difference.Size();

				splineDistancesAway.Emplace(FSplineDistance3(spline, distance, away));
			}
		}
	}

	if (splineDistancesAway.Num() > 0)
	{
		splineDistancesAway.Sort([](const FSplineDistance3& object1, const FSplineDistance3& object2)
			{
				return object1.Away < object2.Away;
			});

		float minDistance = FMath::Max(splineDistancesAway[0].Away, 100.0f * 100.0f);
		FVector baseDirection = splineDistancesAway[0].Spline->GetDirectionAtDistanceAlongSpline(splineDistancesAway[0].Distance, ESplineCoordinateSpace::World);

		for (int32 i = 0; i < splineDistancesAway.Num(); i++)
		{
			if (splineDistancesAway[i].Away <= minDistance)
			{
				FVector direction = splineDistancesAway[i].Spline->GetDirectionAtDistanceAlongSpline(splineDistancesAway[i].Distance, ESplineCoordinateSpace::World);

				if (FVector::DotProduct(direction, baseDirection) > 0.0f)
				{
					nearestSplines.Emplace(splineDistancesAway[i].Spline);
					splineDistances.Emplace(splineDistancesAway[i].Distance);
				}
			}
		}

		return true;
	}

	return false;
}

#pragma endregion NavigationSplines
