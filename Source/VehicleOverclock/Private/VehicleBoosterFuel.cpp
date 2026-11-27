#include "VehicleClock.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "FGVehicle.h"
#include "Resources/FGItemDescriptor.h"

bool VehicleClock::SupportsBooster(const AFGVehicle* Vehicle)
{
    const auto Family = VehicleFamily(Vehicle);
    return Family == EVehicleFamily::Tractor || Family == EVehicleFamily::Explorer || Family == EVehicleFamily::Truck;
}

bool VehicleClock::IsBoosterFuel(TSubclassOf<UFGItemDescriptor> Item)
{
    if (!Item) return false;
    static const TSet<FSoftObjectPath> Fuels = {
        FSoftObjectPath(TEXT("/Game/FactoryGame/Resource/Parts/CompactedCoal/Desc_CompactedCoal.Desc_CompactedCoal_C")),
        FSoftObjectPath(TEXT("/Game/FactoryGame/Resource/Parts/Turbofuel/Desc_TurboFuel.Desc_TurboFuel_C")),
        FSoftObjectPath(TEXT("/Game/FactoryGame/Resource/Parts/RocketFuel/Desc_PackagedRocketFuel.Desc_PackagedRocketFuel_C")),
        FSoftObjectPath(TEXT("/Game/FactoryGame/Resource/Parts/IonizedFuel/Desc_PackagedIonizedFuel.Desc_PackagedIonizedFuel_C"))
    };
    return Fuels.Contains(FSoftObjectPath(Item.Get()));
}

UFGInventoryComponent* AVehicleClock::GetBoosterInventory()
{
    if (IsValid(BoosterInventory)) return BoosterInventory;
    if (!HasAuthority() || !IsValid(Vehicle) || Vehicle->IsActorBeingDestroyed()) return nullptr;
    BoosterInventory = NewObject<UVehicleBoosterInventory>(Vehicle, NAME_None, RF_Transient);
    Vehicle->AddInstanceComponent(BoosterInventory);
    BoosterInventory->SetIsReplicated(false);
    BoosterInventory->SetDefaultSize(1);
    BoosterInventory->RegisterComponent();
    BoosterInventory->Resize(1);
    if (FuelCount > 0 && FuelType) BoosterInventory->AddStack(FInventoryStack(FuelCount, FuelType));
    return BoosterInventory;
}

void AVehicleClock::RefreshBoosterStack()
{
    FInventoryStack Stack;
    if (IsValid(BoosterInventory) && BoosterInventory->GetStackFromIndex(0, Stack) && Stack.NumItems > 0)
    {
        FuelCount = Stack.NumItems;
        FuelType = Stack.Item.GetItemClass();
    }
    else
    {
        FuelCount = 0;
        FuelType = nullptr;
    }
}

void UVehicleClockRemote::TransferFuel_Implementation(AFGVehicle* Target, int32 SourceSlot,
    TSubclassOf<UFGItemDescriptor> Item, bool Unload)
{
    auto* Person = VehicleClock::Operator(GetOwnerPlayerController(), Target);
    auto* Inventory = Person ? Person->GetInventory() : nullptr;
    if (!Inventory || Inventory->IsLocked() || !VehicleClock::SupportsBooster(Target)) return;
    auto* Clock = AVehicleClock::Find(Target);

    if (Unload)
    {
        if (!Clock || Clock->FuelCount <= 0 || Clock->FuelType != Item) return;
        auto* Reserve = Clock->GetBoosterInventory();
        if (!Reserve) return;
        const int32 Stored = Reserve->GetNumItems(Item);
        if (Stored <= 0) return;
        const int32 Returned = Inventory->AddStack(FInventoryStack(Stored, Item), true);
        if (Returned > 0) Reserve->RemoveFromIndex(0, Returned);
        Clock->RefreshBoosterStack();
        Feedback(Target, Clock->FuelCount ? TEXT("Make room in your inventory to unload the remaining fuel.") : TEXT(""));
    }
    else
    {
        if (SourceSlot < 0 || SourceSlot >= Inventory->GetSizeLinear() || !VehicleClock::IsBoosterFuel(Item)) return;
        FInventoryStack Stack;
        if (!Inventory->GetStackFromIndex(SourceSlot, Stack) || Stack.Item.GetItemClass() != Item || Stack.NumItems <= 0) return;
        const auto Ranks = FVehicleHandlingRanks::Read(GetWorld(), Target);
        if (!Ranks.bDrift && !Ranks.bRocket && !Ranks.bAirThruster)
        {
            const auto Family = VehicleFamily(Target);
            Feedback(Target, Family == EVehicleFamily::Tractor ? TEXT("Research Tractor · Forward rocket at the MAM.") :
                Family == EVehicleFamily::Explorer ? TEXT("Research Controlled drift or Explorer · Air thrusters at the MAM.") :
                TEXT("Research Controlled drift at the MAM."));
            return;
        }
        if (Clock && Clock->FuelCount && Clock->FuelType != Item)
        {
            Feedback(Target, TEXT("Unload the current fuel stack before changing fuel."));
            return;
        }
        const int32 Space = UFGItemDescriptor::GetStackSize(Item) - (Clock ? Clock->FuelCount : 0);
        const int32 Transfer = FMath::Min(Space, Stack.NumItems);
        if (Transfer <= 0) { Feedback(Target, TEXT("Booster fuel slot is full.")); return; }
        if (!Clock)
        {
            Clock = GetWorld()->SpawnActor<AVehicleClock>();
            if (!Clock) return;
            Clock->FitTo(Target);
        }
        auto* Reserve = Clock->GetBoosterInventory();
        if (!Reserve) return;
        const int32 Added = Reserve->AddStack(FInventoryStack(Transfer, Item), true);
        if (Added <= 0) return;
        Inventory->RemoveFromIndex(SourceSlot, Added);
        Clock->RefreshBoosterStack();
        Feedback(Target, TEXT(""));
    }
    Clock->ForceNetUpdate();
}
