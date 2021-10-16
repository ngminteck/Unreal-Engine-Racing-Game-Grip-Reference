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

#pragma once

#include "system/gameconfiguration.h"
#include "components/splinemeshcomponent.h"
#include "ai/advancedsplinecomponent.h"
#include "pursuitsplinecomponent.generated.h"

class UPursuitSplineComponent;
class APursuitSplineActor;
struct FPursuitSplineInstanceData;

DECLARE_LOG_CATEGORY_EXTERN(GripLogPursuitSplines, Log, All);

/**
* Pursuit spline types, Military currently unused.
***********************************************************************************/

UENUM(BlueprintType)
enum class EPursuitSplineType : uint8
{
	// General spline for vehicle use.
	General,

	// Spline for military use only, currently unused.
	Military,

	// Spline specifically designed to assist with missile guidance.
	MissileAssistance
};

/**
* A pursuit spline mesh component used solely for rendering pursuit splines. There
* is normally one mesh component for each segment of a pursuit spline component.
***********************************************************************************/

UCLASS(ClassGroup = Rendering, meta = (BlueprintSpawnableComponent))
class GRIP_API UPursuitSplineMeshComponent : public USplineMeshComponent
{
	GENERATED_BODY()

public:

	// Set the spline component for this spline mesh component.
	UFUNCTION(BlueprintCallable, Category = Mesh)
		void SetupSplineComponent(UPursuitSplineComponent* splineComponent, int32 startPoint, int32 endPoint, bool selected);

	// Setup the rendering material for this spline mesh component.
	UFUNCTION(BlueprintCallable, Category = Mesh)
		void SetupMaterial(bool selected);

private:

	// The spline component that we're rendering with this mesh.
	UPursuitSplineComponent* PursuitSplineComponent = nullptr;

	// The start control point index number.
	int32 StartPoint = 0;

	// The end control point index number.
	int32 EndPoint = 0;
};

/**
* Structure for user-specified point data for pursuit splines.
***********************************************************************************/

USTRUCT(BlueprintType)
struct FPursuitPointData
{
	GENERATED_USTRUCT_BODY()

public:

	// The optimum speed in KPH (0 for full throttle) at this point for vehicles using this spline.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Spline)
		float OptimumSpeed = 0.0f;

	// The minimum speed in KPH (0 for none) at this point for vehicles using this spline.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Spline)
		float MinimumSpeed = 0.0f;

	// The maneuvering width in meters at this point for vehicles using this spline.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Spline)
		float ManeuveringWidth = 50.0f;

	// Is exterior weather allowed to be rendered at this point?
	// (we calculate undercover areas anyway so don't worry about those)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Spline)
		bool WeatherAllowed = true;

	// Should projectiles follow the terrain at this point, or just follow the spline if not?
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Spline)
		bool ProjectilesFollowTerrain = true;
};

/**
* Structure for extended automatically-generated point data for pursuit splines.
***********************************************************************************/

USTRUCT(BlueprintType)
struct FPursuitPointExtendedData
{
	GENERATED_USTRUCT_BODY()

public:

	// The distance along the spline at which the point is found.
	UPROPERTY()
		float Distance = 0.0f;

	// The distance around the master spline that this point matches.
	// Intentionally not necessarily the closest point if crossovers and loops are present.
	UPROPERTY()
		float MasterSplineDistance = -1.0f;

	// The maximum diameter of the tunnel if inside a tunnel.
	UPROPERTY()
		float MaxTunnelDiameter = 0.0f;

	// The raw, unfiltered exterior weather allowed to be rendered at this point? (< 1 means not)
	UPROPERTY()
		float RawWeatherAllowed = 0.0f;

	// The filtered, more natural exterior weather allowed to be rendered at this point? (< 1 means not)
	UPROPERTY()
		float UseWeatherAllowed = 0.0f;

	// The index to identify the curvature of the spline in environment space.
	// (i.e. which environment index would you naturally drive along).
	UPROPERTY()
		int32 CurvatureIndex = 0;

	// The raw, unfiltered ground index into the environment distances.
	UPROPERTY()
		int32 RawGroundIndex = 0;

	// The filtered, more natural ground index into the environment distances.
	UPROPERTY()
		int32 UseGroundIndex = 0;

	// Where is the ground relative to this point, in world space?
	// NB. Ground is the closest point, not necessarily below.
	UPROPERTY()
		FVector RawGroundOffset = FVector::ZeroVector;

	// Where is the ground relative to this point, in world space?
	// NB. Ground is the closest point, not necessarily below.
	UPROPERTY()
		FVector UseGroundOffset = FVector::ZeroVector;

	// How far away are the nearest objects to this point for a number of samples, in centimeters.
	UPROPERTY()
		TArray<float> EnvironmentDistances;

	// Does the left-hand driving surface have open edge? (therefore, don't drive over it)
	UPROPERTY()
		bool OpenLeft = false;

	// Does the left-hand driving surface have open edge? (therefore, don't drive over it)
	UPROPERTY()
		bool OpenRight = false;

	// The orientation, cached here for speed.
	UPROPERTY()
		FQuat Quaternion = FQuat::Identity;

	// Does this point reside over level ground?
	bool IsLevelGround()
	{ return (EnvironmentDistances[UseGroundIndex] < 25.0f * 100.0f && UseGroundIndex >= (NumDistances >> 1) - (NumDistances >> 4) && UseGroundIndex <= (NumDistances >> 1) + (NumDistances >> 4)); }

	// Does this point reside under level ceiling?
	bool IsLevelCeiling()
	{ return (EnvironmentDistances[UseGroundIndex] < 25.0f * 100.0f && (UseGroundIndex >= (NumDistances - (NumDistances >> 4)) || UseGroundIndex <= (NumDistances >> 4))); }

	// Get the angle difference between to environment samples.
	static float DifferenceInDegrees(int32 indexFrom, int32 indexTo);

	// The number of environment distances what we sample and store.
	static const int32 NumDistances = 32;
};

#pragma region NavigationSplines

/**
* Structure for describing a distance along a spline.
***********************************************************************************/

struct FSplineDistance
{
public:

	FSplineDistance(UPursuitSplineComponent* spline, float distance)
		: Spline(spline)
		, Distance(distance)
	{ }

	// The spline.
	UPursuitSplineComponent* Spline = nullptr;

	// The distance along the spline.
	float Distance = 0.0f;
};

/**
* Structure for a link between two splines.
***********************************************************************************/

struct FSplineLink
{
public:

	FSplineLink(TWeakObjectPtr<UPursuitSplineComponent> spline, float thisDistance, float nextDistance, bool forwardLink = false)
		: Spline(spline)
		, ThisDistance(thisDistance)
		, NextDistance(nextDistance)
		, ForwardLink(forwardLink)
	{ }

	// Is the spline and distance referenced by this link valid for a route choice decision?
	bool LinkIsRouteChoice() const;

	// Is the spline link broadly the same as another?
	bool operator == (const FSplineLink& other) const
	{ return (Spline == other.Spline && FMath::Abs(ThisDistance - other.ThisDistance) < 100.0f && FMath::Abs(NextDistance - other.NextDistance) < 100.0f); }

	// The spline to link to.
	TWeakObjectPtr<UPursuitSplineComponent> Spline;

	// The distance at which Spline can be found on the parent spline (where this link is contained).
	float ThisDistance = 0.0f;

	// And the next (or other) distance of this junction on Spline itself.
	float NextDistance = 0.0f;

	// Is this a forward link onto Spline?
	bool ForwardLink = false;
};

/**
* Structure for a route choice, a set of splines that can be taken at a branch point
* on a spline.
***********************************************************************************/

struct FRouteChoice
{
public:

	// The distance along a spline at which the decision needs to be made.
	float DecisionDistance = 0.0f;

	// The splines that are available to be taken.
	TArray<FSplineLink> SplineLinks;
};

/**
* Structure for following a sequence of pursuit splines that form a route.
***********************************************************************************/

struct FRouteFollower
{
public:

	// Is this follower attached to a spline right now?
	bool IsValid() const
	{ return GRIP_POINTER_VALID(NextSpline); }

	// Get the average tunnel diameter over a set distance.
	float GetTunnelDiameterOverDistance(float distance, float overDistance, int32 direction, bool minimum) const;

	// The spline that the follower is currently on.
	TWeakObjectPtr<UPursuitSplineComponent> ThisSpline;

	// The spline that the follower is currently aiming for.
	TWeakObjectPtr<UPursuitSplineComponent> NextSpline;

	// The distance along the spline that the follower is currently on.
	float ThisDistance = 0.0f;

	// The distance along the spline that the follower is currently aiming for.
	float NextDistance = 0.0f;

	// The distance on the next spline that switching transfers to.
	float NextSwitchDistance = 0.0f;

#pragma region AINavigation

	// Estimate where we are along the current spline, faster than DetermineThis.
	void EstimateThis(const FVector& position, const FVector& movement, float movementSize, int32 numIterations, float accuracy);

	// Determine where we are along the current spline.
	void DetermineThis(const FVector& position, float movementSize, int32 numIterations, float accuracy);

	// Determine where we are aiming for along the current or next spline, switching splines at branches if necessary.
	void DetermineNext(float ahead, float movementSize, UPursuitSplineComponent* preferSpline, bool forMissile, bool wantPickups, bool highOptimumSpeed, float fastPathways);

	// Check that a connection from one spline to another has been taken.
	bool CheckBranchConnection(UWorld* world, const FVector& position, float atDistance);

	// Choose the next spline to hook onto from the route choice given. Use the parameters specified to determine which is the best spline to select for the use-case given.
	bool ChooseNextSpline(TWeakObjectPtr<UPursuitSplineComponent>& pursuitSpline, float distanceAlong, float& thisSwitchDistance, float& nextSwitchDistance, const FRouteChoice& choice, float movementSize, UPursuitSplineComponent* preferSpline, bool forMissile, bool wantPickups, bool highOptimumSpeed, float fastPathways) const;

	// Switch to a new spline if we've passed the switch distance for it.
	void SwitchSplineAtJunction(const FVector& position, float movementSize, int32 numIterations, float accuracy);

	// Get the minimum optimum speed of the spline in kph over distance.
	float GetMinimumOptimumSpeedOverDistance(float distance, float& overDistance, int32 direction) const;

	// Get the minimum speed of the spline in kph over distance.
	float GetMinimumSpeedOverDistance(float distance, float& overDistance, int32 direction) const;

	// The spline that the follower was last on before the current one.
	TWeakObjectPtr<UPursuitSplineComponent> LastSpline;

	// The distance along the last spline that the follower was on.
	float LastDistance = 0.0f;

	// The distance on this spline to switch to the next spline.
	float ThisSwitchDistance = 0.0f;

	// Are we switching splines right now?
	bool SwitchingSpline = false;

	// The location at which the parent actor of the route follower switched splines.
	FVector SwitchLocation = FVector::ZeroVector;

	// The distance along this spline that the route switch decision was last made, or -1.0 if inactive.
	float DecidedDistance = -1.0f;

#pragma endregion AINavigation

#pragma region AIVehicleControl

	// Get the curvature of the spline in degrees over distance (in withRespectTo space).
	FRotator GetCurvatureOverDistance(float distance, float& overDistance, int32 direction, const FQuat& withRespectTo, bool absolute) const;

#pragma endregion AIVehicleControl

#pragma region VehicleTeleport

	// Rewind the follower to safe ground - normally used when teleporting a vehicle.
	bool RewindToSafeGround(float rewindDistance, float& initialSpeed, bool reset = true);

	// The number of branches we have rewound over when calculating a route.
	int32 NumRewindBranches = 0;

#pragma endregion VehicleTeleport

#pragma region PickupMissile

	// Is a distance along a spline in open space?
	// splineOffset should always be in spline space.
	float GetClearanceOverDistance(float distance, float& overDistance, int32 direction, FVector worldLocation, FVector splineOffset, float clearanceAngle = 90.0f) const;

#pragma endregion PickupMissile

};

#pragma endregion NavigationSplines

/**
* Class for a pursuit spline component, normally one per actor.
***********************************************************************************/

UCLASS(ClassGroup = Navigation, meta = (BlueprintSpawnableComponent))
class GRIP_API UPursuitSplineComponent : public UAdvancedSplineComponent
{
	GENERATED_BODY()

public:

	// Construct a pursuit spline component.
	UPursuitSplineComponent();

	// Always select this spline if enabled?
	UPROPERTY(EditAnywhere, Category = Spline)
		bool AlwaysSelect = false;

	// Can the spline be used for guiding missiles?
	UPROPERTY(EditAnywhere, Category = Spline)
		bool SuitableForMissileGuidance = true;

	// Does this spline contain a bundle of pickups?
	UPROPERTY(EditAnywhere, Category = Spline)
		bool ContainsPickups = false;

	// Is this spline a shortcut?
	UPROPERTY(EditAnywhere, Category = Spline)
		bool IsShortcut = false;

	// Is this spline for careful driving?
	UPROPERTY(EditAnywhere, Category = Spline)
		bool CarefulDriving = false;

	// The type of spline.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Spline)
		EPursuitSplineType Type = EPursuitSplineType::General;

	// What probability is there of this branch being selected (ostensibly between 0 and 1)?
	UPROPERTY(EditAnywhere, Category = Spline)
		float BranchProbability = 1.0f;

	// Get the maneuvering width at a point along a spline.
	UFUNCTION(BlueprintCallable, Category = Spline)
		float GetWidthAtSplinePoint(int32 point) const
	{
		return GetPursuitPoint(point).ManeuveringWidth;
	}

	// Get the optimum speed at a point along a spline.
	UFUNCTION(BlueprintCallable, Category = Spline)
		float GetOptimumSpeedAtSplinePoint(int32 point) const
	{
		return FMath::Min(GetPursuitPoint(point).OptimumSpeed, 1000.0f);
	}

	// Get the minimum speed at a point along a spline.
	UFUNCTION(BlueprintCallable, Category = Spline)
		float GetMinimumSpeedAtSplinePoint(int32 point) const
	{
		return GetPursuitPoint(point).MinimumSpeed;
	}

	UFUNCTION(BlueprintCallable, Category = Spline)
		void EmptySplineMeshes()
	{
		PursuitSplineMeshComponents.Empty();
	}

#pragma region NavigationSplines

public:

	// Post initialize the component.
	virtual void PostInitialize() override;

	// Get the average tunnel diameter over a set distance.
	float GetTunnelDiameterOverDistance(float distance, float overDistance, int32 direction, bool minimum) const;

	// Get the tunnel diameter at a distance along a spline.
	float GetTunnelDiameterAtDistanceAlongSpline(float distance) const;

	// Get the master distance at a distance along a spline.
	float GetMasterDistanceAtDistanceAlongSpline(float distance, float masterSplineLength) const;

	// Calculate distances along the master spline for this spline and each of its links.
	bool CalculateMasterSplineDistances(UPursuitSplineComponent* masterSpline, float masterSplineLength, float startingDistance, int32 degreesOfSeparation, bool report, int32 recalibrate = 0, int32 recalibrationAttempt = 0);

	// Have we calculated the master spline distances for this particular spline?
	bool HasMasterSplineDistances() const
	{ const auto& pointData = GetPursuitPointExtendedData(); return (pointData.Num() == 0 || pointData[0].MasterSplineDistance >= 0.0f); }

	// Get the spline point data at a particular point index.
	FPursuitPointData& GetPursuitPoint(int32 index) const
	{ auto& pointData = GetPursuitPointData(); return pointData[index]; }

	// Get the extended spline point data at a particular point index.
	FPursuitPointExtendedData& GetPursuitPointExtended(int32 index) const
	{ auto& pointData = GetPursuitPointExtendedData(); return pointData[index]; }

	// Clear all of the links along this spline.
	void ClearSplineLinks()
	{ SplineLinks.Empty(); }

	// Add a spline link to this spline component.
	void AddSplineLink(const FSplineLink& link);

	// Calculate the extended point data by examining the scene around the spline.
	void Build(bool fromMenu, bool performChecks, bool bareData, TArray<FVector>* intersectionPoints = nullptr);

	// Is this spline a dead-start where it can't be joined except when spawning a vehicle?
	bool DeadStart = false;

	// Is this spline a dead-end when spline following reselection at end is mandatory?
	bool DeadEnd = false;

	// The links to other splines along this spline.
	TArray<FSplineLink> SplineLinks;

	// The route choices that are available at various distances along this spline.
	TArray<FRouteChoice> RouteChoices;

	// The pursuit spline mesh components used to visualize this pursuit spline component.
	TArray<TWeakObjectPtr<UPursuitSplineMeshComponent>> PursuitSplineMeshComponents;

	// Helper function when using the Editor.
	virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;

	// Helper function when using the Editor.
	void ApplyComponentInstanceData(FPursuitSplineInstanceData* SplineInstanceData, bool bPostUCS);

protected:

	// Destroy the component.
	virtual void DestroyComponent(bool bPromoteChildren = false) override
	{ SplineLinks.Empty(); Super::DestroyComponent(bPromoteChildren); }

	// Calculate the sections of the spline.
	virtual void CalculateSections() override;

private:

	// Bind a point index key to fall within the spline.
	int32 BindKey(int32 key) const
	{ auto& pointData = GetPursuitPointData(); return (IsClosedLoop()
		? ((key < 0) ? pointData.Num() + key : ((key >= pointData.Num()) ? key % pointData.Num() : key))
		: FMath::Clamp(key, 0, pointData.Num() - 1)); }

	// Find the "this" point index key that falls within the spline.
	int32 ThisKey(float key, int32 direction = 1) const
	{ return BindKey((direction >= 0) ? FMath::FloorToInt(key) : FMath::CeilToInt(key)); }

	// Find the "next" point index key that falls within the spline.
	int32 NextKey(float key, int32 direction = 1) const
	{ return ThisKey(key, direction * -1); }

	// Bind an extended point index key to fall within the spline.
	int32 BindExtendedKey(TArray<FPursuitPointExtendedData>& pointData, int32 key) const
	{ return (IsClosedLoop()
		? ((key < 0) ? pointData.Num() + key : ((key >= pointData.Num()) ? key % pointData.Num() : key))
		: FMath::Clamp(key, 0, pointData.Num() - 1)); }

	// Find the "this" extended point index key that falls within the spline.
	int32 ThisExtendedKey(TArray<FPursuitPointExtendedData>& pointData, float key, int32 direction = 1) const
	{ return BindExtendedKey(pointData, (direction >= 0) ? FMath::FloorToInt(key) : FMath::CeilToInt(key)); }

	// Find the "next" extended point index key that falls within the spline.
	int32 NextExtendedKey(TArray<FPursuitPointExtendedData>& pointData, float key, int32 direction = 1) const
	{ return ThisExtendedKey(pointData, key, direction * -1); }

	// Get the extended point keys bounding a distance along the spline.
	void GetExtendedPointKeys(float distance, int32& key0, int32& key1, float& ratio) const;

	// The class that the master distances were found for this spline.
	int32 MasterDistanceClass = 0;

	// The parent actor for this spline.
	APursuitSplineActor* PursuitSplineParent = nullptr;

	// The point data, referenced from the parent actor.
	TArray<FPursuitPointData>& GetPursuitPointData() const;

	// The extended point data, referenced from the parent actor.
	TArray<FPursuitPointExtendedData>& GetPursuitPointExtendedData() const;

#pragma endregion NavigationSplines

#pragma region AINavigation

public:

	// Find the nearest distance along a spline to a given master distance.
	// The fewer iterations and samples you use the faster it will be, but also the less
	// accurate it will be. Conversely, the smaller the difference between startDistance
	// and endDistance the more accurate the result will be.
	float GetNearestDistanceToMasterDistance(float masterDistance, float startDistance = 0.0f, float endDistance = 0.0f, int32 numIterations = 4, int32 numSamples = 50, float earlyExitDistance = 10.0f) const;

	// Get the quaternion in world space at a distance along a spline.
	FQuat GetWorldSpaceQuaternionAtDistanceAlongSpline(float distance) const;

	// Get the up vector in world space at a distance along a spline.
	FVector GetWorldSpaceUpVectorAtDistanceAlongSpline(float distance) const;

	// Get the minimum optimum speed of the spline in kph over distance.
	float GetMinimumOptimumSpeedOverDistance(float distance, float& overDistance, int32 direction) const;

	// Get the minimum speed of the spline in kph over distance.
	float GetMinimumSpeedOverDistance(float distance, float& overDistance, int32 direction) const;

	// Is a distance and location along a spline within the open space around the spline?
	// (this is an inaccurate but cheap test)
	bool IsWorldLocationWithinRange(float distance, FVector location) const;

	// Get the maneuvering width at a distance along a spline.
	float GetWidthAtDistanceAlongSpline(float distance) const;

	// Get the minimum speed in kph at a distance along a spline.
	float GetMinimumSpeedAtDistanceAlongSpline(float distance) const;

	// Get the optimum speed in kph at a distance along a spline.
	float GetOptimumSpeedAtDistanceAlongSpline(float distance) const;

	// How much open space is the around a world location for a given spline offset and clearance angle?
	// In order for this to be useful, location should lie somewhere within the arc around splineOffset and range clearanceAngle.
	// splineOffset should always be in spline space.
	float GetClearance(float distance, FVector location, FVector splineOffset, float clearanceAngle, bool splineSpace, float padding) const;

	// Get all the clearances at a distance along the spline.
	TArray<float> GetClearances(float distance) const;

	// Is this spline about to merge with the given spline at the given distance?
	bool IsAboutToMergeWith(UPursuitSplineComponent* pursuitSpline, float distanceAlong);

	// Is this spline connected to a child spline?
	bool IsSplineConnected(UPursuitSplineComponent* child, float& atDistance, float& childDistance);

	// Get the careful driving at a distance along a spline.
	bool GetCarefulDrivingAtDistanceAlongSpline(float distance) const;

	// Get the world closest position for a distance along the spline.
	FVector GetWorldClosestPosition(float distance, bool raw = false) const;

	// Get the world closest offset for a distance along the spline.
	FVector GetWorldClosestOffset(float distance, bool raw = false) const;

#pragma endregion AINavigation

#pragma region VehicleTeleport

public:

	//Get the continuous surface of the spline over distance.
	bool GetContinuousSurfaceOverDistance(float distance, float& overDistance, int32 direction) const;

	// Rewind a distance to safe ground if possible.
	bool RewindToSafeGround(float& distance, float& initialSpeed);

#pragma endregion VehicleTeleport

#pragma region AIVehicleControl

	// Get the curvature of the spline in degrees over distance (in withRespectTo space).
	virtual FRotator GetCurvatureOverDistance(float distance, float& overDistance, int32 direction, const FQuat& withRespectTo, bool absolute) const override;

#pragma endregion AIVehicleControl

#pragma region PickupMissile

public:

	// Is a distance along a spline in open space?
	// splineOffset should always be in spline space.
	float GetClearanceOverDistance(float distance, float& overDistance, int32 direction, FVector worldLocation, FVector splineOffset, float clearanceAngle = 90.0f) const;

#pragma endregion PickupMissile

#pragma region CameraCinematics

	// Get the surface sections of the spline.
	virtual TArray<FSplineSection> GetSurfaceSections() const override;

	// Get the surface break property of the spline over distance.
	virtual bool GetSurfaceBreakOverDistance(float distance, float& overDistance, int32 direction) const override;

	// Get the grounded property of the spline over distance.
	virtual bool GetGroundedOverDistance(float distance, float& overDistance, int32 direction) const override;

	// Get the clearances of the spline.
	virtual TArray<float> GetClearancesFromSurface() const override;

	// How much open space is the around the spline center line for a given spline offset and clearance angle?
	float GetClearance(float distance, FVector splineOffset, float clearanceAngle = 90.0f) const;

	// Get the weather allowed property of the spline over distance.
	bool GetWeatherAllowedOverDistance(float distance, float& overDistance, int32 direction) const;

	// Is weather allowed at a distance along a spline? Between 0 and 1.
	float IsWeatherAllowed(float distance) const;

#pragma endregion CameraCinematics

};

/**
* Structure used to store spline data during RerunConstructionScripts.
***********************************************************************************/

USTRUCT()
struct FPursuitSplineInstanceData : public FSplineInstanceData
{
	GENERATED_BODY()

public:

	FPursuitSplineInstanceData() = default;

	explicit FPursuitSplineInstanceData(const UPursuitSplineComponent* SourceComponent)
		: FSplineInstanceData(SourceComponent)
	{ }

	virtual ~FPursuitSplineInstanceData() = default;

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		FSplineInstanceData::ApplyToComponent(Component, CacheApplyPhase);
		CastChecked<UPursuitSplineComponent>(Component)->ApplyComponentInstanceData(this, (CacheApplyPhase == ECacheApplyPhase::PostUserConstructionScript));
	}

	UPROPERTY()
		bool bClosedLoop = false;

	UPROPERTY()
		bool bClosedLoopPreUCS = false;

	UPROPERTY()
		EPursuitSplineType Type = EPursuitSplineType::General;

	UPROPERTY()
		EPursuitSplineType TypePreUCS = EPursuitSplineType::General;
};
