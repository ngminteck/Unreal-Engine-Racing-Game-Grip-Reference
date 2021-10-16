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

#include "ai/pursuitsplineactor.h"
#include "system/worldfilter.h"
#include "game/globalgamestate.h"
#include "gamemodes/playgamemode.h"

/**
* Construct pursuit spline.
***********************************************************************************/

APursuitSplineActor::APursuitSplineActor()
{

#pragma region NavigationSplines

#if WITH_EDITORONLY_DATA
	USelection::SelectObjectEvent.AddUObject(this, &APursuitSplineActor::OnObjectSelected);
#endif // WITH_EDITORONLY_DATA

#pragma endregion NavigationSplines

}

/**
* Always select the spline with the given name / route given the choice.
***********************************************************************************/

void APursuitSplineActor::AlwaysSelectPursuitPath(FString routeName, FString actorName, UObject* worldContextObject)
{

#pragma region NavigationSplines

	UWorld* world = worldContextObject->GetWorld();
	APlayGameMode* gameMode = APlayGameMode::Get(world);

	for (APursuitSplineActor* splineActor : gameMode->GetPursuitSplines())
	{
		TArray<UActorComponent*> splines;

		splineActor->GetComponents(UPursuitSplineComponent::StaticClass(), splines);

		for (UActorComponent* component : splines)
		{
			UPursuitSplineComponent* splineComponent = Cast<UPursuitSplineComponent>(component);

			if ((actorName.IsEmpty() == false && splineComponent->ActorName == actorName) ||
				(routeName.IsEmpty() == false && splineComponent->RouteName == routeName))
			{
				splineComponent->Enabled = true;
				splineComponent->AlwaysSelect = true;
			}
		}
	}

#pragma endregion NavigationSplines

}

/**
* Never select the spline with the given name / route given the choice.
***********************************************************************************/

void APursuitSplineActor::NeverSelectPursuitPath(FString routeName, FString actorName, UObject* worldContextObject)
{

#pragma region NavigationSplines

	UWorld* world = worldContextObject->GetWorld();
	APlayGameMode* gameMode = APlayGameMode::Get(world);

	for (APursuitSplineActor* splineActor : gameMode->GetPursuitSplines())
	{
		TArray<UActorComponent*> splines;

		splineActor->GetComponents(UPursuitSplineComponent::StaticClass(), splines);

		for (UActorComponent* component : splines)
		{
			UPursuitSplineComponent* splineComponent = Cast<UPursuitSplineComponent>(component);

			if ((actorName.IsEmpty() == false && splineComponent->ActorName == actorName) ||
				(routeName.IsEmpty() == false && splineComponent->RouteName == routeName))
			{
				splineComponent->Enabled = false;
				splineComponent->AlwaysSelect = false;
			}
		}
	}

#pragma endregion NavigationSplines

}

/**
* Enable / disable the spline with the given name / route.
***********************************************************************************/

void APursuitSplineActor::EnablePursuitPath(FString routeName, FString actorName, bool enabled, UObject* worldContextObject)
{

#pragma region NavigationSplines

	UWorld* world = worldContextObject->GetWorld();
	APlayGameMode* gameMode = APlayGameMode::Get(world);

	for (APursuitSplineActor* splineActor : gameMode->GetPursuitSplines())
	{
		TArray<UActorComponent*> splines;

		splineActor->GetComponents(UPursuitSplineComponent::StaticClass(), splines);

		for (UActorComponent* component : splines)
		{
			UPursuitSplineComponent* splineComponent = Cast<UPursuitSplineComponent>(component);

			if ((actorName.IsEmpty() == false && splineComponent->ActorName == actorName) ||
				(routeName.IsEmpty() == false && splineComponent->RouteName == routeName))
			{
				splineComponent->Enabled = enabled;
			}
		}
	}

#pragma endregion NavigationSplines

}

/**
* Synchronize the pursuit point data with the points on the parent spline.
***********************************************************************************/

bool APursuitSplineActor::SynchronisePointData()
{

#pragma region NavigationSplines

	bool changed = PointExtendedData.Num() == 0;
	TArray<UActorComponent*> splines;

	GetComponents(UPursuitSplineComponent::StaticClass(), splines);

	for (UActorComponent* component : splines)
	{
		UPursuitSplineComponent* splineComponent = Cast<UPursuitSplineComponent>(component);
		int32 numPoints = splineComponent->GetNumberOfSplinePoints();

		// Intelligently resize the PointData array.

		if (PointData.Num() > numPoints)
		{
			changed = true;

			PointData.SetNum(numPoints);
		}
		else
		{
			while (PointData.Num() < numPoints)
			{
				changed = true;

				if (PointData.Num() == 0)
				{
					PointData.Emplace(FPursuitPointData());
				}
				else
				{
					PointData.Emplace(FPursuitPointData(PointData.Last()));
				}
			}
		}
	}

	if (changed == true)
	{
		PointExtendedData.Empty();
	}

	return changed;

#pragma endregion NavigationSplines

}

#pragma region NavigationSplines

const float APursuitSplineActor::MinDistanceForSplineLinksSquared = 10.0f * 100.0f * 10.0f * 100.0f;

/**
* Do some shutdown when the actor is being destroyed.
***********************************************************************************/

void APursuitSplineActor::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	GRIP_REMOVE_FROM_GAME_MODE_LIST(PursuitSplines);

	Super::EndPlay(endPlayReason);
}

#if WITH_EDITORONLY_DATA

/**
* When an object has been selected in the Editor, handle the selected state of
* the pursuit spline mesh component.
***********************************************************************************/

void APursuitSplineActor::OnObjectSelected(UObject* object)
{
	bool selected = Selected;

	if (object == this)
	{
		Selected = true;
	}
	else if (!IsSelected())
	{
		Selected = false;
	}

	if (selected != Selected)
	{
		TArray<UActorComponent*> splines;

		GetComponents(UPursuitSplineComponent::StaticClass(), splines);

		for (UActorComponent* component : splines)
		{
			UPursuitSplineComponent* splineComponent = Cast<UPursuitSplineComponent>(component);

			for (TWeakObjectPtr<UPursuitSplineMeshComponent>& mesh : splineComponent->PursuitSplineMeshComponents)
			{
				if (GRIP_POINTER_VALID(mesh) == true)
				{
					mesh->SetupMaterial(Selected);
				}
			}
		}
	}
}

#endif // WITH_EDITORONLY_DATA

/**
* Small structure for recording splines and distances.
***********************************************************************************/

struct FSplineDistance2
{
	FSplineDistance2(UPursuitSplineComponent* spline, float distanceAway, float distanceAlong)
		: Spline(spline)
		, DistanceAway(distanceAway)
		, DistanceAlong(distanceAlong)
	{ }

	// The spline.
	UPursuitSplineComponent* Spline = nullptr;

	// The distance away from the spline.
	float DistanceAway = 0.0f;

	// The distance along the spline.
	float DistanceAlong = 0.0f;
};

/**
* Determine any splines that this actor has which can link onto the given spline.
***********************************************************************************/

bool APursuitSplineActor::EstablishPursuitSplineLinks(UPursuitSplineComponent* targetSpline) const
{
	check(targetSpline != nullptr);

	float minDistance = MinDistanceForSplineLinksSquared;
	TArray<UActorComponent*> splines;

	GetComponents(UPursuitSplineComponent::StaticClass(), splines);

	int32 numIterations = 5;

	for (UActorComponent* component : splines)
	{
		UPursuitSplineComponent* splineComponent = Cast<UPursuitSplineComponent>(component);

		if (splineComponent != targetSpline)
		{
			// Determine if the end points in this spline fall on the spline we're potentially attaching to.

			// The length of the spline.

			float length = targetSpline->GetSplineLength();
			float thisLength = splineComponent->GetSplineLength();

			// The world position of this spline's start point.

			FVector from0 = splineComponent->GetWorldLocationAtDistanceAlongSpline(0.0f);

			// Distance along the target spline of this spline's start point.

			float distance0 = targetSpline->GetNearestDistance(from0, 0.0f, length, numIterations, targetSpline->GetNumSamplesForRange(length, numIterations, 1.0f, 100), 1.0f);

			// Position on the target spline of this spline's start point.

			FVector to0 = targetSpline->GetWorldLocationAtDistanceAlongSpline(distance0);

			// The world position of this spline's end point.

			FVector from1 = splineComponent->GetWorldLocationAtDistanceAlongSpline(thisLength);

			// Distance along the target spline of this spline's end point.

			float distance1 = targetSpline->GetNearestDistance(from1, 0.0f, length, numIterations, targetSpline->GetNumSamplesForRange(length, numIterations, 1.0f, 100), 1.0f);

			// Position on the target spline of this spline's end point.

			FVector to1 = targetSpline->GetWorldLocationAtDistanceAlongSpline(distance1);

			// See if this spline's end points are in range of the target spline.

			bool thisStartPointConnected = ((from0 - to0).SizeSquared() < minDistance); // May be true for looped splines - probably untrue but harmless if true.
			bool thisEndPointConnected = (((from1 - to1).SizeSquared() < minDistance) && (splineComponent->IsClosedLoop() == false)); // Will never be true for looped splines.

			if (thisStartPointConnected == true)
			{
				FVector direction0 = targetSpline->GetWorldDirectionAtDistanceAlongSpline(FMath::Clamp(distance0, 1.0f, length - 1.0f));
				FVector direction1 = splineComponent->GetWorldDirectionAtDistanceAlongSpline(FMath::Clamp(0.0f, 1.0f, thisLength - 1.0f));

				thisStartPointConnected &= (FVector::DotProduct(direction0, direction1) > 0.0f);
			}

			if (thisEndPointConnected == true)
			{
				FVector direction0 = targetSpline->GetWorldDirectionAtDistanceAlongSpline(FMath::Clamp(distance1, 1.0f, length - 1.0f));
				FVector direction1 = splineComponent->GetWorldDirectionAtDistanceAlongSpline(FMath::Clamp(thisLength, 1.0f, thisLength - 1.0f));

				thisEndPointConnected &= (FVector::DotProduct(direction0, direction1) > 0.0f);
			}

			// If any of the end points are in range of the target spline, then add links in here.
			// Here we're grafting splineComponent onto spline, and of course the other way around. Note
			// that this will only happen once for each link on each spline as there is a check for
			// duplicates on AddSplineLink.

			if (thisStartPointConnected == true)
			{
				// So the start point on splineComponent is connected to targetSpline.

				// Add the start (0) of this spline onto the target spline at the found distance.

				targetSpline->AddSplineLink(FSplineLink(splineComponent, distance0, 0.0f, true));

				// Add the found distance of target spline onto the start (0) of this spline (because
				// it was the start of the this spline).

				splineComponent->AddSplineLink(FSplineLink(targetSpline, 0.0f, distance0, false));
			}

			if (thisEndPointConnected == true)
			{
				// So the end point on splineComponent is connected to targetSpline.

				// Add the end (thisLength) of this spline onto the target spline at the found distance.

				targetSpline->AddSplineLink(FSplineLink(splineComponent, distance1, thisLength, false));

				// Add the found distance of target spline onto the end (thisLength) of this spline
				// (because it was the end of the this spline).

				splineComponent->AddSplineLink(FSplineLink(targetSpline, thisLength, distance1, true));
			}

			// Sort the links according to the distance they're connected to this spline at.

			splineComponent->SplineLinks.Sort([](const FSplineLink& object1, const FSplineLink& object2) { return object1.ThisDistance < object2.ThisDistance; });

			splineComponent->DeadStart = false;
			splineComponent->DeadEnd = false;

			if (splineComponent->IsClosedLoop() == false)
			{
				if (splineComponent->SplineLinks.Num() > 0)
				{
					splineComponent->DeadStart = (splineComponent->SplineLinks[0].ThisDistance > 100.0f);
					splineComponent->DeadEnd = (splineComponent->SplineLinks[splineComponent->SplineLinks.Num() - 1].ThisDistance < splineComponent->GetSplineLength() - 100.0f);
				}
			}
		}
	}

	return true;
}

/**
* Calculate the extended point data by examining the scene around the spline.
***********************************************************************************/

bool APursuitSplineActor::Build(bool fromMenu)
{
	SynchronisePointData();

	TArray<UActorComponent*> splines;

	GetComponents(UPursuitSplineComponent::StaticClass(), splines);

	for (UActorComponent* component : splines)
	{
		UPursuitSplineComponent* spline = Cast<UPursuitSplineComponent>(component);

		spline->Build(fromMenu, false, false);
	}

	return true;
}

#pragma region AINavigation

/**
* Find the nearest pursuit spline to a world space location.
*
* If you opt to matchMasterDistanceAlong then you need to provide that distance
* in distanceAlong.
***********************************************************************************/

bool APursuitSplineActor::FindNearestPursuitSpline(const FVector& location, const FVector& direction, UWorld* world, TWeakObjectPtr<UPursuitSplineComponent>& pursuitSpline, float& distanceAway, float& distanceAlong, EPursuitSplineType type, bool visibleOnly, bool matchMasterDistanceAlong, bool allowDeadStarts, bool allowDeadEnds, float minMatchingDistance)
{
	APlayGameMode* gameMode = APlayGameMode::Get(world);

	if (gameMode == nullptr)
	{
		return false;
	}

	UPursuitSplineComponent* masterSpline = gameMode->MasterRacingSpline.Get();
	float masterSplineLength = gameMode->MasterRacingSplineLength;
	float masterDistance = distanceAlong;

	if (masterSpline == nullptr)
	{
		matchMasterDistanceAlong = false;
	}

	distanceAway = -1.0f;
	pursuitSpline.Reset();

	TArray<FSplineDistance2> sortedSplines;

	for (APursuitSplineActor* splineActor : gameMode->GetPursuitSplines())
	{
		TArray<UActorComponent*> splines;

		splineActor->GetComponents(UPursuitSplineComponent::StaticClass(), splines);

		for (UActorComponent* component : splines)
		{
			UPursuitSplineComponent* splineComponent = Cast<UPursuitSplineComponent>(component);

			if ((allowDeadStarts == true || splineComponent->DeadStart == false) &&
				(allowDeadEnds == true || splineComponent->DeadEnd == false))
			{
				if (splineComponent->Enabled == true &&
					splineComponent->Type == type &&
					splineComponent->GetNumberOfSplinePoints() > 1)
				{
					float distance = 0.0f;
					float maxMatchingDistance = 250.0f * 100.0f;

					if (masterSpline == splineComponent &&
						matchMasterDistanceAlong == true)
					{
						// This is the master spline and we're looking to match a master distance
						// so we can focus our search to a small area.

						distance = splineComponent->GetNearestDistance(location, masterDistance - maxMatchingDistance * 2.0f, masterDistance + maxMatchingDistance * 2.0f);
					}
					else if (matchMasterDistanceAlong == true)
					{
						distance = splineComponent->GetNearestDistanceToMasterDistance(masterDistance);
					}
					else
					{
						distance = splineComponent->GetNearestDistance(location);
					}

					FVector difference = location - splineComponent->GetWorldLocationAtDistanceAlongSpline(distance);

					if (matchMasterDistanceAlong == true)
					{
						float thisMasterDistance = splineComponent->GetMasterDistanceAtDistanceAlongSpline(distance, masterSplineLength);
						float distanceDifference = masterSpline->GetDistanceDifference(masterDistance, thisMasterDistance);
						float maxDistance = FMath::Max(minMatchingDistance, maxMatchingDistance);

						if (distanceDifference > maxDistance)
						{
							continue;
						}
					}

					sortedSplines.Emplace(FSplineDistance2(splineComponent, difference.Size(), distance));
				}
			}
		}
	}

	FCollisionQueryParams queryParams(TEXT("SplineEnvironmentSensor"), false, nullptr);

	sortedSplines.Sort([](const FSplineDistance2& object1, const FSplineDistance2& object2)
		{
			return object1.DistanceAway < object2.DistanceAway;
		});

	while (true)
	{
		FHitResult hit;

		for (FSplineDistance2& sortedSpline : sortedSplines)
		{
			float sortedLength = sortedSpline.Spline->GetSplineLength();
			FVector splineLocation = sortedSpline.Spline->GetWorldLocationAtDistanceAlongSpline(sortedSpline.DistanceAlong);

			if (visibleOnly == false ||
				sortedSpline.Spline->IsWorldLocationWithinRange(sortedSpline.DistanceAlong, location) == true ||
				world->LineTraceSingleByChannel(hit, location, splineLocation, ABaseGameMode::ECC_LineOfSightTest, queryParams) == false)
			{
				// Return this spline to the caller as it now meets our conditions.

				pursuitSpline = sortedSpline.Spline;
				distanceAlong = sortedSpline.DistanceAlong;
				distanceAway = sortedSpline.DistanceAway;

				return visibleOnly;
			}
		}

		if (visibleOnly == true)
		{
			// Fallback to invisible splines if possible.

			visibleOnly = false;
		}
		else
		{
			// Otherwise we already did the invisible splines so break out.

			break;
		}
	}

	// If we couldn't find a suitable spline that you were close to then simply use the master
	// spline itself.

	if (masterSpline != nullptr)
	{
		UE_LOG(GripLogPursuitSplines, Warning, TEXT("Couldn't find a good spline in FindNearestPursuitSpline so just returning the master racing spline instead"));

		FVector splineLocation = masterSpline->GetWorldLocationAtDistanceAlongSpline(masterDistance);
		FVector difference = location - splineLocation;

		distanceAway = difference.Size();

		pursuitSpline = masterSpline;
		distanceAlong = masterDistance;
	}

	return visibleOnly;
}

#pragma endregion AINavigation

#pragma endregion NavigationSplines
