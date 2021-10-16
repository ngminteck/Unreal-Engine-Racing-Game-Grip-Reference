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

#include "ai/advancedsplinecomponent.h"
#include "system/mathhelpers.h"

/**
* Construct an advanced spline component.
***********************************************************************************/

UAdvancedSplineComponent::UAdvancedSplineComponent()
{
	UPrimitiveComponent::SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	UPrimitiveComponent::SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	SetGenerateOverlapEvents(false);

	Mobility = EComponentMobility::Movable;

	// Grab the actor's name and store it locally for easier diagnostic work.

	AActor* actor = GetOwner();

	if (actor != nullptr)
	{
		ActorName = actor->GetName();
	}
}

#pragma region NavigationSplines

/**
* Post initialize the component.
***********************************************************************************/

void UAdvancedSplineComponent::PostInitialize()
{
	// Ensure we have high accuracy in determining distance along the spline.

	if (ReparamStepsPerSegment != 100)
	{
		ReparamStepsPerSegment = 100;

		UpdateSpline();
	}

	CalculateSections();
}

/**
* Find the nearest distance along a spline to a given world location.
* The fewer iterations and samples you use the faster it will be, but also the less
* accurate it will be. Conversely, the smaller the difference between startDistance
* and endDistance the more accurate the result will be.
***********************************************************************************/

float UAdvancedSplineComponent::GetNearestDistance(FVector location, float startDistance, float endDistance, int32 numIterations, int32 numSamples, float earlyExitDistance) const
{
	// This is a relatively slow iterative method, but it works solidly. I tried a couple of analytical
	// methods, which worked a lot of the time, but didn't always, which was frustrating.

	float splineLength = GetSplineLength();

	if (endDistance <= 0.0f)
	{
		endDistance = splineLength;
	}

	if (numIterations <= 0)
	{
		numIterations = 5;
	}

	float minDistance = startDistance;
	float maxDistance = endDistance;
	float minDistanceAway = -1.0f;
	float resultDistance = minDistance;
	float invNumSamples = 1.0f / (float)numSamples;

	// Bring the world location supplied into local space for faster comparison against
	// points on the spline.

	location = GetComponentTransform().InverseTransformPosition(location);

	for (int32 iteration = 0; iteration < numIterations; iteration++)
	{
		float distanceAlong = minDistance;
		float deltaStep = (maxDistance - minDistance) * invNumSamples;
		float lastResultDistance = resultDistance;

		// This will sample between minDistance and maxDistance inclusively.

		for (int32 sample = 0; sample <= numSamples; sample++)
		{
			// Determine the test position on the spline for distanceAlong. Functionally equivalent
			// to GetLocationAtDistanceAlongSpline, but slightly faster.

			float clampedDistanceAlong = ClampDistanceAgainstLength(distanceAlong, splineLength);
			float inputKey = SplineCurves.ReparamTable.Eval(clampedDistanceAlong, 0.0f);
			FVector testPosition = SplineCurves.Position.Eval(inputKey, FVector::ZeroVector);

			// Test against size squared because it's much faster than size.

			float distanceAway = (location - testPosition).SizeSquared();

			if (minDistanceAway == -1.0f ||
				minDistanceAway > distanceAway)
			{
				// If the minimum distanceAway was less than the last then record it.

				minDistanceAway = distanceAway;
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

	return resultDistance;
}

/**
* Find the nearest distance along a spline to a given plane location and direction.
* The fewer iterations and samples you use the faster it will be, but also the less
* accurate it will be. Conversely, the smaller the difference between startDistance
* and endDistance the more accurate the result will be.
***********************************************************************************/

float UAdvancedSplineComponent::GetNearestDistance(FVector planeLocation, FVector planeDirection, float startDistance, float endDistance, int32 numIterations, int32 numSamples, float earlyExitDistance) const
{
	// This is a relatively slow iterative method, but it works solidly. I tried a couple of analytical
	// methods, which worked a lot of the time, but didn't always, which was frustrating.

	float splineLength = GetSplineLength();

	if (endDistance <= 0.0f)
	{
		endDistance = splineLength;
	}

	if (numIterations <= 0)
	{
		numIterations = 5;
	}

	float minDistance = startDistance;
	float maxDistance = endDistance;
	float minDistanceAway = -1.0f;
	float resultDistance = minDistance;
	float invNumSamples = 1.0f / (float)numSamples;

	// Bring the plane location and direction supplied into local space for faster comparison against
	// points on the spline.

	planeLocation = GetComponentTransform().InverseTransformPosition(planeLocation);
	planeDirection = GetComponentTransform().InverseTransformVector(planeDirection); planeDirection.Normalize();

	for (int32 iteration = 0; iteration < numIterations; iteration++)
	{
		float distanceAlong = minDistance;
		float deltaStep = (maxDistance - minDistance) * invNumSamples;
		float lastResultDistance = resultDistance;

		// This will sample between minDistance and maxDistance inclusively.

		for (int32 sample = 0; sample <= numSamples; sample++)
		{
			// Determine the test position on the spline for distanceAlong. Functionally equivalent
			// to GetLocationAtDistanceAlongSpline, but slightly faster.

			float clampedDistanceAlong = ClampDistanceAgainstLength(distanceAlong, splineLength);
			float inputKey = SplineCurves.ReparamTable.Eval(clampedDistanceAlong, 0.0f);
			FVector testPosition = SplineCurves.Position.Eval(inputKey, FVector::ZeroVector);

			// Test against size squared because it's much faster than size.

			float distanceAway = FMath::Abs(FVector::PointPlaneDist(testPosition, planeLocation, planeDirection));

			if (minDistanceAway == -1.0f ||
				minDistanceAway > distanceAway)
			{
				// If the minimum distanceAway was less than the last then record it.

				minDistanceAway = distanceAway;
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

	return resultDistance;
}

/**
* Get the distance between two points on a spline (accounting for looped splines).
* Subtracting distance1 from distance0, notionally if you want an unsigned result.
***********************************************************************************/

float UAdvancedSplineComponent::GetDistanceDifference(float distance0, float distance1, float length, bool signedDifference) const
{
	float difference = distance0 - distance1;

	if (IsClosedLoop() == true)
	{
		if (length == 0.0f)
		{
			length = GetSplineLength();
		}

		float halfLength = length * 0.5f;

		if (FMath::Abs(difference) > halfLength)
		{
			if (distance0 <= halfLength &&
				distance1 >= length - halfLength)
			{
				difference = distance0 + (length - distance1);
			}
			else if (distance1 <= halfLength &&
				distance0 >= length - halfLength)
			{
				difference = -(distance1 + (length - distance0));
			}
		}
	}

	return (signedDifference == true) ? difference : FMath::Abs(difference);
}

/**
* Clamp a distance along the spline to its length if it's not looped, or wrapped
* within its length if looped.
***********************************************************************************/

float UAdvancedSplineComponent::ClampDistanceAgainstLength(float distance, float length) const
{
	if (distance < 0.0f)
	{
		distance = (IsClosedLoop() == true) ? length - FMath::Fmod(-distance, length) : 0.0f;
	}
	else if (distance > length)
	{
		distance = (IsClosedLoop() == true) ? FMath::Fmod(distance, length) : length;
	}

	return distance;
}

/**
* Get which side a world location is on with respect to its nearest point along the
* spline center-line.
***********************************************************************************/

float UAdvancedSplineComponent::GetSide(float distance, const FVector& fromLocation) const
{
	FRotator rotation = GetRotationAtDistanceAlongSpline(distance, ESplineCoordinateSpace::World);
	FVector sideVector = rotation.RotateVector(FVector(0.0f, 1.0f, 0.0f));
	FVector location = GetLocationAtDistanceAlongSpline(distance, ESplineCoordinateSpace::World);

	return (FVector::DotProduct((fromLocation - location), sideVector) >= 0.0f) ? 1.0f : -1.0f;
}

/**
* Calculate the sections of the spline.
***********************************************************************************/

void UAdvancedSplineComponent::CalculateSections()
{

#pragma region CameraCinematics

	// Calculate just the StraightSections of the spline. The DroneSections will
	// need to be done elsewhere as we don't have the information to do that here.

	float length = GetSplineLength();

	StraightSections = GetSurfaceSections();

	// Create a list of rotational differences along the length of the spline
	// for us to quickly examine to determine differences for specific sections.

	float distance = 0.0f;
	float iterationDistance = FMathEx::MetersToCentimeters(ExtendedPointMeters);
	int32 numIterations = FMath::CeilToInt(length / iterationDistance);

	TArray<FRotator> rotations;

	rotations.Reserve(numIterations);

	FRotator lastRotation = GetQuaternionAtDistanceAlongSpline(distance, ESplineCoordinateSpace::Local).Rotator();

	for (int32 i = 0; i < numIterations; i++)
	{
		distance += iterationDistance;
		distance = ClampDistanceAgainstLength(distance, length);

		FRotator rotation = GetQuaternionAtDistanceAlongSpline(distance, ESplineCoordinateSpace::Local).Rotator();

		rotations.Emplace(FMathEx::GetUnsignedDegreesDifference(lastRotation, rotation));

		lastRotation = rotation;
	}

	TArray<float> clearances = GetClearancesFromSurface();

	for (int32 index = 0; index < StraightSections.Num(); index++)
	{
		FSplineSection& section = StraightSections[index];

		int32 i = 0;
		int32 j = 0;
		float d0 = section.StartDistance;
		float d1 = section.EndDistance;
		int32 i0 = FMath::Max(FMath::FloorToInt(d0 / iterationDistance), 0);
		int32 i1 = FMath::Min(FMath::CeilToInt(d1 / iterationDistance), clearances.Num() - 1);

		for (i = i0; i <= i1; i++)
		{
			if (clearances[i] < 12.5f * 100.0f)
			{
				// Too low, so break up this section.

				for (j = i + 1; j <= i1; j++)
				{
					if (clearances[j] >= 12.5f * 100.0f)
					{
						break;
					}
				}

				float ed0 = FMath::Max((float)(i - 1) * iterationDistance, section.StartDistance);
				float ed1 = FMath::Min((float)j * iterationDistance, section.EndDistance);
				float ed2 = section.EndDistance;

				section.EndDistance = ed0;

				if (ed1 < ed2)
				{
					StraightSections.Insert(FSplineSection(ed1, ed2), index + 1);
				}

				break;
			}
		}
	}

	// OK, so now we have a list of section relative close to the ground without any
	// big bumps or drops in the ground closest to the spline.

	// Now we need to remove the sections that have sharp rotational changes.

	float maxCurvaturePerSecond = 75.0f;
	float baseSpeed = FMathEx::KilometersPerHourToMetersPerSecond(700.0f);
	float maxCurvaturePerStep = maxCurvaturePerSecond / (baseSpeed / ExtendedPointMeters);

	for (int32 index = 0; index < StraightSections.Num(); index++)
	{
		FSplineSection& section = StraightSections[index];

		if (section.StartDistance < section.EndDistance)
		{
			int32 i = 0;
			int32 j = 0;
			float d0 = section.StartDistance;
			float d1 = section.EndDistance;
			int32 i0 = FMath::Max(FMath::FloorToInt(d0 / iterationDistance), 0);
			int32 i1 = FMath::Min(FMath::CeilToInt(d1 / iterationDistance), rotations.Num() - 1);

			for (i = i0; i <= i1; i++)
			{
				if (FMath::Max(rotations[i].Yaw, rotations[i].Pitch) > maxCurvaturePerStep ||
					rotations[i].Roll > maxCurvaturePerStep * 2.0f)
				{
					// Too windy, so break up this section.

					for (j = i + 1; j <= i1; j++)
					{
						if (FMath::Max(rotations[j].Yaw, rotations[j].Pitch) <= maxCurvaturePerStep &&
							rotations[j].Roll <= maxCurvaturePerStep * 2.0f)
						{
							break;
						}
					}

					float ed0 = FMath::Max((float)(i - 1) * iterationDistance, section.StartDistance);
					float ed1 = FMath::Min((float)j * iterationDistance, section.EndDistance);
					float ed2 = section.EndDistance;

					section.EndDistance = ed0;

					if (ed1 < ed2)
					{
						StraightSections.Insert(FSplineSection(ed1, ed2), index + 1);
					}

					break;
				}
			}
		}
	}

	for (int32 index = 0; index < StraightSections.Num(); index++)
	{
		FSplineSection& section = StraightSections[index];

		if (section.EndDistance - section.StartDistance < 100.0f * 100.0f)
		{
			StraightSections.RemoveAt(index--);
		}
	}

#pragma endregion CameraCinematics

}

#pragma region AIVehicleControl

/**
* Get the curvature of the spline in degrees over distance (in withRespectTo space).
***********************************************************************************/

FRotator UAdvancedSplineComponent::GetCurvatureOverDistance(float distance, float& overDistance, int32 direction, const FQuat& withRespectTo, bool absolute) const
{
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

	float length = GetSplineLength();
	bool transform = withRespectTo.IsIdentity() == false;
	float iterationDistance = FMathEx::MetersToCentimeters(ExtendedPointMeters);
	FQuat invWithRespectTo = withRespectTo.Inverse();
	int32 numIterations = FMath::CeilToInt(FMath::Abs(endDistance - distance) / iterationDistance);
	FRotator lastRotation = (invWithRespectTo * GetQuaternionAtDistanceAlongSpline(distance, ESplineCoordinateSpace::World)).Rotator();

	for (int32 i = 0; i < numIterations; i++)
	{
		// Calculate the current distance along the spline.

		distance += iterationDistance * direction;
		distance = ClampDistanceAgainstLength(distance, length);

		// Get the rotation at that distance along the spline, with respect to another
		// rotation if given.

		FQuat quaternion = GetQuaternionAtDistanceAlongSpline(distance, ESplineCoordinateSpace::World);
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

#pragma region CameraCinematics

/**
* Get the distance into between a start and end point.
***********************************************************************************/

float UAdvancedSplineComponent::GetDistanceInto(float distance, float start, float end) const
{
	float length = GetSplineLength();

	distance = ClampDistanceAgainstLength(distance, length);
	start = ClampDistanceAgainstLength(start, length);
	end = ClampDistanceAgainstLength(end, length);

	if (start > end)
	{
		if (distance >= start)
		{
			return distance - start;
		}
		else if (distance <= end)
		{
			return distance + (length - start);
		}
	}
	else
	{
		if (distance >= start && distance <= end)
		{
			return distance - start;
		}
	}

	return 0.0f;
}

/**
* Get the distance left between a start and end point.
***********************************************************************************/

float UAdvancedSplineComponent::GetDistanceLeft(float distance, float start, float end) const
{
	float length = GetSplineLength();

	distance = ClampDistanceAgainstLength(distance, length);
	start = ClampDistanceAgainstLength(start, length);
	end = ClampDistanceAgainstLength(end, length);

	if (start > end)
	{
		if (distance >= start)
		{
			return end + (length - distance);
		}
		else if (distance <= end)
		{
			return end - distance;
		}
	}
	else
	{
		if (distance >= start && distance <= end)
		{
			return end - distance;
		}
	}

	return 0.0f;
}

#pragma endregion CameraCinematics

#pragma endregion NavigationSplines
