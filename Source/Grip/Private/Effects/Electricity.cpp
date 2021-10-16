/**
*
* Electricity implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* All of the structures and classes used to render electricity within the game.
* We have single electrical streak components, and also electrical generators to
* generate multiple streaks either on a continuous basis or just for short periods.
* This is used for effects with some of the levels, and also for the vehicle
* destroyed explosion.
*
***********************************************************************************/

#include "effects/electricity.h"
#include "vehicle/flippablevehicle.h"
#include "uobject/constructorhelpers.h"
#include "gamemodes/basegamemode.h"

/**
* Some static data members.
***********************************************************************************/

UMaterialInterface* UElectricalStreakComponent::StandardStreakMaterial = nullptr;
UMaterialInterface* UElectricalStreakComponent::StandardFlareMaterial = nullptr;

/**
* Construct an electrical streak component.
***********************************************************************************/

UElectricalStreakComponent::UElectricalStreakComponent()
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;

	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> asset(TEXT("Material'/Game/Vehicles/Materials/LightStreaks/M_StandardElectricalStreak.M_StandardElectricalStreak'"));
		StandardStreakMaterial = asset.Object;
	}

	StreakMaterial = StandardStreakMaterial;

	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> asset(TEXT("Material'/Game/Vehicles/Materials/LightStreaks/M_StandardLightQuad.M_StandardLightQuad'"));
		StandardFlareMaterial = asset.Object;
	}

	FlareMaterial = StandardFlareMaterial;
}

/**
* Construct an electrical generator.
***********************************************************************************/

AElectricalGenerator::AElectricalGenerator()
{
	PrimaryActorTick.bCanEverTick = true;

	StartLocation = CreateDefaultSubobject<UBillboardComponent>(TEXT("StartLocation"));

	SetRootComponent(StartLocation);

#if WITH_EDITOR
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> TargetIconSpawnObject;
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> TargetIconObject;
		FName ID_TargetPoint;
		FText NAME_TargetPoint;
		FConstructorStatics()
			: TargetIconSpawnObject(TEXT("/Engine/EditorMaterials/TargetIconSpawn"))
			, TargetIconObject(TEXT("/Engine/EditorMaterials/TargetIcon"))
			, ID_TargetPoint(TEXT("TargetPoint"))
			, NAME_TargetPoint(NSLOCTEXT("SpriteCategory", "TargetPoint", "Target Points"))
		{ }
	};

	static FConstructorStatics ConstructorStatics;

	if (GRIP_OBJECT_VALID(StartLocation) == true)
	{
		StartLocation->Sprite = ConstructorStatics.TargetIconObject.Get();
		StartLocation->SpriteInfo.Category = ConstructorStatics.ID_TargetPoint;
		StartLocation->SpriteInfo.DisplayName = ConstructorStatics.NAME_TargetPoint;
		StartLocation->bIsScreenSizeScaled = true;

		StartLocation->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	}
#endif // WITH_EDITOR

	ElectricalStreak = CreateDefaultSubobject<UElectricalStreakComponent>(TEXT("ElectricalStreak"));

	GRIP_ATTACH(ElectricalStreak, StartLocation, NAME_None);

	StartLocationLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StartLocationLight"));

	GRIP_ATTACH(StartLocationLight, StartLocation, NAME_None);

	EndLocationLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EndLocationLight"));

	GRIP_ATTACH(EndLocationLight, StartLocation, NAME_None);
}

/**
* Enable electrical strikes.
***********************************************************************************/

void AElectricalGenerator::EnableStrikes() const
{
	TArray<UActorComponent*> components;

	GetComponents(UElectricalStreakComponent::StaticClass(), components);

	for (UActorComponent* component : components)
	{
		(Cast<UElectricalStreakComponent>(component))->StrikesEnabled = true;
		(Cast<UElectricalStreakComponent>(component))->AutoStrike = true;
	}
}

/**
* Disable electrical strikes.
***********************************************************************************/

void AElectricalGenerator::DisableStrikes() const
{
	TArray<UActorComponent*> components;

	GetComponents(UElectricalStreakComponent::StaticClass(), components);

	for (UActorComponent* component : components)
	{
		(Cast<UElectricalStreakComponent>(component))->StrikesEnabled = false;
		(Cast<UElectricalStreakComponent>(component))->AutoStrike = false;
	}
}

#pragma region ElectricalEffects

/**
* Some static FNames for performance benefit.
***********************************************************************************/

namespace ElectricityParameterNames
{
	static const FName AspectRatioName("AspectRatio");
	static const FName AutoRotateFlareName("AutoRotateFlare");
	static const FName FadeOnAngleDeviationName("FadeOnAngleDeviation");
	static const FName AlphaName("Alpha");
	static const FName ColourName("Colour");
	static const FName EndColourName("EndColour");
	static const FName TextureName("Texture");
	static const FName RotateFlareName("RotateFlare");
	static const FName WidthName("Width");
	static const FName TailShrinkScaleName("TailShrinkScale");
	static const FName AnimationTimerName("AnimationTimer");
	static const FName DistanceTraveledName("DistanceTravelled");
	static const FName LifeTimeName("LifeTime");
	static const FName InvLifeTimeName("InvLifeTime");
	static const FName CameraFacingName("CameraFacing");
	static const FName AlphaFadePowerName("AlphaFadePower");
	static const FName DepthFadeName("DepthFade");
	static const FName LifeTimeAlphaName("LifeTimeAlpha");
	static const FName TendrilAlphaScaleName("TendrilAlphaScale");
	static const FName TendrilShrinkScaleName("TendrilShrinkScale");
	static const FName TendrilTimePowerName("TendrilTimePower");
	static const FName TendrilLengthPowerName("TendrilLengthPower");
};

/**
* Add a new vertex joint.
***********************************************************************************/

static int32 AddElectricityVertexJoint(TArray<FVector>& vertices, TArray<FVector>& normals, TArray<FVector2D>& uv0, TArray<FColor>& colours, const FVector& location, FVector direction, const FVector& horizontalAxis, float time, float alpha, int32 startIndex, int32 numJointVertices, bool cameraFacing)
{
	int32 initialIndex = startIndex;

	direction.Normalize();

	if (vertices.Num() - startIndex >= numJointVertices)
	{
		FColor color = FColor::White;

		// Store the direction or tangent in the color so that I can read it in the material.

		color.R = (uint8)((direction.X * 127.0f) + 128.0f);
		color.G = (uint8)((direction.Y * 127.0f) + 128.0f);
		color.B = (uint8)((direction.Z * 127.0f) + 128.0f);
		color.A = (uint8)(alpha * 255.0f);

		FVector normal = horizontalAxis;

		if (cameraFacing == true)
		{
			normal = direction.ToOrientationQuat().GetAxisY();
		}

		for (int32 i = 0; i < numJointVertices; i++)
		{
			vertices[startIndex] = location;
			normals[startIndex] = normal; normal *= -1.0f;
			uv0[startIndex] = FVector2D(i, time);
			colours[startIndex] = color;

			// So:
			// color is the forwards direction of the light streak in world space.
			// normal is the sideways vector in world space, with no roll applied.
			// uv0 is the U coordinate combined with the time the point was emitted.

			startIndex++;
		}
	}

	if (vertices.Num() - startIndex >= numJointVertices)
	{
		for (int32 i = 0; i < numJointVertices; i++)
		{
			vertices[startIndex] = location;

			startIndex++;
		}
	}

	return startIndex - initialIndex;
}

/**
* Setup a vertex joint.
***********************************************************************************/

static void SetupElectricityVertexJoint(TArray<FVector>& vertices, TArray<FVector>& normals, TArray<FVector2D>& uv0, TArray<FColor>& colours, int32 startIndex, int32 numJointVertices, const FVector& location)
{
	FVector2D uv = FVector2D(0.0f, 0.0f);
	FColor color = FColor::White; color.A = 0;

	for (int32 i = 0; i < numJointVertices; i++)
	{
		vertices[startIndex] = location;
		normals[startIndex] = FVector(0.0f, 1.0f, 0.0f);
		uv0[startIndex] = uv;
		colours[startIndex] = color;

		startIndex++;
	}
}

/**
* Setup a vertex quad.
***********************************************************************************/

static void SetupElectricityFlareQuad(TArray<FVector>& vertices, TArray<FVector>& normals, TArray<FVector2D>& uv0, const FVector& location, const FVector& direction)
{
	int32 startIndex = 0;

	vertices[startIndex] = location;
	normals[startIndex] = direction;
	uv0[startIndex++] = FVector2D(-1.0f, +1.0f);
	vertices[startIndex] = location;
	normals[startIndex] = direction;
	uv0[startIndex++] = FVector2D(+1.0f, +1.0f);
	vertices[startIndex] = location;
	normals[startIndex] = direction;
	uv0[startIndex++] = FVector2D(-1.0f, -1.0f);
	vertices[startIndex] = location;
	normals[startIndex] = direction;
	uv0[startIndex] = FVector2D(+1.0f, -1.0f);
}

/**
* Initialize the component.
***********************************************************************************/

void UElectricalStreakComponent::InitializeComponent()
{
	Super::InitializeComponent();

	StartLocation = GetComponentLocation();
	EndLocations.Emplace(FLocationProbability(StartLocation, 1.0f, FVector::UpVector));

	BaseAlpha = Alpha;
	NumPoints = FMathEx::GetPower2(NumPoints);
	ThisLifeTime = LifeTime.Minimum;

	InitialDelay.GenerateRandom();

	for (TArray<FLineSegment>& segments : Segments)
	{
		segments.Reserve(NumPoints);
	}

	if (StreakEndColour.R < 0.0f)
	{
		StreakEndColour = StreakColour;
	}

	Geometry = NewObject<UProceduralMeshComponent>(this);

#if GRIP_ENGINE_EXTENDED_MODIFICATIONS
	Geometry->SetHasCollision(false);
#endif // GRIP_ENGINE_EXTENDED_MODIFICATIONS

	Geometry->RegisterComponent();

	if (Enabled == true)
	{
		Geometry->SetCastShadow(false);

		FVector location = GetComponentTransform().GetLocation();
		FVector direction = GetComponentTransform().GetRotation().Vector();

		if (Flare == true)
		{
			// Create the light flare stuff.

			DynamicFlareMaterial = Geometry->CreateDynamicMaterialInstance(0, FlareMaterial);

			if (DynamicFlareMaterial != nullptr)
			{
				Geometry->SetMaterial(0, DynamicFlareMaterial);

				FlareColour.A = 1.0f;

				SetFlareColour.Setup(DynamicFlareMaterial, ElectricityParameterNames::ColourName, FlareColour);
				SetFlareAlpha.Setup(DynamicFlareMaterial, ElectricityParameterNames::AlphaName, Alpha);
				SetFlareWidth.Setup(DynamicFlareMaterial, ElectricityParameterNames::WidthName, Size * 0.5f);
				SetFlareAspectRatio.Setup(DynamicFlareMaterial, ElectricityParameterNames::AspectRatioName, AspectRatio);
				SetFlareRotate.Setup(DynamicFlareMaterial, ElectricityParameterNames::RotateFlareName, 0.0f);

				DynamicFlareMaterial->SetTextureParameterValue(ElectricityParameterNames::TextureName, FlareTexture);
				DynamicFlareMaterial->SetScalarParameterValue(ElectricityParameterNames::AutoRotateFlareName, (AutoRotateFlare == true) ? 1.0f : 0.0f);
				DynamicFlareMaterial->SetScalarParameterValue(ElectricityParameterNames::FadeOnAngleDeviationName, 0.0f);
				DynamicFlareMaterial->SetScalarParameterValue(ElectricityParameterNames::DepthFadeName, FlareDepthFade);
			}

			FlareVertices.AddDefaulted(4);
			FlareNormals.AddDefaulted(4);
			FlareUV0.AddDefaulted(4);

			FlareTriangles.Reserve(6);

			SetupElectricityFlareQuad(FlareVertices, FlareNormals, FlareUV0, FVector::ZeroVector, direction);

			FlareTriangles.Emplace(0);
			FlareTriangles.Emplace(1);
			FlareTriangles.Emplace(2);

			FlareTriangles.Emplace(1);
			FlareTriangles.Emplace(3);
			FlareTriangles.Emplace(2);

			Geometry->CreateMeshSection(0, FlareVertices, FlareTriangles, FlareNormals, FlareUV0, FlareColours, FlareTangents, false);
		}

		{
			DynamicStreakMaterial = Geometry->CreateDynamicMaterialInstance(1, StreakMaterial);

			if (DynamicStreakMaterial != nullptr)
			{
				Geometry->SetMaterial(1, DynamicStreakMaterial);

				StreakColour.A = 1.0f;
				StreakEndColour.A = 1.0f;

				SetStreakColour.Setup(DynamicStreakMaterial, ElectricityParameterNames::ColourName, StreakColour);
				SetStreakEndColour.Setup(DynamicStreakMaterial, ElectricityParameterNames::EndColourName, StreakEndColour);
				SetStreakAnimationTimer.Setup(DynamicStreakMaterial, ElectricityParameterNames::AnimationTimerName, 0.0f);
				SetStreakDistanceTraveled.Setup(DynamicStreakMaterial, ElectricityParameterNames::DistanceTraveledName, 0.0f);
				SetStreakLifeTime.Setup(DynamicStreakMaterial, ElectricityParameterNames::LifeTimeName, ThisLifeTime);
				SetStreakInvLifeTime.Setup(DynamicStreakMaterial, ElectricityParameterNames::InvLifeTimeName, 1.0f / ThisLifeTime);
				SetStreakLifeTimeAlpha.Setup(DynamicStreakMaterial, ElectricityParameterNames::LifeTimeAlphaName, 0.0f);

				DynamicStreakMaterial->SetScalarParameterValue(ElectricityParameterNames::CameraFacingName, 1.0f);
				DynamicStreakMaterial->SetScalarParameterValue(ElectricityParameterNames::AlphaFadePowerName, AlphaFadePower);
				DynamicStreakMaterial->SetScalarParameterValue(ElectricityParameterNames::TendrilAlphaScaleName, TendrilAlphaScale);
				DynamicStreakMaterial->SetScalarParameterValue(ElectricityParameterNames::TendrilShrinkScaleName, TendrilShrinkScale);
				DynamicStreakMaterial->SetScalarParameterValue(ElectricityParameterNames::TendrilTimePowerName, TendrilTimePower);
				DynamicStreakMaterial->SetScalarParameterValue(ElectricityParameterNames::TendrilLengthPowerName, TendrilLengthPower);
			}

			// Create the light streak stuff.

			int32 numJoints = NumPoints;

			int32 maxVertices = numJoints * NumJointVertices;

			Vertices.AddDefaulted(maxVertices);
			Normals.AddDefaulted(maxVertices);
			UV0.AddDefaulted(maxVertices);
			Colours.AddDefaulted(maxVertices);

			Triangles.Reserve((numJoints - 1) * NumJointVertices * 6);

			for (int32 i = 0; i < numJoints; i++)
			{
				SetupElectricityVertexJoint(Vertices, Normals, UV0, Colours, i * NumJointVertices, NumJointVertices, location);

				if (i > 0)
				{
					int32 i1 = i * NumJointVertices;
					int32 i0 = i1 - NumJointVertices;

					for (int32 j = 0; j < NumJointVertices - 1; j++)
					{
						int32 imask = (j + 1);

						Triangles.Emplace(i1 + j);
						Triangles.Emplace(i0 + j);
						Triangles.Emplace(i1 + imask);

						Triangles.Emplace(i0 + j);
						Triangles.Emplace(i0 + imask);
						Triangles.Emplace(i1 + imask);
					}
				}
			}

			Geometry->CreateMeshSection(1, Vertices, Triangles, Normals, UV0, Colours, Tangents, false);
		}
	}

	GenerateElectricity();
}

/**
* Do the regular update tick.
***********************************************************************************/

void UElectricalStreakComponent::TickComponent(float deltaSeconds, enum ELevelTick tickType, FActorComponentTickFunction* thisTickFunction)
{
	Super::TickComponent(deltaSeconds, tickType, thisTickFunction);

	float alpha = (Timer < 0.0f) ? 0.0f : Alpha;

	if (Flare == true &&
		DynamicFlareMaterial != nullptr)
	{
		SetFlareAlpha.Set(alpha);

		if (alpha != 0.0f)
		{
			FlareColour.A = 1.0f;

			SetFlareWidth.Set(Size * 0.5f);
			SetFlareAspectRatio.Set(AspectRatio);
			SetFlareColour.Set(FlareColour);
		}
	}

	if (DynamicStreakMaterial != nullptr)
	{
		if (Timer >= 0.0f)
		{
			SetStreakAnimationTimer.Set(FMath::Min(Timer, ThisLifeTime));
		}
		else
		{
			SetStreakAnimationTimer.Set(ThisLifeTime);
		}

		SetStreakDistanceTraveled.Set(0.0f);
		SetStreakLifeTime.Set(ThisLifeTime);
		SetStreakInvLifeTime.Set(1.0f / ThisLifeTime);

		if (alpha != 0.0f)
		{
			StreakColour.A = 1.0f;
			StreakEndColour.A = 1.0f;

			SetStreakColour.Set(StreakColour);
			SetStreakEndColour.Set(StreakEndColour);
		}

		float lifeTimeAlpha = (Timer >= 0.0f && Timer < ThisLifeTime) ? FMath::Pow(1.0f - (FMath::Max(Timer, 0.0f) / ThisLifeTime), AlphaFadePower) : 0.0f;

		SetStreakLifeTimeAlpha.Set(lifeTimeAlpha);
	}

	Timer += deltaSeconds;

	if (AutoStrike == true &&
		Timer > RespawnAt)
	{
		float timer = FMath::Fmod(Timer - RespawnAt, RespawnAt);

		GenerateElectricity();

		Timer = timer;
	}
}

/**
* Generate the electrical streak.
***********************************************************************************/

void UElectricalStreakComponent::GenerateElectricity()
{
	if (LocationsSet == true)
	{
		float total = 0.0f;

		for (FLocationProbability& location : EndLocations)
		{
			total += location.Probability;
		}

		float endSelection = FMath::FRand() * total;

		CurrentEndLocation = EndLocations[EndLocations.Num() - 1];

		total = 0.0f;

		for (FLocationProbability& location : EndLocations)
		{
			total += location.Probability;

			if (total >= endSelection)
			{
				CurrentEndLocation = location;
				break;
			}
		}

		const FTransform& transform = GetComponentTransform();
		FVector start = transform.GetLocation();
		FVector end = transform.TransformPosition(CurrentEndLocation.Location);
		FVector hitNormal = CurrentEndLocation.HitNormal;

		GenerateElectricity(start, end, hitNormal);
	}
}

/**
* Generate the electrical streak, locations in world space.
***********************************************************************************/

void UElectricalStreakComponent::GenerateElectricity(FVector start, FVector end, FVector& hitNormal)
{
	if (StrikesEnabled == true)
	{
		Width.GenerateRandom();
		LifeShrinkScale.GenerateRandom();

		if (DynamicStreakMaterial != nullptr)
		{
			DynamicStreakMaterial->SetScalarParameterValue(ElectricityParameterNames::WidthName, Width.Get() * 0.5f);
			DynamicStreakMaterial->SetScalarParameterValue(ElectricityParameterNames::TailShrinkScaleName, LifeShrinkScale.Get());
		}

		SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector::ZeroVector));

		const FTransform& transform = GetComponentTransform();

		start = transform.InverseTransformPosition(start);
		end = transform.InverseTransformPosition(end);

		int32 flipFlop = 0;
		FRotator rotator = FRotator::ZeroRotator;
		float offsetAmount = end.Size() * Deviation.GetRandom();

		Segments[flipFlop].Empty(NumPoints);
		Segments[flipFlop].Emplace(FLineSegment(start, end));

		float numMetersPerSegment = (end - start).Size();

		while (Segments[flipFlop].Num() < NumPoints)
		{
			TArray<FLineSegment>& oldSegments = Segments[flipFlop];
			TArray<FLineSegment>& newSegments = Segments[flipFlop ^ 1];
			int32 numSegments = oldSegments.Num();

			newSegments.Empty(NumPoints);

			for (int32 segment = 0; segment < numSegments; segment++)
			{
				rotator.Roll = FMath::FRand() * 360.0f;

				FVector segmentStart = oldSegments[segment].Start;
				FVector segmentEnd = oldSegments[segment].End;
				FVector midPoint = (segmentStart + segmentEnd) * 0.5f;
				FVector direction = segmentEnd - segmentStart;
				FQuat quaternion = direction.ToOrientationQuat() * rotator.Quaternion();

				midPoint += quaternion.GetAxisY() * FMath::FRandRange(-offsetAmount, offsetAmount);

				newSegments.Emplace(FLineSegment(segmentStart, midPoint));
				newSegments.Emplace(FLineSegment(midPoint, segmentEnd));
			}

			oldSegments.Empty(NumPoints);

			flipFlop ^= 1;
			offsetAmount *= 0.5f;
			numMetersPerSegment *= 0.5f;

			if (NumMetresPerPoint != 0.0f &&
				numMetersPerSegment < NumMetresPerPoint &&
				newSegments.Num() >= 8)
			{
				break;
			}
		}

		int32 numAdded = 0;
		TArray<FLineSegment>& segments = Segments[flipFlop];
		FVector horizontalAxis = FVector(0.0f, 1.0f, 0.0f);
		int32 lastSegment = (segments.Num() - 1) * NumJointVertices;

		for (FLineSegment& segment : segments)
		{
			FVector pointDirection = segment.End - segment.Start;

			AddElectricityVertexJoint(Vertices, Normals, UV0, Colours, (numAdded == lastSegment) ? segment.End : segment.Start, pointDirection, horizontalAxis, 1.0f - ((float)numAdded / (float)lastSegment), BaseAlpha, numAdded, NumJointVertices, true);

			numAdded += NumJointVertices;
		}

		for (int32 i = numAdded / NumJointVertices; i < NumPoints; i++)
		{
			SetupElectricityVertexJoint(Vertices, Normals, UV0, Colours, i * NumJointVertices, NumJointVertices, segments[segments.Num() - 1].End);

			numAdded += NumJointVertices;
		}

#if GRIP_ENGINE_EXTENDED_MODIFICATIONS
		Geometry->UpdateMeshSection(1, Vertices, Normals, UV0, Colours, Tangents, 0, numAdded);
#else
		Geometry->UpdateMeshSection(1, Vertices, Normals, UV0, Colours, Tangents);
#endif

		// If this electrical component is attached to an electrical generator then give it
		// the chance to do something here as we're generating an electrical streak.

		AElectricalGenerator* generator = Cast<AElectricalGenerator>(GetOwner());

		if (generator != nullptr)
		{
			FVector worldEnd = GetComponentTransform().TransformPosition(end);
			FVector strikeNormal = StartLocation - worldEnd; strikeNormal.Normalize();
			FVector mergedNormal = hitNormal + strikeNormal; mergedNormal.Normalize();
			FVector reflectNormal = FMath::GetReflectionVector(strikeNormal * -1.0f, hitNormal); reflectNormal.Normalize();

			generator->Strike(worldEnd, hitNormal, strikeNormal, mergedNormal, reflectNormal);
		}
	}

	ThisLifeTime = LifeTime.GetRandom();
	RespawnAt = ThisLifeTime + PostDelay.GetRandom();
	Timer = 0.0f;
}

/**
* Inherit the properties of another electrical streak component.
***********************************************************************************/

void UElectricalStreakComponent::Inherit(UElectricalStreakComponent* other)
{
	Width.Value = other->Width.Value;
	LifeTime.Value = other->LifeTime.Value;
	PostDelay.Value = other->PostDelay.Value;
	LifeShrinkScale.Value = other->LifeShrinkScale.Value;
	Timer = other->Timer;
	ThisLifeTime = other->ThisLifeTime;
	RespawnAt = other->RespawnAt;

	if (DynamicStreakMaterial != nullptr)
	{
		static const FName WidthName("Width");
		static const FName TailShrinkScaleName("TailShrinkScale");

		DynamicStreakMaterial->SetScalarParameterValue(WidthName, Width.Get() * 0.5f);
		DynamicStreakMaterial->SetScalarParameterValue(TailShrinkScaleName, LifeShrinkScale.Get());
	}
}

/**
* Do some initialization when the game is ready to play.
***********************************************************************************/

void AElectricalGenerator::BeginPlay()
{
	Super::BeginPlay();

	EndLocations.Empty();

	FlareSize = ElectricalStreak->Size;
	ElectricalStreak->Size = 0.0f;

	TArray<UActorComponent*> components;

	GetComponents(UEndLocationComponent::StaticClass(), components);

	FVector endLocationAvg = FVector::ZeroVector;

	for (UActorComponent* component : components)
	{
		UEndLocationComponent* endLocation = Cast<UEndLocationComponent>(component);

		FVector hitNormal = FVector::UpVector;

		FHitResult hitResult;
		FCollisionQueryParams queryParams("ElectricalNormalTest", false, nullptr);

		FVector endWorld = endLocation->GetComponentLocation();
		FVector toHit = endWorld - StartLocation->GetComponentLocation(); toHit.Normalize();

		if (GetWorld()->LineTraceSingleByChannel(hitResult, endWorld - (toHit * 200.0f), endWorld + (toHit * 200.0f), ABaseGameMode::ECC_LineOfSightTest, queryParams) == true && hitResult.bBlockingHit == true)
		{
			hitNormal = hitResult.ImpactNormal;
		}

		EndLocations.Emplace(FLocationProbability(endLocation->GetRelativeLocation(), endLocation->Probability, hitNormal));

		endLocationAvg += endLocation->GetRelativeLocation();
	}

	endLocationAvg *= 1.0f / components.Num();
	endLocationAvg.Normalize();

	StartLocationLight->SetRelativeLocation(endLocationAvg * StartLocationLight->AttenuationRadius * 0.25f);

	if (EndLocations.Num() > 0)
	{
		FVector location = EndLocations[0].Location;
		FVector direction = location; direction.Normalize();

		EndLocationLight->SetRelativeLocation(location - (direction * EndLocationLight->AttenuationRadius * 0.25f));
	}

	StartLocationLightIntensity = StartLocationLight->Intensity;
	EndLocationLightIntensity = EndLocationLight->Intensity;

	StartLocationLight->SetIntensity(0.0f);
	EndLocationLight->SetIntensity(0.0f);

	if (EndLocations.Num() == 0)
	{
		EndLocations.Emplace(FLocationProbability(FVector::ZeroVector, 1.0f, FVector::UpVector));
	}

	ElectricalStreak->SetLocations(StartLocation->GetComponentLocation(), EndLocations);

	GRIP_ATTACH(ElectricalStreak->GetGeometry(), RootComponent, NAME_None);

	AdditionalStreaks.Emplace(ElectricalStreak);

	for (int32 i = 1; i < NumStreaks; i++)
	{
		UElectricalStreakComponent* streak = NewObject<UElectricalStreakComponent>(this, FName(), EObjectFlags::RF_NoFlags, ElectricalStreak);

		streak->InitialDelay = (streak->LifeTime.Get() / NumStreaks) * i;

		streak->SetLocations(StartLocation->GetComponentLocation(), EndLocations);
		streak->Flare = false;

		streak->RegisterComponent();

		AdditionalStreaks.Emplace(streak);

		GRIP_ATTACH(streak, RootComponent, NAME_None);
		GRIP_ATTACH(streak->GetGeometry(), RootComponent, NAME_None);
	}

	AdditionalPointLights.Emplace(EndLocationLight);

	for (int32 i = 1; i < EndLocations.Num(); i++)
	{
		FVector location = EndLocations[i].Location;
		FVector direction = location; direction.Normalize();
		UPointLightComponent* light = NewObject<UPointLightComponent>(this, FName(), EObjectFlags::RF_NoFlags, EndLocationLight);

		light->SetIntensity(0.0f);

		light->RegisterComponent();

		GRIP_ATTACH(light, RootComponent, NAME_None);

		light->SetRelativeLocation(location - (direction * EndLocationLight->AttenuationRadius * 0.25f));

		AdditionalPointLights.Emplace(light);
	}
}

/**
* Do the regular update tick.
***********************************************************************************/

void AElectricalGenerator::Tick(float deltaSeconds)
{
	float maxSize = 0.0f;

	for (UElectricalStreakComponent* streak : AdditionalStreaks)
	{
		maxSize = FMath::Max(maxSize, streak->GetBrightness());
	}

	ElectricalStreak->Size = ((maxSize * 0.333f) + 0.666f) * FlareSize;
	ElectricalStreak->Alpha = FMath::Pow(maxSize, 0.5f) * ElectricalStreak->BaseAlpha;

	StartLocationLight->SetIntensity(maxSize * StartLocationLightIntensity);

	for (int32 i = 0; i < EndLocations.Num(); i++)
	{
		maxSize = 0.0f;

		for (UElectricalStreakComponent* streak : AdditionalStreaks)
		{
			if (streak->GetCurrentEndLocation().Location.Equals(EndLocations[i].Location) == true)
			{
				maxSize = FMath::Max(maxSize, streak->GetBrightness());
			}
		}

		AdditionalPointLights[i]->SetIntensity(maxSize * EndLocationLightIntensity);
	}
}

#pragma endregion ElectricalEffects
