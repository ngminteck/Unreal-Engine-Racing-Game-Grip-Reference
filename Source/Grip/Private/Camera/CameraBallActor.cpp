/**
*
* Camera ball implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Actually, it's a cube rather than a ball as otherwise it rolls for far too long
* once it hit the ground. This is simply a physics object to which we can attach
* a camera and throw it into the world and let it do whatever it does.
*
* It's used when a vehicle is destroyed and when a vehicle hits a track camera when
* in cinematic camera mode.
*
***********************************************************************************/

#include "camera/cameraballactor.h"

/**
* Construct camera ball.
***********************************************************************************/

ACameraBallActor::ACameraBallActor()
{
	CollisionShape = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionShape"));

	CollisionShape->SetCollisionEnabled(ECollisionEnabled::Type::PhysicsOnly);
	CollisionShape->SetCollisionProfileName(FName("CameraBall"));
	CollisionShape->SetSimulatePhysics(true);
	CollisionShape->SetLinearDamping(0.5f);
	CollisionShape->SetAngularDamping(0.333f);

	SetRootComponent(CollisionShape);
}

#pragma region CameraCinematics

/**
* Launch the camera into orbit.
***********************************************************************************/

void ACameraBallActor::Launch(const FVector& cameraLocation, const FRotator& cameraRotation, FVector direction, float force, bool angleDownwards) const
{
	CollisionShape->SetSimulatePhysics(true);
	CollisionShape->SetCollisionEnabled(ECollisionEnabled::Type::PhysicsOnly);

	float mass = CollisionShape->GetMass();

	CollisionShape->SetWorldLocation(cameraLocation);
	CollisionShape->SetWorldRotation(cameraRotation);
	CollisionShape->SetPhysicsLinearVelocity(direction * force);
	CollisionShape->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

	float x = FMath::FRandRange(mass * 25000.0f, mass * 50000.0f) * (((FMath::Rand() & 1) != 0) ? -1.0f : 1.0f);
	float y = FMath::FRandRange(mass * 100000.0f, mass * 200000.0f) * (((FMath::Rand() & 3) != 0) ? -0.5f : 0.25f);
	float z = FMath::FRandRange(mass * 75000.0f, mass * 100000.0f) * (((FMath::Rand() & 1) != 0) ? -1.0f : 1.0f);

	if (angleDownwards == true)
	{
		x = FMath::FRandRange(mass * 300000.0f, mass * 500000.0f) * (((FMath::Rand() & 1) != 0) ? -1.0f : 1.0f);
		y = FMath::FRandRange(mass * 100000.0f, mass * 200000.0f) * 0.2f;
		z *= 0.5f;
	}

	CollisionShape->AddTorqueInRadians(cameraRotation.RotateVector(FVector(x, 0.0f, 0.0f)));
	CollisionShape->AddTorqueInRadians(cameraRotation.RotateVector(FVector(0.0f, y, 0.0f)));
	CollisionShape->AddTorqueInRadians(cameraRotation.RotateVector(FVector(0.0f, 0.0f, z)));
}

/**
* Hibernate the camera so it doesn't affect anything in the scene.
***********************************************************************************/

void ACameraBallActor::Hibernate() const
{
	CollisionShape->SetSimulatePhysics(false);
	CollisionShape->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
}

#pragma endregion CameraCinematics
