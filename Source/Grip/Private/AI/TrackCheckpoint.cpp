/**
*
* Track checkpoint implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Track checkpoints are used to determine vehicle progress around a track. They're
* attached to the master racing spline so we know their positions along that spline
* and thus when a vehicle crossed their position. They can be set to have a size
* so vehicle need to physically pass through a 3D window to register that passing,
* but this generally isn't necessary - track position is normally enough.
*
***********************************************************************************/

#include "ai/trackcheckpoint.h"

/**
* Construct a checkpoint.
***********************************************************************************/

ATrackCheckpoint::ATrackCheckpoint()
{
	DirectionMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DirectionMesh"));
	DirectionMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	DirectionMesh->SetHiddenInGame(true);

	SetRootComponent(DirectionMesh);

	PassingVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("PassingVolume"));
	PassingVolume->SetBoxExtent(FVector(50.0f, 50.0f, 50.0f));

	GRIP_ATTACH(PassingVolume, RootComponent, NAME_None);
}

#pragma region VehicleRaceDistance

/**
* Do some post initialization just before the game is ready to play.
***********************************************************************************/

void ATrackCheckpoint::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	PassingVolume->SetRelativeScale3D(FVector(0.25f, Width, Height));
}

/**
* Has this checkpoint been crossed, and if so in which direction?
***********************************************************************************/

int32 ATrackCheckpoint::Crossed(float fromDistance, float toDistance, float splineLength, bool crossedSplineStart) const
{
	float distanceAlongMasterRacingSpline = DistanceAlongMasterRacingSpline;

	if (crossedSplineStart == true)
	{
		// Handle wrap-around at the spline start, with lastDistance and thisDistance being on
		// opposing sides of the spline start. So put all of the distances into the same
		// frame of reference along the spline so that we can compare them correctly.

		float halfSplineLength = splineLength * 0.5f;

		if (fromDistance < halfSplineLength)
		{
			fromDistance += splineLength;
		}

		if (toDistance < halfSplineLength)
		{
			toDistance += splineLength;
		}

		if (distanceAlongMasterRacingSpline < halfSplineLength)
		{
			distanceAlongMasterRacingSpline += splineLength;
		}
	}

	if (fromDistance < distanceAlongMasterRacingSpline &&
		toDistance >= distanceAlongMasterRacingSpline)
	{
		// Going forwards past this checkpoint.

		return +1;
	}

	if (fromDistance > distanceAlongMasterRacingSpline &&
		toDistance <= distanceAlongMasterRacingSpline)
	{
		// Going backwards past this checkpoint.

		return -1;
	}

	return 0;
}

/**
* Has this checkpoint been crossed, and if so in which direction?
***********************************************************************************/

int32 ATrackCheckpoint::Crossed(float fromDistance, float toDistance, float splineLength, bool crossedSplineStart, const FVector& fromLocation, const FVector& toLocation, bool ignoreCheckpointSize) const
{
	int32 result = Crossed(fromDistance, toDistance, splineLength, crossedSplineStart);

	// If we've crossed the master racing spline distance for this checkpoint then see if we need
	// to check the window in 3D for the checkpoint too in order to register a crossing.

	if (result != 0 &&
		UseCheckpointSize == true &&
		ignoreCheckpointSize == false)
	{
		// OK, we need to check against the window size too. We do this by computing a point on the plane
		// described by the checkpoint window, where the vehicle has passed through it. We then convert
		// that point from world space into local checkpoint space, via the passing volume which we've
		// already scaled correctly to the width and height of the checkpoint.

		FVector pointOnPlane = FMath::LinePlaneIntersection(fromLocation, toLocation, FPlane(GetActorLocation(), GetActorRotation().Vector()));

		pointOnPlane = PassingVolume->GetComponentTransform().InverseTransformPosition(pointOnPlane);

		// And from there it's pretty easy to just compare the now shrunken coordinates (inverse transform)
		// to the box extents of the passing volume.

		float width = PassingVolume->GetUnscaledBoxExtent().Y;
		float height = PassingVolume->GetUnscaledBoxExtent().Z;

		if (pointOnPlane.Y < -width ||
			pointOnPlane.Y > width ||
			pointOnPlane.Z < -height ||
			pointOnPlane.Z > height)
		{
			// Outside of the width and height so signal no crossing.

			return 0;
		}
	}

	return result;
}

#if WITH_EDITOR

/**
* Ensure the width and height propagate down to the PassingVolume component.
***********************************************************************************/

void ATrackCheckpoint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if ((PropertyChangedEvent.Property != nullptr) &&
		(PropertyChangedEvent.Property->GetName() == "Width" || PropertyChangedEvent.Property->GetName() == "Height"))
	{
		PassingVolume->SetRelativeScale3D(FVector(0.25f, Width, Height));
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif // WITH_EDITOR

#pragma endregion VehicleRaceDistance
