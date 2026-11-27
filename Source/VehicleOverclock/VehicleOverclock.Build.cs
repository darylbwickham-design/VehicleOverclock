using UnrealBuildTool;

public class VehicleOverclock : ModuleRules
{
    public VehicleOverclock(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "FactoryGame", "SML",
            "ChaosVehicles", "ChaosVehiclesCore", "PhysicsCore", "Chaos", "UMG", "Slate", "SlateCore", "InputCore", "EnhancedInput", "Projects"
        });
        RuntimeDependencies.Add("$(PluginDir)/Resources/Research/*.png", StagedFileType.NonUFS);
    }
}
