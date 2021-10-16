/**
*
* Pursuit spline components.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* This kind of spline is used primarily for AI bot track navigation, but also for
* cinematic camera work, weather determination and also for the Assassin missile
* navigation in the full version of the game. They're also critically important for
* race position determination.
*
***********************************************************************************/

#include "ai/pursuitsplinecomponent.h"
#include "ai/pursuitsplineactor.h"
#include "kismet/kismetmathlibrary.h"
#include "kismet/kismetmateriallibrary.h"
#include "system/mathhelpers.h"
#include "gamemodes/playgamemode.h"

DEFINE_LOG_CATEGORY(GripLogPursuitSplines);

/**
* Construct a pursuit spline component.
***********************************************************************************/

UPursuitSplineComponent::UPursuitSplineComponent()
{

#pragma region NavigationSplines

	PursuitSplineParent = Cast<APursuitSplineActor>(GetOwner());

	if (PursuitSplineParent != nullptr)
	{
		ActorName = PursuitSplineParent->GetName();
	}

#pragma endregion NavigationSplines

}

/**
* Set the spline component for this spline mesh component.
***********************************************************************************/

void UPursuitSplineMeshComponent::SetupSplineComponent(UPursuitSplineComponent* splineComponent, int32 startPoint, int32 endPoint, bool selected)
{

#pragma region NavigationSplines

	PursuitSplineComponent = splineComponent;
	PursuitSplineComponent->PursuitSplineMeshComponents.Emplace(this);

	StartPoint = startPoint;
	EndPoint = endPoint;

	SetupMaterial(selected);

#pragma endregion NavigationSplines

}

/**
* Setup the rendering material for this spline mesh component.
***********************************************************************************/

void UPursuitSplineMeshComponent::SetupMaterial(bool selected)
{

#pragma region NavigationSplines

	UStaticMesh* mesh = GetStaticMesh();

	if (mesh != nullptr)
	{
		UMaterialInterface* originalMaterial = GetMaterial(0);
		UMaterialInstanceDynamic* dynamicMaterial = Cast<UMaterialInstanceDynamic>(originalMaterial);

		// Create a dynamic material for this mesh if not already done so.

		if (dynamicMaterial == nullptr)
		{
			dynamicMaterial = UKismetMaterialLibrary::CreateDynamicMaterialInstance(nullptr, originalMaterial);

			SetMaterial(0, dynamicMaterial);
		}

		float s0 = PursuitSplineComponent->GetOptimumSpeedAtSplinePoint(StartPoint);
		float s1 = PursuitSplineComponent->GetOptimumSpeedAtSplinePoint(EndPoint);

		if (s0 == 0.0f)
		{
			s0 = 1000.0f;
		}

		if (s1 == 0.0f)
		{
			s1 = 1000.0f;
		}

		// Colorize the spline according to its optimum speed.

		s0 = FMath::Pow(FMath::Clamp(s0, 0.0f, 1000.0f) / 1000.0f, 0.5f) * 360.0f;
		s1 = FMath::Pow(FMath::Clamp(s1, 0.0f, 1000.0f) / 1000.0f, 0.5f) * 360.0f;

		FLinearColor sc0 = UKismetMathLibrary::HSVToRGB(s0, 1.0f, 0.75f, 1.0f);
		FLinearColor sc1 = UKismetMathLibrary::HSVToRGB(s1, 1.0f, 0.75f, 1.0f);

		if (PursuitSplineComponent->Type == EPursuitSplineType::MissileAssistance)
		{
			// Missile splines always rendered in white.

			sc0 = UKismetMathLibrary::HSVToRGB(s0, 0.0f, 0.5f, 1.0f);
			sc1 = UKismetMathLibrary::HSVToRGB(s1, 0.0f, 0.5f, 1.0f);
		}

		// Set all of the scalar and vector parameters on this material so that it
		// can be rendered with the correct attributes.

		dynamicMaterial->SetScalarParameterValue("Selected", (selected == true) ? 1.0f : 0.0f);
		dynamicMaterial->SetVectorParameterValue("Speed0", sc0);
		dynamicMaterial->SetVectorParameterValue("Speed1", sc1);
		dynamicMaterial->SetScalarParameterValue("Width0", PursuitSplineComponent->GetWidthAtSplinePoint(StartPoint));
		dynamicMaterial->SetScalarParameterValue("Width1", PursuitSplineComponent->GetWidthAtSplinePoint(EndPoint));
		dynamicMaterial->SetScalarParameterValue("Distance0", PursuitSplineComponent->GetDistanceAlongSplineAtSplinePoint(StartPoint) / (10.0f * 100.0f));

		if (EndPoint == 0 &&
			PursuitSplineComponent->IsClosedLoop() == true)
		{
			dynamicMaterial->SetScalarParameterValue("Distance1", PursuitSplineComponent->GetSplineLength() / (10.0f * 100.0f));
		}
		else
		{
			dynamicMaterial->SetScalarParameterValue("Distance1", PursuitSplineComponent->GetDistanceAlongSplineAtSplinePoint(EndPoint) / (10.0f * 100.0f));
		}
	}

#pragma endregion NavigationSplines

}

#pragma region NavigationSplines

static const float UnlimitedSplineDistance = 1000.0f * 100.0f;

/**
* Get the angle difference between to environment samples.
***********************************************************************************/

float FPursuitPointExtendedData::DifferenceInDegrees(int32 indexFrom, int32 indexTo)
{
	float angleFrom = indexFrom * (360.0f / NumDistances);
	float angleTo = indexTo * (360.0f / NumDistances);

	return FMath::Abs(FMathEx::GetUnsignedDegreesDifference(angleFrom, angleTo));
}

/**
* Get the average tunnel diameter over a set distance.
***********************************************************************************/

float FRouteFollower::GetTunnelDiameterOverDistance(float distance, float overDistance, int32 direction, bool minimum) const
{
	float c0 = 0.0f;
	float c1 = 0.0f;

	if (GRIP_POINTER_VALID(ThisSpline) == true)
	{
		c0 = c1 = ThisSpline->GetTunnelDiameterOverDistance(distance, overDistance, direction, minimum);
	}

	if (GRIP_POINTER_VALID(NextSpline) == true &&
		NextSpline != ThisSpline)
	{
		c1 = NextSpline->GetTunnelDiameterOverDistance(NextSwitchDistance, overDistance, direction, minimum);
	}

	if (minimum == true)
	{
		return FMath::Min(c0, c1);
	}
	else
	{
		return (c0 + c1) * 0.5f;
	}
}

/**
* Get the average tunnel diameter over a set distance.
***********************************************************************************/

float UPursuitSplineComponent::GetTunnelDiameterOverDistance(float distance, float overDistance, int32 direction, bool minimum) const
{
	if (PursuitSplineParent->PointExtendedData.Num() < 2)
	{
		return 0.0f;
	}

	float length = GetSplineLength();
	float endDistance = distance + (overDistance * direction);

	if (IsClosedLoop() == false)
	{
		endDistance = ClampDistanceAgainstLength(endDistance, length);
	}

	float averageDiameter = 0.0f;
	float iterationDistance = FMathEx::MetersToCentimeters(ExtendedPointMeters);
	int32 numIterations = FMath::CeilToInt(FMath::Abs(endDistance - distance) / iterationDistance);

	for (int32 i = 0; i <= numIterations; i++)
	{
		float diameter = GetTunnelDiameterAtDistanceAlongSpline(distance);

		if (minimum == true)
		{
			if (i == 0 ||
				averageDiameter > diameter)
			{
				averageDiameter = diameter;
			}
		}
		else
		{
			averageDiameter += diameter;
		}

		distance = ClampDistanceAgainstLength(distance + (iterationDistance * direction), length);
	}

	if (minimum == true)
	{
		return averageDiameter;
	}
	else
	{
		return averageDiameter / (numIterations + 1);
	}
}

/**
* Get the tunnel diameter at a distance along a spline.
***********************************************************************************/

float UPursuitSplineComponent::GetTunnelDiameterAtDistanceAlongSpline(float distance) const
{
	if (PursuitSplineParent->PointExtendedData.Num() < 2)
	{
		return 0.0f;
	}

	int32 thisKey = 0;
	int32 nextKey = 0;
	float ratio = 0.0f;

	GetExtendedPointKeys(distance, thisKey, nextKey, ratio);

	float v0 = PursuitSplineParent->PointExtendedData[thisKey].MaxTunnelDiameter;
	float v1 = PursuitSplineParent->PointExtendedData[nextKey].MaxTunnelDiameter;

	const float notATunnel = 100.0f * 100.0f;

	if (v0 <= 0.0f &&
		v1 <= 0.0f)
	{
		return notATunnel;
	}

	if (v0 <= 0.0f)
	{
		v0 = notATunnel;
	}

	if (v1 <= 0.0f)
	{
		v1 = notATunnel;
	}

	return FMath::Min(FMath::Lerp(v0, v1, ratio), notATunnel);
}

/**
* Is the spline and distance referenced by this link valid for a route choice
* decision?
***********************************************************************************/

bool FSplineLink::LinkIsRouteChoice() const
{
	// Either a closed loop or at least 50m left on the spline at the point we link to it
	// in order for it to be worthwhile.

	return (ForwardLink == true && (Spline->IsClosedLoop() == true || (Spline->GetSplineLength() - NextDistance) >= 50.0f * 100.0f));
}

/**
* Add a spline link to this spline component.
***********************************************************************************/

void UPursuitSplineComponent::AddSplineLink(const FSplineLink& link)
{
	if (SplineLinks.Find(link) == INDEX_NONE)
	{
		SplineLinks.Emplace(link);
	}
}

/**
* Calculate the extended point data by examining the scene around the spline.
***********************************************************************************/

void UPursuitSplineComponent::Build(bool fromMenu, bool performChecks, bool bareData, TArray<FVector>* intersectionPoints)
{
	APursuitSplineActor* owner = Cast<APursuitSplineActor>(GetAttachmentRootActor());

	if (owner != nullptr)
	{
		CalculateSections();
	}
}

/**
* Post initialize the component.
***********************************************************************************/

void UPursuitSplineComponent::PostInitialize()
{
	Build(false, false, true, nullptr);

	Super::PostInitialize();

	int32 numPoints = GetNumberOfSplinePoints();

	ensureMsgf(numPoints > 1, TEXT("Not enough points on a pursuit spline"));

	TArray<FPursuitPointExtendedData>& pursuitPointExtendedData = PursuitSplineParent->PointExtendedData;

	for (FPursuitPointExtendedData& point : pursuitPointExtendedData)
	{
		point.Quaternion = GetQuaternionAtDistanceAlongSpline(point.Distance, ESplineCoordinateSpace::World);
	}
}

/**
* Get the master distance at a distance along a spline.
***********************************************************************************/

float UPursuitSplineComponent::GetMasterDistanceAtDistanceAlongSpline(float distance, float masterSplineLength) const
{
	if (PursuitSplineParent->PointExtendedData.Num() < 2)
	{
		return 0.0f;
	}

	int32 thisKey = 0;
	int32 nextKey = 0;
	float ratio = 0.0f;

	GetExtendedPointKeys(distance, thisKey, nextKey, ratio);

	float v0 = PursuitSplineParent->PointExtendedData[thisKey].MasterSplineDistance;
	float v1 = PursuitSplineParent->PointExtendedData[nextKey].MasterSplineDistance;

	ensureMsgf(v0 != -1.0f && v1 != -1.0f, TEXT("Bad master spline distance"));

	if (v1 >= v0 ||
		masterSplineLength == 0.0f ||
		v0 - v1 < masterSplineLength * 0.25f)
	{
		// Handle the easy case of master distance interpolation.

		return FMath::Lerp(v0, v1, ratio);
	}
	else
	{
		// Need to work out the break going across the wrap here. This normally happens
		// because the master spline has wrapped, it's starting point, happens to be
		// between the two extended data points that we need to sample.

		float l0 = masterSplineLength - v0;	// end length
		float l1 = v1;						// start length
		float lt = l0 + l1;					// total length
		float l = ratio * lt;

		if (l <= l0 &&
			l0 > 0.0f)
		{
			return FMath::Lerp(v0, masterSplineLength, l / l0);
		}
		else if (l1 > 0.0f)
		{
			return FMath::Lerp(0.0f, v1, (l - l0) / l1);
		}
		else
		{
			return v1;
		}
	}
}

/**
* Get the extended point keys bounding a distance along the spline.
***********************************************************************************/

void UPursuitSplineComponent::GetExtendedPointKeys(float distance, int32& key0, int32& key1, float& ratio) const
{
	TArray<FPursuitPointExtendedData>& pursuitPointExtendedData = PursuitSplineParent->PointExtendedData;
	int32 numIndices = pursuitPointExtendedData.Num();

	if (numIndices > 1)
	{
		float length = GetSplineLength();

		distance = ClampDistanceAgainstLength(distance, length);

		// Ratio between 0 and 1 for the entire spline.

		float pointLength = length / (float)(numIndices - 1);

		ratio = distance / pointLength;

		key0 = ThisExtendedKey(pursuitPointExtendedData, ratio);
		key1 = NextExtendedKey(pursuitPointExtendedData, ratio);

		int32 attempts = 2;

		while (attempts-- > 0)
		{
			FPursuitPointExtendedData& p0 = pursuitPointExtendedData[key0];

			if (distance < p0.Distance)
			{
				key0 = BindExtendedKey(pursuitPointExtendedData, key0 - 1);
				key1 = BindExtendedKey(pursuitPointExtendedData, key1 - 1);
			}
			else if (distance - p0.Distance > pointLength * 1.5f)
			{
				key0 = BindExtendedKey(pursuitPointExtendedData, key0 + 1);
				key1 = BindExtendedKey(pursuitPointExtendedData, key1 + 1);
			}
			else
			{
				break;
			}
		}

		FPursuitPointExtendedData& p0 = pursuitPointExtendedData[key0];

		ratio = (distance - p0.Distance) / pointLength;
		ratio = FMath::Clamp(ratio, 0.0f, 1.0f);

		ensure(key0 >= 0 && key0 < numIndices);
		ensure(key1 >= 0 && key1 < numIndices);
	}
	else
	{
		ratio = 0.0f;
		key0 = key1 = 0;
	}
}

/**
* Calculate distances along the master spline for this spline and each of its links.
***********************************************************************************/

bool UPursuitSplineComponent::CalculateMasterSplineDistances(UPursuitSplineComponent* masterSpline, float masterSplineLength, float startingDistance, int32 degreesOfSeparation, bool report, int32 recalibrate, int32 recalibrationAttempt)
{
	bool reportGoodData = recalibrate == 2;

	if (recalibrate != 0 &&
		MasterDistanceClass < 2)
	{
		return reportGoodData;
	}

	// Do the calculation.

	bool result = false;
	int32 dataClass = degreesOfSeparation;

	if (recalibrate != 0 ||
		HasMasterSplineDistances() == false)
	{
		TArray<FPursuitPointExtendedData>& pursuitPointExtendedData = PursuitSplineParent->PointExtendedData;
		int32 numExtendedPoints = pursuitPointExtendedData.Num();

		if (numExtendedPoints > 0)
		{
			// UE_LOG(GripLogPursuitSplines, Log, TEXT("CalculateMasterSplineDistances for %s with %d points starting at %0.01f"), *ActorName, numExtendedPoints, startingDistance);

			if (this == masterSpline)
			{
				// Simple case, this is the master spline so just copy across the regular distances.

				if (recalibrate == 0)
				{
					for (int32 i = 0; i < numExtendedPoints; i++)
					{
						FPursuitPointExtendedData& point = pursuitPointExtendedData[i];

						point.MasterSplineDistance = point.Distance;
					}

					if (report == true)
					{
						UE_LOG(GripLogPursuitSplines, Log, TEXT("Pursuit spline %s calculated master distances with class %d data."), *ActorName, degreesOfSeparation + 1);
					}

					MasterDistanceClass = dataClass;

					result = true;
				}
				else
				{
					result = reportGoodData;
				}
			}
			else
			{
				float accuracy = 1.0f;
				float scanSpan = 16.0f;
				int32 numIterations = 5;
				float masterDistance = startingDistance;
				float movementSize = FMathEx::MetersToCentimeters(ExtendedPointMeters);
				int32 numSamples = masterSpline->GetNumSamplesForRange(movementSize * scanSpan, numIterations, accuracy);

				// UE_LOG(GripLogPursuitSplines, Log, TEXT("CalculateMasterSplineDistances numSamples is %d"), numSamples);

				bool linkedStart = false;
				bool linkedEnd = false;
				float startDistance = 0.0f;
				float endDistance = 0.0f;
				float startDistanceOffset = 0.0f;
				float endDistanceOffset = 0.0f;
				float splineLength = GetSplineLength();

				UPursuitSplineComponent* startSpline = nullptr;
				UPursuitSplineComponent* endSpline = nullptr;

				for (FSplineLink& link : SplineLinks)
				{
					if (link.Spline == masterSpline)
					{
						if (link.ThisDistance < KINDA_SMALL_NUMBER)
						{
							linkedStart = true;
							startSpline = link.Spline.Get();
							startDistance = link.NextDistance;
						}
						else if (link.ThisDistance >= splineLength - KINDA_SMALL_NUMBER)
						{
							linkedEnd = true;
							endSpline = link.Spline.Get();
							endDistance = link.NextDistance;
						}
					}
				}

				if (degreesOfSeparation > 0)
				{
					if (linkedStart == false)
					{
						// We have no start link. See if any of the splines we're linked to are connected
						// to the master spline at their starts.

						for (FSplineLink& link : SplineLinks)
						{
							if (link.ThisDistance < KINDA_SMALL_NUMBER)
							{
								for (FSplineLink& childLink : link.Spline->SplineLinks)
								{
									if (childLink.Spline == masterSpline &&
										childLink.ThisDistance < KINDA_SMALL_NUMBER)
									{
										startSpline = link.Spline.Get();

										if (link.Spline->HasMasterSplineDistances() == true)
										{
											// It's best if we can grab a master distance directly from the connected spline.

											linkedStart = true;
											startDistance = link.Spline->GetMasterDistanceAtDistanceAlongSpline(link.NextDistance, masterSplineLength);
											break;
										}
										else if (degreesOfSeparation > 1)
										{
											// This is OK too, but it's not as accurate and can deviate by hundreds of meters.

											linkedStart = true;
											startDistance = childLink.NextDistance;
											startDistanceOffset = link.NextDistance;
											break;
										}
									}
								}
							}

							if (linkedStart == true)
							{
								break;
							}
						}
					}

					if (linkedStart == true &&
						linkedEnd == false)
					{
						// We have a start link, but no end. See if any of the splines we're linked to are connected
						// to the master spline at their ends.

						for (FSplineLink& link : SplineLinks)
						{
							if (link.ThisDistance >= splineLength - KINDA_SMALL_NUMBER)
							{
								float childSplineLength = link.Spline->GetSplineLength();

								for (FSplineLink& childLink : link.Spline->SplineLinks)
								{
									if (childLink.Spline == masterSpline &&
										childLink.ThisDistance >= childSplineLength - KINDA_SMALL_NUMBER)
									{
										endSpline = link.Spline.Get();

										if (link.Spline->HasMasterSplineDistances() == true)
										{
											// It's best if we can grab a master distance directly from the connected spline.

											linkedEnd = true;
											endDistance = link.Spline->GetMasterDistanceAtDistanceAlongSpline(link.NextDistance, masterSplineLength);
											break;
										}
										else if (degreesOfSeparation > 1)
										{
											// This is OK too, but it's not as accurate and can deviate by hundreds of meters.

											linkedEnd = true;
											endDistance = childLink.NextDistance;
											endDistanceOffset = childLink.ThisDistance - link.NextDistance;
											break;
										}
									}
								}
							}

							if (linkedEnd == true)
							{
								break;
							}
						}
					}
				}

				if (recalibrate == 1 &&
					recalibrationAttempt > 0)
				{
					if (linkedStart == false ||
						linkedEnd == false)
					{
						startSpline = endSpline = nullptr;

						for (FSplineLink& link : SplineLinks)
						{
							if (link.Spline->HasMasterSplineDistances() == true)
							{
								if (startSpline == nullptr && link.Spline->MasterDistanceClass < 3 && link.ThisDistance < KINDA_SMALL_NUMBER)
								{
									linkedStart = true;
									startSpline = link.Spline.Get();
									startDistance = link.Spline->GetMasterDistanceAtDistanceAlongSpline(link.NextDistance, masterSplineLength);
								}
								else if (endSpline == nullptr && link.Spline->MasterDistanceClass < 3 && link.ThisDistance >= splineLength - KINDA_SMALL_NUMBER)
								{
									linkedEnd = true;
									endSpline = link.Spline.Get();
									endDistance = link.Spline->GetMasterDistanceAtDistanceAlongSpline(link.NextDistance, masterSplineLength);
								}
							}
						}
					}
				}

				float totalSplineLength = startDistanceOffset + splineLength + endDistanceOffset;

				if (linkedStart == true &&
					linkedEnd == true &&
					splineLength > KINDA_SMALL_NUMBER &&
					totalSplineLength > KINDA_SMALL_NUMBER)
				{
					bool regenerate = false;

					if (recalibrate != 0)
					{
						FPursuitPointExtendedData& first = pursuitPointExtendedData[0];
						FPursuitPointExtendedData& last = pursuitPointExtendedData.Last();
						float startDifference = masterSpline->GetDistanceDifference(startDistance, first.MasterSplineDistance);
						float endDifference = masterSpline->GetDistanceDifference(endDistance, last.MasterSplineDistance);

						int32 numGood = 0;
						int32 numBad = 0;

						if (recalibrate == 1 &&
							startDifference > 25.0f * 100.0f)
						{
							if (report == true)
							{
								UE_LOG(GripLogPursuitSplines, Log, TEXT("Pursuit spline %s calculated master distances are out at the start by %dm"), *ActorName, (int32)(startDifference / 100.0f));
							}

							if (startSpline != nullptr &&
								startSpline->CalculateMasterSplineDistances(masterSpline, masterSplineLength, startingDistance, degreesOfSeparation, false, 2) == true)
							{
								numGood++;

								if (report == true)
								{
									UE_LOG(GripLogPursuitSplines, Log, TEXT("Pursuit spline %s it's connected to has good data"), *startSpline->ActorName);
								}
							}
							else
							{
								numBad++;

								if (report == true)
								{
									UE_LOG(GripLogPursuitSplines, Log, TEXT("Pursuit spline %s it's connected to has insufficient data"), *startSpline->ActorName);
								}
							}
						}

						if (recalibrate == 1 &&
							endDifference > 25.0f * 100.0f)
						{
							if (report == true)
							{
								UE_LOG(GripLogPursuitSplines, Log, TEXT("Pursuit spline %s calculated master distances are out at the end by %dm"), *ActorName, (int32)(endDifference / 100.0f));
							}

							if (endSpline != nullptr &&
								endSpline->CalculateMasterSplineDistances(masterSpline, masterSplineLength, startingDistance, degreesOfSeparation, false, 2) == true)
							{
								numGood++;

								if (report == true)
								{
									UE_LOG(GripLogPursuitSplines, Log, TEXT("Pursuit spline %s it's connected to has good data"), *endSpline->ActorName);
								}
							}
							else
							{
								numBad++;

								if (report == true)
								{
									UE_LOG(GripLogPursuitSplines, Log, TEXT("Pursuit spline %s it's connected to has insufficient data"), *endSpline->ActorName);
								}
							}
						}

						regenerate = numGood > 0 && numBad == 0;

						if (reportGoodData == true)
						{
							result = startDifference <= 25.0f * 100.0f && endDifference <= 25.0f * 100.0f;
						}

						if (recalibrate == 1 &&
							regenerate == true)
						{
							if (startSpline == nullptr)
							{
								startSpline = this;
							}

							if (endSpline == nullptr)
							{
								endSpline = this;
							}

							dataClass = FMath::Max(startSpline->MasterDistanceClass, endSpline->MasterDistanceClass);

							UE_LOG(GripLogPursuitSplines, Log, TEXT("Pursuit spline %s is being regenerated from the good data"), *ActorName);
						}
					}

					if (recalibrate == 0 ||
						regenerate == true)
					{
						// Easy case where the start and end points of the spline are connected directly to the master spline,
						// or indirectly via splines we're directly connected to which are in themselves directly connected
						// to the master spline - so only one degree of separation.

						float masterSectionLength = (startDistance < endDistance) ? endDistance - startDistance : (masterSplineLength - startDistance) + endDistance;

						for (int32 i = 0; i < numExtendedPoints; i++)
						{
							FPursuitPointExtendedData& point = pursuitPointExtendedData[i];
							float distance = (point.Distance + startDistanceOffset) / totalSplineLength;

							distance *= masterSectionLength;
							distance += startDistance;
							distance = FMath::Fmod(distance, masterSplineLength);

							point.MasterSplineDistance = distance;
						}

						if (report == true)
						{
							UE_LOG(GripLogPursuitSplines, Log, TEXT("Pursuit spline %s calculated master distances with class %d data."), *ActorName, dataClass + 1);
						}

						MasterDistanceClass = dataClass;

						result = true;
					}
				}
				else if (degreesOfSeparation == 3)
				{
					if (recalibrate == 0)
					{
						for (int32 i = 0; i < numExtendedPoints; i++)
						{
							FPursuitPointExtendedData& point = pursuitPointExtendedData[i];
							float t0 = masterDistance - (movementSize * scanSpan * 0.5f);
							float t1 = masterDistance + (movementSize * scanSpan * 0.5f);

							point.MasterSplineDistance = masterSpline->GetNearestDistance(GetWorldLocationAtDistanceAlongSpline(point.Distance), t0, t1, numIterations, numSamples);

							masterDistance = point.MasterSplineDistance;
						}

						if (report == true)
						{
							UE_LOG(GripLogPursuitSplines, Log, TEXT("Pursuit spline %s calculated master distances with class %d data."), *ActorName, dataClass + 1);
						}

						MasterDistanceClass = dataClass;

						result = true;
					}
				}
				else
				{
					return result;
				}
			}

			if (recalibrate == 0)
			{
				for (FSplineLink& link : SplineLinks)
				{
					if (link.ForwardLink == true &&
						link.NextDistance < 100.0f &&
						link.Spline->HasMasterSplineDistances() == false)
					{
						// UE_LOG(GripLogPursuitSplines, Log, TEXT("Linking forward to spline %s at distance %0.01f"), *link.Spline->ActorName, link.ThisDistance);

						result |= link.Spline->CalculateMasterSplineDistances(masterSpline, masterSplineLength, GetMasterDistanceAtDistanceAlongSpline(link.ThisDistance, masterSplineLength), degreesOfSeparation, report);
					}
					else
					{
						// UE_LOG(GripLogPursuitSplines, Log, TEXT("Rejected spline link to %s at distance %0.01f"), *link.Spline->ActorName, link.ThisDistance);
					}
				}
			}
		}
		else
		{
			UE_LOG(GripLogPursuitSplines, Log, TEXT("No extended points in CalculateMasterSplineDistances"));
		}
	}

	return result;
}

/**
* Helper function when using the Editor.
***********************************************************************************/

TStructOnScope<FActorComponentInstanceData> UPursuitSplineComponent::GetComponentInstanceData() const
{
	auto InstanceData = MakeStructOnScope<FActorComponentInstanceData, FPursuitSplineInstanceData>(this);
	FPursuitSplineInstanceData* SplineInstanceData = InstanceData.Cast<FPursuitSplineInstanceData>();

	if (bSplineHasBeenEdited)
	{
		SplineInstanceData->SplineCurves = SplineCurves;
		SplineInstanceData->bClosedLoop = IsClosedLoop();
		SplineInstanceData->Type = Type;
	}

	SplineInstanceData->bSplineHasBeenEdited = bSplineHasBeenEdited;

	return InstanceData;
}

/**
* Helper function when using the Editor.
***********************************************************************************/

void UPursuitSplineComponent::ApplyComponentInstanceData(FPursuitSplineInstanceData* SplineInstanceData, const bool bPostUCS)
{
	check(SplineInstanceData);

	if (bPostUCS)
	{
		if (bInputSplinePointsToConstructionScript)
		{
			// Don't reapply the saved state after the UCS has run if we are inputting the points to it.
			// This allows the UCS to work on the edited points and make its own changes.
			return;
		}
		else
		{
			bModifiedByConstructionScript = (SplineInstanceData->SplineCurvesPreUCS != SplineCurves);
			bModifiedByConstructionScript |= (SplineInstanceData->bClosedLoop != IsClosedLoop());
			bModifiedByConstructionScript |= (SplineInstanceData->TypePreUCS != Type);

			// If we are restoring the saved state, unmark the SplineCurves property as 'modified'.
			// We don't want to consider that these changes have been made through the UCS.
			TArray<FProperty*> Properties;
			Properties.Emplace(FindFProperty<FProperty>(USplineComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(USplineComponent, SplineCurves)));
			RemoveUCSModifiedProperties(Properties);

			Properties.Empty();
			Properties.Emplace(FindFProperty<FProperty>(USplineComponent::StaticClass(), FName(TEXT("bClosedLoop"))));
			RemoveUCSModifiedProperties(Properties);

			Properties.Empty();
			Properties.Emplace(FindFProperty<FProperty>(UPursuitSplineComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(UPursuitSplineComponent, Type)));
			RemoveUCSModifiedProperties(Properties);
		}
	}
	else
	{
		SplineInstanceData->SplineCurvesPreUCS = SplineCurves;
		SplineInstanceData->bClosedLoopPreUCS = IsClosedLoop();
		SplineInstanceData->TypePreUCS = Type;
	}

	if (SplineInstanceData->bSplineHasBeenEdited)
	{
		SplineCurves = SplineInstanceData->SplineCurves;
		SetClosedLoop(SplineInstanceData->bClosedLoop);
		Type = SplineInstanceData->Type;

		bModifiedByConstructionScript = false;
	}

	bSplineHasBeenEdited = SplineInstanceData->bSplineHasBeenEdited;

	UpdateSpline();
}

/**
* Calculate the sections of the spline.
***********************************************************************************/

void UPursuitSplineComponent::CalculateSections()
{
	Super::CalculateSections();

#pragma region CameraCinematics

	float length = GetSplineLength();

	DroneSections.Empty();

	if (true)
	{
		// Now we need to determine the straight sections of this spline. We do
		// this by iterating 100m forwards at a time, and measuring the curvature
		// of the track 100m in front of the point and storing those in a list.
		// We then join any straight sections to each other to form a complete
		// length. To that, we then iterate more slowly from each of the ends
		// until the curvature becomes too great and cap them.

		float distance = 0.0f;
		float maxCurvature = 50.0f;
		float baselargeSectionLength = 100.0f * 100.0f;
		float largeSectionLength = baselargeSectionLength;
		int32 numSections = FMath::CeilToInt(length / baselargeSectionLength);

		largeSectionLength = length / numSections;
		maxCurvature *= largeSectionLength / baselargeSectionLength;

		for (int32 i = 0; i < numSections; i++)
		{
			float overDistance = largeSectionLength;
			bool grounded = GetGroundedOverDistance(distance, overDistance, 1);
			overDistance = largeSectionLength;
			bool broken = GetSurfaceBreakOverDistance(distance, overDistance, 1);
			overDistance = largeSectionLength;
			bool openAir = GetWeatherAllowedOverDistance(distance, overDistance, 1);
			overDistance = largeSectionLength;
			FRotator curvature = UAdvancedSplineComponent::GetCurvatureOverDistance(distance, overDistance, 1, FQuat::Identity, true);

			if (grounded == true &&
				broken == false &&
				openAir == true &&
				curvature.Yaw < maxCurvature &&
				curvature.Pitch < maxCurvature)
			{
				overDistance = largeSectionLength - overDistance;

				float extendEnd = FMath::Min(distance + overDistance, length);

				if (DroneSections.Num() > 0 &&
					FMath::Abs(DroneSections[DroneSections.Num() - 1].EndDistance - distance) < 1.0f)
				{
					// Extend the last section.

					DroneSections[DroneSections.Num() - 1].EndDistance = extendEnd;
				}
				else
				{
					// Begin a new section.

					DroneSections.Emplace(FSplineSection(distance, extendEnd));
				}
			}

			distance += largeSectionLength;
		}

		if (IsClosedLoop() == true &&
			DroneSections.Num() > 1 &&
			DroneSections[0].StartDistance < 1.0f &&
			DroneSections[DroneSections.Num() - 1].EndDistance > length - 1.0f)
		{
			// The first section and the last section are contiguous, so we need to merge them.

			DroneSections[0].StartDistance = DroneSections[DroneSections.Num() - 1].StartDistance;
			DroneSections.RemoveAt(DroneSections.Num() - 1);
		}

		numSections = DroneSections.Num();

		for (int32 i = 0; i < numSections; i++)
		{
			FSplineSection& section = DroneSections[i];

			if (FMath::Abs((section.EndDistance - section.StartDistance) - length) < 1.0f)
			{
				// This section already encompasses the whole spline so no need to extend it.

				continue;
			}

			for (int32 j = 0; j < 2; j++)
			{
				float extend = 0.0f;
				int32 numIterations = 5;
				float smallSectionLength = largeSectionLength / numIterations;
				float start = (j == 0) ? section.StartDistance : section.EndDistance;
				float direction = (j == 0) ? -1.0f : 1.0f;

				for (int32 k = 0; k < numIterations; k++)
				{
					float overDistance = smallSectionLength;
					bool grounded = GetGroundedOverDistance(start + (smallSectionLength * k * direction), overDistance, direction);
					overDistance = smallSectionLength;
					bool broken = GetSurfaceBreakOverDistance(start + (smallSectionLength * k * direction), overDistance, direction);
					overDistance = smallSectionLength;
					bool openAir = GetWeatherAllowedOverDistance(start + (smallSectionLength * k * direction), overDistance, direction);
					overDistance = smallSectionLength;
					FRotator curvature = UAdvancedSplineComponent::GetCurvatureOverDistance(start + (smallSectionLength * k * direction), overDistance, direction, FQuat::Identity, true);

					if (grounded == false ||
						broken == true ||
						openAir == false ||
						curvature.Yaw > maxCurvature / numIterations ||
						curvature.Pitch > maxCurvature / numIterations)
					{
						break;
					}

					extend += smallSectionLength - overDistance;
				}

				if (j == 0)
				{
					section.StartDistance = ClampDistanceAgainstLength(section.StartDistance - extend, length);
				}
				else
				{
					section.EndDistance = ClampDistanceAgainstLength(section.EndDistance + extend, length);

					if (IsClosedLoop() == false &&
						FMath::Abs(section.EndDistance - section.StartDistance) < 100.0f * 100.0f)
					{
						numSections--;
						DroneSections.RemoveAt(i--);
					}
				}
			}
		}
	}
	else
	{
		DroneSections.Emplace(FSplineSection(0.0f, length));
	}

#pragma endregion CameraCinematics

}

/**
* The point data, referenced from the parent actor.
***********************************************************************************/

TArray<FPursuitPointData>& UPursuitSplineComponent::GetPursuitPointData() const
{
	return PursuitSplineParent->PointData;
}

/**
* The extended point data, referenced from the parent actor.
***********************************************************************************/

TArray<FPursuitPointExtendedData>& UPursuitSplineComponent::GetPursuitPointExtendedData() const
{
	return PursuitSplineParent->PointExtendedData;
}

#pragma region AINavigation

/**
* Console variable for testing all track branches equally randomly.
***********************************************************************************/

#if WITH_EDITOR
TAutoConsoleVariable<int32> CVarTestTrackBranches(
	TEXT("grip.TestTrackBranches"),
	0,
	TEXT("Test the track branches.\n")
	TEXT("  0: Off\n")
	TEXT("  1: On\n"),
	ECVF_Default);
#endif

/**
* Weight a probability of a spline based on desirability.
***********************************************************************************/

static float WeightProbability(UPursuitSplineComponent* spline, float pickupWeighting, float shortcutWeighting)
{
	float probability = spline->BranchProbability;
	float result = probability;

	if (spline->IsShortcut == true)
	{
		result += probability * shortcutWeighting;
	}

	if (spline->ContainsPickups == true)
	{
		result += probability * pickupWeighting;
	}

	return result;
}

/**
* Check that a connection from one spline to another has been taken.
***********************************************************************************/

bool FRouteFollower::CheckBranchConnection(UWorld* world, const FVector& position, float atDistance)
{
	bool result = false;

	if (SwitchingSpline == true &&
		(position - SwitchLocation).Size() > atDistance)
	{
		float accuracy = 1.0f;
		int32 numIterations = 5;

		float t0 = LastDistance - atDistance;
		float t1 = LastDistance + atDistance;

		float d = LastSpline->GetNearestDistance(position, t0, t1, numIterations, LastSpline->GetNumSamplesForRange(t1 - t0, numIterations, accuracy));
		FVector pl = LastSpline->GetWorldLocationAtDistanceAlongSpline(d);
		FVector pt = ThisSpline->GetWorldLocationAtDistanceAlongSpline(ThisDistance);

		float dl = (position - pl).Size();
		float dt = (position - pt).Size();

		if (dl > dt)
		{
			// Looks like we're closer to the spline we were aiming for, excellent!
		}
		else
		{
			bool tooFarAway = dt > (ThisSpline->GetWidthAtDistanceAlongSpline(ThisDistance) * 100.0f);

			if (tooFarAway == true)
			{
				result = true;
			}
		}

		SwitchingSpline = false;
	}

	return result;
}

/**
* Estimate where we are along the current spline, faster than DetermineThis.
*
* This will drift out of sync fairly quickly though, so call DetermineThis on
* a regular basis to correct the drift.
***********************************************************************************/

void FRouteFollower::EstimateThis(const FVector& position, const FVector& movement, float movementSize, int32 numIterations, float accuracy)
{
	if (GRIP_POINTER_VALID(ThisSpline) == true)
	{
		FVector splineDirection = ThisSpline->GetWorldSpaceQuaternionAtDistanceAlongSpline(ThisDistance).GetAxisX();
		FVector direction = movement; direction.Normalize();

		// We don't use movementSize here as it can be different to movement.Size(), and we need the latter.

		float splineMovement = movement.Size() * FVector::DotProduct(splineDirection, direction);

		ThisDistance = ThisSpline->ClampDistance(ThisDistance + splineMovement);

		SwitchSplineAtJunction(position, movementSize, numIterations, accuracy);
	}
}

/**
* Determine where we are along the current spline.
***********************************************************************************/

void FRouteFollower::DetermineThis(const FVector& position, float movementSize, int32 numIterations, float accuracy)
{
	if (GRIP_POINTER_VALID(ThisSpline) == true)
	{
		// Do some intelligent nearest point detection that optimizes the number of
		// samples taken to achieve that.

		float t0 = ThisDistance - (movementSize * GRIP_SPLINE_MOVEMENT_MULTIPLIER);
		float t1 = ThisDistance + (movementSize * GRIP_SPLINE_MOVEMENT_MULTIPLIER);

		ThisDistance = ThisSpline->GetNearestDistance(position, t0, t1, numIterations, ThisSpline->GetNumSamplesForRange(t1 - t0, numIterations, accuracy));

		SwitchSplineAtJunction(position, movementSize, numIterations, accuracy);
	}
}

/**
* Determine where we are aiming for along the current or next spline, switching
* splines at branches if necessary.
***********************************************************************************/

void FRouteFollower::DetermineNext(float ahead, float movementSize, UPursuitSplineComponent* preferSpline, bool forMissile, bool wantPickups, bool highOptimumSpeed, float fastPathways)
{
#if WITH_EDITOR
	if (CVarTestTrackBranches->GetInt() != 0)
	{
		fastPathways = 0.0f;
		highOptimumSpeed = false;
	}
#endif // WITH_EDITOR

	if (GRIP_POINTER_VALID(ThisSpline) == true)
	{
		float lastDistance = NextDistance;

		if (ThisSpline == NextSpline)
		{
			NextDistance = NextSpline->ClampDistance(ThisDistance + ahead);

			if (lastDistance > 1.0f &&
				lastDistance == NextDistance)
			{
				lastDistance -= 1.0f;
			}

			// Scan the decision points on this spline to see if we've just crossed one.

			for (FRouteChoice& choice : NextSpline->RouteChoices)
			{
				if ((lastDistance != 0.0f) &&
					(FMath::Abs(lastDistance - NextDistance) < 50.0f * 100.0f) &&
					((lastDistance < NextDistance && lastDistance < choice.DecisionDistance && NextDistance >= choice.DecisionDistance) || (lastDistance >= NextDistance && NextDistance < choice.DecisionDistance && lastDistance >= choice.DecisionDistance)))
				{
					if (DecidedDistance != choice.DecisionDistance)
					{
						// We've just come into the window of having to make a spline choice here.

						ThisSwitchDistance = 0.0f;

						float thisSwitchDistance = 0.0f;
						float nextSwitchDistance = 0.0f;
						float distanceAlong = NextSpline->ClampDistance(NextDistance);

						ChooseNextSpline(NextSpline, distanceAlong, thisSwitchDistance, nextSwitchDistance, choice, movementSize, preferSpline, forMissile, wantPickups, highOptimumSpeed, fastPathways);

						DecidedDistance = choice.DecisionDistance;

						if (ThisSpline != NextSpline)
						{
							// We switched spline, so use the new distance along the new spline.

							ThisSwitchDistance = thisSwitchDistance;
							NextSwitchDistance = nextSwitchDistance;
						}
					}

					break;
				}
			}
		}

		if (ThisSpline != NextSpline)
		{
			NextDistance = ThisDistance + ahead;

			// See if NextSpline is still valid for where we currently are - we could have
			// started to go backwards or the ahead value might have reduced since NextSpline
			// was originally set.

			// #TODO: This seems to be misfiring on occasion.

			if (DecidedDistance >= 0.0f &&
				lastDistance < DecidedDistance &&
				NextDistance < DecidedDistance)
			{
				DecidedDistance = -1.0f;
				ThisSwitchDistance = 0.0f;
				NextSpline = ThisSpline;
			}
			else
			{
				// Recalculate the distance into the aiming spline. It's already different
				// to the current spline so we don't look for a new one here.

				if (NextDistance > ThisSwitchDistance)
				{
					NextDistance -= ThisSwitchDistance;
					NextDistance += NextSwitchDistance;
				}
			}
		}
	}
}

/**
* Choose the next spline to hook onto from the route choice given. Use the
* parameters specified to determine which is the best spline to select for the
* use-case given.
***********************************************************************************/

bool FRouteFollower::ChooseNextSpline(TWeakObjectPtr<UPursuitSplineComponent>& pursuitSpline, float distanceAlong, float& thisSwitchDistance, float& nextSwitchDistance, const FRouteChoice& choice, float movementSize, UPursuitSplineComponent* preferSpline, bool forMissile, bool wantPickups, bool highOptimumSpeed, float fastPathways) const
{
	if (choice.SplineLinks.Num() > 0)
	{
		bool foundPreferred = false;
		float totalProbability = 0.0f;
		float pickupWeighting = (wantPickups == true) ? 1.0f : 0.5f;
		float shortcutWeighting = FMath::Clamp(fastPathways * 2.0f, -1.0f, 2.0f);
		FSplineLink useSpline = FSplineLink(pursuitSpline, distanceAlong, distanceAlong);
		bool addPursuitSpline = true;
		TArray<FSplineLink> connectedSplines;

		for (const FSplineLink& link : choice.SplineLinks)
		{
			const TWeakObjectPtr<UPursuitSplineComponent>& spline = link.Spline;

			if (spline->Enabled == true)
			{
				if ((forMissile == false && link.Spline->Type == EPursuitSplineType::General) ||
					(forMissile == true && (link.Spline->Type == EPursuitSplineType::MissileAssistance || (link.Spline->Type == EPursuitSplineType::General && link.Spline->SuitableForMissileGuidance == true))))
				{
					// OK, so this spline is suitable for what we want to use it for.

					useSpline = link;

					connectedSplines.Emplace(link);
					totalProbability += WeightProbability(spline.Get(), pickupWeighting, shortcutWeighting);

					if (spline == pursuitSpline)
					{
						addPursuitSpline = false;
					}

					if (spline->AlwaysSelect == true)
					{
						if (forMissile == false ||
							spline->SuitableForMissileGuidance == true)
						{
							// The spline is set to always select for vehicles so indicate that we've found
							// the preferred spline.

							foundPreferred = true;
							break;
						}
					}
				}
			}
		}

		if (foundPreferred == false)
		{
			// If we've still a way to go on the current spline then also add this as
			// a choice for the next spline.

			if (pursuitSpline->IsClosedLoop() == true ||
				distanceAlong < pursuitSpline->GetSplineLength() - (100.0f * 100.0f))
			{
				if (addPursuitSpline == true)
				{
					connectedSplines.Emplace(FSplineLink(pursuitSpline, distanceAlong, distanceAlong));
					totalProbability += WeightProbability(pursuitSpline.Get(), pickupWeighting, shortcutWeighting);
				}
			}

			if (foundPreferred == false &&
				forMissile == true)
			{
				// If we're tracking a missile then prefer to use specific missile splines as
				// they're designed to keep missiles out of trouble.

				for (FSplineLink& link : connectedSplines)
				{
					if (link.Spline->Type == EPursuitSplineType::MissileAssistance)
					{
						useSpline = link;
						foundPreferred = true;
						break;
					}
				}
			}

			if (foundPreferred == false &&
				preferSpline != nullptr)
			{
				// Look for the preferred spline that we've been passed in this branch.
				// For missiles, this is the spline the target vehicle is on.

				for (FSplineLink& link : connectedSplines)
				{
					if (preferSpline == link.Spline)
					{
						useSpline = link;
						foundPreferred = true;
						break;
					}
				}

				if (foundPreferred == false)
				{
					// Look for the preferred spline that we've been passed in all the branches
					// of the directly connected splines.

					for (FSplineLink& link : connectedSplines)
					{
						for (FSplineLink& nextSpline : link.Spline->SplineLinks)
						{
							if (preferSpline == nextSpline.Spline)
							{
								useSpline = link;
								foundPreferred = true;
								break;
							}
						}

						if (foundPreferred == true)
						{
							break;
						}
					}
				}
			}

			if (foundPreferred == false &&
				forMissile == true)
			{
				// If we're tracking a missile then prefer to use closed loops (the main track)
				// as opposed to side branches.

				for (FSplineLink& link : connectedSplines)
				{
					if (link.Spline->IsClosedLoop() == true)
					{
						useSpline = link;
						foundPreferred = true;
						break;
					}
				}
			}

			if (foundPreferred == false &&
				highOptimumSpeed == true)
			{
				// Look for the spline with the highest optimum speed as we've like got a vehicle
				// here with a turbo boost currently in use.

				float maxOptimumSpeed = 0.0f;
				float minOptimumSpeed = 1000.0f;
				float avgOptimumSpeed = 0.0f;

				for (FSplineLink& link : connectedSplines)
				{
					float overDistance = 500.0f * 100.0f;
					float optimumSpeed = link.Spline->GetMinimumOptimumSpeedOverDistance(link.NextDistance, overDistance, 1);

					optimumSpeed = (optimumSpeed == 0.0f) ? 1000.0f : optimumSpeed;
					minOptimumSpeed = FMath::Min(minOptimumSpeed, optimumSpeed);
					avgOptimumSpeed += optimumSpeed;
				}

				avgOptimumSpeed /= connectedSplines.Num();

				for (FSplineLink& link : connectedSplines)
				{
					float overDistance = 500.0f * 100.0f;
					float optimumSpeed = link.Spline->GetMinimumOptimumSpeedOverDistance(link.NextDistance, overDistance, 1);

					optimumSpeed = (optimumSpeed == 0.0f) ? 1000.0f : optimumSpeed;

					if ((maxOptimumSpeed < optimumSpeed) &&
						(optimumSpeed > avgOptimumSpeed + 50.0f || optimumSpeed > minOptimumSpeed + 100.0f))
					{
						useSpline = link;
						maxOptimumSpeed = optimumSpeed;
						foundPreferred = true;
					}
				}
			}

			if (foundPreferred == false)
			{
				// Right, OK, just look for the spline using the weighting system as it is normally
				// designed to do.

				float amount = 0.0f;
				float probability = FMath::FRand() * totalProbability;

				for (FSplineLink& link : connectedSplines)
				{
					amount += WeightProbability(link.Spline.Get(), pickupWeighting, shortcutWeighting);

					if (probability <= amount)
					{
						useSpline = link;
						foundPreferred = true;
						break;
					}
				}
			}
		}

		if (foundPreferred == false &&
			connectedSplines.Num() > 0)
		{
			useSpline = connectedSplines[connectedSplines.Num() - 1];
		}

		pursuitSpline = useSpline.Spline;
		thisSwitchDistance = useSpline.ThisDistance;
		nextSwitchDistance = useSpline.NextDistance;

		return true;
	}
	else
	{
		return false;
	}
}

/**
* Switch to a new spline if we've passed the switch distance for it.
***********************************************************************************/

void FRouteFollower::SwitchSplineAtJunction(const FVector& position, float movementSize, int32 numIterations, float accuracy)
{
	// So now we know where we are, determine if a new pursuit spline is necessary.
	// We will have identified this already because we aim ahead of where the car
	// actually is, so it's just a question of swapping over.

	if (ThisSwitchDistance != 0.0f &&
		ThisDistance >= ThisSwitchDistance)
	{
		if (ThisSpline != NextSpline)
		{
			for (FSplineLink& link : ThisSpline->SplineLinks)
			{
				if (link.Spline == NextSpline &&
					link.ThisDistance == ThisSwitchDistance &&
					link.NextDistance == NextSwitchDistance)
				{
					SwitchingSpline = true;

					LastSpline = ThisSpline;
					LastDistance = ThisDistance;
					SwitchLocation = position;

					float t0 = link.NextDistance;
					float t1 = link.NextDistance + (movementSize * GRIP_SPLINE_MOVEMENT_MULTIPLIER);

					ThisSpline = NextSpline;
					ThisDistance = ThisSpline->GetNearestDistance(position, t0, t1, numIterations, ThisSpline->GetNumSamplesForRange(t1 - t0, numIterations, accuracy));
					DecidedDistance = -1.0f;

					break;
				}
			}
		}

		ThisSwitchDistance = 0.0f;
	}
}

/**
* Is this spline about to merge with the given spline at the given distance?
***********************************************************************************/

bool UPursuitSplineComponent::IsAboutToMergeWith(UPursuitSplineComponent* pursuitSpline, float distanceAlong)
{
	// Scan the decision points on this spline to see if we've just crossed one.

	for (FRouteChoice& choice : RouteChoices)
	{
		if (distanceAlong >= choice.DecisionDistance - 50.0f * 100.0f &&
			distanceAlong <= choice.DecisionDistance)
		{
			for (FSplineLink& link : choice.SplineLinks)
			{
				if (link.Spline == pursuitSpline &&
					link.ForwardLink == true)
				{
					return true;
				}
			}
		}
	}

	return false;
}

/**
* Is this spline connected to a child spline?
***********************************************************************************/

bool UPursuitSplineComponent::IsSplineConnected(UPursuitSplineComponent* child, float& atDistance, float& childDistance)
{
	for (FSplineLink& link : SplineLinks)
	{
		if (link.Spline == child &&
			link.ForwardLink == true)
		{
			atDistance = link.ThisDistance;
			childDistance = link.NextDistance;

			return true;
		}
	}

	return false;
}

/**
* Get the careful driving at a distance along a spline.
***********************************************************************************/

bool UPursuitSplineComponent::GetCarefulDrivingAtDistanceAlongSpline(float distance) const
{
	if (CarefulDriving == true)
	{
		return true;
	}

	if (PursuitSplineParent->PointExtendedData.Num() < 2)
	{
		return false;
	}

	int32 thisKey = 0;
	int32 nextKey = 0;
	float ratio = 0.0f;

	GetExtendedPointKeys(distance, thisKey, nextKey, ratio);

	FPursuitPointExtendedData& p0 = PursuitSplineParent->PointExtendedData[thisKey];
	FPursuitPointExtendedData& p1 = PursuitSplineParent->PointExtendedData[nextKey];

	bool v0 = p0.OpenLeft || p0.OpenRight;
	bool v1 = p1.OpenLeft || p1.OpenRight;

	return v0 || v1;
}

/**
* Get the maneuvering width at a distance along a spline.
***********************************************************************************/

float UPursuitSplineComponent::GetWidthAtDistanceAlongSpline(float distance) const
{
	float key = SplineCurves.ReparamTable.Eval(distance, 0.0f);
	int32 thisKey = ThisKey(key);
	int32 nextKey = NextKey(key);

	float v0 = PursuitSplineParent->PointData[thisKey].ManeuveringWidth;
	float v1 = PursuitSplineParent->PointData[nextKey].ManeuveringWidth;

	return FMath::Lerp(v0, v1, key - thisKey);
}

/**
* Is a distance and location along a spline within the open space around the spline?
* (this is an inaccurate but cheap test)
***********************************************************************************/

bool UPursuitSplineComponent::IsWorldLocationWithinRange(float distance, FVector location) const
{
	// Get the distance at which the nearest extended point to this location is found on the spline.

	location = WorldSpaceToSplineSpace(location, distance, true);
	location.X = 0.0f;

	FVector splineOffset = location;

	if (splineOffset.Normalize() == false)
	{
		splineOffset = FVector(0.0f, 0.0f, 1.0f);
	}

	return (GetClearance(distance, location, splineOffset, 45.0f, true, 250.0f) > 1.0f);
}

/**
* Get the distance between a 2D point and a line.
***********************************************************************************/

static float PointLineDistance(const FVector2D& point, const FVector2D& origin, FVector2D direction)
{
	// Calculate the scalar for the nearest point on the line to the point that we
	// are comparing.

	FVector2D difference = point - origin;
	float lengthSqr = direction.SizeSquared();

	if (lengthSqr > KINDA_SMALL_NUMBER)
	{
		float pointOnLine = FVector2D::DotProduct(direction, difference) / lengthSqr;

		if (pointOnLine > 0.0f)
		{
			if (pointOnLine < 1.0f)
			{
				// Get the nearest point on the line to the point that we are comparing and
				// return the distance between them.

				direction *= pointOnLine;
			}

			difference -= direction;
		}
	}

	return difference.Size();
}

/**
* Do two 2D line segments intersect one another, and if so, where?
***********************************************************************************/

static bool LineSegmentIntersection(const FVector2D& p0, const FVector2D& p1, const FVector2D& p2, const FVector2D& p3, FVector2D& intersection, bool considerCollinearOverlapAsIntersect = false)
{
	FVector2D r = p1 - p0;
	FVector2D s = p3 - p2;
	float rxs = FVector2D::CrossProduct(r, s);
	float qpxr = FVector2D::CrossProduct(p2 - p0, r);

	// If r x s = 0 and (p2 - p0) x r = 0, then the two lines are collinear.

	if (rxs == 0 && qpxr == 0)
	{
		// 1. If either  0 <= (p2 - p0) * r <= r * r or 0 <= (p0 - p2) * s <= * s
		// then the two lines are overlapping,

		if (considerCollinearOverlapAsIntersect == true)
		{
			if ((0 <= FVector2D::DotProduct(p2 - p0, r) && FVector2D::DotProduct(p2 - p0, r) <= FVector2D::DotProduct(r, r)) ||
				(0 <= FVector2D::DotProduct(p0 - p2, s) && FVector2D::DotProduct(p0 - p2, s) <= FVector2D::DotProduct(s, s)))
			{
				return true;
			}
		}

		// 2. If neither 0 <= (p2 - p0) * r = r * r nor 0 <= (p0 - p2) * s <= s * s
		// then the two lines are collinear but disjoint.
		// No need to implement this expression, as it follows from the expression above.

		return false;
	}

	// 3. If r x s = 0 and (p2 - p0) x r != 0, then the two lines are parallel and non-intersecting.

	if (rxs == 0 && qpxr != 0)
	{
		return false;
	}

	// t = (p2 - p0) x s / (r x s)

	float t = FVector2D::CrossProduct(p2 - p0, s) / rxs;

	// u = (p2 - p0) x r / (r x s)

	float u = FVector2D::CrossProduct(p2 - p0, r) / rxs;

	// 4. If r x s != 0 and 0 <= t <= 1 and 0 <= u <= 1
	// the two line segments meet at the point p0 + t r = p2 + u s.

	if (rxs != 0 && (0 <= t && t <= 1) && (0 <= u && u <= 1))
	{
		// We can calculate the intersection point using either t or u.

		intersection = p0 + (t * r);

		// An intersection was found.

		return true;
	}

	// 5. Otherwise, the two line segments are not parallel but do not intersect.

	return false;
}

/**
* How much open space is the around a world location for a given spline offset
* and clearance angle?
*
* In order for this to be useful, location should lie somewhere within the arc
* around splineOffset and range clearanceAngle.
*
* splineOffset should always be in spline space.
***********************************************************************************/

float UPursuitSplineComponent::GetClearance(float distance, FVector location, FVector splineOffset, float clearanceAngle, bool splineSpace, float padding) const
{
	ensure(clearanceAngle <= 180.0f);

	clearanceAngle = FMath::Min(clearanceAngle, 180.0f);

	TArray<FPursuitPointExtendedData>& pursuitPointExtendedData = PursuitSplineParent->PointExtendedData;

	if (pursuitPointExtendedData.Num() < 2)
	{
		return 0.0f;
	}

	if (splineSpace == false)
	{
		location = WorldSpaceToSplineSpace(location, distance, true);
	}

	FVector2D localOffset = FVector2D(location.Y, location.Z);

	// The angle in radians of the offset we've been given compared to the spline's center.

	float radians = FMath::Atan2(splineOffset.Y, splineOffset.Z);

	if (radians < 0.0f)
	{
		radians = PI * 2.0f + radians;
	}

	// Convert the angle in radians to an index number in our lookup table.

	float center = (radians / (PI * 2.0f)) * FPursuitPointExtendedData::NumDistances;
	int32 centerInt = FMath::RoundToInt(center);

	// Convert the clearance angle in degrees to an index number in our lookup table.

	int32 numIndices = 1;

	if (clearanceAngle > KINDA_SMALL_NUMBER)
	{
		numIndices = FMath::CeilToInt((clearanceAngle / 360.0f) * FPursuitPointExtendedData::NumDistances) & ~1;
		numIndices = FMath::Max(numIndices, 2);
		numIndices |= 1;
	}

	int32 thisKey = 0;
	int32 nextKey = 0;
	float ratio = 0.0f;

	GetExtendedPointKeys(distance, thisKey, nextKey, ratio);

	FPursuitPointExtendedData& p0 = pursuitPointExtendedData[thisKey];
	FPursuitPointExtendedData& p1 = pursuitPointExtendedData[nextKey];

	float distances[FPursuitPointExtendedData::NumDistances];

	static bool sinCosComputed = false;
	static float angles[FPursuitPointExtendedData::NumDistances];
	static FVector2D sinCos[FPursuitPointExtendedData::NumDistances];

	for (int32 i = 0; i < FPursuitPointExtendedData::NumDistances; i++)
	{
		float d0 = p0.EnvironmentDistances[i];
		float d1 = p1.EnvironmentDistances[i];
		float d2 = UnlimitedSplineDistance;

		if (d0 >= 0.0f &&
			d1 >= 0.0f)
		{
			d2 = FMath::Lerp(d0, d1, ratio);
		}
		else if (d0 >= 0.0f)
		{
			d2 = d0;
		}
		else if (d1 >= 0.0f)
		{
			d2 = d1;
		}

		distances[i] = d2 + padding;

		if (sinCosComputed == false)
		{
			angles[i] = ((float)i / (float)FPursuitPointExtendedData::NumDistances) * PI * 2.0f;

			FMath::SinCos(&sinCos[i].X, &sinCos[i].Y, angles[i]);
		}
	}

	sinCosComputed = true;

	// Do a line segment intersection test with a line from the location to somewhere known for sure
	// to be outside of the spline area, against all the lines that form the edges of the spline area.

	int32 numIntersections = 0;
	int32 distancesMask = FPursuitPointExtendedData::NumDistances - 1;
	FVector2D outside = FVector2D(UnlimitedSplineDistance * 1.1f, 0.0f);
	FVector2D lastIntersection = FVector2D::ZeroVector;
	FVector2D intersection = lastIntersection;

	for (int32 i = 0; i < FPursuitPointExtendedData::NumDistances; i++)
	{
		int32 i1 = (i + 1) & distancesMask;
		FVector2D o0 = sinCos[i] * distances[i];
		FVector2D o1 = sinCos[i1] * distances[i1];

		if (LineSegmentIntersection(localOffset, outside, o0, o1, intersection) == true)
		{
			if (lastIntersection.Equals(intersection, 1.0f) == false)
			{
				numIntersections++;
			}

			lastIntersection = intersection;
		}
	}

	// The number of line intersections from the tests we've just done indicate whether the location
	// is inside or outside of the spline area. If we have an odd number of intersections then we're
	// inside of the area, and outside for an even number.

	if ((numIntersections & 1) == 1)
	{
		// The location is inside.

		// Default to a kilometer clearance.

		float minDistance = UnlimitedSplineDistance;

		for (int32 i = 0; i < numIndices - 1; i++)
		{
			int32 i0 = ((centerInt - (numIndices >> 1)) + i);

			i0 = (i0 < 0) ? FPursuitPointExtendedData::NumDistances + i0 : i0 & distancesMask;

			int32 i1 = (i0 + 1) & distancesMask;

			FVector2D o0 = sinCos[i0] * distances[i0];
			FVector2D o1 = sinCos[i1] * distances[i1];

			minDistance = FMath::Min(minDistance, PointLineDistance(localOffset, o0, o1 - o0));
		}

		return minDistance;
	}
	else
	{
		// The location is outside.

		return 0.0f;
	}
}

/**
* Is a distance along a spline in open space?
***********************************************************************************/

TArray<float> UPursuitSplineComponent::GetClearances(float distance) const
{
	TArray<float> result;

	if (PursuitSplineParent->PointExtendedData.Num() < 2)
	{
		return result;
	}

	int32 thisKey = 0;
	int32 nextKey = 0;
	float ratio = 0.0f;

	GetExtendedPointKeys(distance, thisKey, nextKey, ratio);

	FPursuitPointExtendedData& p0 = PursuitSplineParent->PointExtendedData[thisKey];
	FPursuitPointExtendedData& p1 = PursuitSplineParent->PointExtendedData[nextKey];

	for (int32 i = 0; i < FPursuitPointExtendedData::NumDistances; i++)
	{
		int32 index = i;
		float d0 = p0.EnvironmentDistances[index];
		float d1 = p1.EnvironmentDistances[index];
		float d2 = -1.0f;

		if (d0 >= 0.0f && d1 >= 0.0f)
		{
			d2 = FMath::Lerp(d0, d1, ratio);
		}
		else if (d0 >= 0.0f)
		{
			d2 = d0;
		}
		else if (d1 >= 0.0f)
		{
			d2 = d1;
		}

		result.Emplace(d2);
	}

	return result;
}

/**
* Get the minimum optimum speed of the route in kph over distance.
***********************************************************************************/

float FRouteFollower::GetMinimumOptimumSpeedOverDistance(float distance, float& overDistance, int32 direction) const
{
	float m0 = 1000.0f;
	float m1 = 1000.0f;

	if (GRIP_POINTER_VALID(ThisSpline) == true)
	{
		m0 = m1 = ThisSpline->GetMinimumOptimumSpeedOverDistance(distance, overDistance, direction);
	}

	if (GRIP_POINTER_VALID(NextSpline) == true &&
		NextSpline != ThisSpline)
	{
		m1 = NextSpline->GetMinimumOptimumSpeedOverDistance(NextSwitchDistance, overDistance, direction);
	}

	return FMath::Min(m0, m1);
}

/**
* Get the minimum optimum speed of the spline in kph over distance.
***********************************************************************************/

float UPursuitSplineComponent::GetMinimumOptimumSpeedOverDistance(float distance, float& overDistance, int32 direction) const
{
	float minimumSpeed = 1000.0f;
	float length = GetSplineLength();
	float endDistance = distance + (overDistance * direction);

	if (IsClosedLoop() == false)
	{
		endDistance = ClampDistanceAgainstLength(endDistance, length);
		overDistance -= FMath::Abs(endDistance - distance);
	}
	else
	{
		overDistance = 0.0f;
	}

	float iterationDistance = FMathEx::MetersToCentimeters(ExtendedPointMeters);
	int32 numIterations = FMath::CeilToInt(FMath::Abs(endDistance - distance) / iterationDistance);

	for (int32 i = 0; i <= numIterations; i++)
	{
		float optimumSpeed = GetOptimumSpeedAtDistanceAlongSpline(distance);

		if (optimumSpeed > 0.0f)
		{
			minimumSpeed = FMath::Min(minimumSpeed, optimumSpeed);
		}

		distance = ClampDistanceAgainstLength(distance + (iterationDistance * direction), length);
	}

	return minimumSpeed;
}

/**
* Get the minimum speed of the route in kph over distance.
***********************************************************************************/

float FRouteFollower::GetMinimumSpeedOverDistance(float distance, float& overDistance, int32 direction) const
{
	float m0 = 0.0f;
	float m1 = 0.0f;

	if (GRIP_POINTER_VALID(ThisSpline) == true)
	{
		m0 = m1 = ThisSpline->GetMinimumSpeedOverDistance(distance, overDistance, direction);
	}

	if (GRIP_POINTER_VALID(NextSpline) == true &&
		NextSpline != ThisSpline)
	{
		m1 = NextSpline->GetMinimumSpeedOverDistance(NextSwitchDistance, overDistance, direction);
	}

	return FMath::Max(m0, m1);
}

/**
* Get the optimum speed in kph at a distance along a spline.
***********************************************************************************/

float UPursuitSplineComponent::GetOptimumSpeedAtDistanceAlongSpline(float distance) const
{
	float key = SplineCurves.ReparamTable.Eval(distance, 0.0f);
	int32 thisKey = ThisKey(key);
	int32 nextKey = NextKey(key);

	float v0 = FMath::Min(PursuitSplineParent->PointData[thisKey].OptimumSpeed, 1000.0f);
	float v1 = FMath::Min(PursuitSplineParent->PointData[nextKey].OptimumSpeed, 1000.0f);

	if (v0 == 0.0f &&
		v1 == 0.0f)
	{
		return 0.0f;
	}

	if (v0 == 0.0f)
	{
		v0 = 1000.0f;
	}

	if (v1 == 0.0f)
	{
		v1 = 1000.0f;
	}

	return FMath::Lerp(v0, v1, key - thisKey);
}

/**
* Get the minimum speed of the spline in kph over distance.
***********************************************************************************/

float UPursuitSplineComponent::GetMinimumSpeedOverDistance(float distance, float& overDistance, int32 direction) const
{
	float minimumSpeed = 0.0f;
	float length = GetSplineLength();
	float endDistance = distance + (overDistance * direction);

	if (IsClosedLoop() == false)
	{
		endDistance = ClampDistanceAgainstLength(endDistance, length);
		overDistance -= FMath::Abs(endDistance - distance);
		overDistance = FMath::Max(0.0f, overDistance);
	}
	else
	{
		overDistance = 0.0f;
	}

	float iterationDistance = FMathEx::MetersToCentimeters(ExtendedPointMeters);
	int32 numIterations = FMath::CeilToInt(FMath::Abs(endDistance - distance) / iterationDistance);

	for (int32 i = 0; i <= numIterations; i++)
	{
		float optimumSpeed = GetMinimumSpeedAtDistanceAlongSpline(distance);

		if (optimumSpeed > KINDA_SMALL_NUMBER)
		{
			if (minimumSpeed == 0.0f)
			{
				minimumSpeed = optimumSpeed;
			}
			else
			{
				minimumSpeed = FMath::Max(minimumSpeed, optimumSpeed);
			}
		}

		distance = ClampDistanceAgainstLength(distance + (iterationDistance * direction), length);
	}

	return minimumSpeed;
}

/**
* Get the minimum speed in kph at a distance along a spline.
***********************************************************************************/

float UPursuitSplineComponent::GetMinimumSpeedAtDistanceAlongSpline(float distance) const
{
	float key = SplineCurves.ReparamTable.Eval(distance, 0.0f);
	int32 thisKey = ThisKey(key);
	int32 nextKey = NextKey(key);

	float v0 = PursuitSplineParent->PointData[thisKey].MinimumSpeed;
	float v1 = PursuitSplineParent->PointData[nextKey].MinimumSpeed;

	return FMath::Lerp(v0, v1, key - thisKey);
}

/**
* Get the world closest position for a distance along the spline.
***********************************************************************************/

FVector UPursuitSplineComponent::GetWorldClosestPosition(float distance, bool raw) const
{
	return GetWorldLocationAtDistanceAlongSpline(distance) + GetWorldClosestOffset(distance, raw);
}

/**
* Get the world closest offset for a distance along the spline.
***********************************************************************************/

FVector UPursuitSplineComponent::GetWorldClosestOffset(float distance, bool raw) const
{
	if (PursuitSplineParent->PointExtendedData.Num() < 2)
	{
		return FVector::ZeroVector;
	}

	int32 thisKey = 0;
	int32 nextKey = 0;
	float ratio = 0.0f;

	GetExtendedPointKeys(distance, thisKey, nextKey, ratio);

	FPursuitPointExtendedData& p0 = PursuitSplineParent->PointExtendedData[thisKey];
	FPursuitPointExtendedData& p1 = PursuitSplineParent->PointExtendedData[nextKey];

	FVector d0 = (raw == true) ? p0.RawGroundOffset : p0.UseGroundOffset;
	FVector d1 = (raw == true) ? p1.RawGroundOffset : p1.UseGroundOffset;

	FVector d2 = FVector::ZeroVector;

	if (d0.IsZero() == false && d1.IsZero() == false)
	{
		// We need some intelligence here to nicely interpolate spherically between angles when
		// they're reasonably close together, and not when they're far apart. The reason being
		// we don't want to circle around when there has been no surface in between the
		// surfaces of the two adjacent data points.

		d2 = FMath::Lerp(d0, d1, ratio);

		if (FVector::DotProduct(d0, d1) >= 0.0f)
		{
			d2.Normalize();
			d2 *= FMath::Lerp(d0.Size(), d1.Size(), ratio);
		}
	}
	else if (d0.IsZero() == false)
	{
		d2 = d0;
	}
	else if (d1.IsZero() == false)
	{
		d2 = d1;
	}

	return d2;
}

/**
* Find the nearest distance along a spline to a given master distance.
*
* The fewer iterations and samples you use the faster it will be, but also the less
* accurate it will be. Conversely, the smaller the difference between startDistance
* and endDistance the more accurate the result will be.
***********************************************************************************/

float UPursuitSplineComponent::GetNearestDistanceToMasterDistance(float masterDistance, float startDistance, float endDistance, int32 numIterations, int32 numSamples, float earlyExitDistance) const
{
	float splineLength = GetSplineLength();

	if (endDistance <= 0.0f)
	{
		endDistance = splineLength;
	}

	if (numIterations <= 0)
	{
		numIterations = 5;
	}

	float resultDistance = startDistance;
	APlayGameMode* gameMode = APlayGameMode::Get(this);
	float masterSplineLength = gameMode->MasterRacingSplineLength;
	UPursuitSplineComponent* masterSpline = gameMode->MasterRacingSpline.Get();

	if (masterSpline != nullptr)
	{
		float minDistance = startDistance;
		float maxDistance = endDistance;
		float minSeparation = -1.0f;
		float invNumSamples = 1.0f / (float)numSamples;

		for (int32 iteration = 0; iteration < numIterations; iteration++)
		{
			float distanceAlong = minDistance;
			float deltaStep = (maxDistance - minDistance) * invNumSamples;
			float lastResultDistance = resultDistance;

			// This will sample between minDistance and maxDistance inclusively.

			for (int32 sample = 0; sample <= numSamples; sample++)
			{
				// Determine the master distance on the spline for distanceAlong.

				float clampedDistanceAlong = ClampDistanceAgainstLength(distanceAlong, splineLength);
				float testDistance = GetMasterDistanceAtDistanceAlongSpline(clampedDistanceAlong, masterSplineLength);
				float separation = masterSpline->GetDistanceDifference(masterDistance, testDistance);

				if (minSeparation == -1.0f ||
					minSeparation > separation)
				{
					// If the minimum separation was less than the last then record it.

					minSeparation = separation;
					resultDistance = clampedDistanceAlong;
				}

				distanceAlong += deltaStep;
			}

			if (iteration > 0 &&
				deltaStep < earlyExitDistance * 2.0f &&
				GetDistanceDifference(resultDistance, lastResultDistance) < earlyExitDistance)
			{
				// Early break if the last refinement only took us less than a set distance away from the last.

				break;
			}

			minDistance = resultDistance - deltaStep;
			maxDistance = resultDistance + deltaStep;
		}
	}

	return resultDistance;
}

/**
* Get the quaternion in world space at a distance along a spline.
***********************************************************************************/

FQuat UPursuitSplineComponent::GetWorldSpaceQuaternionAtDistanceAlongSpline(float distance) const
{
	if (PursuitSplineParent->PointExtendedData.Num() < 2)
	{
		return FQuat::Identity;
	}

	int32 key0 = 0;
	int32 key1 = 0;
	float ratio = 0.0f;

	GetExtendedPointKeys(distance, key0, key1, ratio);

	TArray<FPursuitPointExtendedData>& pursuitPointExtendedData = PursuitSplineParent->PointExtendedData;

	return FQuat::Slerp(pursuitPointExtendedData[key0].Quaternion, pursuitPointExtendedData[key1].Quaternion, ratio);
}

/**
* Get the up vector in world space at a distance along a spline.
***********************************************************************************/

FVector UPursuitSplineComponent::GetWorldSpaceUpVectorAtDistanceAlongSpline(float distance) const
{
	if (PursuitSplineParent->PointExtendedData.Num() < 2)
	{
		return FVector::UpVector;
	}

	int32 key0 = 0;
	int32 key1 = 0;
	float ratio = 0.0f;

	GetExtendedPointKeys(distance, key0, key1, ratio);

	TArray<FPursuitPointExtendedData>& pursuitPointExtendedData = PursuitSplineParent->PointExtendedData;

	FQuat quaternion = FQuat::Slerp(pursuitPointExtendedData[key0].Quaternion, pursuitPointExtendedData[key1].Quaternion, ratio);

	return quaternion.GetAxisZ();
}

#pragma endregion AINavigation

#pragma region AIVehicleControl

/**
* Get the curvature of the route in degrees over distance (in withRespectTo space).
***********************************************************************************/

FRotator FRouteFollower::GetCurvatureOverDistance(float distance, float& overDistance, int32 direction, const FQuat& withRespectTo, bool absolute) const
{
	FRotator d0 = FRotator::ZeroRotator;
	FRotator d1 = FRotator::ZeroRotator;

	if (GRIP_POINTER_VALID(ThisSpline) == true)
	{
		d0 = ThisSpline->GetCurvatureOverDistance(distance, overDistance, direction, withRespectTo, absolute);
	}

	if (GRIP_POINTER_VALID(NextSpline) == true &&
		NextSpline != ThisSpline)
	{
		d1 = NextSpline->GetCurvatureOverDistance(NextSwitchDistance, overDistance, direction, withRespectTo, absolute);
	}

	return d0 + d1;
}

/**
* Get the curvature of the spline in degrees over distance (in withRespectTo space).
***********************************************************************************/

FRotator UPursuitSplineComponent::GetCurvatureOverDistance(float distance, float& overDistance, int32 direction, const FQuat& withRespectTo, bool absolute) const
{
	TArray<FPursuitPointExtendedData>& pursuitPointExtendedData = PursuitSplineParent->PointExtendedData;

	if (pursuitPointExtendedData.Num() < 2)
	{
		return FRotator::ZeroRotator;
	}

	FRotator degrees = FRotator::ZeroRotator;
	float endDistance = distance + (overDistance * direction);

	if (IsClosedLoop() == false)
	{
		endDistance = ClampDistance(endDistance);
		overDistance -= FMath::Abs(endDistance - distance);
	}
	else
	{
		overDistance = 0.0f;
	}

	int32 key0 = 0;
	int32 key1 = 0;
	float ratio = 0.0f;
	bool transform = withRespectTo.IsIdentity() == false;
	float iterationDistance = FMathEx::MetersToCentimeters(ExtendedPointMeters);
	FQuat invWithRespectTo = withRespectTo.Inverse();
	int32 numIterations = FMath::CeilToInt(FMath::Abs(endDistance - distance) / iterationDistance);
	int32 numPoints = pursuitPointExtendedData.Num();

	GetExtendedPointKeys(distance, key0, key1, ratio);

	FRotator lastRotation = (invWithRespectTo * pursuitPointExtendedData[key0].Quaternion).Rotator();

	for (int32 i = 0; i < numIterations; i++)
	{
		// Calculate the array index number for this iteration.

		if (++key0 >= numPoints)
		{
			key0 = (IsClosedLoop() == true) ? key0 - numPoints : numPoints - 1;
		}

		// Get the rotation at this sample point, with respect to another rotation if given.

		FQuat quaternion = pursuitPointExtendedData[key0].Quaternion;
		FRotator rotation = (transform == true) ? (invWithRespectTo * quaternion).Rotator() : quaternion.Rotator();

		// Now calculate and sum the angular differences between this sample and the last.

		if (absolute == true)
		{
			degrees += FMathEx::GetUnsignedDegreesDifference(lastRotation, rotation);
		}
		else
		{
			degrees += FMathEx::GetSignedDegreesDifference(lastRotation, rotation);
		}

		lastRotation = rotation;
	}

	return degrees;
}

#pragma endregion AIVehicleControl

#pragma region VehicleTeleport

/**
* Rewind the follower to safe ground - normally used when teleporting a vehicle.
***********************************************************************************/

bool FRouteFollower::RewindToSafeGround(float rewindDistance, float& initialSpeed, bool reset)
{
	if (reset == true)
	{
		NumRewindBranches = 0;
	}
	else
	{
		NumRewindBranches++;
	}

	if (GRIP_POINTER_VALID(ThisSpline) == true)
	{
		float distance = ThisDistance;

		DecidedDistance = -1.0f;

		UE_LOG(GripTeleportationLog, Log, TEXT("Rewind distance is %d"), (int32)rewindDistance);
		UE_LOG(GripTeleportationLog, Log, TEXT("Rewind from spline %s at distance %d"), *ThisSpline->ActorName, (int32)ThisDistance);

		ThisDistance = ThisDistance - (rewindDistance * 100.0f);

		if (ThisSpline->IsClosedLoop() == true)
		{
			ThisDistance = ThisSpline->ClampDistance(ThisDistance);
		}

		if (ThisDistance < 0.0f ||
			ThisSpline->RewindToSafeGround(ThisDistance, initialSpeed) == false)
		{
			if (ThisDistance < 0.0f)
			{
				UE_LOG(GripTeleportationLog, Log, TEXT("Rewind failed because the rewind distance is before the beginning of the spline"));
			}
			else
			{
				UE_LOG(GripTeleportationLog, Log, TEXT("Rewind failed"));
			}

			if (NumRewindBranches < 5)
			{
				// Runaway iteration check.

				ThisDistance = distance;

				if (ThisSpline->SplineLinks.Num() > 0)
				{
					// Find the first backward link that is in front of our distance, then iterate back to the one prior.
					// They will have been sorted at creation on ThisDistance.

					int32 i = 0;

					for (; i < ThisSpline->SplineLinks.Num(); i++)
					{
						FSplineLink& link = ThisSpline->SplineLinks[i];

						// Look for splines that flow onto this one, because that means we can run back down along them.

						if (link.ForwardLink == false &&
							link.ThisDistance > ThisDistance)
						{
							// Go back one branch to land behind ThisDistance.

							if (--i < 0)
							{
								i += ThisSpline->SplineLinks.Num();
							}

							break;
						}
					}

					// If no splines were found to be in front, then all must be behind so choose the last one.

					i = FMath::Min(i, ThisSpline->SplineLinks.Num() - 1);

					int32 first = i;

					do
					{
						FSplineLink& link = ThisSpline->SplineLinks[i];

						// Look for splines that flow onto this one, because that means we can run back down along them.

						if (link.ForwardLink == false)
						{
							TWeakObjectPtr<UPursuitSplineComponent> thisSpline = ThisSpline;
							float thisDistance = ThisDistance;
							float thisRewindDistance = FMath::Max(0.0f, rewindDistance - FMath::Max(0.0f, thisDistance - link.ThisDistance));

							// Use the new splines at its join distance and start to rewind down that.

							ThisSpline = link.Spline;
							ThisDistance = link.NextDistance;

							if (RewindToSafeGround(thisRewindDistance, initialSpeed, false) == true)
							{
								SwitchingSpline = false;

								LastSpline = thisSpline;
								LastDistance = thisDistance;

								NumRewindBranches--;

								return true;
							}

							ThisSpline = thisSpline;
							ThisDistance = thisDistance;
						}

						if (--i < 0)
						{
							i += ThisSpline->SplineLinks.Num();
						}
					}
					while (i != first);
				}
			}
		}
		else
		{
			NextSpline = ThisSpline;
			NextDistance = ThisDistance;

			NumRewindBranches--;

			return true;
		}
	}

	NumRewindBranches--;

	return false;
}

/**
* Rewind a distance to safe ground if possible.
***********************************************************************************/

bool UPursuitSplineComponent::RewindToSafeGround(float& distance, float& initialSpeed)
{
	initialSpeed = 100.0f;

	if (PursuitSplineParent->PointExtendedData.Num() < 2)
	{
		return true;
	}

	int32 thisKey = 0;
	int32 nextKey = 0;
	float ratio = 0.0f;
	TArray<FPursuitPointExtendedData>& pursuitPointExtendedData = PursuitSplineParent->PointExtendedData;

	UE_LOG(GripTeleportationLog, Log, TEXT("Looking for level ground from %d on spline %s"), (int32)distance, *ActorName);

	GetExtendedPointKeys(distance, thisKey, nextKey, ratio);

	thisKey = nextKey;

	// All we care about here is pitch curvature, making sure we don't try to make a very hard vertical turn.

	do
	{
		FPursuitPointExtendedData& p0 = pursuitPointExtendedData[thisKey];
		float minCurvatureLength = 250.0f;
		float curvatureLength = minCurvatureLength * 100.0f;
		FRotator curvature = GetCurvatureOverDistance(p0.Distance, curvatureLength, 1, FQuat::Identity, false);

		if (curvature.Pitch < 25.0f)
		{
			// OK, so we have some manageable vertical curvature.

			float continuousLength = minCurvatureLength * 100.0f;

			if (GetContinuousSurfaceOverDistance(p0.Distance, continuousLength, 1) == true)
			{
				// And it doesn't swap driving surfaces.

				UE_LOG(GripTeleportationLog, Log, TEXT("Found good ground at %d"), (int32)p0.Distance);

				distance = p0.Distance;

				// Add in an adjustment to the speed to take into account upward curvature.

				if (curvature.Pitch > 0.0f)
				{
					float boost = FMath::Min(50.0f, curvature.Pitch) * 8.0f;

					UE_LOG(GripTeleportationLog, Log, TEXT("Added %d kph for upward curvature"), (int32)boost);

					initialSpeed += boost;
				}

				FRotator rotation = GetQuaternionAtDistanceAlongSpline(distance, ESplineCoordinateSpace::World).Rotator();

				if (rotation.Pitch > 0.0f)
				{
					// Scale up to 400kph when reaching up to 15 degrees incline or more.

					float boost = (FMath::Min(rotation.Pitch, 15.0f) / 15.0f) * 400.0f;

					UE_LOG(GripTeleportationLog, Log, TEXT("Setting minimum of %d kph for upward incline"), (int32)boost);

					initialSpeed = FMath::Max(initialSpeed, boost);
				}

				float overDistance = FMathEx::KilometersPerHourToCentimetersPerSecond(initialSpeed) * 2.0f;
				float minimumSpeed = FMath::Min(500.0f, GetMinimumSpeedOverDistance(distance, overDistance, 1));

				overDistance = FMathEx::KilometersPerHourToCentimetersPerSecond(initialSpeed) * 2.0f;
				float optimumSpeed = GetMinimumOptimumSpeedOverDistance(distance, overDistance, 1);

				if (minimumSpeed > KINDA_SMALL_NUMBER)
				{
					initialSpeed = FMath::Max(initialSpeed, minimumSpeed);
				}

				if (optimumSpeed > KINDA_SMALL_NUMBER)
				{
					initialSpeed = FMath::Min(initialSpeed, optimumSpeed);
				}

				FVector difference = GetWorldClosestPosition(distance) - GetWorldLocationAtDistanceAlongSpline(distance);

				difference.Normalize();

				// difference is now the direction of the ground in world space.
				// Scale speed with ground orientation.

				initialSpeed = FMath::Max(initialSpeed, FMath::Lerp(100.0f, 350.0f, FMathEx::NegativePow((difference.Z * 0.5f) + 0.5f, 0.5f)));

				return true;
			}
		}

		if (--thisKey < 0)
		{
			if (IsClosedLoop() == false)
			{
				break;
			}

			thisKey += pursuitPointExtendedData.Num();
		}
	}
	while (thisKey != nextKey);

	UE_LOG(GripTeleportationLog, Log, TEXT("Gave up looking for level ground"));

	return false;
}

/**
* Get the continuous surface of the spline over distance.
***********************************************************************************/

bool UPursuitSplineComponent::GetContinuousSurfaceOverDistance(float distance, float& overDistance, int32 direction) const
{
	bool continuous = true;

	TArray<FPursuitPointExtendedData>& pursuitPointExtendedData = PursuitSplineParent->PointExtendedData;

	if (pursuitPointExtendedData.Num() < 2)
	{
		return continuous;
	}

	float endDistance = distance + (overDistance * direction);

	if (IsClosedLoop() == false)
	{
		endDistance = ClampDistance(endDistance);
		overDistance -= FMath::Abs(endDistance - distance);
	}
	else
	{
		overDistance = 0.0f;
	}

	int32 thisKey = 0;
	int32 nextKey = 0;
	float ratio = 0.0f;

	GetExtendedPointKeys(distance, thisKey, nextKey, ratio);

	int32 key0 = (direction < 0) ? nextKey : thisKey;

	GetExtendedPointKeys(endDistance, thisKey, nextKey, ratio);

	int32 key1 = (direction < 0) ? thisKey : nextKey;
	int32 numKeys = pursuitPointExtendedData.Num();

	for (int32 i = key0; i != key1;)
	{
		FPursuitPointExtendedData& p0 = pursuitPointExtendedData[i];
		FPursuitPointExtendedData& p1 = pursuitPointExtendedData[FMath::Clamp(i + direction, 0, numKeys - 1)];

		float degrees = FPursuitPointExtendedData::DifferenceInDegrees(p0.UseGroundIndex, p1.UseGroundIndex);

		if (degrees > 45.0f ||
			p0.EnvironmentDistances[p0.UseGroundIndex] < 0.0f ||
			p0.EnvironmentDistances[p0.UseGroundIndex] > 25.0f * 100.0f)
		{
			// If the change in degrees is too rapid or the nearest surface is more than 25 meters away,
			// then this isn't a continuous surface.

			continuous = false;
			break;
		}

		if (direction < 0)
		{
			if (--i < 0)
			{
				i = numKeys - 1;
			}
		}
		else
		{
			if (++i == numKeys)
			{
				i = 0;
			}
		}
	}

	return continuous;
}

#pragma endregion VehicleTeleport

#pragma region PickupMissile

/**
* Is a distance along a route in open space?
*
* splineOffset should always be in spline space.
***********************************************************************************/

float FRouteFollower::GetClearanceOverDistance(float distance, float& overDistance, int32 direction, FVector worldLocation, FVector splineOffset, float clearanceAngle) const
{
	float c0 = 0.0f;
	float c1 = 0.0f;

	if (GRIP_POINTER_VALID(ThisSpline) == true)
	{
		c0 = c1 = ThisSpline->GetClearanceOverDistance(distance, overDistance, direction, worldLocation, splineOffset, clearanceAngle);
	}

	if (GRIP_POINTER_VALID(NextSpline) == true &&
		NextSpline != ThisSpline)
	{
		c1 = NextSpline->GetClearanceOverDistance(NextSwitchDistance, overDistance, direction, worldLocation, splineOffset, clearanceAngle);
	}

	return FMath::Min(c0, c1);
}

/**
* Get all the clearances at a distance along the spline.
*
* splineOffset should always be in spline space.
***********************************************************************************/

float UPursuitSplineComponent::GetClearanceOverDistance(float distance, float& overDistance, int32 direction, FVector worldLocation, FVector splineOffset, float clearanceAngle) const
{
	TArray<FPursuitPointExtendedData>& pursuitPointExtendedData = PursuitSplineParent->PointExtendedData;

	if (pursuitPointExtendedData.Num() < 2)
	{
		return 0.0f;
	}

	float minClearance = -1.0f;
	float length = GetSplineLength();
	float endDistance = distance + (overDistance * direction);

	if (IsClosedLoop() == false)
	{
		endDistance = ClampDistanceAgainstLength(endDistance, length);
		overDistance -= FMath::Abs(endDistance - distance);
	}
	else
	{
		overDistance = 0.0f;
	}

	float iterationDistance = FMathEx::MetersToCentimeters(ExtendedPointMeters);
	int32 numIterations = FMath::CeilToInt(FMath::Abs(endDistance - distance) / iterationDistance);
	FVector offset = WorldSpaceToSplineSpace(worldLocation, distance, true);

	for (int32 i = 0; i <= numIterations; i++)
	{
		float clearance = GetClearance(distance, offset, splineOffset, clearanceAngle, true, 0.0f);

		if (minClearance < 0.0f ||
			minClearance > clearance)
		{
			minClearance = clearance;
		}

		distance = ClampDistanceAgainstLength(distance + (iterationDistance * direction), length);
	}

	return minClearance;
}

#pragma endregion PickupMissile

#pragma region CameraCinematics

/**
* Get the surface sections of the spline.
***********************************************************************************/

TArray<FSplineSection> UPursuitSplineComponent::GetSurfaceSections() const
{
	// NOTE: This assumes that a spline will start unbroken, and makes no attempt
	// to determine brokenness over the loop point of a looped spline.

	bool broken = false;
	bool nowBroken = false;
	TArray<FPursuitPointExtendedData>& pursuitPointExtendedData = PursuitSplineParent->PointExtendedData;
	int32 numKeys = pursuitPointExtendedData.Num();

	TArray<FSplineSection> sections;

	int32 i = 0;
	int32 firstKey = 0;
	FVector groundOffset = FVector::ZeroVector;

	for (i = 0; i < numKeys; i++)
	{
		FPursuitPointExtendedData& p0 = pursuitPointExtendedData[i];

		// If ground is 25m or more away, or there's a 5m or more difference
		// in the course of one 10m length, then consider the surface broken.

		nowBroken = false;

		if (p0.EnvironmentDistances[p0.UseGroundIndex] < 0.0f ||
			p0.EnvironmentDistances[p0.UseGroundIndex] > 25.0f * 100.0f)
		{
			nowBroken = true;
		}
		else if (i != 0)
		{
			if (FVector::DotProduct(groundOffset, p0.UseGroundOffset) < 0.0f ||
				(groundOffset - p0.UseGroundOffset).Size() > 5.0f * 100.0f)
			{
				nowBroken = true;
			}
		}

		groundOffset = p0.UseGroundOffset;

		if (nowBroken == false)
		{
			if (broken == true)
			{
				firstKey = i;
			}
		}
		else
		{
			if (broken == false)
			{
				if (i - 1 > firstKey)
				{
					sections.Emplace(FSplineSection(pursuitPointExtendedData[firstKey].Distance, pursuitPointExtendedData[i - 1].Distance));
				}
			}
		}

		broken = nowBroken;
	}

	if (nowBroken == false)
	{
		i--;

		if (firstKey < i)
		{
			sections.Emplace(FSplineSection(pursuitPointExtendedData[firstKey].Distance, pursuitPointExtendedData[i].Distance));
		}
	}

	return sections;
}

/**
* Get the surface break property of the spline over distance.
***********************************************************************************/

bool UPursuitSplineComponent::GetSurfaceBreakOverDistance(float distance, float& overDistance, int32 direction) const
{
	bool broken = false;

	TArray<FPursuitPointExtendedData>& pursuitPointExtendedData = PursuitSplineParent->PointExtendedData;

	if (pursuitPointExtendedData.Num() < 2)
	{
		return broken;
	}

	float endDistance = distance + (overDistance * direction);

	if (IsClosedLoop() == false)
	{
		endDistance = ClampDistance(endDistance);
		overDistance -= FMath::Abs(endDistance - distance);
	}
	else
	{
		overDistance = 0.0f;
	}

	float offset = FMathEx::MetersToCentimeters(ExtendedPointMeters);

	distance = ClampDistance(distance - offset * direction);
	endDistance = ClampDistance(endDistance + (offset * 2.0f * direction));

	int32 thisKey = 0;
	int32 nextKey = 0;
	float ratio = 0.0f;

	GetExtendedPointKeys(distance, thisKey, nextKey, ratio);

	int32 key0 = (direction < 0) ? nextKey : thisKey;

	GetExtendedPointKeys(endDistance, thisKey, nextKey, ratio);

	int32 key1 = (direction < 0) ? thisKey : nextKey;
	int32 numKeys = pursuitPointExtendedData.Num();
	FVector groundOffset = FVector::ZeroVector;

	for (int32 i = key0; i != key1;)
	{
		FPursuitPointExtendedData& p0 = pursuitPointExtendedData[i];

		if (p0.EnvironmentDistances[p0.UseGroundIndex] < 0.0f ||
			p0.EnvironmentDistances[p0.UseGroundIndex] > 25.0f * 100.0f)
		{
			broken = true;
			break;
		}

		if (i != key0)
		{
			if (FVector::DotProduct(groundOffset, p0.UseGroundOffset) < 0.0f ||
				(groundOffset - p0.UseGroundOffset).Size() > 5.0f * 100.0f)
			{
				broken = true;
				break;
			}
		}

		groundOffset = p0.UseGroundOffset;

		if (direction < 0)
		{
			if (--i < 0)
			{
				i = numKeys - 1;
			}
		}
		else
		{
			if (++i == numKeys)
			{
				i = 0;
			}
		}
	}

	return broken;
}

/**
* Get the grounded property of the spline over distance.
* Grounded meaning is there ground directly underneath the spline in world space?
***********************************************************************************/

bool UPursuitSplineComponent::GetGroundedOverDistance(float distance, float& overDistance, int32 direction) const
{
	bool grounded = true;

	TArray<FPursuitPointExtendedData>& pursuitPointExtendedData = PursuitSplineParent->PointExtendedData;

	if (pursuitPointExtendedData.Num() < 2)
	{
		return grounded;
	}

	float endDistance = distance + (overDistance * direction);

	if (IsClosedLoop() == false)
	{
		endDistance = ClampDistance(endDistance);
		overDistance -= FMath::Abs(endDistance - distance);
	}
	else
	{
		overDistance = 0.0f;
	}

	int32 thisKey = 0;
	int32 nextKey = 0;
	float ratio = 0.0f;

	GetExtendedPointKeys(distance, thisKey, nextKey, ratio);

	int32 key0 = (direction < 0) ? nextKey : thisKey;

	GetExtendedPointKeys(endDistance, thisKey, nextKey, ratio);

	int32 key1 = (direction < 0) ? thisKey : nextKey;
	int32 numKeys = pursuitPointExtendedData.Num();

	for (int32 i = key0; i != key1;)
	{
		FPursuitPointExtendedData& p0 = pursuitPointExtendedData[i];

		if (p0.EnvironmentDistances[FPursuitPointExtendedData::NumDistances >> 1] < 0.0f ||
			p0.EnvironmentDistances[FPursuitPointExtendedData::NumDistances >> 1] > 100.0f * 100.0f)
		{
			grounded = false;
			break;
		}

		if (direction < 0)
		{
			if (--i < 0)
			{
				i = numKeys - 1;
			}
		}
		else
		{
			if (++i == numKeys)
			{
				i = 0;
			}
		}
	}

	return grounded;
}

/**
* Get the clearances of the spline.
***********************************************************************************/

TArray<float> UPursuitSplineComponent::GetClearancesFromSurface() const
{
	TArray<float> clearances;

	TArray<FPursuitPointExtendedData>& pursuitPointExtendedData = PursuitSplineParent->PointExtendedData;
	int32 numKeys = pursuitPointExtendedData.Num();

	for (int32 i = 0; i < numKeys; i++)
	{
		FPursuitPointExtendedData& p0 = pursuitPointExtendedData[i];
		float clearance = 0.0f;
		int32 center = p0.UseGroundIndex;
		float d0 = p0.EnvironmentDistances[center];

		clearance += (d0 > 0.0f) ? d0 : UnlimitedSplineDistance;

		center = (p0.UseGroundIndex + (FPursuitPointExtendedData::NumDistances >> 1)) % FPursuitPointExtendedData::NumDistances;
		d0 = p0.EnvironmentDistances[center];

		clearance += (d0 > 0.0f) ? d0 : UnlimitedSplineDistance;

		clearances.Emplace(clearance);
	}

	return clearances;
}

/**
* How much open space is the around the spline center line for a given spline offset
* and clearance angle?
***********************************************************************************/

float UPursuitSplineComponent::GetClearance(float distance, FVector splineOffset, float clearanceAngle) const
{
	TArray<FPursuitPointExtendedData>& pursuitPointExtendedData = PursuitSplineParent->PointExtendedData;

	if (pursuitPointExtendedData.Num() < 2)
	{
		return 0.0f;
	}

	int32 thisKey = 0;
	int32 nextKey = 0;
	float ratio = 0.0f;

	GetExtendedPointKeys(distance, thisKey, nextKey, ratio);

	FPursuitPointExtendedData& p0 = pursuitPointExtendedData[thisKey];
	FPursuitPointExtendedData& p1 = pursuitPointExtendedData[nextKey];

	// The angle in radians of the location we've been given compared to the spline's center.

	float radians = FMath::Atan2(splineOffset.Y, splineOffset.Z);

	if (radians < 0.0f)
	{
		radians = PI * 2.0f + radians;
	}

	// Convert the angle in radians to an index number in our lookup table.

	float radiansToNumDistances = (radians / (PI * 2.0f)) * FPursuitPointExtendedData::NumDistances;
	int32 center = FMath::FloorToInt(radiansToNumDistances);
	float distanceRatio = FMath::Frac(radiansToNumDistances);

	// Convert the clearance angle in degrees to an index number in our lookup table.

	int32 numIndices = 1;
	int32 startIndex = center;

	if (clearanceAngle > KINDA_SMALL_NUMBER)
	{
		distanceRatio = 0.0f;
		numIndices = FMath::CeilToInt((clearanceAngle / 360.0f) * FPursuitPointExtendedData::NumDistances) & ~1;
		numIndices = FMath::Max(numIndices, 2);
		startIndex -= (numIndices >> 1);
		numIndices |= 1;
	}

	// Default to a kilometer clearance.

	float minDistance = UnlimitedSplineDistance;

	for (int32 i = 0; i < numIndices; i++)
	{
		float d3[2] = { 0.0f, 0.0f };

		for (int32 j = 0; j < 2; j++)
		{
			int32 index = startIndex + i + j;

			index = (index < 0) ? FPursuitPointExtendedData::NumDistances + index : index % FPursuitPointExtendedData::NumDistances;

			float d0 = p0.EnvironmentDistances[index];
			float d1 = p1.EnvironmentDistances[index];
			float d2 = -1.0f;

			if (d0 >= 0.0f && d1 >= 0.0f)
			{
				d2 = FMath::Lerp(d0, d1, ratio);
			}
			else if (d0 >= 0.0f)
			{
				d2 = d0;
			}
			else if (d1 >= 0.0f)
			{
				d2 = d1;
			}

			d3[j] = d2;
		}

		float d = -1.0f;

		if (d3[0] >= 0.0f && d3[1] >= 0.0f)
		{
			d = d3[0] * (1.0f - distanceRatio) + d3[1] * distanceRatio;
		}
		else if (d3[0] >= 0.0f)
		{
			d = d3[0];
		}
		else if (d3[1] >= 0.0f)
		{
			d = d3[1];
		}

		if (d >= 0.0f)
		{
			if (minDistance > d)
			{
				minDistance = d;
			}
		}
	}

	return minDistance;
}

/**
* Get the weather allowed property of the spline over distance.
***********************************************************************************/

bool UPursuitSplineComponent::GetWeatherAllowedOverDistance(float distance, float& overDistance, int32 direction) const
{
	bool weatherAllowed = true;
	TArray<FPursuitPointExtendedData>& pursuitPointExtendedData = PursuitSplineParent->PointExtendedData;

	if (pursuitPointExtendedData.Num() < 2)
	{
		return weatherAllowed;
	}

	float endDistance = distance + (overDistance * direction);

	if (IsClosedLoop() == false)
	{
		endDistance = ClampDistance(endDistance);
		overDistance -= FMath::Abs(endDistance - distance);
	}
	else
	{
		overDistance = 0.0f;
	}

	int32 thisKey = 0;
	int32 nextKey = 0;
	float ratio = 0.0f;

	GetExtendedPointKeys(distance, thisKey, nextKey, ratio);

	int32 key0 = (direction < 0) ? nextKey : thisKey;

	GetExtendedPointKeys(endDistance, thisKey, nextKey, ratio);

	int32 key1 = (direction < 0) ? thisKey : nextKey;
	int32 numKeys = pursuitPointExtendedData.Num();

	for (int32 i = key0; i != key1;)
	{
		FPursuitPointExtendedData& p0 = pursuitPointExtendedData[i];

		if (p0.UseWeatherAllowed < 1.0f - KINDA_SMALL_NUMBER)
		{
			weatherAllowed = false;
			break;
		}

		if (direction < 0)
		{
			if (--i < 0)
			{
				i = numKeys - 1;
			}
		}
		else
		{
			if (++i == numKeys)
			{
				i = 0;
			}
		}
	}

	return weatherAllowed;
}

/**
* Is weather allowed at a distance along a spline? Between 0 and 1.
***********************************************************************************/

float UPursuitSplineComponent::IsWeatherAllowed(float distance) const
{
	float key = SplineCurves.ReparamTable.Eval(distance, 0.0f);
	int32 thisKey = ThisKey(key);
	int32 nextKey = NextKey(key);

	float v0 = PursuitSplineParent->PointData[thisKey].WeatherAllowed ? 1.0f : 0.0f;
	float v1 = PursuitSplineParent->PointData[nextKey].WeatherAllowed ? 1.0f : 0.0f;

	return FMath::Lerp(v0, v1, key - thisKey);
}

#pragma endregion CameraCinematics

#pragma endregion NavigationSplines
