#include "VehicleDriftInput.h"
#include "FGInputLibrary.h"
#include "UObject/UnrealType.h"
#include "InputAction.h"
#include "Input/FGInputMappingContext.h"
#include "PlayerMappableKeySettings.h"
#include "GameFramework/PlayerController.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    TStrongObjectPtr<UFGInputMappingContext> VehicleControls;
    TWeakObjectPtr<UInputAction> DriftAction;
}

void VehicleDriftInput::Register()
{
    if (DriftAction.IsValid() || IsRunningDedicatedServer()) return;
    auto* Context = LoadObject<UFGInputMappingContext>(nullptr,
        TEXT("/Game/FactoryGame/Inputs/Vehicles/Context/MC_Vehicles.MC_Vehicles"));
    if (!Context) return;
    VehicleControls.Reset(Context);
    auto* Action = NewObject<UInputAction>(Context, TEXT("IA_VehicleOverclock_Drift"));
    Action->bConsumeInput = false;
    Action->ActionDescription = NSLOCTEXT("VehicleOverclock", "DriftHelp", "Hold to fire the researched booster. Steer for a drift or drive a Tractor for forward thrust.");
    auto* Settings = NewObject<UPlayerMappableKeySettings>(Action);
    Settings->Name = MappingName;
    Settings->DisplayName = NSLOCTEXT("VehicleOverclock", "Drift", "Vehicle drift");
    Settings->DisplayCategory = NSLOCTEXT("VehicleOverclock", "Vehicles", "Vehicles");
    FindFieldChecked<FObjectPropertyBase>(UInputAction::StaticClass(), TEXT("PlayerMappableKeySettings"))
        ->SetObjectPropertyValue_InContainer(Action, Settings);
    Context->MapKey(Action, EKeys::LeftControl);
    DriftAction = Action;
}

UPlayerMappableKeySettings* VehicleDriftInput::KeySettings()
{
    return DriftAction.IsValid() ? DriftAction->GetPlayerMappableKeySettings().Get() : nullptr;
}

bool VehicleDriftInput::IsHeld(APlayerController* Controller, bool& JustPressed)
{
    JustPressed = false;
    if (!Controller || !DriftAction.IsValid()) return false;
    FKey Key;
    TArray<FKey> Modifiers;
    if (!UFGInputLibrary::GetCurrentMappingForAction(Controller, MappingName, Key, Modifiers)
        || !Key.IsValid()) return false;
    for (const FKey& Modifier : Modifiers)
        if (!Controller->IsInputKeyDown(Modifier)) return false;
    JustPressed = Controller->WasInputKeyJustPressed(Key);
    return Controller->IsInputKeyDown(Key);
}
