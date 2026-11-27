#include "VehicleBoostOptions.h"
#include "VehicleDriftInput.h"
#include "Configuration/ConfigManager.h"
#include "FGGameUserSettings.h"
#include "FGInputLibrary.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SInputKeySelector.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "VehicleOverclockOptions"

namespace
{
    TArray<FKey> ModifiersFor(const FInputChord& Chord)
    {
        TArray<FKey> Keys;
        if (Chord.bCtrl) Keys.Add(EKeys::LeftControl);
        if (Chord.bShift) Keys.Add(EKeys::LeftShift);
        if (Chord.bAlt) Keys.Add(EKeys::LeftAlt);
        if (Chord.bCmd) Keys.Add(EKeys::LeftCommand);
        return Keys;
    }
}

UVehicleBoostConfiguration::UVehicleBoostConfiguration()
{
    ConfigId.ModReference = TEXT("vehiclephysics");
    DisplayName = LOCTEXT("Title", "Vehicle Overclock");
    RootSection = CreateDefaultSubobject<UVehicleBoostSettingsSection>(TEXT("Settings"));
}

UUserWidget* UVehicleBoostSettingsSection::CreateEditorWidget_Implementation(UUserWidget* ParentWidget) const
{
    return ParentWidget ? CreateWidget<UVehicleBoostOptions>(ParentWidget, UVehicleBoostOptions::StaticClass()) : nullptr;
}

void UVehicleBoostOptionsRegistration::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (IsRunningDedicatedServer()) return;
    Collection.InitializeDependency<UConfigManager>();
    GetGameInstance()->GetSubsystem<UConfigManager>()->RegisterModConfiguration(UVehicleBoostConfiguration::StaticClass());
}

FInputChord UVehicleBoostOptions::CurrentKey() const
{
    const UFGGameUserSettings* Settings = UFGGameUserSettings::GetFGGameUserSettings();
    FFGCustomInputActionMapping Saved;
    if (!Settings || !Settings->GetPlayerMappedKey(VehicleDriftInput::MappingName, Saved))
        return FInputChord(EKeys::LeftControl);
    const auto& Mods = Saved.ModifierKeys;
    return FInputChord(Saved.PrimaryKey,
        Mods.Contains(EKeys::LeftShift) || Mods.Contains(EKeys::RightShift),
        Mods.Contains(EKeys::LeftControl) || Mods.Contains(EKeys::RightControl),
        Mods.Contains(EKeys::LeftAlt) || Mods.Contains(EKeys::RightAlt),
        Mods.Contains(EKeys::LeftCommand) || Mods.Contains(EKeys::RightCommand));
}

void UVehicleBoostOptions::SaveKey(const FInputChord& Chord)
{
    if (auto* PC = GetOwningPlayer())
    {
        const TArray<FKey> Mods = ModifiersFor(Chord);
        UFGInputLibrary::RebindEnhancedKeyMapping(PC, VehicleDriftInput::MappingName, Chord.Key, Mods,
            VehicleDriftInput::KeySettings());
        if (auto* Settings = UFGGameUserSettings::GetFGGameUserSettings())
        {
            // The main menu has no active vehicle context. Store the same native override there too.
            if (CurrentKey() != Chord)
                Settings->AddPlayerMappedKey(FFGCustomInputActionMapping(VehicleDriftInput::MappingName, Chord.Key, Mods));
            Settings->SaveSettings();
        }
    }
    PendingKey.Reset();
}

void UVehicleBoostOptions::SelectKey(const FInputChord& Chord)
{
    PendingKey.Reset();
    if (!Chord.Key.IsValid() || Chord == CurrentKey() || !GetOwningPlayer()) return;
    TArray<FEnhancedActionKeyMapping> Hard, Soft;
    UFGInputLibrary::GetOverlappingEnhancedKeyMappings(GetOwningPlayer(), VehicleDriftInput::MappingName,
        Chord.Key, ModifiersFor(Chord), Hard, Soft);
    for (const auto& Binding : Hard)
    {
        if (Binding.GetMappingName() == VehicleDriftInput::MappingName) continue;
        Conflict = FText::Format(LOCTEXT("Conflict", "{0} is bound to {1}. Replace that binding?"),
            Chord.Key.GetDisplayName(), Binding.GetDisplayName());
        PendingKey = Chord;
        return;
    }
    SaveKey(Chord);
}

TSharedRef<SWidget> UVehicleBoostOptions::RebuildWidget()
{
    return SNew(SVerticalBox)
    + SVerticalBox::Slot().AutoHeight().Padding(12,8)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
        [SNew(STextBlock).Text(LOCTEXT("Boost", "Boost hotkey"))]
        + SHorizontalBox::Slot().AutoWidth()
        [
            SNew(SBox).MinDesiredWidth(160)
            [
                SNew(SInputKeySelector).Tag(TEXT("VehicleBoostKey"))
                .SelectedKey_UObject(this, &UVehicleBoostOptions::CurrentKey)
                .OnKeySelected_UObject(this, &UVehicleBoostOptions::SelectKey)
                .KeySelectionText(LOCTEXT("Capture", "Press a key"))
                .AllowModifierKeys(true).AllowGamepadKeys(false)
            ]
        ]
    ]
    + SVerticalBox::Slot().AutoHeight().Padding(12,2)
    [
        SNew(SHorizontalBox).Visibility_Lambda([this] { return PendingKey.IsSet() ? EVisibility::Visible : EVisibility::Collapsed; })
        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
        [SNew(STextBlock).Text_Lambda([this]{ return Conflict; }).AutoWrapText(true)]
        + SHorizontalBox::Slot().AutoWidth().Padding(4,0)
        [SNew(SButton).Text(LOCTEXT("Replace","Replace")).OnClicked_Lambda([this] {
            if (PendingKey.IsSet()) { const auto Key = PendingKey.GetValue(); SaveKey(Key); }
            return FReply::Handled();
        })]
        + SHorizontalBox::Slot().AutoWidth()
        [SNew(SButton).Text(LOCTEXT("Cancel","Cancel")).OnClicked_Lambda([this] {
            PendingKey.Reset(); return FReply::Handled();
        })]
    ];
}

#undef LOCTEXT_NAMESPACE
