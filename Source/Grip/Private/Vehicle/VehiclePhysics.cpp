/**
*
* Vehicle physics implementation.
*
* Original author: Rob Baker.
* Current maintainer: Rob Baker.
*
* Copyright Caged Element Inc, code provided for educational purposes only.
*
* Handle all of the physics-related activity of the vehicle. Most, if not all of
* this, will be executed via the SubstepPhysics function, and so via the physics
* sub-step, so we need to be very careful what we do here. All of the vehicle
* dynamics code can be found here.
*
***********************************************************************************/

#include "vehicle/vehiclephysics.h"
#include "vehicle/flippablevehicle.h"
#include "effects/drivingsurfacecharacteristics.h"
#include "pickups/shield.h"

#if WITH_PHYSX
#include "pxcontactmodifycallback.h"
#include "runtime/engine/private/physicsengine/physxsupport.h"
#endif // WITH_PHYSX

/**
* Do the regular physics update tick, for every sub-step.
*
* This is executed just prior to apply all forces and torques to this particular
* vehicle, though not necessarily before or after any other vehicles.
*
* Once all vehicles have been sub-stepped and forces / torques applied in this way
* the simulation itself is then stepped. Hence, any transforms within the physics
* system that are used in calculations here will be from the last physics sub-step.
*
* Consider this function here called in preparation for running this physics
* sub-step iteration.
*
* As the regular vehicle actor Tick is run PostPhysics you can do any cleanup work
* at the beginning of that Tick function, knowing that you'll be reading the most
* up-to-date information from the physics system.
***********************************************************************************/

void ABaseVehicle::SubstepPhysics(float deltaSeconds, FBodyInstance* bodyInstance)
{
	if (World == nullptr)
	{
		return;
	}

#pragma region VehicleBasicForces

	if (Physics.StaticHold.Active == true)
	{
		// Lock the vehicle in position when on the start line before play begins.

		if (Wheels.BurnoutForce == 0.0f)
		{
			if (PlayGameMode == nullptr)
			{
				VehicleMesh->SetPhysicsLocationAndQuaternionSubstep(VehicleMesh->GetPhysicsLocation(), Physics.StaticHold.Rotation);
			}
			else
			{
				VehicleMesh->SetPhysicsLocationAndQuaternionSubstep(Physics.StaticHold.Location, Physics.StaticHold.Rotation);
				VehicleMesh->SetPhysicsLinearVelocitySubstep(FVector::ZeroVector);
				VehicleMesh->SetPhysicsAngularVelocityInRadiansSubstep(FVector::ZeroVector);
			}
		}
	}

#pragma endregion VehicleBasicForces

	// If the vehicle is idle-locked then clamp it by settings its location and orientation
	// and nullifying any velocity.

	if (VehicleMesh->UpdateIdleLock(true) == true)
	{
		VehicleMesh->SetPhysicsLocationAndQuaternionSubstep(VehicleMesh->GetIdleLocation(), VehicleMesh->GetIdleRotation());
		VehicleMesh->SetPhysicsLinearVelocitySubstep(FVector::ZeroVector);
		VehicleMesh->SetPhysicsAngularVelocityInRadiansSubstep(FVector::ZeroVector);
	}

	// Adjust the time passed to take into account custom time dilation for this actor.
	// This will always be 1 in this stripped version of the code, but it's important
	// that if you ever extend this to use CustomTimeDilation that we factor this in
	// right here.

	deltaSeconds *= CustomTimeDilation;
	deltaSeconds = FMath::Max(deltaSeconds, KINDA_SMALL_NUMBER);

	bool firstFrame = (Physics.Timing.TickCount <= 0);

	Physics.Timing.TickCount++;

	if (Physics.Timing.TickCount > 0)
	{
		Physics.Timing.TickSum += deltaSeconds;
	}

#pragma region VehicleAntiGravity

	HoverNoise.Tick(deltaSeconds * FMath::Lerp(0.25f, 0.75f, FMathEx::GetRatio(GetSpeedKPH(), 0.0f, 400.0f)));

#pragma endregion VehicleAntiGravity

	// Grab a few things directly from the physics body and keep them in local variables,
	// sharing them around the update where appropriate.

	const FTransform& transform = VehicleMesh->GetPhysicsTransform();
	FQuat transformQuaternion = transform.GetRotation();
	FVector xdirection = transform.GetUnitAxis(EAxis::X);
	FVector ydirection = transform.GetUnitAxis(EAxis::Y);
	FVector zdirection = transform.GetUnitAxis(EAxis::Z);

	check(xdirection.ContainsNaN() == false);
	check(ydirection.ContainsNaN() == false);
	check(zdirection.ContainsNaN() == false);

	Physics.LastPhysicsTransform = Physics.PhysicsTransform;
	Physics.PhysicsTransform = transform;
	Physics.Direction = xdirection;

#pragma region VehiclePhysicsTweaks

#if GRIP_MANAGE_MAX_ANGULAR_VELOCITY

	// Set the maximum angular velocity of the vehicle based on its current speed to help with
	// collision responses. We found this helped to smooth things out a bit, but is probably not
	// particularly necessary these days.

	float fromMAV = 250.0f;
	float toMAV = 200.0f;
	float alpha = FMath::Sin(FMathEx::GetRatio(GetSpeedKPH(), 0.0f, 250.0f) * PI * 0.5f);

	if (IsAirborne() == true)
	{
		Physics.MAVTimer -= deltaSeconds;
	}
	else
	{
		Physics.MAVTimer += deltaSeconds;
	}

	Physics.MAVTimer = FMath::Clamp(Physics.MAVTimer, 0.0f, 1.0f);

	alpha *= Physics.MAVTimer;

	// Low speed or airborne gets the most amount of max angular velocity. High speed and grounded
	// gets the least amount of max angular velocity.

	Physics.MAV = FMath::Lerp(fromMAV, toMAV, alpha);

	if (PhysicsBody != nullptr &&
		FMath::Abs(PhysicsBody->MaxAngularVelocity - Physics.MAV) > 1.0f)
	{
		PhysicsBody->SetMaxAngularVelocityInRadians(FMath::DegreesToRadians(Physics.MAV), false, true);
	}

#endif // GRIP_MANAGE_MAX_ANGULAR_VELOCITY

#pragma endregion VehiclePhysicsTweaks

	// Get the world and local velocity in meters per second of the vehicle.

	FVector lastVelocity = Physics.VelocityData.Velocity;

	Physics.VelocityData.SetVelocities(VehicleMesh->GetPhysicsLinearVelocity(), VehicleMesh->GetPhysicsAngularVelocityInDegrees(), xdirection);

	// Calculate the acceleration vector of the vehicle in meters per second.

	Physics.VelocityData.AccelerationWorldSpace = (Physics.VelocityData.Velocity - lastVelocity) / deltaSeconds;
	Physics.VelocityData.AccelerationLocalSpace = transform.InverseTransformVector(Physics.VelocityData.AccelerationWorldSpace);
	Physics.DistanceTraveled += GetSpeedMPS() * deltaSeconds;
	Physics.AntigravitySideSlip = FMath::Max(0.0f, Physics.AntigravitySideSlip - (deltaSeconds * 0.333f));
	Physics.VelocityData.AngularVelocity = transform.InverseTransformVector(VehicleMesh->GetPhysicsAngularVelocityInDegrees());
	Physics.VehicleTBoned = FMath::Max(Physics.VehicleTBoned - deltaSeconds, 0.0f);
	Physics.SpringScaleTimer = FMath::Max(Physics.SpringScaleTimer - deltaSeconds, 0.0f);
	Physics.CurrentMass = Physics.StockMass;

#pragma region VehicleGrip

	float gripScale = 1.0f;
	float steeringPosition = Control.SteeringPosition;

#pragma endregion VehicleGrip

#pragma region VehicleAntiGravity

	if (Antigravity == true)
	{
		// Perform some extended smoothing on the steering position for antigravity vehicle
		// as it they react too sharply they just feel wrong.

		float steering = steeringPosition * FMathEx::GetRatio(GetSpeedKPH() * FMath::Abs(FVector::DotProduct(GetDirection(), GetVelocityDirection())), 10.0f, 100.0f) * (1.0f - Control.BrakePosition);
		float ratio = FMathEx::GetSmoothingRatio(0.9f, deltaSeconds);
		float newPosition0 = FMathEx::GravitateToTarget(Control.AntigravitySteeringPosition, steering, deltaSeconds * 1.5f);
		float newPosition1 = FMath::Lerp(steering, Control.AntigravitySteeringPosition, ratio);

		Control.AntigravitySteeringPosition = (FMath::Abs(newPosition1 - steering) < FMath::Abs(newPosition0 - steering)) ? newPosition0 : newPosition1;

#pragma region VehicleTeleport

		float lag = FMath::Lerp(0.9f, GRIP_ANTIGRAVITY_LAGGY_STEERING, FMath::Min(1.0f, (VehicleClock - Teleportation.LastVehicleClock) * 0.5f));

#pragma endregion VehicleTeleport

		steeringPosition = FMath::Lerp(steeringPosition, Control.AntigravitySteeringPosition, lag);
	}
	else
	{
		Control.AntigravitySteeringPosition = steeringPosition;
	}

#pragma endregion VehicleAntiGravity

#pragma region VehicleContactSensors

	// Update the springs and record how many wheels are in contact with surfaces.
	// This is the core processing of contact sensors and most the work required for
	// them resides in UpdateContactSensors.

	Wheels.NumWheelsInContact = UpdateContactSensors(deltaSeconds, transform, xdirection, ydirection, zdirection);
	Wheels.FrontAxlePosition = transform.TransformPosition(FVector(Wheels.FrontAxleOffset, 0.0f, 0.0f));
	Wheels.RearAxlePosition = transform.TransformPosition(FVector(Wheels.RearAxleOffset, 0.0f, 0.0f));

#pragma endregion VehicleContactSensors

#pragma region VehiclePhysicsTweaks

#if GRIP_VARIABLE_MASS_AND_INERTIA_TENSOR

	if (Physics.InertiaTensorScaleTimer > 0.0f)
	{
		if (Physics.ContactData.Grounded == true &&
			Physics.ContactData.ModeTime > 0.2f &&
			Physics.InertiaTensorScaleTimer > 1.0f)
		{
			Physics.InertiaTensorScaleTimer = 1.0f;
		}

		Physics.InertiaTensorScaleTimer = FMath::Max(Physics.InertiaTensorScaleTimer - deltaSeconds, 0.0f);

		Physics.CurrentMass = FMath::Lerp(7000.0f, Physics.StockMass, (Physics.InertiaTensorScaleTimer < 1.0f) ? 1.0f - Physics.InertiaTensorScaleTimer : 0.0f);
	}
	else
	{
		Physics.CurrentMass = Physics.StockMass;
	}

	// Drop the mass of the vehicle if we're in a hard, centrifugal corner.

	float currentMass = Physics.StockMass;
	float pitchRate = FMath::Abs(GetAngularVelocity().Y);

	if (pitchRate > 20.0f &&
		IsGrounded() == true)
	{
		float compression = 0.0f;

		for (FVehicleWheel& wheel : Wheels.Wheels)
		{
			compression += FMath::Min(2.0f, wheel.GetActiveSensor().GetNormalizedCompression());
		}

		compression /= Wheels.Wheels.Num();
		compression /= 2.0f;

		float ratio = FMath::Min((pitchRate - 20.0f) / 10.0f, 1.0f) * compression;

		currentMass = FMath::Lerp(Physics.StockMass, 5000.0f, ratio);
	}

	currentMass = FMath::Lerp(currentMass, Physics.CompressedMass, FMathEx::GetSmoothingRatio(0.9f, deltaSeconds));

	Physics.CompressedMass = FMathEx::GravitateToTarget(Physics.CompressedMass, currentMass, Physics.StockMass * deltaSeconds);
	Physics.CurrentMass = FMath::Min(Physics.CurrentMass, Physics.CompressedMass);

#if GRIP_ENGINE_PHYSICS_MODIFIED
	if (PhysicsBody != nullptr &&
		PhysicsBody->bOverrideInertiaTensor == true)
	{
		VehicleMesh->SetPhysicsMassAndInertiaTensorSubstep(Physics.CurrentMass, PhysicsBody->InertiaTensor);
	}
#endif // GRIP_ENGINE_PHYSICS_MODIFIED

#endif // GRIP_VARIABLE_MASS_AND_INERTIA_TENSOR

#pragma endregion VehiclePhysicsTweaks

#pragma region PickupTurbo

	ApplyTurboRaiseForce(deltaSeconds, transform);

	if (Propulsion.RaiseFrontScale != 0.0f)
	{
		// Reduce the grip scale when using a charged turbo to encourage players to
		// only use this pickup when on a straight and enhance the sense of turbo power.

		gripScale = FMath::Min(gripScale, 1.0f - (Propulsion.RaiseFrontScale * 0.5f));
	}

	gripScale *= Propulsion.BoostGripScale;

#pragma endregion PickupTurbo

#pragma region VehicleAntiGravity

	// Update the air power of the antigravity vehicles.

	if (PlayGameMode != nullptr &&
		PlayGameMode->PastGameSequenceStart() == true)
	{
		if (Propulsion.AirPowerCut > 0.0f)
		{
			Propulsion.AirPowerCut = FMath::Max(0.0f, Propulsion.AirPowerCut - deltaSeconds);
		}

		if (Propulsion.AirPowerCut == 0.0f)
		{
			Propulsion.AirPower = FMath::Min(Propulsion.AirPower + deltaSeconds * 0.5f, 1.0f);
		}
	}

#pragma endregion VehicleAntiGravity

#pragma region VehicleBasicForces

	// Handle the engine power. Only apply the power if at least two wheels on the ground. This is
	// fairly arbitrary right now, as we're simulating a jet engine at the back of the vehicle, which
	// could be active even if the wheels were in the air, but just "feels" wrong when playing the game.

	// The first thing we need to do is calculate how much thrust the driver is wanting, between -1 and +1.
	// We enter that into Propulsion.JetEngineThrottle.

	{
		Propulsion.JetEngineThrottle = Control.GetBrakedThrottle();
	}

	// Now calculate the piston engine thrust, though this is a conceptual value which we only use to
	// simulate the effects of a piston engine, it doesn't increase the speed of the vehicle over and
	// above the jet engine throttle.

	Propulsion.PistonEngineThrottle = (Wheels.NumWheelsInContact >= 2) ? Propulsion.JetEngineThrottle : 0.0f;

#pragma endregion VehicleBasicForces

#pragma region VehicleControls

	float brakePosition = AutoBrakePosition(xdirection);

#pragma endregion VehicleControls

#pragma region VehicleGrip

	// Calculate the front and rear axle positions, as well as whether all of their wheels are in
	// contact with the driving surface.

	Wheels.RearAxleDown = true;
	Wheels.FrontAxleDown = true;
	Wheels.RearWheelDown = false;
	Wheels.FrontWheelDown = false;

	float rearCompression = 0.0f;
	float frontCompression = 0.0f;

	for (FVehicleWheel& wheel : Wheels.Wheels)
	{
		if (wheel.HasFrontPlacement() == true)
		{
			Wheels.FrontAxleDown &= wheel.GetActiveSensor().IsInEffect();
			Wheels.FrontWheelDown |= wheel.GetActiveSensor().IsInEffect();

			if (wheel.GetActiveSensor().IsInContact() == true)
			{
				frontCompression = FMath::Max(frontCompression, wheel.GetActiveSensor().GetNormalizedCompression());
			}
		}
		else if (wheel.HasRearPlacement() == true)
		{
			Wheels.RearAxleDown &= wheel.GetActiveSensor().IsInEffect();
			Wheels.RearWheelDown |= wheel.GetActiveSensor().IsInEffect();

			if (wheel.GetActiveSensor().IsInContact() == true)
			{
				rearCompression = FMath::Max(rearCompression, wheel.GetActiveSensor().GetNormalizedCompression());
			}
		}
	}

#pragma endregion VehicleGrip

#pragma region SpeedPads

	// Manage the speed pad boosts.

	Physics.SpeedPadBoost = 0.0f;

	for (int32 i = 0; i < Propulsion.SpeedPadBoosts.Num(); i++)
	{
		FSpeedpadVehicleBoost& boost = Propulsion.SpeedPadBoosts[i];

		Physics.SpeedPadBoost += boost.Amount;

		boost.Timer += deltaSeconds;

		if (boost.Timer >= boost.Duration)
		{
			Propulsion.SpeedPadBoosts.RemoveAt(i--);
		}
	}

#pragma endregion SpeedPads

#pragma region VehicleDrifting

	UpdateDriftingPhysics(deltaSeconds, steeringPosition, xdirection);

#pragma endregion VehicleDrifting

#pragma region VehicleBasicForces

	// General force scale, so we can easily modify all applied forces if desired.

	float forceScale = 50.0f;

	// Determine the location in world space of all the wheels, along with their velocity.

	for (FVehicleWheel& wheel : Wheels.Wheels)
	{
		// We grab the standard wheel location here, which keeps the application of grip
		// consistent across different vehicles, so that we can tune it more easily when
		// we want it to be different for each vehicle model.

		wheel.Location = GetStandardWheelLocation(wheel, transform);
		wheel.Velocity = VehicleMesh->GetPhysicsLinearVelocityAtPoint(wheel.Location);
		wheel.LateralForceVector = FVector::ZeroVector;
	}

#pragma endregion VehicleBasicForces

#pragma region VehicleBidirectionalTraction

	// Traction control, switching between biasing the grip more to the front / rear depending on the
	// orientation of the vehicle compared to its velocity vector. This is to fix the reversing problem
	// we have previously where the front-end would skid around uncontrollably. Now the vehicle is as
	// controllable going backwards and it is forwards.

	float smoothedSteeringBias = 1.0f;

	if (IsPracticallyGrounded() == true)
	{
		// The bias is computed from the facing direction versus the velocity vector. So +1 for
		// pointing in the direction we're moving, and -1 if pointing in the opposite direction.

		smoothedSteeringBias = FVector::DotProduct(GetFacingDirection(), GetVelocityOrFacingDirection());

		// Reverse driving only goes up to 200 kph max under normal circumstances, so we only need the
		// benefit of this bias under that speed. Above that speed we transition the bias back to fully
		// forwards for normal driving, the reason being it helps us to recover from crashes more quickly
		// as the only reason you'd be facing backwards and traveling that fast is if you'd just crashed.
		// And if you've just crashed, strong rear grip helps the vehicle naturally recover more quickly.

		float forwardRatio = FMathEx::GetRatio(GetSpeedKPH(), 200.0f, 300.0f);

		smoothedSteeringBias = FMath::Lerp(smoothedSteeringBias, 1.0f, forwardRatio);
	}

	// Smooth the steering bias, specifically to allow the front wheels time to kick back around after
	// a bad landing. If we transition the bias too quickly, then we lose that game-play advantage.

	float smoothingRatio = FMathEx::GetSmoothingRatio(0.9f, deltaSeconds);

	Physics.SmoothedSteeringBias = FMath::Lerp(smoothedSteeringBias, Physics.SmoothedSteeringBias, smoothingRatio);

	// Apply a power curve to the bias so that it moves quickly between the two extremes.

	Physics.SteeringBias = FMathEx::NegativePow(Physics.SmoothedSteeringBias, 0.25f);

	// We lose all bias when under 25 kph, and we have full bias above 50 kph - speeds and subsequent
	// control effects derived from play-testing.

	Physics.SteeringBias = FMath::Lerp(0.0f, Physics.SteeringBias, FMathEx::GetRatio(GetSpeedKPH(), 25.0f, 50.0f));

#pragma endregion VehicleBidirectionalTraction

#pragma region VehiclePhysicsTweaks

#if GRIP_NORMALIZED_WEIGHT_ON_WHEEL

	float averageWeight = 0.0f;
	float maximumWeightFront = 0.0f;
	float maximumWeightRear = 0.0f;

	if (GetNumWheels() > 0)
	{
		for (FVehicleWheel& wheel : Wheels.Wheels)
		{
			float weight = GetWeightActingOnWheel(wheel);

			averageWeight += GetWeightActingOnWheel(wheel);

			if (wheel.HasFrontPlacement() == true)
			{
				maximumWeightFront = FMath::Max(maximumWeightFront, weight);
			}
			else if (wheel.HasRearPlacement() == true)
			{
				maximumWeightRear = FMath::Max(maximumWeightRear, weight);
			}
		}

		averageWeight /= GetNumWheels();
	}

#endif // GRIP_NORMALIZED_WEIGHT_ON_WHEEL

#pragma endregion VehiclePhysicsTweaks

#pragma region VehicleAntiGravity

	float forwardRatio = 1.0f;
	float scaleAntigravity = 1.0f;

	if (Antigravity == true)
	{
		// scaleAntigravity simply means less grip the more sideways we're moving as we want to have
		// great grip when traveling forwards but not have the vehicle solid on the ground when collided
		// against when hit from the side - it is floating after all with no apparent friction force
		// to hold it in place.

		// Do some processing for antigravity vehicles.

		UpdateAntigravityForwardsAndScale(deltaSeconds, brakePosition, forwardRatio, scaleAntigravity);
	}

#pragma endregion VehicleAntiGravity

#pragma region VehicleGrip

	// Now, let's deal with all of the wheel forces.

	float stablisingGripVsSpeed = TireFrictionModel->RearLateralGripVsSpeed.GetRichCurve()->Eval(GetSpeedKPH());

	for (FVehicleWheel& wheel : Wheels.Wheels)
	{
		float surfaceFriction = 1.0f;
		FVector wheelForce = FVector::ZeroVector;
		FQuat wheelQuaternion = wheel.GetSteeringTransform(transformQuaternion, Antigravity);

		if (DrivingSurfaceCharacteristics != nullptr)
		{
			EGameSurface surfaceType = wheel.GetActiveSensor().GetGameSurface();

			surfaceFriction = DrivingSurfaceCharacteristics->GetTireFriction(surfaceType);
		}

		// Calculate the rotations per second of the wheel. This also calculates its longitudinal
		// slip which we'll use for braking shortly.

		CalculateWheelRotationRate(wheel, GetVelocityOrFacingDirection(), Physics.VelocityData.Speed, brakePosition, deltaSeconds);

		if (wheel.Velocity.SizeSquared() > 0.01f &&
			wheel.GetActiveSensor().IsInContact() == true)
		{
			// Apply friction / traction if the wheel is in contact with a surface.

#pragma region VehiclePhysicsTweaks

#if GRIP_NORMALIZED_WEIGHT_ON_WHEEL
			float weightOnWheel = averageWeight;

			if (Antigravity == false)
			{
				// Dirty hack to stop people whining about loss of control. This ensures
				// that we have symmetrical grip for each wheel on a particular axle at
				// least, if not the vehicle as a whole.

				if (wheel.HasFrontPlacement() == true)
				{
					if (maximumWeightFront < KINDA_SMALL_NUMBER)
					{
						weightOnWheel = 0.0f;
					}
				}
				else if (wheel.HasRearPlacement() == true)
				{
					if (maximumWeightRear < KINDA_SMALL_NUMBER)
					{
						weightOnWheel = 0.0f;
					}
				}
			}
#else // GRIP_NORMALIZED_WEIGHT_ON_WHEEL
			float weightOnWheel = GetWeightActingOnWheel(wheel);
#endif // GRIP_NORMALIZED_WEIGHT_ON_WHEEL

#pragma endregion VehiclePhysicsTweaks

			// Handle the longitudinal braking.

			bool fakeBrake = false;
			float longitudinalSlip = wheel.LongitudinalSlip;
			float longitudinalGripCoefficient = TireFrictionModel->LongitudinalGripCoefficient;

#pragma region VehicleLowSpeedHandling

			if (GetSpeedKPH() < 50.0f &&
				FMath::Abs(Control.ThrottleInput) < 0.01f)
			{
				// Apply the brake if we're going pretty slow and are not attempting to use the throttle.
				// This brings the vehicle to a nice, natural halt and avoids very low speed handling issues.

				fakeBrake = true;

				// Limit the longitudinal slip, the more horizontally flat we are the more slip we'll receive.

				float angleScale = FMath::Pow(FMath::Abs(zdirection.Z), 2.0f);

				if (brakePosition > 0.0f)
				{
					longitudinalSlip = FMath::Max(FMath::Abs(longitudinalSlip), 0.01f * angleScale) * FMathEx::UnitSign(longitudinalSlip);
				}
				else
				{
					longitudinalSlip = 0.01f * angleScale;
				}
			}

#pragma endregion VehicleLowSpeedHandling

			Physics.CentralizeGrip = false;

#pragma region VehiclePhysicsTweaks

#if GRIP_NORMALIZE_GRIP_ON_LANDING

			if (Antigravity == false)
			{
				// No need to centralize grip on antigravity vehicles as all springs will share the same
				// value with regard to grip ratio.

				if (wheel.HasFrontPlacement() == true &&
					frontCompression > rearCompression + 0.333f &&
					(IsAirborne() == true || Physics.ContactData.ModeTime < 1.0f))
				{
					// Centralize the grip if this is a front wheel and we're not getting as much rear wheel contact.

					Physics.CentralizeGrip = true;
				}
			}

#endif // GRIP_NORMALIZE_GRIP_ON_LANDING

#pragma endregion VehiclePhysicsTweaks

			if (wheel.HasCenterPlacement() == false)
			{
				if ((longitudinalSlip > 0.0f) &&
					(fakeBrake == true || brakePosition > 0.0f))
				{
					float longitudinalGrip = CalculateLongitudinalGripRatioForSlip(longitudinalSlip);

					// Counter the velocity vector of the wheel with the longitudinal grip.
					// Never impart movement on the local Z axis of the vehicle though, we
					// only want horizontal forces.

					FVector wheelVelocity = transform.InverseTransformVector(wheel.Velocity);

					wheelVelocity.Z = 0.0f;
					wheelVelocity.Normalize();
					wheelVelocity = transform.TransformVectorNoScale(wheelVelocity);

					FVector longitudinalForce = wheelVelocity * -longitudinalGripCoefficient * longitudinalGrip * weightOnWheel * forceScale * surfaceFriction * BrakingCoefficient * gripScale;

					if (Physics.CentralizeGrip == true)
					{
						VehicleMesh->AddForceSubstep(longitudinalForce);
					}
					else
					{
						wheelForce += longitudinalForce;
					}
				}
			}

			// Now let's look at the lateral grip, stopping the tires from sliding sideways, which also
			// handles the steering forces as a useful by-product.

			float lateralGripScale = 1.0f;
			float lateralForce = 0.0f;
			FVector wyNormalized = wheelQuaternion.GetAxisY();

			// Non-rolling wheels should have no lateral friction at all - it makes no sense to be able to
			// steer when the wheels are not turning.

			float absWheelRPS = FMath::Abs(wheel.RPS);
			float rpsReduction = 1.0f - FMathEx::GetRatio(absWheelRPS, 0.0f, 0.005f);

			// We have to kill lateral grip when the wheels aren't rotating, so rpsReduction is 1
			// for full reduction and 0 for no reduction. Otherwise, you'd never be able to do a handbrake turn.

#pragma region VehicleLowSpeedHandling

			if (rpsReduction > SMALL_NUMBER)
			{
				// At exceedingly low speeds, drop the steering vector in favor of the overall vehicle
				// side vector which is less susceptible to mathematical inaccuracy.

				wyNormalized = FMath::Lerp(wyNormalized, GetSideDirection(), rpsReduction);
				wyNormalized.Normalize();
			}

#pragma endregion VehicleLowSpeedHandling

			float lateralSlip = 0.0f;
			FVector lateralAxis = wyNormalized;
			FVector wvNormalized = GetHorizontalVelocity(wheel, transform);

#pragma region VehicleDrifting

			if (wheel.HasRearPlacement() == true)
			{
				// For rear wheels we modify the lateral axis for lateral grip if we're drifting,
				// so that it matches the drift angle we're achieving.

				FRotator driftRotation = FRotator(0.0f, Physics.Drifting.RearDriftAngle * ((IsFlipped() == true) ? -1.0f : 1.0f), 0.0f);

				lateralAxis = wheelQuaternion.RotateVector(driftRotation.RotateVector(FVector::RightVector));
			}

#pragma endregion VehicleDrifting

			if (wvNormalized.Normalize(0.01f) == true)
			{
				lateralSlip = FVector::DotProduct(wvNormalized, lateralAxis);
			}

			if (TireFrictionModel->Model == ETireFrictionModel::Arcade)
			{
				// Invert the lateral friction as we want to oppose the side-slip force.

				lateralForce = -LateralFriction(lateralGripScale, lateralSlip, wheel) * scaleAntigravity;

#pragma region VehicleDrifting

				// The more you're drifting, the more grip boost you get. The reason for this is we introduced
				// drifting as a way of taking corners more quickly by decreasing the turning circle with
				// increased grip.

				float scaleGrip = 1.0f + (GetDriftingRatio() * TireFrictionModel->GripBoostWhenDrifting);

#pragma endregion VehicleDrifting

#pragma region VehicleAntiGravity

				// This smooths out the regaining of traction when you've been sideways. Don't be tempted to
				// remove this or amalgamate with scaleAntigravity, it's important for smoothing things out as
				// Physics.AntigravityLateralGrip itself is smoothed over time.

				float antigravityGrip = FMath::Lerp(1.0f, Physics.AntigravityLateralGrip, FMathEx::GetRatio(GetSpeedKPH(), 50.0f, 150.0f) * forwardRatio);

				scaleGrip *= FMath::Lerp(antigravityGrip, 1.0f, brakePosition);

#pragma endregion VehicleAntiGravity

				// Take the wheel side direction, then multiply by lateral force and the computed scales.

				float lateralForceStrength = wheel.LateralForceStrength;

				wheel.LateralForceStrength = lateralForce * scaleGrip;
				wheel.LateralForceVector = lateralAxis * wheel.LateralForceStrength;

#pragma region VehicleAntiGravity

				if (Antigravity == true &&
					forwardRatio < 1.0f - KINDA_SMALL_NUMBER &&
					FMath::Abs(steeringPosition) > KINDA_SMALL_NUMBER)
				{
					// We're going to do some thrust vectoring from the steering here rather than using
					// classic tire grip. That's because antigravity vehicles can't really do anything
					// once they enter a sideways state in order to correct themselves, so we give them
					// this additional steering ability here.

					float vectoredStrength = 5.0f * steeringPosition * ((IsFlipped() == true) ? -1.0f : 1.0f);
					float yawRate = GetAngularVelocity().Z * ((IsFlipped() == true) ? -1.0f : 1.0f);

					if (FMathEx::UnitSign(yawRate) == FMathEx::UnitSign(steeringPosition))
					{
						vectoredStrength *= 1.0f - FMath::Min(1.0f, FMath::Abs(yawRate) / (FMath::Abs(steeringPosition) * 25.0f));
					}

					FVector vectoredForce = vectoredStrength * GetSideDirection();

					if (wheel.BoneOffset.X < 0.0f)
					{
						vectoredForce *= -1.0f;
					}

					float vectoredRatio = (1.0f - forwardRatio) * FMath::Abs(steeringPosition) * FMathEx::GetRatio(GetSpeedKPH(), 5.0f, 10.0f);

					wheel.LateralForceStrength += vectoredStrength * vectoredRatio;
					wheel.LateralForceVector += vectoredForce * vectoredRatio;
				}

#pragma endregion VehicleAntiGravity

				wheel.TwoFrameLateralForceStrength = (lateralForceStrength + wheel.LateralForceStrength) * 0.5f;

				// We want to lose lateral grip when the wheels lock up - it makes no sense to be
				// able to steer when the wheels are not turning.

				wheel.LateralForceVector *= 1.0f - rpsReduction;

				check(wheel.LateralForceVector.ContainsNaN() == false);

				// The general lateral force calculation is now generally complete, and all that is left is to
				// apply some playability hacks.
			}

#pragma region VehicleDrifting

			if (FMath::Abs(lateralForce) > 100.0f)
			{
				// If we have considerable lateral force being applied then induce the skid audio.

				float volume = FMath::Min((FMath::Abs(lateralForce) - 100.0f) / 25.0f, 1.0f);

				volume *= FMath::Min(FMath::Max(GetSpeedKPH() - 150.0f, 0.0f) / 100.0f, 1.0f);

				Wheels.SkidAudioVolumeTarget = FMath::Max(Wheels.SkidAudioVolumeTarget, volume);
			}

#pragma endregion VehicleDrifting

			if (TireFrictionModel->Model == ETireFrictionModel::Arcade)
			{

#pragma region VehicleCatchup

				if (GetRaceState().DragCatchupRatio < 0.0f)
				{
					bool scaleGrip = UsingTrailingCatchup;

					if (AI.BotVehicle == true)
					{
#if !GRIP_BOT_TRAILING_GRIPINESS
						scaleGrip = false;
#endif // !GRIP_BOT_TRAILING_GRIPINESS
					}
					else
					{
#if !GRIP_HOM_TRAILING_GRIPINESS
						scaleGrip = false;
#endif // !GRIP_HOM_TRAILING_GRIPINESS
					}

					if (scaleGrip == true)
					{
						// Handle the tightening of grip if we're trying to catchup and have been given
						// an artificial speed boost to do so.

						wheel.LateralForceVector *= 1.0f + (((AI.BotVehicle == true) ? CatchupCharacteristics.GripScaleAtRearNonHumans : CatchupCharacteristics.GripScaleAtRearHumans) * -GetRaceState().DragCatchupRatio);

						check(FMath::IsNaN(GetRaceState().DragCatchupRatio) == false);
						check(wheel.LateralForceVector.ContainsNaN() == false);
					}
				}

#if GRIP_BOT_LEADING_SLIPPINESS

				if (GetRaceState().DragCatchupRatio > 0.0f)
				{
					if (AI.BotVehicle == true)
					{
						// Handle the loosening of grip if we're a leading bot vehicle and want human
						// players to catchup (by the bot vehicles not making corners due to lost grip).

						wheel.LateralForceVector *= 1.0f - CatchupCharacteristics.GripScaleAtFrontNonHumans * GetRaceState().DragCatchupRatio;

						check(FMath::IsNaN(GetRaceState().DragCatchupRatio) == false);
						check(wheel.LateralForceVector.ContainsNaN() == false);
					}
				}

#endif // GRIP_BOT_LEADING_SLIPPINESS

#pragma endregion VehicleCatchup

#pragma region VehicleBidirectionalTraction

				if (absWheelRPS > KINDA_SMALL_NUMBER)
				{
					float stablisingGrip = 1.0f;

					check(FMath::IsNaN(stablisingGripVsSpeed) == false);
					check(FMath::IsNaN(Physics.SteeringBias) == false);

					// The code biases grip towards the "rear" wheels depending on the direction of travel.
					// This gives us good steering response no matter what the vehicle is doing and enables
					// us to steer well when reversing where previously it wasn't possible.

					if (wheel.HasRearPlacement() == true)
					{
						// This is a rear wheel.

						if (Physics.SteeringBias > 0.0f)
						{
							// We're facing the direction we're traveling in.

							stablisingGrip = FMath::Lerp(1.0f, stablisingGripVsSpeed, Physics.SteeringBias);
						}
					}
					else
					{
						// This is a front wheel.

						if (Physics.SteeringBias < 0.0f)
						{
							// We're not facing the direction we're traveling in.

							stablisingGrip = FMath::Lerp(1.0f, stablisingGripVsSpeed, -Physics.SteeringBias);
						}
					}

#pragma region AIVehicleControl

					if (AI.BotDriver == true)
					{
						check(FMath::IsNaN(AI.FishtailRecovery) == false);

						if ((AI.FishtailRecovery != 0.0f) &&
							(surfaceFriction > KINDA_SMALL_NUMBER) &&
							((wheel.HasRearPlacement() == true && Physics.SteeringBias > 0.0f) ||
							(wheel.HasRearPlacement() == false && Physics.SteeringBias < 0.0f)))
						{
							float baseSurfaceFriction = 1.0f;

							if (DrivingSurfaceCharacteristics != nullptr)
							{
								baseSurfaceFriction = DrivingSurfaceCharacteristics->GetTireFriction(EGameSurface::Asphalt);
							}

							float scale = baseSurfaceFriction / surfaceFriction;

							check(FMath::IsNaN(baseSurfaceFriction) == false);
							check(FMath::IsNaN(surfaceFriction) == false);
							check(FMath::IsNaN(AI.FishtailRecovery) == false);

							stablisingGrip *= 1.0f + (AI.FishtailRecovery * 0.333f * scale);
						}
					}

#pragma endregion AIVehicleControl

					// Usually stablisingGrip applies more grip on the end opposite the driving
					// direction to provide solid control - the rear end when driving forwards
					// for example.

					// Handbrake turn, reducing that additional grip if we actively want the
					// vehicle to spin around.

					if ((wheel.HasRearPlacement() == true && Physics.BrakingSteeringBias > 0.0f) ||
						(wheel.HasRearPlacement() == false && Physics.BrakingSteeringBias < 0.0f))
					{
						// We have different ratios for handbrake turns depending on whether this
						// is a front or rear wheel, to make J turns more achievable.

						float handbrakeGripRatio = (wheel.HasRearPlacement() == true) ? HandBrakeRearGripRatio : HandBrakeRearGripRatio * 0.25f;

						stablisingGrip = FMath::Lerp(stablisingGrip, handbrakeGripRatio, brakePosition * FMath::Abs(steeringPosition));
					}

					wheel.LateralForceVector *= stablisingGrip;

					check(FMath::IsNaN(stablisingGrip) == false);
					check(wheel.LateralForceVector.ContainsNaN() == false);
				}

#pragma endregion VehicleBidirectionalTraction

				// Now finally apply the lateral force.

				check(wheel.LateralForceVector.ContainsNaN() == false);
				check(FMath::IsNaN(weightOnWheel) == false);
				check(FMath::IsNaN(forceScale) == false);
				check(FMath::IsNaN(surfaceFriction) == false);

				FVector lateralForceVector = wheel.LateralForceVector * weightOnWheel * forceScale * surfaceFriction * GripCoefficient * gripScale;

#pragma region VehicleAntiGravity

				if (Antigravity == true &&
					RaceState.RaceTime > 2.0f)
				{
					// Lose grip when we've lost power, but not on the start line.

					lateralForceVector *= 0.25f + (Propulsion.AirPower * 0.75f);
				}

#pragma endregion VehicleAntiGravity

				if (wheel.HasCenterPlacement() == false)
				{

#pragma region VehiclePhysicsTweaks

					if (Physics.CentralizeGrip == true)
					{
						// By doing this, the remove forces that will spin the vehicle.

						VehicleMesh->AddForceSubstep(lateralForceVector);
					}
					else

#pragma endregion VehiclePhysicsTweaks

					{

#pragma region VehicleLowSpeedHandling

						// At slow speeds and high angles from the horizontal we account for the correct offset of grip application more.
						// This has us falling off walls realistically rather than having mental grip. We don't do this all of the time as
						// you might expect because the vehicle constantly falls all over itself with the weight transfer.

						float angleScale = 1.0f - FMath::Abs(GetLaunchDirection().Z);
						float speedScale = 1.0f - FMathEx::GetRatio(GetSpeedKPH(), 150.0f, 250.0f);
						float offsetRatio = speedScale * angleScale;
						FVector surfacePosition = wheel.Location + wheel.Radius * GetSurfaceDirection();

						check(FMath::IsNaN(angleScale) == false);
						check(FMath::IsNaN(speedScale) == false);
						check(FMath::IsNaN(offsetRatio) == false);

						if (offsetRatio > 0.01f)
						{
							VehicleMesh->AddForceAtLocationSubstep(lateralForceVector, FMath::Lerp(wheel.Location, surfacePosition, offsetRatio));
						}
						else
						{
							wheelForce += lateralForceVector;
						}

#pragma endregion VehicleLowSpeedHandling

					}
				}
			}
		}

		if (wheelForce != FVector::ZeroVector)
		{
			VehicleMesh->AddForceAtLocationSubstep(wheelForce, wheel.Location);
		}
	}

#pragma endregion VehicleGrip

#pragma region VehicleBasicForces

	FVector location = VehicleMesh->GetPhysicsLocation();
	FVector movement = (firstFrame == true || Physics.ResetLastLocation == true) ? Physics.VelocityData.Velocity * deltaSeconds : location - Physics.LastLocation;

	Physics.LastLocation = location;
	Physics.ResetLastLocation = false;

#pragma region VehicleAirControl

	// Handle the air control.

	bool wasAirborneControlActive = Control.AirborneControlActive;

	Control.AirborneControlActive = (Propulsion.ThrottleOffWhileAirborne == true) &&
		Physics.ContactData.Airborne == true &&
		Physics.ContactData.ModeTime > 0.25f;

	if (Control.AirborneControlActive == true &&
		wasAirborneControlActive == false)
	{
		// At the beginning of using air control, determine the pitch direction
		// so it's the most intuitive direction.

		FMinimalViewInfo viewInfo;

		Camera->GetCameraViewNoPostProcessing(0.0f, viewInfo);

		FVector cameraUp = viewInfo.Rotation.Quaternion().GetUpVector();
		FVector vehicleUp = GetActorRotation().Quaternion().GetUpVector();

		Control.AirborneControlScale = ((FVector::DotProduct(vehicleUp, cameraUp) < 0.0f) ? -1.0f : 1.0f);
	}

	bool airborneControlActive = Control.AirborneControlActive;
	float airborneRollPosition = Control.AirborneRollPosition;
	float airbornePitchPosition = Control.AirbornePitchPosition;

	if (airborneControlActive == true)
	{
		// Use air control if the vehicle has been airborne and the player has indicated so
		// with the controller.

		float secondsToMaxDegrees = 1.0f;
		float maxDegreesPerSecond = 120.0f;
		FVector airborneForce = FVector::ZeroVector;
		const FVector& angularVelocity = Physics.VelocityData.AngularVelocity;

		if (FMath::Abs(airborneRollPosition) > 0.25f)
		{
			// Roll the vehicle if not already rotating too fast.

			if ((airborneRollPosition > 0.0f && angularVelocity.X > -maxDegreesPerSecond) ||
				(airborneRollPosition < 0.0f && angularVelocity.X < maxDegreesPerSecond))
			{
				airborneForce.X -= (maxDegreesPerSecond / secondsToMaxDegrees) * airborneRollPosition * AirborneThrustersPower * deltaSeconds;
			}
		}

		if (FMath::Abs(airbornePitchPosition) > 0.25f)
		{
			// Pitch the vehicle if not already rotating too fast.

			if ((airbornePitchPosition < 0.0f && angularVelocity.Y < maxDegreesPerSecond) ||
				(airbornePitchPosition > 0.0f && angularVelocity.Y > -maxDegreesPerSecond))
			{
				airborneForce.Y += (maxDegreesPerSecond / secondsToMaxDegrees) * airbornePitchPosition * AirborneThrustersPower * deltaSeconds * Control.AirborneControlScale;
			}
		}

		if (airborneForce != FVector::ZeroVector)
		{
			// If we have some air control to impart, then convert it from vehicle space
			// into world space and then apply it to the vehicle. This isn't actually an
			// impulse despite the name, as we're passing it as a velocity change.

			VehicleMesh->AddAngularImpulseInDegreesSubstep(transform.TransformVector(airborneForce), NAME_None, true);
		}
	}

	if (Control.AirborneControlActive == false)
	{
		Control.AirborneRollPosition = 0.0f;
		Control.AirbornePitchPosition = 0.0f;
	}

#pragma endregion VehicleAirControl

	// Apply the engine power now, to the appropriate axle depending on which direction we
	// are heading in.

	if (IsPowerAvailable() == true &&
		Propulsion.JetEngineThrottle != 0.0f)
	{
		FVector jetForce = FVector::ZeroVector;
		float jetPower = GetJetEnginePower(Wheels.NumWheelsInContact, xdirection);

		// Propulsion.JetEngineThrottle represents the strength of the jet throttle between -1 and +1
		// negative being with the reverse control.

		if (IsAirborne() == true)
		{
			// When in the air, only apply the jet-thrust down the horizontal plane of the velocity direction,
			// proportional to the direction the vehicle is facing compared to that velocity direction. We
			// don't want people flying sideways, or up and down, under jet thrust.

			FVector direction = Physics.VelocityData.VelocityDirection;

			direction.Z = 0.0f;

			if (direction.Normalize(0.001f) == true)
			{
				float scale = FVector::DotProduct(xdirection, Physics.VelocityData.VelocityDirection);

				scale *= FMathEx::GetRatio(GetSpeedKPH(), 100.0f, 200.0f);

				if (scale > KINDA_SMALL_NUMBER)
				{
					// So add this controlled airborne jet thrust as a force here.

					VehicleMesh->AddForceSubstep(direction * Propulsion.JetEngineThrottle * jetPower * forceScale * scale);
				}
			}
		}
		else
		{
			// If on the ground then apply the jet engine in the direction the vehicle is facing,
			// or opposite to that if the vehicle reverse throttling.

			jetForce += xdirection * Propulsion.JetEngineThrottle * jetPower * forceScale;
		}

#pragma region SpeedPads

		// Apply any speed boost from speed pads here.

		if (Physics.SpeedPadBoost > KINDA_SMALL_NUMBER)
		{
			jetForce += xdirection * Physics.SpeedPadBoost * jetPower * forceScale;
		}

#pragma endregion SpeedPads

		if (jetForce != FVector::ZeroVector)
		{
			// Add in the normal jet thrust as a force here.

			FVector position = (Control.ThrottleInput >= 0.0f) ? Wheels.FrontAxlePosition : Wheels.RearAxlePosition;

			VehicleMesh->AddForceAtLocationSubstep(jetForce, position);
		}
	}

	// Handle the drag force.

	VehicleMesh->AddForceSubstep(GetDragForce() * forceScale);

	// Handle the rolling resistance.

	VehicleMesh->AddForceSubstep(GetRollingResistanceForce(xdirection) * forceScale);

	// Handle the down force.

	VehicleMesh->AddForceSubstep(GetDownForce());

	// Finally handle the gravity scaling.

	if (FMath::Abs(GravityScale - 1.0f) > KINDA_SMALL_NUMBER)
	{
		VehicleMesh->AddForceSubstep(GetGravityForce(false));
	}

	// Update the tracking of vehicle movement over time. This information is used to make
	// decisions about vehicle behavior at other points in the code base. VelocityPitchList
	// for example is used to determine whether we should try to mitigate bouncing as part
	// of the physics tweaks we apply to make the game more playable.

	float physicsClock = Physics.Timing.TickSum;

	if (Physics.Timing.TickCount > 0)
	{
		FVector direction = GetVelocityOrFacingDirection();

		if (FVector::DotProduct(direction, xdirection) < 0.0f)
		{
			direction *= -1.0f;
		}

		FRotator directionChange = Physics.LastPhysicsTransform.InverseTransformVector(xdirection).Rotation();
		float yawChange = directionChange.Yaw;

		yawChange /= Physics.Timing.LastSubstepDeltaSeconds; yawChange /= 100.0f;

		Physics.PitchChangeList.AddValue(Physics.Timing.TickSum, directionChange.Pitch);
		Physics.VelocityPitchList.AddValue(Physics.Timing.TickSum, direction.Rotation().Pitch);
		Physics.VelocityList.AddValue(Physics.Timing.TickSum, Physics.VelocityData.Velocity);

		if (IsPracticallyGrounded() == true &&
			movement.Size() > 10.0f * 100.0f * deltaSeconds)
		{
			FVector difference = movement; difference.Normalize(); difference -= Physics.VelocityData.LastVelocityDirection;
			FVector offset = Physics.VelocityData.LastVelocityDirection.ToOrientationQuat().UnrotateVector(difference);

			Physics.AngularPitchList.AddValue(Physics.Timing.TickSum, -Physics.VelocityData.AngularVelocity.Y);
			Physics.DirectionVsVelocityList.AddValue(Physics.Timing.TickSum, offset);

#if UE_BUILD_DEBUG
			FVector predicted = (Physics.LastVelocityDirection.ToOrientationQuat().RotateVector(Physics.DirectionVsVelocityList.LastValue()) + Physics.LastVelocityDirection);
			predicted.Normalize();
			predicted *= movement.Size();
			ensureMsgf((predicted - movement).IsNearlyZero(0.01f) == true, TEXT("Something wrong with vehicle path prediction"));
#endif // UE_BUILD_DEBUG
		}
		else
		{
			Physics.AngularPitchList.AddValue(Physics.Timing.TickSum, 0.0f);
			Physics.DirectionVsVelocityList.AddValue(Physics.Timing.TickSum, FVector::ForwardVector);
		}
	}

	Physics.VelocityData.LastVelocityDirection = Physics.VelocityData.VelocityDirection;

#pragma region VehiclePhysicsTweaks

#if GRIP_ANTI_SKYWARD_LAUNCH

	// So the aim is to stop vehicles bouncing off terrain and heading skyward then taking
	// an age to get down on the ground. How do we do that?

	// First, record the velocity vector over time, more specifically the Z value.

	// Second, monitor rate of angular change in that vector over the last, say, a third of a second.

	// Third, if that rate of change, in the upward direction, is too high, then mitigate
	// the velocity by applying a reverse force to slow the vehicle down. While we're in contact
	// with the ground this will help slow the launch before it has begun if we're in contact
	// for long enough.

	// Lastly, apply mitigation for a short moment following a high rate of change to allow it
	// time to work.

	if (EnableBounceImpactMitigation == true ||
		EnableVerticalImpactMitigation == true)
	{
		float time = 0.333f;
		float since = physicsClock - time;
		float speed = GetSpeedKPH();
		float mitigationTime = 1.0f;

		// Pitch will be positive for upward direction and negative for downward direction.

		if (speed > 50.0f)
		{
			int32 numValues = Physics.VelocityPitchList.GetNumValues();

			if (numValues > 2)
			{
				float bounceRatio = 0.0f;
				float maxPitchAngle = 0.0f;
				float maxPitchDifference = 0.0f;

				for (int32 i = numValues - 1; i > 0; i--)
				{
					auto& i0 = Physics.VelocityPitchList[i];

					if (i0.Time >= since)
					{
						auto& i1 = Physics.VelocityPitchList[i - 1];
						float t0 = i0.Time;
						float t1 = i1.Time;
						float t2 = t0 - 2.0f;
						float p0 = i0.Value;
						float p1 = i1.Value;

						float difference = 0.0f;

						if (!FMath::IsNearlyEqual(t0, t1))
						{
							difference = FMathEx::GetUnsignedDegreesDifference(p0, p1, false) / (t0 - t1);
						}

						// difference is now the change in pitch in degrees per second.

						float scale = 1.0f - ((physicsClock - i0.Time) / time);

						// Tail off the difference with regard to time.

						difference *= scale;

						if (maxPitchDifference < difference)
						{
							bounceRatio = 0.0f;
							maxPitchAngle = (p0 + p1) * 0.5f;
							maxPitchDifference = difference;

							int32 numAirborneSamples = 0;
							int32 numAirborneValues = Physics.ContactData.AirborneList.GetNumValues();

							for (int32 j = numAirborneValues - 1; j >= 0; j--)
							{
								auto& j0 = Physics.ContactData.AirborneList[j];
								float jtime = j0.Time;

								if (jtime < t2)
								{
									break;
								}

								if (jtime < t0)
								{
									numAirborneSamples++;
									bounceRatio += j0.Value;
								}
							}

							if (numAirborneSamples > 0)
							{
								bounceRatio /= numAirborneSamples;
							}
						}
					}
					else
					{
						break;
					}
				}

				float scale = 0.0f;
				bool bouncing = (bounceRatio > 0.5f);

				if (bouncing == true &&
					EnableBounceImpactMitigation == true)
				{
					scale = BounceImpactMitigation.GetRichCurve()->Eval(maxPitchDifference);

					// Inhibit bounce less the more vertical the pitch is.

					scale *= 1.0f - FMath::Min(FMath::Max(FMath::Abs(maxPitchAngle) - 45.0f, 0.0f) / 45.0f, 1.0f);
				}
				else if (EnableVerticalImpactMitigation == true)
				{
					scale = VerticalImpactMitigation.GetRichCurve()->Eval(maxPitchDifference);
				}

				float amount = GetSpeedMPS() * scale;
				float currently = (Physics.VelocityPitchMitigationTime * Physics.VelocityPitchMitigationAmount) + (amount * 0.15f);

				if (amount > currently)
				{
					for (FVehicleWheel& wheel : Wheels.Wheels)
					{
						if (wheel.GetActiveSensor().HasNearestContactPoint(wheel.Velocity, 0.0f) == true)
						{
							if (PlayGameMode != nullptr &&
								PlayGameMode->ShouldActorLimitCollisionResponse(wheel.GetActiveSensor().GetHitResult().Actor.Get()) == true)
							{
								Physics.VelocityPitchMitigationTime = mitigationTime;
								Physics.VelocityPitchMitigationAmount = amount;
								Physics.VelocityPitchMitigationRatio = (bouncing == true) ? BounceImpactMitigationRatio : VerticalImpactMitigationRatio;

								break;
							}
						}
					}
				}
			}
		}

#pragma region VehicleLaunch

		if ((GetVehicleClock() - LastLaunchTime) > mitigationTime)

#pragma endregion VehicleLaunch

		{
			float scale = Physics.VelocityPitchMitigationTime * Physics.VelocityPitchMitigationAmount;

			scale *= FMathEx::GetRatio(speed - 50.0f, 0.0f, 200.0f);

			Physics.VelocityPitchMitigationForce = scale;

			if (scale > KINDA_SMALL_NUMBER)
			{
				scale *= -1.0f;

				// Scale the braking response to be less in the world horizontal plane than the vertical plane.

				FVector response(Physics.VelocityPitchMitigationRatio * scale, Physics.VelocityPitchMitigationRatio * scale, scale);

				VehicleMesh->IdleUnlock();

				VehicleMesh->AddForceSubstep(FMathEx::MetersToCentimeters(Physics.VelocityData.VelocityDirection * response) * forceScale * Physics.CurrentMass * 0.01f);
			}
		}

		Physics.VelocityPitchMitigationTime = FMath::Max(Physics.VelocityPitchMitigationTime - (deltaSeconds / time), 0.0f);
	}

#endif // GRIP_ANTI_SKYWARD_LAUNCH

#pragma endregion VehiclePhysicsTweaks

	Physics.Timing.LastSubstepDeltaSeconds = deltaSeconds;

#pragma endregion VehicleBasicForces

}

#pragma region VehicleContactSensors

/**
* Update the contact sensors.
***********************************************************************************/

int32 ABaseVehicle::UpdateContactSensors(float deltaSeconds, const FTransform& transform, const FVector& xdirection, const FVector& ydirection, const FVector& zdirection)
{
	static FName noSurface("None");

	Wheels.SurfaceName = noSurface;

#pragma region VehicleAudio

	SkiddingSound = nullptr;

#pragma endregion VehicleAudio

	float physicsClock = Physics.Timing.TickSum;
	int32 numWheels = Wheels.Wheels.Num();
	int32 numUpContact = 0;
	int32 numDownContact = 0;
	bool bounceCompression = false;

	if (numWheels != 0)
	{
		// This is an optimization to halve the number of sweeps performed of the car was
		// completely on the ground last frame and still is again this frame.

		int32 halfTheWheels = numWheels >> 1;
		int32 numAxles = halfTheWheels;
		bool estimate =
			((/*We're in the air and have no wheels within 2m of the ground*/Physics.ContactData.Airborne == true && IsPracticallyGrounded(200.0f, true) == false) ||
				(/*We're grounded and have been for a moment*/Physics.ContactData.Grounded == true && Physics.ContactData.GroundedList.GetAbsMeanValue(physicsClock - 0.333f) > 1.0f - KINDA_SMALL_NUMBER));

		if (PlayGameMode == nullptr)
		{
			estimate = false;
		}

#if GRIP_CYCLE_SUSPENSION == GRIP_CYCLE_SUSPENSION_NONE
#define SHOULD_ESTIMATE false
#endif

#if GRIP_CYCLE_SUSPENSION == GRIP_CYCLE_SUSPENSION_BY_AXLE
		// Do this for axle per frame. This assumes two wheels per axle, added in axle order
		// in the WheelAssignments array.

#define SHOULD_ESTIMATE (((wheelIndex++ >> 1) % numAxles) != (Physics.Timing.TickCount % numAxles))
#endif

		if (Physics.ContactData.Grounded == true)
		{
			// If the vehicle is grounded then we can do less work, by ticking the contact sensors
			// in a specific way, the in-contact set first and the alternate set second - the
			// alternate set performing a very minimal Tick where possible.

			bool allInContact = true;
			int32 wheelIndex = 0;

			for (FVehicleWheel& wheel : Wheels.Wheels)
			{
				FVehicleContactSensor& sensor = wheel.Sensors[Wheels.GroundedSensorSet];
				FVector springTop = GetWheelBoneLocation(wheel, transform, true);

				sensor.Tick(deltaSeconds, World, transform, springTop, zdirection, true, estimate == true && SHOULD_ESTIMATE, IsFlippable());

				allInContact &= sensor.IsInContact();
			}

			wheelIndex = 0;

			for (FVehicleWheel& wheel : Wheels.Wheels)
			{
				FVehicleContactSensor& sensor = wheel.Sensors[Wheels.GroundedSensorSet ^ 1];
				FVector springTop = GetWheelBoneLocation(wheel, transform, true);

				sensor.Tick(deltaSeconds, World, transform, springTop, zdirection, (allInContact == false), estimate == true && SHOULD_ESTIMATE, IsFlippable());
			}
		}
		else
		{
			// If we're not properly grounded then tick the contact sensors in the less optimal way.

			int32 wheelIndex = 0;

			for (FVehicleWheel& wheel : Wheels.Wheels)
			{
				for (FVehicleContactSensor& sensor : wheel.Sensors)
				{
					FVector springTop = GetWheelBoneLocation(wheel, transform, true);

					sensor.Tick(deltaSeconds, World, transform, springTop, zdirection, true, estimate == true && SHOULD_ESTIMATE, IsFlippable());
				}
			}
		}

		// Determine the compression characteristics of the contact sensors, or how hard
		// the suspension is working.

		bool surfaceSet = false;
		bool hardCompression = false;
		float rearCompression = 0.0f;
		float frontCompression = 0.0f;

		for (FVehicleWheel& wheel : Wheels.Wheels)
		{
			for (FVehicleContactSensor& sensor : wheel.Sensors)
			{
				bool compressedHard = sensor.WasCompressedHard();

				hardCompression |= compressedHard;

				if (compressedHard == true &&
					Physics.ContactData.AirborneList.GetMeanValue(physicsClock - 2.0f) > 0.75f)
				{
					sensor.SpawnCompressionEffect();
				}

				if (surfaceSet == false &&
					sensor.IsInContact() == true)
				{
					// On the first contact for this frame and this vehicle, determine the surface and
					// skidding sound.

					surfaceSet = true;

					EGameSurface surfaceType = sensor.GetGameSurface();

					Wheels.SurfaceName = GetNameFromSurfaceType(surfaceType);

#pragma region VehicleAudio

					SkiddingSound = DrivingSurfaceCharacteristics->GetSkiddingSound(surfaceType);

#pragma endregion VehicleAudio

				}

				if (wheel.HasRearPlacement() == true)
				{
					rearCompression = FMath::Max(rearCompression, sensor.GetNormalizedCompression());
				}
				else
				{
					frontCompression = FMath::Max(frontCompression, sensor.GetNormalizedCompression());
				}
			}
		}

		if (hardCompression == true)
		{
			if (Wheels.HardCompressionTime == 0.0f)
			{
				Wheels.HardCompression = true;
			}

			Wheels.HardCompressionTime = 0.2f;
		}

		Wheels.HardCompressionTime = FMath::Max(Wheels.HardCompressionTime - deltaSeconds, 0.0f);

#pragma region VehiclePhysicsTweaks

#if GRIP_VEHICLE_SUSPENSION_BOUNCE_MITIGATION

		if (PhysicsBody != nullptr)
		{
			// Deal with suspension force application to stop it inadvertently doing bad things to
			// the vehicle.

			float rearScale = 1.0f;
			float frontScale = 1.0f;

			// First check if just the front or rear axle is in contact with the ground. If so,
			// determine the pitch velocity and reduce the suspension force if the velocity is
			// already high enough too avoid applying suspension (which would just exaggerate it and
			// make the vehicle bounce badly).

			if (rearCompression >= 1.25f &&
				frontCompression < 1.25f)
			{
				// Rear axle is coming down hard while the front axle isn't.

				if (Physics.ContactData.AirborneList.GetMeanValue(physicsClock - 1.0f) > 0.75f)
				{
					float scale = 1.0f - (FMath::Min(rearCompression - frontCompression, 0.25f) * 4.0f);

					rearScale = FMath::Min(rearScale, scale);
				}
			}

			if (rearCompression <= 1.25f &&
				frontCompression >= 1.25f)
			{
				// Front axle is coming down hard while the rear axle isn't.

				if (Physics.ContactData.AirborneList.GetMeanValue(physicsClock - 1.0f) > 0.75f)
				{
					// We're mostly airborne for the last second.

					float pitchRate = Physics.VelocityData.AngularVelocity.Y * (IsFlipped() ? 1.0f : -1.0f);

					if (pitchRate > 0.0f)
					{
						// Back-end is flying up compared to the ground.

						float scale = 1.0f - (FMath::Min(frontCompression - rearCompression, 0.25f) * 4.0f);

						frontScale = FMath::Min(frontScale, scale);
					}
				}
			}

			if (Antigravity == false)
			{
				// Scale the suspension forces.

				for (FVehicleWheel& wheel : Wheels.Wheels)
				{
					for (FVehicleContactSensor& sensor : wheel.Sensors)
					{
						if (wheel.HasRearPlacement() == true)
						{
							sensor.ForceToApply *= rearScale;
						}
						else if (wheel.HasFrontPlacement() == true)
						{
							sensor.ForceToApply *= frontScale;
						}
					}
				}
			}
		}

#endif // GRIP_VEHICLE_SUSPENSION_BOUNCE_MITIGATION

#pragma endregion VehiclePhysicsTweaks

		int32 numUpNear = 0;
		int32 numDownNear = 0;
		float contactSeconds = 1.5f;

		FVector upNormal = FVector::ZeroVector;
		FVector downNormal = FVector::ZeroVector;
		FVector upLocation = FVector::ZeroVector;
		FVector downLocation = FVector::ZeroVector;

		// Determine which wheels are in contact with or are close to the ground.

		for (FVehicleWheel& wheel : Wheels.Wheels)
		{
			// Identify the contact sensor to be used for the wheel.

			// Sensors 0 = up, 1 = down (opposite if vehicle flipped)

			if (wheel.Sensors[0].IsInEffect() == true)
			{
				numUpContact++;

				upNormal += wheel.Sensors[0].GetNearestContactNormal();
				upLocation = wheel.Sensors[0].GetNearestContactPoint();
			}
			else if (wheel.Sensors[0].HasNearestContactPoint(wheel.Velocity, 0.0f) == true)
			{
				numUpNear++;

				upNormal += wheel.Sensors[0].GetNearestContactNormal();
				upLocation = wheel.Sensors[0].GetNearestContactPoint();
			}

			if (wheel.Sensors[1].IsInEffect() == true)
			{
				numDownContact++;

				downNormal += wheel.Sensors[1].GetNearestContactNormal();
				downLocation = wheel.Sensors[1].GetNearestContactPoint();
			}
			else if (wheel.Sensors[1].HasNearestContactPoint(wheel.Velocity, 0.0f))
			{
				numDownNear++;

				downNormal += wheel.Sensors[1].GetNearestContactNormal();
				downLocation = wheel.Sensors[1].GetNearestContactPoint();
			}
		}

		if (numUpContact + numUpNear > 0)
		{
			upNormal *= 1.0f / (numUpContact + numUpNear);
			upNormal.Normalize();
		}

		if (numDownContact + numDownNear > 0)
		{
			downNormal *= 1.0f / (numDownContact + numDownNear);
			downNormal.Normalize();
		}

		Physics.ContactData.WasAirborne = Physics.ContactData.Airborne;
		Physics.ContactData.Airborne = (numUpContact + numDownContact == 0);
		Physics.ContactData.Grounded = (numUpContact == numWheels || numDownContact == numWheels);

		// Manage the amount of time the car has been falling back to earth.
		// (We're officially falling if we've been falling back to earth for more than 0.666 seconds)

		if (Physics.ContactData.Airborne == true &&
			Physics.VelocityData.Velocity.Z < 0.0f)
		{
			Physics.ContactData.FallingTime += deltaSeconds;
		}
		else
		{
			Physics.ContactData.FallingTime = 0.0f;
		}

#pragma region VehiclePhysicsTweaks

		// Loop around for each sensor set, determining the maximum force and force vector
		// for all of the wheel in that sensor set.

		check(Wheels.Wheels.Num() == 0 || GRIP_NUM_ELEMENTS(Wheels.Wheels[0].Sensors) == 2);

		float maxForce[2] = { 0.0f, 0.0f };
		FVector maxForceVector[2] = { FVector::ZeroVector, FVector::ZeroVector };

		for (FVehicleWheel& wheel : Wheels.Wheels)
		{
			int32 setIndex = 0;

			for (FVehicleContactSensor& sensor : wheel.Sensors)
			{
				float force = sensor.ForceToApply.Size();

				if (maxForce[setIndex] < force)
				{
					maxForce[setIndex] = force;
					maxForceVector[setIndex] = sensor.ForceToApply;
				}

				setIndex++;
			}
		}

#if GRIP_VEHICLE_SUSPENSION_BOUNCE_NORMALIZE
		if (Antigravity == false &&
			Physics.SpringScaleTimer != 0.0f &&
			Physics.ContactData.Grounded == true)
		{
			// If the vehicle has all its wheels on the ground, then set the maximum force
			// we observed for each sensor set, to all of the wheels in that set. We only do this
			// when the vehicle was recently airborne, so effectively has just landed.
			// SpringScaleTimer is 1.0 when airborne, and decrements down to 0.0 over time
			// when not, so we get an interpolation between maximum forces and normal forces.

			// This sounds very dumb, but in fact, it's there to balance the suspension forces
			// if we've just made a landing so that we don't get any corners just popping up
			// and rotating the vehicle around. It's not at all realistic, but it works. It
			// feels dirty, but it made for a better game in GRIP.

			for (FVehicleWheel& wheel : Wheels.Wheels)
			{
				int32 setIndex = 0;

				for (FVehicleContactSensor& sensor : wheel.Sensors)
				{
					float forceApplied = FMath::Lerp(sensor.ForceToApply.Size(), maxForce[setIndex], Physics.SpringScaleTimer);

					sensor.ForceToApply.Normalize();
					sensor.ForceToApply *= forceApplied;

					setIndex++;
				}
			}
		}
#endif // GRIP_VEHICLE_SUSPENSION_BOUNCE_NORMALIZE

#pragma endregion VehiclePhysicsTweaks

#pragma region VehicleAntiGravity

		else if (Antigravity == true)
		{
			// Try to balance the forces when not in contact so that we don't get the back-end
			// pushing you over if the front-end has no contact, like coming off a ramp for example.

			for (FVehicleWheel& wheel : Wheels.Wheels)
			{
				int32 setIndex = 0;

				for (FVehicleContactSensor& sensor : wheel.Sensors)
				{
					if (maxForce[setIndex] > 0.0f &&
						sensor.GetNonContactTime() > 0.0f)
					{
						sensor.ForceToApply = FMath::Lerp(maxForceVector[setIndex], sensor.ForceToApply, FMathEx::GetRatio(sensor.GetNonContactTime() - 0.1f, 0.0f, 0.5f));
					}

					setIndex++;
				}
			}
		}

#pragma endregion VehicleAntiGravity

		// Determine which is the currently grounded sensor set, if any.

		if (numUpContact == numWheels)
		{
			Wheels.GroundedSensorSet = 0;
		}
		else if (numDownContact == numWheels)
		{
			Wheels.GroundedSensorSet = 1;
		}

		// Manage the time spent in airborne / non-airborne states.

		bool mostlyGrounded = (numUpContact > halfTheWheels || numDownContact > halfTheWheels);

		Physics.ContactData.GroundedList.AddValue(physicsClock, (mostlyGrounded == true) ? 1.0f : 0.0f);
		Physics.ContactData.AirborneList.AddValue(physicsClock, (Physics.ContactData.Airborne == true) ? 1.0f : 0.0f);

		if (Physics.ContactData.WasAirborne != Physics.ContactData.Airborne)
		{
			Physics.ContactData.LastModeTime = Physics.ContactData.ModeTime;
			Physics.ContactData.ModeTime = 0.0f;
		}
		else
		{
			Physics.ContactData.ModeTime += deltaSeconds;
		}

		if (Physics.ContactData.Grounded == true &&
			Physics.ContactData.ModeTime > 2.0f)
		{
			Physics.ContactData.RespawnLanded = true;
		}

		// Now try to figure out what's going on with the vehicle, mostly about whether it's flipped
		// or not. We put a lot of work into this because primarily, this flipped state affects the
		// spring arm and therefore the camera, and so we want no erratic changes in the flipped state
		// and try to determine it as best we can, only changing it when we're sure we need to.

		float d0 = 0.0f;
		float d1 = 0.0f;
		float dp0 = 0.0f;
		float dp1 = 0.0f;
		FVector i0 = FVector::ZeroVector;
		FVector i1 = FVector::ZeroVector;
		FVector location = transform.GetTranslation();
		bool upContactImminent = numUpContact > 0;
		bool downContactImminent = numDownContact > 0;
		FVector rayDirection = Physics.VelocityData.VelocityDirection;
		float cornerAngle = FMathEx::DotProductToDegrees(FVector::DotProduct(upNormal, downNormal));

		if (upContactImminent == false &&
			numUpNear + numUpContact != 0)
		{
			upContactImminent = (FVector::DotProduct(rayDirection, upNormal) < 0.0f && FMathEx::RayIntersectsPlane(location, rayDirection, upLocation, upNormal, i0) == true);

			if (upContactImminent == true)
			{
				d0 = (i0 - location).Size();

				if (d0 / Physics.VelocityData.Speed > contactSeconds)
				{
					upContactImminent = false;
				}
			}
		}

		if (downContactImminent == false &&
			numDownNear + numDownContact != 0)
		{
			downContactImminent = (FVector::DotProduct(rayDirection, downNormal) < 0.0f && FMathEx::RayIntersectsPlane(location, rayDirection, downLocation, downNormal, i1) == true);

			if (downContactImminent == true)
			{
				d1 = (i1 - location).Size();

				if (d1 / Physics.VelocityData.Speed > contactSeconds)
				{
					downContactImminent = false;
				}
			}
		}

		if (numUpNear + numUpContact != 0)
		{
			FVector p0 = FVector::PointPlaneProject(location, upLocation, upNormal);

			dp0 = (p0 - location).Size();
		}

		if (numDownNear + numDownContact != 0)
		{
			FVector p1 = FVector::PointPlaneProject(location, downLocation, downNormal);

			dp1 = (p1 - location).Size();
		}

		// Managed the detection of flip direction.

		bool flipped = Wheels.SoftFlipped;

		Wheels.SurfacesVincinal = true;

		if (IsFlippable() == false)
		{
			// If the vehicle isn't flippable then always indicate not flipped.

			Wheels.FlipDetection = 0;
			Wheels.SoftFlipped = false;
		}
		else if ((numUpContact != 0 && numDownContact == 0) ||
			(numUpContact == 0 && numDownContact != 0))
		{
			// We've a definite surface in contact with nothing on the other side. Simple case.

			Wheels.FlipDetection = 0;
			Wheels.SoftFlipped = numUpContact != 0;
		}
		else if (numUpContact != 0 && numDownContact != 0 && cornerAngle < 120.0f)
		{
			// We have contacts on both sides so we need to discriminate.

			// We're jammed in a corner.

			Wheels.FlipDetection = 2;

			// Figure out which surface we're most oriented towards and pick that if it's clear.

			if (dp0 < dp1 * 0.666f)
			{
				Wheels.SoftFlipped = true;
			}
			else if (dp1 < dp0 * 0.666f)
			{
				Wheels.SoftFlipped = false;
			}
		}
		else if (upContactImminent != downContactImminent)
		{
			// We've a surface coming into contact with nothing imminent on the other side. Another relatively simple case.

			Wheels.FlipDetection = 1;

			if (upContactImminent == true &&
				Wheels.SoftFlipped == false &&
				(dp0 < dp1 * 0.666f || dp1 == 0.0f))
			{
				Wheels.SoftFlipped = true;
			}
			else if (downContactImminent == true &&
				Wheels.SoftFlipped == true &&
				(dp1 < dp0 * 0.666f || dp0 == 0.0f))
			{
				Wheels.SoftFlipped = false;
			}
		}
		else if (IsFalling() == true)
		{
			Wheels.FlipDetection = 4;
			Wheels.SoftFlipped = (zdirection.Z < 0.0f);
			Wheels.SurfacesVincinal = false;
		}
		else
		{
			Wheels.FlipDetection = 5;
			Wheels.SurfacesVincinal = false;
		}

		if (flipped != Wheels.SoftFlipped)
		{
			Wheels.FlipTimer = 1.0f;
		}

		// NOTE: Only now is the current contact sensor set known, but we still need to update each wheel
		// so that they also know before using GetActiveSensor().

		Wheels.DetectedSurfaces = false;
		Wheels.FlipTimer = FMath::Max(Wheels.FlipTimer - (deltaSeconds * 4.0f), 0.0f);

		bounceCompression = (PlayGameMode != nullptr && PlayGameMode->PastGameSequenceStart());

#pragma region VehicleAntiGravity

		float minAntigravityCompression = 100.0f;

#pragma endregion VehicleAntiGravity

		for (FVehicleWheel& wheel : Wheels.Wheels)
		{
			wheel.SensorIndex = (Wheels.SoftFlipped == true) ? 0 : 1;

			if (wheel.GetActiveSensor().HasNearestContactPoint(wheel.Velocity, 0.0f) == true)
			{
				Wheels.DetectedSurfaces = true;
			}

#pragma region VehicleAntiGravity

			if (Antigravity == true)
			{
				minAntigravityCompression = FMath::Min(minAntigravityCompression, wheel.GetActiveSensor().GetAntigravityNormalizedCompression());

				if (wheel.GetActiveSensor().GetNormalizedCompression() < 1.33f)
				{
					bounceCompression = false;
				}
			}
			else

#pragma endregion VehicleAntiGravity

			{
				if (wheel.GetActiveSensor().GetNormalizedCompression() < 1.0f)
				{
					bounceCompression = false;
				}
			}
		}

#pragma region VehicleAntiGravity

		bool blocked = false;
		float steering = Control.SteeringPosition;
		float offsetY = (IsFlipped() == true) ? -1.0f : 1.0f;

#pragma endregion VehicleAntiGravity

		for (FVehicleWheel& wheel : Wheels.Wheels)
		{

#pragma region VehicleAntiGravity

			if (Antigravity == true)
			{
				wheel.GetActiveSensor().SetUnifiedAntigravityNormalizedCompression(minAntigravityCompression);
			}

#pragma endregion VehicleAntiGravity

			// Finally, actually apply the suspension forces to the vehicle for each wheel.

			for (FVehicleContactSensor& sensor : wheel.Sensors)
			{
				if (sensor.IsInContact() == true)
				{
					FVector forcesLocation = GetSuspensionForcesLocation(wheel, transform, deltaSeconds);

					sensor.ForceApplied = FMath::Max(sensor.ForceApplied, sensor.ForceToApply.Size());

					sensor.ApplyForce(forcesLocation);
				}
			}

			// Calculate how long a wheel has either been in contact or not in contact with a
			// driving surface through its ModeTime.

			bool wasInContact = wheel.IsInContact;

			wheel.IsInContact = wheel.GetActiveSensor().IsInContact();

			if (wasInContact != wheel.IsInContact)
			{
				wheel.ModeTime = 0.0f;
			}
			else
			{
				wheel.ModeTime += deltaSeconds;
			}

#pragma region VehicleAntiGravity

			if (Antigravity == true)
			{
				// Calculate the outboard offset for the contact sensor, allowing it to adjust
				// its tilt direction towards the outboard direction in order to transition the
				// vehicle to a different surface - a very sharp transition from a wall to a
				// floor for example. If we didn't do this, then the vehicle would get stuck on
				// the wall until the scenery geometry changed naturally to a more amenable
				// angle between them.

				float& offset = wheel.GetActiveSensor().OutboardOffset;

				wheel.GetActiveSensor().TiltScale = 1.0f;

				if ((wheel.StandardBoneOffset.Y * offsetY < 0.0f) &&
					((AI.LastHardCollisionBlockage & VehicleBlockedLeft) != 0))
				{
					// We're blocked on the left side with a non-vehicle contact.

					blocked = true;

					if (steering < -GRIP_STEERING_PURPOSEFUL)
					{
						// We're steering into the side blockage.

						offset += deltaSeconds * 2.0f;
					}
				}
				else if ((wheel.StandardBoneOffset.Y * offsetY > 0.0f) &&
					((AI.LastHardCollisionBlockage & VehicleBlockedRight) != 0))
				{
					// We're blocked on the right side with a non-vehicle contact.

					blocked = true;

					if (steering > GRIP_STEERING_PURPOSEFUL)
					{
						// We're steering into the side blockage.

						offset += deltaSeconds * 2.0f;
					}
				}
				else
				{
					offset -= deltaSeconds * 4.0f;
				}

				offset = FMath::Clamp(offset, 0.0f, 1.0f);
			}

#pragma endregion VehicleAntiGravity

		}

#pragma region VehicleAntiGravity

		if (blocked == true &&
			Antigravity == true)
		{
			// If we're blocked on this wheel then kill the tilt scale.

			for (FVehicleWheel& wheel : Wheels.Wheels)
			{
				for (FVehicleContactSensor& sensor : wheel.Sensors)
				{
					sensor.TiltScale = 0.0f;
				}
			}
		}

#pragma endregion VehicleAntiGravity

		if (Wheels.HardFlipped != Wheels.SoftFlipped &&
			IsPracticallyGrounded() == true)
		{
			Wheels.HardFlipped = Wheels.SoftFlipped;
		}

		Wheels.SurfacesVincinal &= IsPracticallyGrounded(250.0f, true);

#pragma region VehiclePhysicsTweaks

#if GRIP_VARIABLE_MASS_AND_INERTIA_TENSOR

		if (numUpNear + numDownNear + numUpContact + numDownContact == 0)
		{
			Physics.SpringScaleTimer = 1.0f;
			Physics.InertiaTensorScaleTimer = 2.0f;
		}

		if (Physics.InertiaTensorScaleTimer > 1.0f &&
			numUpContact + numDownContact == 0)
		{
			Physics.SpringScaleTimer = 1.0f;
			Physics.InertiaTensorScaleTimer = 2.0f;
		}

#endif // GRIP_VARIABLE_MASS_AND_INERTIA_TENSOR

#pragma endregion VehiclePhysicsTweaks

	}

#pragma region VehiclePhysicsTweaks

#if GRIP_VEHICLE_BOUNCE_CONTROL

	if (bounceCompression == true &&
		Physics.Bounce.Stage == 0 &&
		Physics.ContactData.ModeTime < 0.5f)
	{
		// Look for good bounce setup for up to a quarter of a second after landing.

		Physics.Bounce.Stage = 1;
		Physics.Bounce.Timer = 0.25f;
		Physics.Bounce.Direction = GetSurfaceNormal();
	}

	if (Physics.Bounce.Stage == 1)
	{
		FVector localVelocity = Physics.Bounce.Direction.ToOrientationQuat().UnrotateVector(Physics.VelocityData.VelocityDirection);

		if (localVelocity.X >= 0.0f)
		{
			// Determine how hard we came down.

			float maxSpeed = 0.0f;
			int32 numValues = Physics.VelocityList.GetNumValues();

			for (int32 i = numValues - 1; i >= 0; i--)
			{
				if (Physics.VelocityList[i].Time < Physics.Timing.TickSum - 0.25f)
				{
					break;
				}

				localVelocity = Physics.Bounce.Direction.ToOrientationQuat().UnrotateVector(Physics.VelocityList[i].Value);

				if (localVelocity.X < 0.0f)
				{
					maxSpeed = FMath::Max(maxSpeed, -localVelocity.X);
				}
			}

			FVector surfaceNormal = GetSurfaceNormal();
			float scale = FMathEx::GetRatio(maxSpeed, 1500.0f, 5000.0f);

			if (scale > KINDA_SMALL_NUMBER)
			{

#pragma region VehicleAntiGravity

				if (Antigravity == true)
				{
					scale = scale * 0.25f + 0.5f;
				}
				else

#pragma endregion VehicleAntiGravity

				{
					scale = scale * 0.25f + 0.4f;
				}

				scale = FMath::Lerp(0.0f, scale, FMathEx::GetRatio(surfaceNormal.Z, -1.0f, 1.0f));

#pragma region NavigationSplines

				if (Antigravity == false)
				{
					if (AI.RouteFollower.ThisSpline != nullptr &&
						AI.RouteFollower.NextSpline != nullptr)
					{
						// Reduce the bounce the more sideways the vehicle is compared to the direction of the track.

						FVector splineDirection0 = AI.RouteFollower.ThisSpline->GetDirectionAtDistanceAlongSpline(AI.RouteFollower.ThisDistance, ESplineCoordinateSpace::World);
						FVector splineDirection1 = AI.RouteFollower.NextSpline->GetDirectionAtDistanceAlongSpline(AI.RouteFollower.NextDistance, ESplineCoordinateSpace::World);

						splineDirection0 = transform.InverseTransformVector(splineDirection0);
						splineDirection1 = transform.InverseTransformVector(splineDirection1);

						float difference = FMath::Max(FMath::Atan2(splineDirection0.Y, splineDirection0.X), FMath::Atan2(splineDirection1.Y, splineDirection1.X));
						float angleRatio = FMathEx::GetRatio(FMath::Abs(FMath::RadiansToDegrees(difference)), 10.0f, 30.0f);

						scale *= 1.0f - angleRatio;
					}
				}

#pragma endregion NavigationSplines

				scale = FMath::Min(scale, AntigravityBounceScale);

				if (scale > KINDA_SMALL_NUMBER)
				{
					Physics.Bounce.Timer = 1.0f;
					Physics.Bounce.Force = scale;
					Physics.Bounce.Direction = surfaceNormal;
					Physics.Bounce.Stage = 2;
				}
			}
			else
			{
				Physics.Bounce.Stage = 3;
			}
		}
		else
		{
			Physics.Bounce.Timer -= deltaSeconds;

			if (Physics.Bounce.Timer <= 0.0f)
			{
				Physics.Bounce.Stage = 3;
			}
		}
	}

	if (Physics.Bounce.Stage == 2)
	{
		if (Physics.Bounce.Timer > 0.0f)
		{
			FVector angularVelocity = VehicleMesh->GetPhysicsAngularVelocityInDegrees();

			angularVelocity.X *= 1.0f - Physics.Bounce.Timer;
			angularVelocity.Y *= 1.0f - Physics.Bounce.Timer;

			VehicleMesh->SetPhysicsAngularVelocityInDegreesSubstep(angularVelocity);

			VehicleMesh->IdleUnlock();

			VehicleMesh->AddForceSubstep(Physics.Bounce.Direction * Physics.Bounce.Force * Physics.Bounce.Timer * Physics.CurrentMass * 25000.0f);
		}

#pragma region VehicleAntiGravity

		if (Antigravity == true)
		{
			Physics.Bounce.Timer -= deltaSeconds * 8.0f;
		}
		else

#pragma endregion VehicleAntiGravity

		{
			Physics.Bounce.Timer -= deltaSeconds * 12.0f;
		}

		if (Physics.Bounce.Timer <= -2.0f)
		{
			Physics.Bounce.Stage = 0;
		}
	}

	if (Physics.Bounce.Stage == 3)
	{
		if (IsAirborne() == true)
		{
			Physics.Bounce.Stage = 0;
		}
	}

#endif // GRIP_VEHICLE_BOUNCE_CONTROL

#pragma endregion VehiclePhysicsTweaks

	return numUpContact + numDownContact;
}

/**
* Are we allowed to engage the throttle to the wheels? (correct race state)
***********************************************************************************/

bool ABaseVehicle::IsPowerAvailable() const
{
	return (PlayGameMode != nullptr) ? PlayGameMode->PastGameSequenceStart() : true;
}

#pragma endregion VehicleContactSensors

#pragma region VehicleBasicForces

/**
* Get the force of gravity to apply to the vehicle over one second.
***********************************************************************************/

FVector ABaseVehicle::GetGravityForce(bool totalGravity) const
{
	float force = 0.0f;
	float worldGravity = World->GetGravityZ();

	force += worldGravity * (GravityScale - 1.0f);

	// Compute either the additional gravity above stock PhysX gravity, or the
	// total gravity combined depending on what has been requested.

	if (totalGravity == true)
	{
		force += worldGravity;
	}

	// Always multiply by mass so its consistent.

	force *= Physics.CurrentMass;

	return FVector(0.0f, 0.0f, force);
}

/**
* Get the drag force based on the velocity given and the vehicle's drag coefficient.
***********************************************************************************/

FVector ABaseVehicle::GetDragForceFor(FVector velocity) const
{
	int32 level = GameState->GetDifficultyLevel();

#if WITH_EDITOR
	if (PlayGameMode != nullptr &&
		PlayGameMode->GameStateOverrides != nullptr &&
		PlayGameMode->GameStateOverrides->SeriousBotBehaviour == true)
	{
		level = 2;
	}
#endif // WITH_EDITOR

	float scale = GameState->GeneralOptions.GetDragScale(level) * RaceState.DragScale;

	// The drag coefficient is scaled to meters, so we have to convert back and forth
	// here otherwise the squaring with velocity.Size() will fail.

	velocity = FMathEx::CentimetersToMeters(velocity);

	// The main drag calculation using a drag coefficient with some scaling for
	// difficulty level and catchup rubber-banding if switched on.

	FVector drag = velocity * -(DragCoefficient * scale) * velocity.Size();

	// Note that DragCoefficient is a constant per-vehicle here, and works just fine.
	// But you could just as easily compute it from the vehicle body's orientation
	// and exposed surface area to the incoming air stream, and maybe even air density
	// which changes with temperature and altitude too.

	return FMathEx::MetersToCentimeters(drag);
}

/**
* Get the rolling resistance force based on the velocity given and the vehicle's
* rolling resistance coefficient.
***********************************************************************************/

FVector ABaseVehicle::GetRollingResistanceForceFor(float speed, const FVector& velocityDirection, const FVector& xdirection) const
{
	if (speed > KINDA_SMALL_NUMBER)
	{
		// Note that this rolling resistance is just for effect and not physically correct.
		// As we don't have a real piston-engined vehicle there is no real rolling resistance
		// to model, so we just emulate it here.

		// Negate the dot product to push against the vehicle's velocity rather than with it.

		float scale = -FVector::DotProduct(xdirection, velocityDirection);

		// Cap the rolling resistance so it's not too harsh at higher speeds.

		// This capping is not physically correct at all, but gave us the behavior we wanted
		// for GRIP. Ideally, this value of 5 should be in the VehicleEngineModel somewhere.

		scale *= FMath::Min(5.0f, speed * VehicleEngineModel->EngineBrakingCoefficient);

		// Scale with the number of wheels on the ground.

		float wheelRatio = (Wheels.Wheels.Num() > 0) ? Wheels.NumWheelsInContact / Wheels.Wheels.Num() : 1.0f;

		return xdirection * scale * Physics.CurrentMass * wheelRatio;
	}

	return FVector::ZeroVector;
}

/**
* Get the current jet engine power.
***********************************************************************************/

float ABaseVehicle::GetJetEnginePower(int32 numWheelsInContact, const FVector& xdirection)
{
	float enginePower = Propulsion.CurrentJetEnginePower;

	if (IsReversing() == true)
	{
		enginePower *= 1.0f - FMath::Pow(FMath::Min((GetSpeedKPH() / (GetGearSpeedRange() * 1.8f)), 1.0f), 4.0f);
	}

#pragma region PickupTurbo

	// Boost the power with a turbo-type device like the Firestorm turbo.

	enginePower += (Propulsion.MaxJetEnginePower * Propulsion.Boost);

#pragma endregion PickupTurbo

#pragma region VehiclePickups

	if (PickupIsCharging(true) == true)
	{
		// Slow the vehicle down if it's charging a pickup slot.

		if (GetSpeedKPH() < 250.0f)
		{
			enginePower *= 1.0f - ((GetSpeedKPH() / 250.0f) * 0.5f);
		}
		else
		{
			enginePower *= 0.5f;
		}
	}

#pragma endregion VehiclePickups

#pragma region VehicleDrifting

	// Now add in the extra power that we give for drifting. If we don't do this, then
	// vehicles can slow down too much while drifting and displeases players.

	float driftingBoost = 1.0f + (GetDriftingRatio() * TireFrictionModel->SpeedBoostWhenDrifting);

	return enginePower * driftingBoost;

#pragma endregion VehicleDrifting

}

/**
* Get the down force based on the velocity of the vehicle and its down force
* coefficient.
***********************************************************************************/

FVector ABaseVehicle::GetDownForce()
{
	// Note that down-force is an inaccurate term here, but most players would perceive
	// the effect we produce as what they know to be down-force. Really what it's doing
	// though it having the vehicles act as if they're magnetized towards their nearest
	// driving surface. The more upside-down the surface is, the more magnetism produced
	// in order to counter gravity.

	// Force is computed from speed.

	float force = 1.0f;
	float speed = GetSpeedKPH();
	const float maxSpeed = 600.0f;

	if (speed < maxSpeed)
	{
		// Scale the force up to 1 using a sine curve starting at 0.

		force = FMath::Sin((speed / maxSpeed) * PI * 0.5f);
		force *= force;
	}

	// The direction of the driving surface for the vehicle.

	FVector direction = GetSurfaceDirection();

	// Scale to 0 when driving surface is underneath the vehicle and 1 when it is above.
	// We want to push harder when we're working against gravity than when working with it.

	float scale = FMathEx::NegativePow(((direction.Z * 0.5f) + 0.5f), 0.5f);

	float maxDistance = 4.0f * 100.0f;
	float maxWheelRadius = GetMaxWheelRadius();

#pragma region VehicleAntiGravity

	if (Antigravity == true)
	{
		maxDistance += HoverDistance * GetAirPower();
	}

#pragma endregion VehicleAntiGravity

	// No down force if one of the axles is properly airborne, or more than
	// maxDistance away from the driving surface.

	bool axleAirborne[2] = { true, true };
	float averageSamples = 0.0f;
	float averageCompression = 0.0f;

	for (FVehicleWheel& wheel : Wheels.Wheels)
	{
		float distance = wheel.GetActiveSensor().GetSurfaceDistance();

#pragma region PickupTurbo

		// Try to keep the vehicle on the ceiling when doing a charged turbo.

		if (wheel.HasRearPlacement() == false &&
			Propulsion.RaiseFrontScale > KINDA_SMALL_NUMBER)
		{
			distance = wheel.Radius;
		}

#pragma endregion PickupTurbo

		averageSamples += 1.0f;
		averageCompression += wheel.GetActiveSensor().GetNormalizedCompression();

		if (distance != 0.0f &&
			FMath::Max(0.0f, distance - wheel.Radius) < maxDistance)
		{
			axleAirborne[(wheel.HasFrontPlacement() == true) ? 0 : 1] = false;
		}
	}

	// Apply less down force when springs are highly compressed, as this will help with cornering in tunnels.
	// Not strictly necessary, and not part of the normal down force model, so here it's just used to ameliorate
	// the forces and try not to push the vehicle hard against a surface when it's already been pressed hard
	// against a surface.

	averageCompression = FMath::Clamp((averageCompression / averageSamples) - 1.25f, 0.0f, 1.0f);

	scale = FMath::Lerp(scale, scale * 0.666f, averageCompression);

	float invDistanceScale = 1.0f;

	if (axleAirborne[0] == false &&
		axleAirborne[1] == false)
	{
		// If both axes are close enough the driving surface then calculate how far away the driving surface
		// is from the vehicle in general.

		float distance = GetSurfaceDistance(true);

		// Scale with wheel surface distance from the driving surface, losing all force at maxDistance away.

		// Only if we're in effect for down force do we calculate an inverse scale for it.
		// 0 for full force, and 1 for no force.

		invDistanceScale = FMath::Max(0.0f, distance - maxWheelRadius);
		invDistanceScale = FMath::Min(invDistanceScale / maxDistance, 1.0f);
		invDistanceScale = FMath::Pow(invDistanceScale, 4.0f);
	}

#pragma region VehicleLaunch

	// Remove down force just after a vehicle launch.

	float jumpTime = GetVehicleClock() - LastLaunchTime;

	if (jumpTime < 2.0f &&
		FVector::DotProduct(GuessSurfaceNormal(), LaunchSurfaceNormal) > -0.5f)
	{
		scale *= FMathEx::GetRatio(jumpTime - 1.0f, 0.0f, 1.0f);
	}

#pragma endregion VehicleLaunch

	// Apply all the constituents together.

	force = force * Physics.GravityStrength * scale * (1.0f - invDistanceScale) * 2.0f;

	return direction * force;
}

/**
* Get the predicted velocity based on recorded velocity information.
***********************************************************************************/

FVector ABaseVehicle::GetPredictedVelocity() const
{
	// When hard cornering the vehicle's velocity lags behind its apparent velocity.
	// So here we're trying to predict the apparent velocity based on the most recent
	// velocity data instead. This can be useful when being targeted by missiles and
	// they're maneuvering towards the vehicle with some lead applied.

	FVector velocity = Physics.VelocityData.Velocity;
	FVector offset = Physics.DirectionVsVelocityList.GetMeanValue(Physics.Timing.TickSum - 0.25f);
	FVector direction = Physics.VelocityData.VelocityDirection.ToOrientationQuat().RotateVector(offset) + Physics.VelocityData.VelocityDirection; direction.Normalize();

	return direction * velocity.Size();
}

#pragma endregion VehicleBasicForces

#pragma region VehicleGrip

/**
* Calculate the rotations per second rate of a wheel.
***********************************************************************************/

void ABaseVehicle::CalculateWheelRotationRate(FVehicleWheel& wheel, const FVector& velocityDirection, float vehicleSpeed, float brakePosition, float deltaSeconds)
{
	float rps1 = 0.0f;
	float rps0 = wheel.RPS;
	float circumference = FMathEx::CentimetersToMeters(wheel.Radius) * PI * 2.0f;

	vehicleSpeed = FMathEx::CentimetersToMeters(vehicleSpeed);

	if (wheel.GetActiveSensor().IsInContact() == false)
	{
		// If we're airborne, and the wheel isn't a driven wheel, then slow it up a little.
		// We're not really going to notice this in the game but it's physically correct.
		// Reduce by half a rotation per second, stop at zero.

		rps1 = rps0 - (0.5f * deltaSeconds * FMath::Sign(rps0));

		// Clamp to zero if we've crossed that mark.

		if (FMathEx::UnitSign(rps0) != FMathEx::UnitSign(rps1))
		{
			rps1 = 0.0f;
		}

		// Invert the rotation if necessary.

		if (wheel.RPSFlipped != Wheels.SoftFlipped)
		{
			rps1 *= -1.0f;
		}
	}
	else
	{
		// If the wheel is in contact with the ground, then we want the wheel rotate at the
		// speed governed by the ground speed the vehicle is traveling.

		// Find the angle of the wheel vs the velocity in the horizontal plane so we can figure out
		// how much rotation to apply to the wheel. If they are parallel then full rotation and
		// perpendicular then no rotation.

		float dot = FVector::DotProduct(velocityDirection, wheel.Transform.GetAxisX());

		// Take into account steering velocity vs direction.

		rps1 = (vehicleSpeed / circumference) * dot;

		// Rotate the other way if flipped upside-down.

		if (Wheels.SoftFlipped == false)
		{
			rps1 *= -1.0f;
		}
	}

	// So now rps1 is the "natural" rotation of the wheel.

	wheel.RPSFlipped = Wheels.SoftFlipped;

	// Now apply brakes to this wheel.

	// Technically, it's much easier to skid at low speed than high, because of the slip ratio.

	if ((brakePosition > 0.0f) &&
		(IsWheelBraked(wheel) == true))
	{
		float decelerationMPS = TireFrictionModel->BrakingDeceleration * brakePosition;
		float decelerationRPS = decelerationMPS / circumference;
		float rps2 = rps0 - (decelerationRPS * deltaSeconds * FMath::Sign(rps0));

		// Clamp to zero if we've crossed that mark.

		if (FMathEx::UnitSign(rps0) != FMathEx::UnitSign(rps2))
		{
			rps2 = 0.0f;
		}

		rps1 = FMath::Min(FMath::Abs(rps1), FMath::Abs(rps2)) * FMathEx::UnitSign(rps1);
	}

	// rps0 is the current wheel rotation rate.
	// rps1 is the rotation rate demanded by the ground speed or braked.

	wheel.RPS = rps1;

	if (vehicleSpeed < KINDA_SMALL_NUMBER ||
		wheel.GetActiveSensor().IsInContact() == false)
	{
		// No slip if no speed or no contact.

		wheel.LongitudinalSlip = 0.0f;
	}
	else
	{
		// Calculate the slip value for the tire vs the surface its on.
		// This returns a ratio between 0 and +/-1. Negative values for the wheel spinning
		// too fast (wheel spinning) and positive if too slow (braking).

		vehicleSpeed *= FMathEx::UnitSign(Physics.VelocityData.DirectedSpeed);

		wheel.LongitudinalSlip = (vehicleSpeed - (wheel.GetUnflippedRPS() * circumference)) / vehicleSpeed;
	}
}

/**
* Get the lateral friction for a dot product result between normalized wheel
* velocity vs the wheel side vector. More side-slip should mean more lateral force.
***********************************************************************************/

float ABaseVehicle::LateralFriction(float baselineFriction, float sideSlip, FVehicleWheel& wheel) const
{
	// sideSlip is the cosine of the angle of the normalized wheel velocity vs the wheel side
	// vector. so 0 means no side-slip and +-1 means full side slip. velocity is the wheel's
	// velocity in meters per second.

	float speed = wheel.Velocity.Size();

	// Generally grip should be constant, but we add more at very speeds to avoid sliding around.
	// (about 50% more)

	float grip = TireFrictionModel->LateralGripVsSpeed.GetRichCurve()->Eval(FMathEx::CentimetersPerSecondToKilometersPerHour(speed));

	// We want the car to have good lateral friction when heading forwards but slide a bit when
	// the car gets sideways - but only at high speeds, we need good sticking friction when the
	// car is stationary.

	// We want to try to keep grip hard in normal circumstances to control the car effectively.
	// But at some point in the side-slip curve the friction should break and become less grippy.
	// This is kind-of like the difference between static and sliding friction.

	// Note also. This lower friction at higher slip-angles helps massively to stop rear-end slip
	// and this loss of speed. The lower the friction, the less rear-end slip you get.

	float angle = FMathEx::DotProductToDegrees(1.0f - FMath::Abs(sideSlip));
	float scale = TireFrictionModel->LateralGripVsSlip.GetRichCurve()->Eval(angle * TireFrictionModel->LateralGripVsSlipScale);
	float friction = grip * scale;

	// However, we do need longitudinal friction to be at play here in this case, to stop
	// you sliding down a hill sideways for instance.

	return baselineFriction * FMathEx::UnitSign(sideSlip) * friction;
}

/**
* Calculate the longitudinal grip ratio for a slip value.
* Slip is between -1 to 1, 0 meaning no slip, -1 meaning wheel spinning hard and 1
* meaning braking hard (fully locked up in fact).
***********************************************************************************/

float ABaseVehicle::CalculateLongitudinalGripRatioForSlip(float slip) const
{
	slip = FMath::Max(slip, -1.0f);

	return TireFrictionModel->LongitudinalGripVsSlip.GetRichCurve()->Eval(FMath::Abs(slip * 100.0f));
}

/**
* Get the horizontal velocity vector for a wheel, for use in slip calculations.
***********************************************************************************/

FVector ABaseVehicle::GetHorizontalVelocity(const FVehicleWheel& wheel, const FTransform& transform)
{
	FVector localVelocity = transform.InverseTransformVector(wheel.Velocity);

	// Kill any vertical velocity so we can just measure horizontal.

	localVelocity.Z = 0.0f;

	FVector velocity = transform.TransformVector(localVelocity);

	check(velocity.ContainsNaN() == false);

	return velocity;
}

/**
* Get the weight acting on a wheel for this point in time, in kilograms.
***********************************************************************************/

float ABaseVehicle::GetWeightActingOnWheel(FVehicleWheel& wheel)
{
	float mass = Physics.CurrentMass;

	if (TireFrictionModel->Model == ETireFrictionModel::Arcade)
	{
		// In the simplified model, all grip is applied equally. This gives us the best
		// overall handling for GRIP vehicles, as something more realistic just makes it
		// more unmanageable. Effectively, what we're doing here, is spreading the mass
		// the vehicle equally across all of the available wheels and not using any kind
		// of static loading. We tried static loading in GRIP, it destroyed the handling.

		mass /= (float)GetNumWheels(true);
	}

	// For the mass acting on this wheel, get the grip ratio to use based on its current
	// compression state.

	return mass * GetGripRatio(wheel.GetActiveSensor());
}

#pragma endregion VehicleGrip

#pragma region VehicleDrifting

/**
* Update the drifting of the back end physics.
***********************************************************************************/

void ABaseVehicle::UpdateDriftingPhysics(float deltaSeconds, float steeringPosition, const FVector& xdirection)
{
	// Handle the rear-end drift.

	float directionScale = 1.0f;
	float targetDriftAngle = 0.0f;
	float maxDrift = TireFrictionModel->RearEndDriftAngle;
	float velocityVsDirection = FMathEx::DotProductToDegrees(FMath::Max(0.0f, FVector::DotProduct(Physics.VelocityData.VelocityDirection, xdirection)));

	if (velocityVsDirection > maxDrift)
	{
		// Drop drifting off with velocity vector away from the direction + maximum drift vector (up to 20 degrees further).

		directionScale = FMath::Lerp(1.0f, 0.0f, FMath::Min(velocityVsDirection - maxDrift, 20.0f) / 20.0f);
	}

	if (IsDrifting() == true)
	{
		// We're in a manually invoked drift, to set the desired angle from that.

		targetDriftAngle = maxDrift * -steeringPosition * directionScale;
	}

	if (IsGrounded() == true &&
		Wheels.SkidAudioVolumeTarget > 0.0f)
	{
		// See if we have some natural drift to apply based on the skid audio volume (which directly
		// relates to the tire side-loading). This is a game play improvement, were many players were
		// oblivious they could drift, so here we give them some drift automatically.

		float angle = maxDrift * -steeringPosition * directionScale * Wheels.SkidAudioVolumeTarget * 0.666f;

		if (FMath::Abs(targetDriftAngle) < FMath::Abs(angle))
		{
			targetDriftAngle = angle;
		}
	}

#pragma region VehicleAntiGravity

	// Less drifting the more we lose grip due to going sideways with Airblades, because we can't
	// drift and slide at the same time.

	targetDriftAngle *= Physics.AntigravityLateralGrip;

#pragma endregion VehicleAntiGravity

	// If we were airborne for a little while, and have recently landed, give a little time for no
	// drifting and then give a short time to ease drifting back in. This gives you a chance
	// straighten up after a landing without drifting interfering.

	if (targetDriftAngle != 0.0f &&
		Physics.ContactData.Airborne == false &&
		Physics.ContactData.LastModeTime > 1.5f)
	{
		if (Physics.ContactData.ModeTime < 1.5f)
		{
			targetDriftAngle = 0.0f;
		}
		else if (Physics.ContactData.ModeTime - 1.5f < 2.0f)
		{
			targetDriftAngle *= (Physics.ContactData.ModeTime - 1.5f) / 2.0f;
		}
	}

	// Smooth towards the desired drift angle.

	float driftRatio = 1.0f;

	if (FMath::Abs(Physics.Drifting.RearDriftAngle) >= FMath::Abs(targetDriftAngle))
	{
		// Coming out of drift.

		driftRatio = FMathEx::GetSmoothingRatio(0.8f, deltaSeconds);
	}
	else
	{
		// Going into drift.

		float difference = 0.975f;

		if (FMath::Abs(targetDriftAngle) > KINDA_SMALL_NUMBER)
		{
			driftRatio = FMath::Abs(Physics.Drifting.RearDriftAngle) / FMath::Abs(targetDriftAngle);
			driftRatio = FMath::Sqrt(driftRatio);
		}

		driftRatio = FMathEx::GetSmoothingRatio(FMath::Lerp(0.9f, 0.975f, difference), deltaSeconds);
	}

	Physics.Drifting.RearDriftAngle = FMath::Lerp(targetDriftAngle, Physics.Drifting.RearDriftAngle, driftRatio);

	Wheels.SkidAudioVolumeTarget = 0.0f;
}

#pragma endregion VehicleDrifting

#pragma region PickupTurbo

/**
* Apply the turbo raise force when using a charged turbo pickup.
***********************************************************************************/

void ABaseVehicle::ApplyTurboRaiseForce(float deltaSeconds, const FTransform& transform)
{
	if (Wheels.RearAxleDown == true &&
		Propulsion.Boost > KINDA_SMALL_NUMBER &&
		Propulsion.RaiseFrontScale > KINDA_SMALL_NUMBER)
	{
		FVector velocity = GetVelocityOrFacingDirection();
		FVector direction = transform.InverseTransformVector(velocity);

		if (direction.X > 0.0f)
		{
			float angle = FMath::RadiansToDegrees(FMath::Atan2(direction.Z, direction.X));

			if (IsFlipped() == false)
			{
				angle *= -1.0f;
			}

			FVector force = GetLaunchDirection();
			float maxDegrees = 12.0f;

			if (angle < maxDegrees)
			{
				float angleScale = FMath::Pow(1.0f - (FMath::Max(angle, 0.0f) / maxDegrees), 0.5f);

				if (angle > maxDegrees * 0.8f &&
					Propulsion.RaiseFrontAchieved == 0.0f)
				{
					Propulsion.RaiseFrontAchieved = VehicleClock;
				}

				float raiseScale = Propulsion.RaiseFrontScale;

				if (Propulsion.RaiseFrontAchieved != 0.0f)
				{
					float timeSince = VehicleClock - Propulsion.RaiseFrontAchieved;

					raiseScale = Propulsion.RaiseFrontScale * (1.0f - FMath::Min(timeSince / 0.75f, 1.0f));
				}

				force *= 0.45f;
				force *= angleScale;
				force *= Propulsion.Boost * 0.125f;
				force *= raiseScale;
				force *= VehicleMesh->GetPhysicsInertiaTensor().Y;
				force *= FMath::Min(GetSpeedKPH() / 300.0f, 1.0f);

				// Apply an upwards force to the front axle position to raise the vehicle up,
				// currently only during the charged turbo boost.

				VehicleMesh->AddForceAtLocationSubstep(force, Wheels.FrontAxlePosition);
			}
			else if (Propulsion.RaiseFrontAchieved == 0.0f)
			{
				Propulsion.RaiseFrontAchieved = VehicleClock;
			}
		}
	}
}

#pragma endregion PickupTurbo

#pragma region VehicleAntiGravity

/**
* Update the forwards and antigravity scaling ratios for antigravity vehicles.
***********************************************************************************/

void ABaseVehicle::UpdateAntigravityForwardsAndScale(float deltaSeconds, float brakePosition, float& forwardRatio, float& scaleAntigravity)
{
	// Lose "grip" when sliding sideways in antigravity vehicles.

	float sideSlip = 0.0f;

	if (RaceState.RaceTime > 2.0f)
	{
		sideSlip = FVector::DotProduct(GetVelocityOrFacingDirection(), GetSideDirection());
		sideSlip = FMath::Lerp(0.0f, sideSlip, FMathEx::GetRatio(GetSpeedKPH(), 2.0f, 4.0f));
	}

	float angle = FMath::RadiansToDegrees(FMath::Acos(1.0f - FMath::Abs(sideSlip)));
	FRichCurveKey lastKey = TireFrictionModel->LateralGripVsSlip.GetRichCurve()->GetLastKey();
	float lastAngle = lastKey.Time;

	if (angle > lastAngle)
	{
		// Lose up to 66% of your grip after you get sideways by a reasonable amount.
		// The more sideways you are, the more grip you lose.

		float gripLoss = FMath::Lerp(0.666f, 0.333f, Physics.AntigravitySideSlip);
		float angleRatio = FMathEx::GetRatio(angle, lastAngle, lastAngle + 30.0f);
		float newScale = FMath::InterpEaseInOut(1.0f, gripLoss, angleRatio, 2.0f);

		forwardRatio = FMath::InterpEaseInOut(1.0f, 0.0f, angleRatio, 2.0f);

		// The more we're braking, the less we're sliding.

		scaleAntigravity = FMath::Lerp(newScale, 1.0f, brakePosition * (1.0f - Physics.VehicleTBoned));
	}

	// Calculate a grip scale for antigravity vehicles based on how far away
	// the direction of the vehicle is compared to its velocity vector. The
	// more sideways we are, the less grip we'll give the vehicle. This is
	// used to transition smoothly between sideways slipperiness and hard
	// grip when getting back to facing the velocity vector with no harsh
	// jerking.

	if (IsAirborne() == true)
	{
		Physics.AntigravityLateralGrip = 1.0f;
		Physics.AntigravitySideSlip = FMath::Min(Physics.AntigravitySideSlip + (deltaSeconds * 0.666f), 1.0f);
	}
	else
	{
		Physics.AntigravityLateralGrip = FMathEx::GravitateUpToTarget(Physics.AntigravityLateralGrip, forwardRatio, deltaSeconds * 0.25f);
	}
}

#pragma endregion VehicleAntiGravity

#if WITH_PHYSX
#if GRIP_ENGINE_PHYSICS_MODIFIED

/**
* Modify a collision contact.
*
* Be very careful here! This is called from the physics sub-stepping at the same
* time as other game code may be executing its normal ticks. Therefore, this code
* needs to be thread-safe and be able to handle re-entrancy.
***********************************************************************************/

bool ABaseVehicle::ModifyContact(uint32 bodyIndex, AActor* other, physx::PxContactSet& contacts)
{

#pragma region VehicleCollision

	float stockVehicleCollisionInertia = 0.1f;
	float vehicleCollisionInertia = stockVehicleCollisionInertia;

	// We've hit something so unlock the idle state.

	VehicleMesh->IdleUnlock();

	if (other != nullptr)
	{
		ABaseVehicle* otherVehicle = Cast<ABaseVehicle>(other);

		if (otherVehicle != nullptr)
		{
			// Unlock the idle state for the opposing vehicle.

			otherVehicle->VehicleMesh->IdleUnlock();

			// Vehicle / vehicle collision - try to prevent twisting motion, within reason.
			// The more parallel the vehicles are then the more we attempt to stop the twisting.

			float dp = FVector::DotProduct(GetVelocityOrFacingDirection(), otherVehicle->GetVelocityOrFacingDirection());

			vehicleCollisionInertia = FMath::Lerp(vehicleCollisionInertia * 2.0f, vehicleCollisionInertia, FMath::Pow(FMath::Abs(dp), 0.5f));

#pragma region PickupShield

			if (IsShieldActive() == true)
			{
				FVector velocity = GetVelocity() - otherVehicle->GetVelocity();
				float force = velocity.Size();
				float vehicleCollisionMass = 0.25f;

				if (Shield->IsCharged() == true)
				{
					if (force > 15.0f * 100.0f)
					{
						// If the closing velocity is greater than 15m per second then scrub off the grip
						// on the other vehicle, while adjusting its velocity by up to 50m per second.

						force = FMath::Max(force, 25.0f * 100.0f);

						if (force > 50.0f * 100.0f)
						{
							velocity.Normalize();
							velocity *= 50.0f * 100.0f;
						}

						otherVehicle->RemoveGripForAMoment(velocity * otherVehicle->GetPhysics().CurrentMass);
					}

					vehicleCollisionMass *= 0.1f;
					vehicleCollisionInertia *= 0.5f;
				}
				else
				{
					vehicleCollisionMass *= 0.5f;
					vehicleCollisionInertia *= 0.5f;
				}

				// Act like this vehicle has a lot more weight than it really does in response
				// to the collision if the shield is active.

				if (bodyIndex == 0)
				{
					contacts.setInvMassScale0(vehicleCollisionMass);
				}
				else if (bodyIndex == 1)
				{
					contacts.setInvMassScale1(vehicleCollisionMass);
				}
			}

#pragma endregion PickupShield

#pragma region VehicleAntiGravity

			// By default, antigravity vehicles are more prone to rotational collision response
			// than wheeled vehicles.

			float antigravityVehicleCollisionInertia = vehicleCollisionInertia * 2.0f;

			if (Antigravity == true)
			{
				bool hitOurSide = false;

				float width = VehicleCollision->GetUnscaledBoxExtent().Y * 0.75f;
				float otherWidth = otherVehicle->VehicleCollision->GetUnscaledBoxExtent().Y * 0.75f;

				// Examine the contacts for this body to see if they are forward or rearward.
				// If forward, then don't damp so much as it's implausible.

				int32 pointNum = 0;
				float pointAvg = 0.0f;
				const FTransform& transform = VehicleMesh->GetPhysicsTransform();

				for (uint32 i = 0; i < contacts.size(); i++)
				{
					FVector point = transform.InverseTransformPosition(*(FVector*)&contacts.getPoint(i));

					pointAvg += point.X;
					pointNum++;

					if (FMath::Abs(point.Y) > width)
					{
						hitOurSide = true;
					}
				}

				// Manage the side-impact t-boned grip reduction by registering the event to
				// be handled later in the physics sub-step.

				float tbonedRatio = FMath::Abs(FVector::DotProduct(GetSideDirection(), otherVehicle->GetVelocityOrFacingDirection()));

				if (hitOurSide == true &&
					tbonedRatio > 0.5f)
				{
					Physics.VehicleTBoned = FMath::Max(Physics.VehicleTBoned, tbonedRatio);
				}

				tbonedRatio *= FMathEx::GetRatio(otherVehicle->GetSpeedKPH(), 0.0f, 50.0f);

				Physics.AntigravitySideSlip = FMath::Max(Physics.AntigravitySideSlip, tbonedRatio);

				if (pointNum != 0)
				{
					pointAvg /= pointNum;

					float ratio = FMathEx::GetRatio(pointAvg / 200.0f, 0.0f, 1.0f);

					// Not sure why we have to increase inertia tensor compared to other vehicles when the impact is
					// at the front but we do - we observe horrendous spinning out problems if you take a front
					// impact otherwise even if they appear to be quite flat, parallel collisions.

					antigravityVehicleCollisionInertia = FMath::Lerp(antigravityVehicleCollisionInertia, vehicleCollisionInertia, ratio);
				}
			}

			// Depending on which body we are, set the contact accordingly.

			if (bodyIndex == 0)
			{
				contacts.setInvInertiaScale0((Antigravity == true) ? FMath::Min(vehicleCollisionInertia, antigravityVehicleCollisionInertia) : vehicleCollisionInertia);
			}
			else if (bodyIndex == 1)
			{
				contacts.setInvInertiaScale1((Antigravity == true) ? FMath::Min(vehicleCollisionInertia, antigravityVehicleCollisionInertia) : vehicleCollisionInertia);
			}

#pragma endregion VehicleAntiGravity

		}
	}

#pragma endregion VehicleCollision

	return false;
}

#endif // GRIP_ENGINE_PHYSICS_MODIFIED
#endif // WITH_PHYSX

/**
* Set the velocities and related data for the physics state.
***********************************************************************************/

void FPhysicsVelocityData::SetVelocities(const FVector& linearVelocity, const FVector& angularVelocity, const FVector& xdirection)
{
	check(linearVelocity.ContainsNaN() == false);
	check(angularVelocity.ContainsNaN() == false);

	Velocity = linearVelocity;
	VelocityDirection = Velocity;

	if (VelocityDirection.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		VelocityDirection = xdirection;
	}
	else
	{
		VelocityDirection.Normalize();
	}

	Speed = Velocity.Size();

	// Set a maximum speed of 2,000 kph to help stop code breakages further down the line.

	float maxSpeed = 55555.555f;

	if (Speed > maxSpeed)
	{
		Speed = maxSpeed;
		Velocity = VelocityDirection * Speed;
	}

	DirectedSpeed = Speed;

	if (Speed > 10.0f)
	{
		DirectedSpeed *= FVector::DotProduct(xdirection, VelocityDirection);
	}
}

/**
* Construct a UTireFrictionModel structure.
***********************************************************************************/

UTireFrictionModel::UTireFrictionModel()
{
	LateralGripVsSlip.GetRichCurve()->AddKey(0, 0.0f);
	LateralGripVsSlip.GetRichCurve()->AddKey(2, 0.3f);
	LateralGripVsSlip.GetRichCurve()->AddKey(4, 0.5f);
	LateralGripVsSlip.GetRichCurve()->AddKey(8, 0.7f);
	LateralGripVsSlip.GetRichCurve()->AddKey(16, 1.0f);
	LateralGripVsSlip.GetRichCurve()->AddKey(32, 1.3f);

	LongitudinalGripVsSlip.GetRichCurve()->AddKey(0.0f, 0.0f);
	LongitudinalGripVsSlip.GetRichCurve()->AddKey(21.0f, 0.75f);
	LongitudinalGripVsSlip.GetRichCurve()->AddKey(28.0f, 0.8f);
	LongitudinalGripVsSlip.GetRichCurve()->AddKey(100.0f, 0.5f);

	LateralGripVsSpeed.GetRichCurve()->AddKey(0.0f, 128.0f);
	LateralGripVsSpeed.GetRichCurve()->AddKey(100.0f, 175.0f);
	LateralGripVsSpeed.GetRichCurve()->AddKey(200.0f, 285.0f);
	LateralGripVsSpeed.GetRichCurve()->AddKey(300.0f, 400.0f);
	LateralGripVsSpeed.GetRichCurve()->AddKey(400.0f, 525.0f);
	LateralGripVsSpeed.GetRichCurve()->AddKey(500.0f, 650.0f);
	LateralGripVsSpeed.GetRichCurve()->AddKey(600.0f, 775.0f);

	GripVsSuspensionCompression.GetRichCurve()->AddKey(0.0f, 0.0f);
	GripVsSuspensionCompression.GetRichCurve()->AddKey(0.5f, 0.8f);
	GripVsSuspensionCompression.GetRichCurve()->AddKey(1.0f, 1.0f);
	GripVsSuspensionCompression.GetRichCurve()->AddKey(2.0f, 2.0f);

	RearLateralGripVsSpeed.GetRichCurve()->AddKey(0.0f, 1.25f);
	RearLateralGripVsSpeed.GetRichCurve()->AddKey(500.0f, 1.25f);
}

/**
* Construct a UVehicleEngineModel structure.
***********************************************************************************/

UVehicleEngineModel::UVehicleEngineModel()
{
	GearPowerRatios.Emplace(0.75f);
	GearPowerRatios.Emplace(0.5f);
	GearPowerRatios.Emplace(0.75f);
}

/**
* Construct a USteeringModel structure.
***********************************************************************************/

USteeringModel::USteeringModel()
{
	FrontSteeringVsSpeed.GetRichCurve()->AddKey(0.0f, 1.0f);
	FrontSteeringVsSpeed.GetRichCurve()->AddKey(88.0f, 0.65f);
	FrontSteeringVsSpeed.GetRichCurve()->AddKey(166.0f, 0.4f);
	FrontSteeringVsSpeed.GetRichCurve()->AddKey(300.0f, 0.3f);
	FrontSteeringVsSpeed.GetRichCurve()->AddKey(450.0f, 0.25f);

	BackSteeringVsSpeed.GetRichCurve()->AddKey(0.0f, 1.0f);
	BackSteeringVsSpeed.GetRichCurve()->AddKey(50, 0.66f);
	BackSteeringVsSpeed.GetRichCurve()->AddKey(100.0f, 0.0f);
}
