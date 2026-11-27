#pragma once
#include "CoreMinimal.h"

class APlayerController;
class UPlayerMappableKeySettings;
namespace VehicleDriftInput
{
    inline const FName MappingName(TEXT("VehicleOverclock_Drift"));
    UPlayerMappableKeySettings* KeySettings();
    void Register();
    bool IsHeld(APlayerController* Controller, bool& JustPressed);
}
