#pragma once

#include "Blueprint/UserWidget.h"
#include "Configuration/ModConfiguration.h"
#include "Framework/Commands/InputChord.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VehicleBoostOptions.generated.h"

UCLASS()
class UVehicleBoostOptions : public UUserWidget
{
    GENERATED_BODY()
protected:
    TSharedRef<SWidget> RebuildWidget() override;
private:
    void SelectKey(const FInputChord& Chord);
    void SaveKey(const FInputChord& Chord);
    FInputChord CurrentKey() const;
    TOptional<FInputChord> PendingKey;
    FText Conflict;
};

UCLASS()
class UVehicleBoostSettingsSection : public UConfigPropertySection
{
    GENERATED_BODY()
public:
    UUserWidget* CreateEditorWidget_Implementation(UUserWidget* ParentWidget) const override;
};

UCLASS()
class UVehicleBoostConfiguration : public UModConfiguration
{
    GENERATED_BODY()
public:
    UVehicleBoostConfiguration();
};

UCLASS()
class UVehicleBoostOptionsRegistration : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    void Initialize(FSubsystemCollectionBase& Collection) override;
};
