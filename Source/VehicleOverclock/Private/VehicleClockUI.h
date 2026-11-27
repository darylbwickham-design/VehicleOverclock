#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/WorldSubsystem.h"
#include "VehicleClock.h"
#include "VehicleClockUI.generated.h"

class IInputProcessor;
class UFGInteractWidget;
class UVehicleFuelSlot;

class UHorizontalBox;
class UNamedSlot;
class USlider;
class UEditableText;
class UVehicleClockPage;

/** Mouse and inventory-drop target on top of a stock machine cost slot. */
UCLASS()
class UVehicleClockShardSlot : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(UVehicleClockPage* InPage, int32 InIndex);
protected:
    TSharedRef<SWidget> RebuildWidget() override;
    FReply NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
    bool NativeOnDragOver(const FGeometry&, const FDragDropEvent&, UDragDropOperation*) override;
    bool NativeOnDrop(const FGeometry&, const FDragDropEvent&, UDragDropOperation*) override;
private:
    TWeakObjectPtr<UVehicleClockPage> Page;
    int32 Index = 0;
};

UCLASS()
class UVehicleClockPage : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(AFGVehicle* InVehicle, bool ShowDetails);
    bool TransferFrom(UObject* SourceSlot, bool Commit);
protected:
    TSharedRef<SWidget> RebuildWidget() override;
    void NativeTick(const FGeometry& Geometry,float DeltaSeconds) override;
private:
    friend class UVehicleClockShardSlot;
    FReply Edit(EClockEdit Operation, int32 Value = 0);
    TSharedRef<SWidget> BuildMachineControls();
    void RefreshControls(bool Force = false);
    bool CanDropShard(UDragDropOperation* Operation) const;
    UFUNCTION() void BeginClockDrag();
    UFUNCTION() void EndClockDrag();
    UFUNCTION() void ChangeClock(float Value);
    UFUNCTION() void CommitPercent(const FText& Text, ETextCommit::Type How);
    UFUNCTION() void CommitSpeed(const FText& Text, ETextCommit::Type How);
    int32 Slots() const;
    int32 Clock() const;
    UPROPERTY(Transient) TObjectPtr<AFGVehicle> Vehicle;
    UPROPERTY(Transient) TObjectPtr<UVehicleFuelSlot> FuelSlot;
    UPROPERTY(Transient) FSlateBrush MachineBrush;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> Machine;
    UPROPERTY(Transient) TObjectPtr<USlider> ClockSlider;
    UPROPERTY(Transient) TObjectPtr<UEditableText> PercentInput;
    UPROPERTY(Transient) TObjectPtr<UEditableText> SpeedInput;
    int32 ShownClock = -1, ShownShards = -1, DragPercent = 100;
    bool bDragging = false, bUpdatingSlider = false;
    bool bDetails = false;
    FVehicleHandlingRanks Learned;
    float UntilRefresh = 0.f;
};

UCLASS()
class UVehicleFuelSlot : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(AFGVehicle* Target, UVehicleClockPage* Host);
protected:
    TSharedRef<SWidget> RebuildWidget() override;
    void NativeTick(const FGeometry&, float Seconds) override;
    bool NativeOnDragOver(const FGeometry&, const FDragDropEvent&, UDragDropOperation*) override;
    bool NativeOnDrop(const FGeometry&, const FDragDropEvent&, UDragDropOperation*) override;
    FReply NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
private:
    TWeakObjectPtr<AFGVehicle> Vehicle;
    TWeakObjectPtr<UVehicleClockPage> Page;
    TWeakObjectPtr<AVehicleClock> Clock;
    UPROPERTY(Transient) FSlateBrush ItemBrush;
    UPROPERTY(Transient) FSlateBrush SocketBrush;
    UPROPERTY(Transient) TObjectPtr<UClass> ShownItem;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> EmptyIcon;
    float SearchIn = 0.f;
    bool bHUD = false;
};

UCLASS()
class UVehicleClockHUD : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    void Tick(float DeltaTime) override;
    TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UVehicleClockHUD, STATGROUP_Tickables); }
    void Deinitialize() override;
    UVehicleClockPage* ActiveFuelPage() const;
private:
    bool Attach(UFGInteractWidget* Host, AFGVehicle* Target);
    void Detach();
    void UpdateFuelHUD(APlayerController* PC, AFGVehicle* Driven, bool InMenu);
    UPROPERTY(Transient) TObjectPtr<UVehicleFuelSlot> FuelHUD;
    TWeakObjectPtr<AFGVehicle> HUDVehicle;
    TSharedPtr<IInputProcessor> FuelInput;
    void RegenerateTabs(int32 Selected);
    TWeakObjectPtr<UFGInteractWidget> Menu;
    TWeakObjectPtr<AFGVehicle> Vehicle;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> Headers;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> Slider;
    UPROPERTY(Transient) TObjectPtr<UHorizontalBox> Pages;
    UPROPERTY(Transient) TObjectPtr<UNamedSlot> Body;
    UPROPERTY(Transient) TObjectPtr<UWidget> OriginalBody;
    int32 InitialDefinitions = 0;
    int32 InitialPageCount = 0;
    TWeakObjectPtr<AFGVehicle> DriftVehicle;
    bool bDriftPressed = false;
    float UntilFlightInput = 0.f;
    bool bOpenedWhileDriving = false;
};
