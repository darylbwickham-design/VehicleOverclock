#pragma once

#include "CoreMinimal.h"
#include "FGResearchTree.h"
#include "FGSchematic.h"
#include "AvailabilityDependencies/FGSchematicPurchasedDependency.h"
#include "Unlocks/FGUnlockInfoOnly.h"
#include "Subsystems/WorldSubsystem.h"
#include "VehicleHandlingResearch.generated.h"

struct FVehicleHandlingRanks
{
    int32 Grip = 0;
    int32 Aero = 0;
    int32 Brakes = 0;
    bool bCalibrated = false;
    bool bStability = false;
    static FVehicleHandlingRanks Read(UWorld* World);
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
