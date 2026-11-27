#include "VehicleClock.h"
#include "FGGameMode.h"
#include "GameFramework/GameModeBase.h"
#include "Modules/ModuleManager.h"

class FVehicleOverclockModule : public IModuleInterface
{
    FDelegateHandle RegisterHandle;
    void RegisterRemote(AGameModeBase* GameMode)
    {
        if (auto* Mode = Cast<AFGGameMode>(GameMode))
            Mode->RegisterRemoteCallObjectClass(UVehicleClockRemote::StaticClass());
    }
public:
    void StartupModule() override
    {
        RegisterHandle = FGameModeEvents::GameModeInitializedEvent.AddRaw(this, &FVehicleOverclockModule::RegisterRemote);
    }
    void ShutdownModule() override
    {
        FGameModeEvents::GameModeInitializedEvent.Remove(RegisterHandle);
    }
};
IMPLEMENT_MODULE(FVehicleOverclockModule, VehicleOverclock)
