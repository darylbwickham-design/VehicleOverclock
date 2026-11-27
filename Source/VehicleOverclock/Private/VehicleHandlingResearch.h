#pragma once

#include "CoreMinimal.h"
#include "FGResearchTree.h"
#include "FGSchematic.h"
#include "AvailabilityDependencies/FGSchematicPurchasedDependency.h"
#include "Unlocks/FGUnlockInfoOnly.h"
#include "Subsystems/WorldSubsystem.h"
#include "VehicleHandlingResearch.generated.h"

class AFGVehicle;

enum class EVehicleFamily : uint8 { Other, Tractor, Explorer, Truck, Rail };
EVehicleFamily VehicleFamily(const AFGVehicle* Vehicle);

struct FVehicleHandlingRanks
{
    int32 Grip = 0;
    int32 Aero = 0;
    int32 Brakes = 0;
    bool bCalibrated = false;
    bool bStability = false;
    int32 Ballast = 0;
    int32 Steering = 0;
    int32 Efficiency = 0;
    bool bDrift = false;
    bool bRocket = false;
    int32 DriftControl = 0;
    int32 AirControl = 0;
    int32 Landing = 0;
    bool bDriftExit = false;
    bool bAirThruster = false;
    static FVehicleHandlingRanks Read(UWorld* World, const AFGVehicle* Vehicle);
    static bool OverclockUnlocked(UWorld* World);
};

UCLASS()
class UVehicleHandlingPrerequisite : public UFGSchematicPurchasedDependency
{
    GENERATED_BODY()
public:
    void Set(const TArray<TSubclassOf<UFGSchematic>>& Parents);
};

UCLASS()
class UVehicleHandlingUnlock : public UFGUnlockInfoOnly
{
    GENERATED_BODY()
public:
    void Set(const FText& Title, const FText& Description, UTexture2D* Image);
};

UCLASS(Abstract)
class UVehicleHandlingSchematic : public UFGSchematic
{
    GENERATED_BODY()
public:
    FPrimaryAssetId GetPrimaryAssetId() const override;
    void Configure(const TCHAR* Title, const TCHAR* Description, float Seconds,
        const TArray<FItemAmount>& Cost, const TArray<TSubclassOf<UFGSchematic>>& Parents, UTexture2D* Icon);
};

UCLASS()
class UVehicleCalibration : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UVehicleGripI : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UVehicleGripII : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UVehicleGripIII : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UVehicleAeroI : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UVehicleAeroII : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UVehicleAeroIII : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UVehicleBrakesI : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UVehicleBrakesII : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UVehicleBrakesIII : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UVehicleStability : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};
UCLASS()
class UTractorWorks : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UExplorerWorks : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UExplorerBallast : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UExplorerDampers : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UExplorerBrakes : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UExplorerStability : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UTruckWorks : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UTruckGrip : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UTruckBrakes : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UTruckStability : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UVehicleDrift : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UTractorRocket : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UFleetEfficiencyI : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UFleetEfficiencyII : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UFleetEfficiencyIII : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UFleetEfficiencyIV : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UExplorerSteeringI : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UExplorerSteeringII : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UExplorerSteeringIII : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UExplorerDriftI : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};
UCLASS()
class UExplorerDriftII : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};
UCLASS()
class UExplorerDriftExit : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};
UCLASS()
class UExplorerAirI : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};
UCLASS()
class UExplorerAirII : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};
UCLASS()
class UExplorerAirIII : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};
UCLASS()
class UExplorerAirThruster : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};
UCLASS()
class UExplorerLandingI : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};
UCLASS()
class UExplorerLandingII : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};
UCLASS()
class UTractorAirI : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};
UCLASS()
class UTractorAirII : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};
UCLASS()
class UTractorAirIII : public UVehicleHandlingSchematic
{
    GENERATED_BODY()
};

UCLASS()
class UVehicleHandlingTree : public UFGResearchTree
{
    GENERATED_BODY()
public:
    FPrimaryAssetId GetPrimaryAssetId() const override;
    bool Configure();
};

UCLASS()
class UVehicleHandlingResearch : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    void Initialize(FSubsystemCollectionBase& Collection) override;
};
