#pragma once
#include "CoreMinimal.h"

class APlayerController;
class UPlayerMappableKeySettings;
namespace VehicleDriftInput
{
    inline const FName MappingName(TEXT("VehicleOverclock_Drift"));
    inline const FName AirMappingName(TEXT("VehicleOverclock_AirControl"));
    UPlayerMappableKeySettings* KeySettings(bool AirControl = false);
    void Register();
    bool IsHeld(APlayerController* Controller, bool& JustPressed, bool AirControl = false);
}
