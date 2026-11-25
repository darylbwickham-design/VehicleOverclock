#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "FGRemoteCallObject.h"
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
    inline float Speed(float Clock) { return 1.f + 0.6f * (Clock - 1.f); }
    // log(4) / log(2.5) gives four times the energy demand at full clock.
    inline float Energy(float Clock) { return FMath::Pow(Clock, 1.51294159f); }
    TSubclassOf<UFGItemDescriptor> Shard();
    AFGVehicle* DrivenBy(APlayerController* Controller);
    UClass* MenuClass(const AFGVehicle* Vehicle);
    AFGCharacterPlayer* Operator(APlayerController* Player, AFGVehicle* Vehicle);
    bool NeedsFuel(const AFGVehicle* Vehicle);
}

UENUM()
enum class EClockEdit : uint8 { Insert, Remove, SetPercent };

// A save actor is created only after the first shard is fitted. It can also
// represent an upgrade on a vehicle placed before the mod was installed.
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
    void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    bool ShouldSave_Implementation() const override;
    bool NeedTransform_Implementation() override { return false; }
    void GatherDependencies_Implementation(TArray<UObject*>& Out) override;
    void BeginPlay() override;
    void EndPlay(const EEndPlayReason::Type Reason) override;
    void Tick(float DeltaSeconds) override;

    UPROPERTY(SaveGame, Replicated) TObjectPtr<AFGVehicle> Vehicle;
    UPROPERTY(SaveGame, Replicated) int32 Shards = 0;
    UPROPERTY(SaveGame, Replicated) int32 Percent = VehicleClock::Stock;

private:
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
    void SetWheelHandling(float GripBonus, float BrakeBonus);
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
    UFUNCTION(Client, Reliable) void Feedback(AFGVehicle* ExpectedVehicle, const FString& Message);
    void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(Replicated) bool bReady = true;
    TWeakObjectPtr<AFGVehicle> FeedbackVehicle;
    FString LastMessage;
};


