/**
*
* Attractable interface.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* An interface to use for having objects be attractive to other objects, in the case
* of GRIP these objects are always attractive to AI bots.
*
* For example you might have speed pads be attractive to AI bots by simply having
* the speed pads inherit the IAttractableInterface class and define its virtual
* functions.
*
***********************************************************************************/

#include "system/attractable.h"
#include "system/mathhelpers.h"

/**
* Construct a UAttractableInterface, has to be defined here as can't be defined
* in the header file for some reason.
***********************************************************************************/

UAttractableInterface::UAttractableInterface(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{ }

/**
* Is this attractor in range from a given location and direction?
***********************************************************************************/

bool IAttractableInterface::IsAttractorInRange(const FVector& fromLocation, const FVector& fromDirection, bool alreadyCaptured)
{

#pragma region AIAttraction

	float angleRange = GetAttractionAngleRange();
	float distanceRange = GetAttractionDistanceRange();

	if (angleRange > KINDA_SMALL_NUMBER &&
		distanceRange > KINDA_SMALL_NUMBER)
	{
		FVector attractionLocation = GetAttractionLocation();
		float distanceSqr = (fromLocation - attractionLocation).SizeSquared();

		if (distanceSqr < distanceRange * distanceRange)
		{
			float distance = FMath::Sqrt(distanceSqr);

			if (alreadyCaptured == true ||
				distance < GetAttractionMinCaptureDistanceRange())
			{
				FVector toTarget = fromLocation - attractionLocation;

				toTarget.Normalize();

				// We increase the range up to 180 degrees the closer the target is to the attractor,
				// but using squared rather than linear interpolation, so all the increase in range is
				// when the target is very close to the attractor.

				float ratio = 1.0f - (distance / distanceRange);

				angleRange = FMath::Lerp(angleRange, FMath::Min(180.0f, angleRange * 3.0f), ratio * ratio);

				// Better to use ConeDegreesToDotProduct here instead of DotProductToDegrees later
				// as Acos is slower than Cos.

				float dotProductRange = FMathEx::ConeDegreesToDotProduct(angleRange);

				// The attraction direction, or FVector::ZeroVector if no direction.

				FVector attractionDirection = GetAttractionDirection();

				// Ensure the angle towards the attractor is in range.

				if (attractionDirection == FVector::ZeroVector ||
					FVector::DotProduct(attractionDirection, toTarget) > dotProductRange)
				{
					// Now ensure the orientation of the caller is in range.

					if ((FVector::DotProduct(fromDirection, toTarget) * -1.0f) > dotProductRange)
					{
						return true;
					}
				}
			}
		}
	}

#pragma endregion AIAttraction

	return false;
}
