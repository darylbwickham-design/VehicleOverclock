#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "FGRemoteCallObject.h"
#include "FGInventoryComponent.h"
#include "FGSaveInterface.h"
#include "GameFramework/Actor.h"
#include "VehicleHandlingResearch.h"
#include "VehicleClock.generated.h"

class AFGTrain;
class AFGVehicle;
class AFGCharacterPlayer;
class UFGItemDescriptor;

namespace VehicleClock
{
    constexpr int32 SlotCount = 3;
    constexpr int32 Stock = 100;
    inline int32 Limit(int32 Shards) { return Stock + 50 * Shards; }
    float Speed(float Clock, const AFGVehicle* Vehicle = nullptr);
    float ClockForSpeed(float Speed, const AFGVehicle* Vehicle);
    inline float Energy(float Clock, int32 Efficiency)
    {
        // Each rank removes one sixth of the original overclock surcharge.
        const float Surcharge = FMath::Pow(Clock, 1.51294159f) - 1.f;
        return 1.f + Surcharge * (1.f - FMath::Clamp(Efficiency, 0, 4) / 6.f);
    }
    TSubclassOf<UFGItemDescriptor> Shard();
    AFGVehicle* DrivenBy(APlayerController* Controller);
    UClass* MenuClass(const AFGVehicle* Vehicle);
    AFGCharacterPlayer* Operator(APlayerController* Player, AFGVehicle* Vehicle);
    bool IsBoosterFuel(TSubclassOf<UFGItemDescriptor> Item);
    bool SupportsBooster(const AFGVehicle* Vehicle);
    bool NeedsFuel(const AFGVehicle* Vehicle);
}

UENUM()
enum class EClockEdit : uint8 { Insert, Remove, SetPercent };

// The save actor records the reserve stack alongside the burning unit. The
// live inventory belongs to the vehicle so the build gun can refund it.
UCLASS(Transient)
class UVehicleBoosterInventory : public UFGInventoryComponent
{
    GENERATED_BODY()
public:
    bool ShouldSave_Implementation() const override { return false; }
};

// Stores fitted shards, unopened booster fuel and the charge in the burning unit.
UCLASS(NotPlaceable)
class AVehicleClock : public AActor, public IFGSaveInterface
{
    GENERATED_BODY()
public:
    AVehicleClock();
    static AVehicleClock* Find(const AFGVehicle* Vehicle);
    void FitTo(AFGVehicle* InVehicle, AFGCharacterPlayer* Installer = nullptr);
    bool SyncDriver();
    bool CanEdit(AFGCharacterPlayer* Player) const { return !ShardOwner || ShardOwner == Player; }
    void UpdateDrive();
    bool HasBoosterFuel() const { return FuelCount > 0 || FuelCharge > 0.f; }
    UFGInventoryComponent* GetBoosterInventory();
    void RefreshBoosterStack();
    void ClearShardOwner();
    void RequestDrift(bool Held);
    void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    bool ShouldSave_Implementation() const override;
    bool NeedTransform_Implementation() override { return false; }
    void GatherDependencies_Implementation(TArray<UObject*>& Out) override;
    void BeginPlay() override;
    void EndPlay(const EEndPlayReason::Type Reason) override;
    void Tick(float DeltaSeconds) override;

    UPROPERTY(SaveGame, Replicated) TObjectPtr<AFGVehicle> Vehicle;
    UPROPERTY(SaveGame, Replicated) int32 Shards = 0;
    UPROPERTY(SaveGame, Replicated) TSubclassOf<UFGItemDescriptor> FuelType;
    UPROPERTY(SaveGame, Replicated) int32 FuelCount = 0;
    UPROPERTY(SaveGame, Replicated) TSubclassOf<UFGItemDescriptor> BurningType;
    UPROPERTY(SaveGame, Replicated) float FuelCharge = 0.f;
    UPROPERTY(Replicated) int8 ThrustDirection = 0; // -1/1 for drift, 2 for forward thrust
    UPROPERTY(SaveGame, Replicated) int32 Percent = VehicleClock::Stock;

private:
    UPROPERTY(Transient) TObjectPtr<UVehicleBoosterInventory> BoosterInventory;
    UFUNCTION() void VehicleDestroyed(AActor* DestroyedVehicle);
    UFUNCTION() void ControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);
    UPROPERTY(SaveGame) TObjectPtr<AFGCharacterPlayer> ShardOwner;
    UPROPERTY(SaveGame) bool bRefundPending = false;
    UPROPERTY(SaveGame) bool bWaitingForDriver = false;
    void ReturnShards(AActor* DropAt);
    void SetWheelClock(float Clock);
    void SetRailClock(float Clock);
    void SetTrainSpeedLimit();
    void UpdateHandling(float DeltaSeconds);
    void SetWheelHandling(float GripBonus, float BrakeBonus, float RearSlip = 0.f);
    void SetBallast(int32 Rank);
    void SetSteering(int32 Rank);
    float MeterBooster(float Seconds);
    float DriftEntrySpeed = 0.f;
    void UpdateBoost(float Seconds, float Steering, bool Grounded);
    UPROPERTY(Replicated) bool bDriftHeld = false;
    float DriftBlend = 0.f, LastRearSlip = 0.f;
    int32 BallastRank = 0;
    int32 SteeringRank = 0;
    int32 AppliedEfficiency = 0;
    float StockMass = 0.f;
    FVector StockCOMNudge = FVector::ZeroVector;
    void SetRailBraking(float Bonus);
    float Applied = 1.f;
    FVehicleHandlingRanks Handling;
    float StockRoadSpeed = 0.f;
    float StockRailAirBrake = 0.f;
    float LastGripBonus = 0.f;
    float LastBrakeBonus = 0.f;
    float UntilResearchCheck = 0.f;
    float UntilDriverCheck = 0.f;
    float StockFinalDrive = 0.f;
    float StockTorque = 0.f;
    float StockDrag = 0.f;
    float StockFuel = 0.f;
    float StockRailSpeed = 0.f;
    FFloatInterval StockRailPower{0.f, 0.f};
    UPROPERTY(Transient) FRuntimeFloatCurve StockEffort;
    TWeakObjectPtr<AFGTrain> TunedTrain;
};

UCLASS()
class UVehicleClockRemote : public UFGRemoteCallObject
{
    GENERATED_BODY()
public:
    UFUNCTION(Server, Reliable) void Edit(AFGVehicle* ExpectedVehicle, EClockEdit Operation, int32 Value);
    UFUNCTION(Server, Reliable) void TransferFuel(AFGVehicle* Vehicle, int32 SourceSlot, TSubclassOf<UFGItemDescriptor> Item, bool Unload);
    UFUNCTION(Server, Reliable) void Drift(AFGVehicle* ExpectedVehicle, bool Held);
    UFUNCTION(Client, Reliable) void Feedback(AFGVehicle* ExpectedVehicle, const FString& Message);
    void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(Replicated) bool bReady = true;
    TWeakObjectPtr<AFGVehicle> FeedbackVehicle;
    FString LastMessage;
};


