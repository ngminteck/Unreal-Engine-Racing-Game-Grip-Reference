/**
*
* Light streak implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Light streaks to replace the broken implementation of the ribbon emitter for
* particle systems. Used primarily on vehicles to accentuate speed, but also for
* sundry other things, like missile trails.
*
***********************************************************************************/

#include "effects/lightstreakcomponent.h"
#include "vehicle/flippablevehicle.h"
#include "uobject/constructorhelpers.h"

UMaterialInterface* ULightStreakComponent::StandardStreakMaterial = nullptr;
UMaterialInterface* ULightStreakComponent::StandardFlareMaterial = nullptr;

/**
* Construct a light streak component.
***********************************************************************************/

ULightStreakComponent::ULightStreakComponent()
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;

	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> asset(TEXT("Material'/Game/Vehicles/Materials/LightStreaks/M_StandardLightStreak.M_StandardLightStreak'"));
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
* Construct a vehicle light streak component.
***********************************************************************************/

UVehicleLightStreakComponent::UVehicleLightStreakComponent()
{
	Width = 15.0f;
	MinDistance = 25.0f;
	MaxDistance = 100.0f;
	MaxAngle = 0.5f;
	MinSpeed = 300.0f;
	MaxSpeed = 550.0f;
	LifeTime = 0.15f;
	TailShrinkScale = 0.25f;
	StreakNoise = 0.666f;
	FadeStreakOnVelocityDeviation = true;
	FadeStreakOnVelocityDeviationAmount = 0.95f;
	StreakColour = FLinearColor(1.0f, 0.195f, 0.0f, 1.0f);
	Size = 150.0f;
	FadeFlareOnAngleDeviation = true;
	FadeFlareOnAngleDeviationAmount = 0.666f;
	FlareColour = FLinearColor(1.0f, 0.195f, 0.0f, 1.0f);

	SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));

	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.SetTickFunctionEnable(false);
}

#pragma region VehicleLightStreaks

namespace LightStreakParameterNames
{
	static const FName AspectRatioName("AspectRatio");
	static const FName AutoRotateFlareName("AutoRotateFlare");
	static const FName CentreShrinkFlareName("CentreShrink");
	static const FName CentreGrowFlareName("CentreGrow");
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
};

/**
* Add a new vertex joint.
***********************************************************************************/

static int32 AddStreakVertexJoint(TArray<FVector>& vertices, TArray<FVector>& normals, TArray<FVector2D>& uv0, TArray<FColor>& colours, const FVector& location, FVector direction, const FVector& horizontalAxis, float time, float alpha, int32 startIndex, int32 numJointVertices, bool cameraFacing)
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

static void SetupStreakVertexJoint(TArray<FVector>& vertices, TArray<FVector>& normals, TArray<FVector2D>& uv0, TArray<FColor>& colours, int32 startIndex, int32 numJointVertices, const FVector& location)
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

static void SetupStreakFlareQuad(TArray<FVector>& vertices, TArray<FVector>& normals, TArray<FVector2D>& uv0, const FVector& location, const FVector& direction)
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

void ULightStreakComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (Cast<UVehicleLightStreakComponent>(this) == nullptr)
	{
		PrimaryComponentTick.SetTickFunctionEnable(ManualTick == false);
	}
	else
	{
		PrimaryComponentTick.bStartWithTickEnabled = false;
		PrimaryComponentTick.SetTickFunctionEnable(false);
	}

	BaseAlpha = Alpha;
	BaseLifeTime = LifeTime;

	if (StreakEndColour.R < 0.0f)
	{
		StreakEndColour = StreakColour;
	}

	Geometry = NewObject<UProceduralMeshComponent>(this);

	if (Geometry != nullptr)
	{
#if GRIP_ENGINE_EXTENDED_MODIFICATIONS
		Geometry->SetHasCollision(false);
#endif // GRIP_ENGINE_EXTENDED_MODIFICATIONS

		Geometry->RegisterComponent();

		if (Enabled == true)
		{
			Geometry->SetCastShadow(false);

			FVector location = GetComponentTransform().GetLocation();
			FVector direction = GetComponentTransform().GetRotation().Vector();
			int32 index = 0;

			if (Flare == true)
			{
				// Create the light flare stuff.

				DynamicFlareMaterial = Geometry->CreateDynamicMaterialInstance(index, FlareMaterial);

				if (DynamicFlareMaterial != nullptr)
				{
					Geometry->SetMaterial(index, DynamicFlareMaterial);

					FlareColour.A = 1.0f;

					SetFlareColour.Setup(DynamicFlareMaterial, LightStreakParameterNames::ColourName, FlareColour);
					SetFlareAlpha.Setup(DynamicFlareMaterial, LightStreakParameterNames::AlphaName, Alpha);
					SetFlareWidth.Setup(DynamicFlareMaterial, LightStreakParameterNames::WidthName, Size * 0.5f);
					SetFlareAspectRatio.Setup(DynamicFlareMaterial, LightStreakParameterNames::AspectRatioName, AspectRatio);
					SetFlareRotate.Setup(DynamicFlareMaterial, LightStreakParameterNames::RotateFlareName, (UseFlareRotation == true) ? FMath::DegreesToRadians(GetRelativeRotation().Roll) : 0.0f);

					DynamicFlareMaterial->SetTextureParameterValue(LightStreakParameterNames::TextureName, FlareTexture);
					DynamicFlareMaterial->SetScalarParameterValue(LightStreakParameterNames::AutoRotateFlareName, (AutoRotateFlare == true) ? 1.0f : 0.0f);
					DynamicFlareMaterial->SetScalarParameterValue(LightStreakParameterNames::CentreShrinkFlareName, (CentralFlareTexture != nullptr && CentralFlareMaterial != nullptr) ? 1.0f : 0.0f);
					DynamicFlareMaterial->SetScalarParameterValue(LightStreakParameterNames::CentreGrowFlareName, 0.0f);
					DynamicFlareMaterial->SetScalarParameterValue(LightStreakParameterNames::FadeOnAngleDeviationName, (FadeFlareOnAngleDeviation == true) ? FadeFlareOnAngleDeviationAmount : 0.0f);
				}

				FlareVertices.AddDefaulted(4);
				FlareNormals.AddDefaulted(4);
				FlareUV0.AddDefaulted(4);

				FlareTriangles.Reserve(6);

				SetupStreakFlareQuad(FlareVertices, FlareNormals, FlareUV0, location, direction);

				FlareTriangles.Emplace(0);
				FlareTriangles.Emplace(1);
				FlareTriangles.Emplace(2);

				FlareTriangles.Emplace(1);
				FlareTriangles.Emplace(3);
				FlareTriangles.Emplace(2);

				Geometry->CreateMeshSection(index++, FlareVertices, FlareTriangles, FlareNormals, FlareUV0, FlareColours, FlareTangents, false);

				if (CentralFlareTexture != nullptr &&
					CentralFlareMaterial != nullptr)
				{
					DynamicCentralFlareMaterial = Geometry->CreateDynamicMaterialInstance(index, CentralFlareMaterial);

					if (DynamicCentralFlareMaterial != nullptr)
					{
						Geometry->SetMaterial(index, DynamicCentralFlareMaterial);

						FlareColour.A = 1.0f;

						SetCentreFlareColour.Setup(DynamicCentralFlareMaterial, LightStreakParameterNames::ColourName, FlareColour);
						SetCentreFlareAlpha.Setup(DynamicCentralFlareMaterial, LightStreakParameterNames::AlphaName, Alpha);
						SetCentreFlareWidth.Setup(DynamicCentralFlareMaterial, LightStreakParameterNames::WidthName, CentralSize * 0.5f);
						SetCentreFlareAspectRatio.Setup(DynamicCentralFlareMaterial, LightStreakParameterNames::AspectRatioName, CentralAspectRatio);
						SetCentreFlareRotate.Setup(DynamicCentralFlareMaterial, LightStreakParameterNames::RotateFlareName, (UseFlareRotation == true) ? FMath::DegreesToRadians(GetRelativeRotation().Roll) : 0.0f);

						DynamicCentralFlareMaterial->SetTextureParameterValue(LightStreakParameterNames::TextureName, CentralFlareTexture);
						DynamicCentralFlareMaterial->SetScalarParameterValue(LightStreakParameterNames::AutoRotateFlareName, 0.0f);
						DynamicCentralFlareMaterial->SetScalarParameterValue(LightStreakParameterNames::CentreShrinkFlareName, 0.0f);
						DynamicCentralFlareMaterial->SetScalarParameterValue(LightStreakParameterNames::CentreGrowFlareName, 1.0f);
						DynamicCentralFlareMaterial->SetScalarParameterValue(LightStreakParameterNames::FadeOnAngleDeviationName, (FadeFlareOnAngleDeviation == true) ? FadeFlareOnAngleDeviationAmount : 0.0f);
					}

					Geometry->CreateMeshSection(index++, FlareVertices, FlareTriangles, FlareNormals, FlareUV0, FlareColours, FlareTangents, false);
				}
			}

			if (Streak == true)
			{
				DynamicStreakMaterial = Geometry->CreateDynamicMaterialInstance(index, StreakMaterial);

				StreakSectionIndex = index;

				if (DynamicStreakMaterial != nullptr)
				{
					Geometry->SetMaterial(index, DynamicStreakMaterial);

					StreakColour.A = 1.0f;
					StreakEndColour.A = 1.0f;

					SetStreakColour.Setup(DynamicStreakMaterial, LightStreakParameterNames::ColourName, StreakColour);
					SetStreakEndColour.Setup(DynamicStreakMaterial, LightStreakParameterNames::EndColourName, StreakEndColour);
					SetStreakAnimationTimer.Setup(DynamicStreakMaterial, LightStreakParameterNames::AnimationTimerName, 0.0f);
					SetStreakDistanceTraveled.Setup(DynamicStreakMaterial, LightStreakParameterNames::DistanceTraveledName, DistanceTraveled);
					SetStreakLifeTime.Setup(DynamicStreakMaterial, LightStreakParameterNames::LifeTimeName, LifeTime);
					SetStreakInvLifeTime.Setup(DynamicStreakMaterial, LightStreakParameterNames::InvLifeTimeName, 1.0f / LifeTime);

					DynamicStreakMaterial->SetScalarParameterValue(LightStreakParameterNames::WidthName, Width * 0.5f);
					DynamicStreakMaterial->SetScalarParameterValue(LightStreakParameterNames::TailShrinkScaleName, TailShrinkScale);
					DynamicStreakMaterial->SetScalarParameterValue(LightStreakParameterNames::CameraFacingName, (CameraFacing == true) ? 1.0f : 0.0f);
					DynamicStreakMaterial->SetScalarParameterValue(LightStreakParameterNames::AlphaFadePowerName, AlphaFadePower);
				}

				// Create the light streak stuff.

				int32 numJoints = NumJointsPerSection;

				MaxVertices = numJoints * NumJointVertices;

				Vertices.AddDefaulted(MaxVertices);
				Normals.AddDefaulted(MaxVertices);
				UV0.AddDefaulted(MaxVertices);
				Colours.AddDefaulted(MaxVertices);

				Triangles.Reserve((numJoints - 1) * NumJointVertices * 6);

				for (int32 i = 0; i < numJoints; i++)
				{
					SetupStreakVertexJoint(Vertices, Normals, UV0, Colours, i * NumJointVertices, NumJointVertices, location);

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

				Geometry->CreateMeshSection(index, Vertices, Triangles, Normals, UV0, Colours, Tangents, false);

				SectionsDisusedAt.Emplace(0.0f);
			}
		}
	}

	DormantTimer = BaseLifeTime;
}

/**
* Switch to a new section for adding new points.
***********************************************************************************/

void ULightStreakComponent::SwitchSection(bool reset, bool empty)
{
	int32 numJoints = NumJointsPerSection;
	FVector location = GetComponentTransform().GetLocation();

	SectionsDisusedAt[SectionIndex] = Timer;

	int32 i = 0;

	for (; i < SectionsDisusedAt.Num(); i++)
	{
		if (SectionsDisusedAt[i] < 0.0f)
		{
			break;
		}
	}

	bool createSection = i == SectionsDisusedAt.Num();

	if (createSection == true)
	{
		SectionsDisusedAt.Emplace(0.0f);
	}

	int32 lastIndex = StartIndex - NumJointVertices;

	SectionIndex = i;
	StartIndex = NumJointVertices;

	Geometry->SetMaterial(SectionIndex + StreakSectionIndex, DynamicStreakMaterial);

	if (empty == true)
	{
		StartIndex = 0;

		for (i = 0; i < numJoints; i++)
		{
			SetupStreakVertexJoint(Vertices, Normals, UV0, Colours, i * NumJointVertices, NumJointVertices, location);
		}
	}
	else if (reset == true)
	{
		for (i = 0; i < numJoints; i++)
		{
			SetupStreakVertexJoint(Vertices, Normals, UV0, Colours, i * NumJointVertices, NumJointVertices, location);
		}
	}
	else
	{
		for (i = 0; i < NumJointVertices; i++)
		{
			Vertices[i] = Vertices[i + lastIndex];
			Normals[i] = Normals[i + lastIndex];
			UV0[i] = UV0[i + lastIndex];
			Colours[i] = Colours[i + lastIndex];
		}

		for (i = 1; i < numJoints; i++)
		{
			SetupStreakVertexJoint(Vertices, Normals, UV0, Colours, i * NumJointVertices, NumJointVertices, location);
		}
	}

	Geometry->CreateMeshSection(SectionIndex + StreakSectionIndex, Vertices, Triangles, Normals, UV0, Colours, Tangents, false);
}

/**
* Calculate the alpha value for a point.
***********************************************************************************/

float ULightStreakComponent::CalculateAlpha() const
{
	float alpha = Alpha;

	if (alpha > KINDA_SMALL_NUMBER)
	{
		FVector velocity = GetOwner()->GetVelocity();

		if (MaxSpeed > KINDA_SMALL_NUMBER)
		{
			float speed = FMathEx::CentimetersPerSecondToKilometersPerHour(velocity.Size());

			alpha *= FMathEx::GetRatio(speed, MinSpeed, MaxSpeed);
		}

		if (FadeStreakOnVelocityDeviation == true)
		{
			FVector velocityDirection = velocity; velocityDirection.Normalize();
			FVector direction = GetOwner()->GetActorQuat().GetAxisX();
			float fade = (FVector::DotProduct(velocityDirection, direction) * 0.5f) + 0.5f;

			if (fade < FadeStreakOnVelocityDeviationAmount)
			{
				alpha = 0.0f;
			}
			else
			{
				fade = (fade - FadeStreakOnVelocityDeviationAmount) / (1.0f - FadeStreakOnVelocityDeviationAmount);

				alpha *= fade * fade;
			}
		}
	}

	return (alpha < 0.005f) ? 0.0f : alpha;
}

/**
* Do the regular update tick.
***********************************************************************************/

void ULightStreakComponent::TickComponent(float deltaSeconds, enum ELevelTick tickType, FActorComponentTickFunction* thisTickFunction)
{
	Super::TickComponent(deltaSeconds, tickType, thisTickFunction);

	if (ManualTick == false)
	{
		Update(deltaSeconds);
	}
}

/**
* Do the regular update tick.
***********************************************************************************/

void UVehicleLightStreakComponent::TickComponent(float deltaSeconds, enum ELevelTick tickType, FActorComponentTickFunction* thisTickFunction)
{
	Super::TickComponent(deltaSeconds, tickType, thisTickFunction);

	if (Timer - LastPointAdded > LifeTime)
	{
		PrimaryComponentTick.SetTickFunctionEnable(false);
	}
}

/**
* Update the streak.
***********************************************************************************/

void ULightStreakComponent::Update(float deltaSeconds)
{
	float alpha = CalculateAlpha();
	float flareAlpha = alpha;

	if (FadeInTime > KINDA_SMALL_NUMBER)
	{
		alpha *= FMathEx::GetRatio(Timer, 0.0f, FadeInTime);
	}

	if (Enabled == true &&
		ManualConstruction == false)
	{
		if (Streak == true)
		{
			for (int32 i = 0; i < SectionsDisusedAt.Num(); i++)
			{
				if (i != SectionIndex &&
					SectionsDisusedAt[i] >= 0.0f &&
					Timer - SectionsDisusedAt[i] > LifeTime)
				{
					SectionsDisusedAt[i] = -1.0f;

					Geometry->ClearMeshSection(i + StreakSectionIndex);
				}
			}

			bool addJumpPoint = false;

			if (alpha == 0.0f)
			{
				if (NumZeroAlpha < 2)
				{
					NumZeroAlpha++;
				}
			}
			else
			{
				addJumpPoint = (NumZeroAlpha >= 2);
				NumZeroAlpha = 0;
			}

			if (NumZeroAlpha < 2)
			{
				if (addJumpPoint == true)
				{
					AddPoint(0.0f);
				}

				AddPoint(alpha);
			}
		}
	}

	if (Flare == true &&
		DynamicFlareMaterial != nullptr)
	{
		SetFlareAlpha.Set(flareAlpha);

		if (flareAlpha != 0.0f)
		{
			FlareColour.A = 1.0f;

			SetFlareWidth.Set(Size * 0.5f);
			SetFlareAspectRatio.Set(AspectRatio);
			SetFlareColour.Set(FlareColour);

			if (AutoRotateFlare == false &&
				UseFlareRotation == true)
			{
				SetFlareRotate.Set(FMath::DegreesToRadians(GetComponentRotation().Roll));
			}
		}

		if (DynamicCentralFlareMaterial != nullptr)
		{
			SetCentreFlareAlpha.Set(flareAlpha);

			if (flareAlpha != 0.0f)
			{
				FlareColour.A = 1.0f;

				SetCentreFlareWidth.Set(CentralSize * 0.5f);
				SetCentreFlareAspectRatio.Set(CentralAspectRatio);
				SetCentreFlareColour.Set(FlareColour);

				if (AutoRotateFlare == false &&
					UseFlareRotation == true)
				{
					SetCentreFlareRotate.Set(FMath::DegreesToRadians(GetComponentRotation().Roll));
				}
			}
		}
	}

	if (Streak == true &&
		DynamicStreakMaterial != nullptr)
	{
		if (Timer - LastPointAdded < LifeTime + 0.25f)
		{
			SetStreakAnimationTimer.Set(Timer);
			SetStreakDistanceTraveled.Set(DistanceTraveled);
		}

		SetStreakLifeTime.Set(LifeTime);
		SetStreakInvLifeTime.Set(1.0f / LifeTime);

		if (alpha != 0.0f)
		{
			StreakColour.A = 1.0f;
			StreakEndColour.A = 1.0f;

			SetStreakColour.Set(StreakColour);
			SetStreakEndColour.Set(StreakEndColour);
		}
	}

	if ((Timer != 0.0f) ||
		(Enabled == true && AddPoints == true))
	{
		Timer += deltaSeconds;
	}

	if (alpha == 0.0f)
	{
		DormantTimer += deltaSeconds;
	}
	else
	{
		DormantTimer = 0.0f;
	}
}

/**
* Add a new point to the streak.
***********************************************************************************/

void ULightStreakComponent::AddPoint(float alpha, bool force)
{
	if (AddPoints == true)
	{
		FVector location = GetComponentTransform().GetLocation();
		FVector direction = GetComponentTransform().GetRotation().Vector();

		if (StreakNoise != 0.0f)
		{
			alpha *= 1.0f - (FMath::Clamp(Noise(DistanceTraveled / 25.0f), 0.0f, 1.0f) * StreakNoise);
		}

		bool extend = false;
		FVector& p0 = LastLocations[0];
		FVector& p1 = LastLocations[1];
		FVector& p2 = LastLocations[2];
		float distanceTraveled = (location - p0).Size();

		if (NumPointsAdded > 0)
		{
			if (distanceTraveled > 50.0f * 100.0f)
			{
				// If we've just jumped a long way then assume the parent object has
				// teleported or something. In this case, kill the trail and start over.

				for (int32 i = 0; i < SectionsDisusedAt.Num(); i++)
				{
					SectionsDisusedAt[i] = -1.0f;

					Geometry->ClearMeshSection(i + StreakSectionIndex);
				}

				NumPointsAdded = 0;
				Timer = LastPointAdded = 0.0f;

				SwitchSection(true, false);
			}
			else
			{
				DistanceTraveled += distanceTraveled;
			}
		}

		// So now we need to add new vertices to the rendered mesh. We'll do this by forming a
		// circle at each joint.

		if (force == false &&
			Timer - LastPointAdded < (LifeTime / 10.0f) &&
			MaxDistance > KINDA_SMALL_NUMBER)
		{
			float lastHardDistance = (location - p1).Size();

			if (NumPointsAdded > 1 &&
				lastHardDistance < MaxDistance)
			{
				extend = true;

				if (NumPointsAdded > 2)
				{
					// Compare the direction of this extension vs the direction of the couplet.

					FVector d0 = p1 - p2; d0.Normalize();
					FVector d1 = location - p1; d1.Normalize();
					FVector d2 = location - p0; d2.Normalize();
					float r0 = FMathEx::DotProductToDegrees(FVector::DotProduct(d0, d1));
					float r1 = FMathEx::DotProductToDegrees(FVector::DotProduct(d0, d2));

					if ((r1 > 90.0f) ||
						(r0 > MaxAngle && lastHardDistance > MinDistance))
					{
						extend = false;
					}
				}
				else
				{
					// We need to handle the case of the first point not winding around and
					// around until we hit the maximum distance.

					if (lastHardDistance >= MinDistance)
					{
						extend = false;
					}
				}
			}
		}

		FVector pointDirection = location - p1;

		if (NumPointsAdded == 0)
		{
			pointDirection = direction;
		}

		FVector horizontalAxis = GetComponentTransform().GetUnitAxis(EAxis::Y);

		if (extend == true)
		{
			int32 lastIndex = StartIndex - NumJointVertices;

			AddStreakVertexJoint(Vertices, Normals, UV0, Colours, location, pointDirection, horizontalAxis, Timer, alpha, lastIndex, NumJointVertices, CameraFacing);

#if GRIP_ENGINE_EXTENDED_MODIFICATIONS
			Geometry->UpdateMeshSection(SectionIndex + StreakSectionIndex, Vertices, Normals, UV0, Colours, Tangents, lastIndex, numAdded);
#else
			Geometry->UpdateMeshSection(SectionIndex + StreakSectionIndex, Vertices, Normals, UV0, Colours, Tangents);
#endif
		}
		else
		{
			if (StartIndex >= MaxVertices)
			{
				// We're out of space, so start a new section.

				SwitchSection(false, false);
			}

			AddStreakVertexJoint(Vertices, Normals, UV0, Colours, location, pointDirection, horizontalAxis, Timer, alpha, StartIndex, NumJointVertices, CameraFacing);

#if GRIP_ENGINE_EXTENDED_MODIFICATIONS
			Geometry->UpdateMeshSection(SectionIndex + StreakSectionIndex, Vertices, Normals, UV0, Colours, Tangents, StartIndex, numAdded);
#else
			Geometry->UpdateMeshSection(SectionIndex + StreakSectionIndex, Vertices, Normals, UV0, Colours, Tangents);
#endif

			StartIndex += NumJointVertices;

			p2 = p1;
			p1 = location;

			NumPointsAdded++;
			LastPointAdded = Timer;
		}

		p0 = location;

		if (Flare == true)
		{
			SetupStreakFlareQuad(FlareVertices, FlareNormals, FlareUV0, location, direction);

#if GRIP_ENGINE_EXTENDED_MODIFICATIONS
			Geometry->UpdateMeshSection(0, FlareVertices, FlareNormals, FlareUV0, FlareColours, FlareTangents, 0, 4);
#else
			Geometry->UpdateMeshSection(0, FlareVertices, FlareNormals, FlareUV0, FlareColours, FlareTangents);
#endif

			if (DynamicCentralFlareMaterial != nullptr)
			{
#if GRIP_ENGINE_EXTENDED_MODIFICATIONS
				Geometry->UpdateMeshSection(1, FlareVertices, FlareNormals, FlareUV0, FlareColours, FlareTangents, 0, 4);
#else
				Geometry->UpdateMeshSection(1, FlareVertices, FlareNormals, FlareUV0, FlareColours, FlareTangents);
#endif
			}
		}
	}
}

/**
* Set the controlling global amount for alpha and lifetime.
***********************************************************************************/

void ULightStreakComponent::SetGlobalAmount(float alphaAmount, float lifeTimeAmount)
{
	float alpha = Alpha;
	float lifeTime = LifeTime;

	if (alphaAmount < KINDA_SMALL_NUMBER)
	{
		Alpha = 0.0f;
	}
	else
	{
		Alpha = alphaAmount * BaseAlpha;
	}

	if (lifeTimeAmount < KINDA_SMALL_NUMBER)
	{
		LifeTime = 0.0f;
	}
	else
	{
		LifeTime = lifeTimeAmount * BaseLifeTime;
	}

	if (alpha != Alpha ||
		lifeTime != LifeTime)
	{
		if (PrimaryComponentTick.IsTickFunctionEnabled() == false)
		{
			PrimaryComponentTick.SetTickFunctionEnable(true);
		}
	}
}

/**
* Get a noise value.
***********************************************************************************/

float ULightStreakComponent::Noise(float value) const
{
	float height = PerlinNoise.Noise1(value * 0.03125f);

	height += PerlinNoise.Noise1(value * 0.0625f) * 0.5f;
	height += PerlinNoise.Noise1(value * 0.125f) * 0.25f;
	height += PerlinNoise.Noise1(value * 0.25f) * 0.125f;

	return height + 0.625f;
}

#pragma endregion VehicleLightStreaks
