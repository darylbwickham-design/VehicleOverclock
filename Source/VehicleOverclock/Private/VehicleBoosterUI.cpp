#include "VehicleClockUI.h"
#include "FGCharacterPlayer.h"
#include "FGHUD.h"
#include "FGInventoryComponent.h"
#include "FGPlayerController.h"
#include "FGVehicle.h"
#include "Resources/FGItemDescriptor.h"
#include "Blueprint/DragDropOperation.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Texture2D.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

bool UVehicleClockPage::TransferFrom(UObject* SourceSlot, bool Commit)
{
    auto* Person = VehicleClock::Operator(GetOwningPlayer(), Vehicle);
    auto* Inventory = Person ? Person->GetInventory() : nullptr;
    if (!Inventory || !SourceSlot) return false;
    auto* OwnerField = FindFProperty<FObjectPropertyBase>(SourceSlot->GetClass(), TEXT("mCachedInventoryComponent"));
    auto* IndexField = FindFProperty<FIntProperty>(SourceSlot->GetClass(), TEXT("mSlotIdx"));
    if (!OwnerField || !IndexField || OwnerField->GetObjectPropertyValue_InContainer(SourceSlot) != Inventory) return false;
    const int32 Index = IndexField->GetPropertyValue_InContainer(SourceSlot);
    FInventoryStack Stack;
    if (Index < 0 || Index >= Inventory->GetSizeLinear() || !Inventory->GetStackFromIndex(Index, Stack) ||
        !Stack.NumItems || !VehicleClock::IsBoosterFuel(Stack.Item.GetItemClass())) return false;
    if (Commit)
    {
        auto* PC = Cast<AFGPlayerController>(GetOwningPlayer());
        auto* Remote = PC ? PC->GetRemoteCallObjectOfClass<UVehicleClockRemote>() : nullptr;
        if (!Remote) return false;
        Remote->TransferFuel(Vehicle, Index, Stack.Item.GetItemClass(), false);
    }
    return true;
}

void UVehicleFuelSlot::Configure(AFGVehicle* Target, UVehicleClockPage* Host)
{
    Vehicle = Target;
    Page = Host;
    bHUD = !Host;
    EmptyIcon = LoadObject<UTexture2D>(nullptr,TEXT("/Game/FactoryGame/Interface/UI/Assets/Misc/TXUI_Fuel_64.TXUI_Fuel_64"));
    ItemBrush.SetResourceObject(EmptyIcon);
    if (!bHUD)
    {
        SocketBrush.SetResourceObject(LoadObject<UTexture2D>(nullptr,
            TEXT("/Game/FactoryGame/Interface/UI/Assets/Shared/ConstructorWindows/TXUI_VehicleStorageBG.TXUI_VehicleStorageBG")));
        SocketBrush.SetUVRegion(FBox2f(FVector2f(273.f/512.f,157.f/512.f),FVector2f(344.f/512.f,256.f/512.f)));
        SocketBrush.ImageSize = FVector2D(83,116);
    }
    SetVisibility(bHUD ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Visible);
}

void UVehicleFuelSlot::NativeTick(const FGeometry& Geometry, float Seconds)
{
    Super::NativeTick(Geometry, Seconds);
    SearchIn -= Seconds;
    if (!Clock.IsValid() && SearchIn <= 0.f)
    {
        Clock = AVehicleClock::Find(Vehicle.Get());
        SearchIn = .2f;
    }
    UClass* Item = Clock.IsValid() ? (Clock->FuelType ? Clock->FuelType.Get() : Clock->BurningType.Get()) : nullptr;
    if (Item != ShownItem)
    {
        ShownItem = Item;
        ItemBrush.SetResourceObject(Item ? UFGItemDescriptor::GetSmallIcon(Item) : EmptyIcon.Get());
        InvalidateLayoutAndVolatility();
    }
}

TSharedRef<SWidget> UVehicleFuelSlot::RebuildWidget()
{
    const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
    const FLinearColor Orange(.88f,.31f,.045f);
    static const FProgressBarStyle Gauge = FProgressBarStyle()
        .SetBackgroundImage(FSlateColorBrush(FLinearColor(.09f,.085f,.08f)))
        .SetFillImage(FSlateColorBrush(FLinearColor::White));
    const int32 Size = bHUD ? 40 : 44;
    auto StackIcon = SNew(SBox).WidthOverride(Size).HeightOverride(Size)
        [SNew(SOverlay)
        + SOverlay::Slot()[SNew(SBorder).BorderImage(White).BorderBackgroundColor(FLinearColor(.56f,.55f,.54f)).Padding(3)
            [SNew(SImage).Image(&ItemBrush).ColorAndOpacity_Lambda([this]{ return ShownItem ? FLinearColor::White : FLinearColor(.45f,.44f,.43f); })]]
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom)
            [SNew(SBorder).BorderImage(White).BorderBackgroundColor(Orange).Padding(FMargin(3,0))
            [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",bHUD ? 10 : 13)).Text_Lambda([this]{
                return FText::AsNumber(Clock.IsValid() ? Clock->FuelCount : 0);
            })]]];
    TSharedRef<SWidget> Socket = StackIcon;
    if (!bHUD)
    {
        Socket = SNew(SBox).WidthOverride(83).HeightOverride(116)[SNew(SOverlay)
            + SOverlay::Slot()[SNew(SImage).Image(&SocketBrush)]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0,34,0,0)[StackIcon]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,10)
              [SNew(SBorder).BorderImage(White).BorderBackgroundColor(FLinearColor(.10f,.10f,.10f)).Padding(7,0)
               [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",9)).Text(FText::FromString(TEXT("FUEL")))]]];
    }
    auto Info = SNew(SVerticalBox);
    Info->AddSlot().AutoHeight()[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",bHUD ? 11 : 16))
        .ColorAndOpacity(bHUD ? FLinearColor::White : Orange)
        .Text(FText::FromString(bHUD ? TEXT("Boost") : VehicleFamily(Vehicle.Get()) == EVehicleFamily::Tractor ? TEXT("Forward rocket fuel") : TEXT("Drift booster fuel")))];
    if (!bHUD)
    {
        Info->AddSlot().AutoHeight().Padding(0,3)
            [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",12)).Text_Lambda([this]{
                if (ShownItem) return UFGItemDescriptor::GetItemName(ShownItem.Get());
                return FText::FromString(TEXT("Compacted Coal · Packaged Turbofuel, Rocket or Ionized Fuel"));
            })];
        Info->AddSlot().AutoHeight().Padding(0,0,0,6)[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",11))
            .Text(FText::FromString(TEXT("Shift-click or drag fuel here. Click the slot to unload.")))];
    }
    Info->AddSlot().AutoHeight().Padding(0,2)[SNew(SBox).HeightOverride(bHUD ? 4 : 6)
        [SNew(SProgressBar).Style(&Gauge).BorderPadding(FVector2D::ZeroVector).FillColorAndOpacity(Orange).Percent_Lambda([this]() -> TOptional<float> {
            if (!Clock.IsValid()) return 0.f;
            if (!Clock->BurningType) return Clock->FuelCount ? 1.f : 0.f;
            const float Capacity = UFGItemDescriptor::GetEnergyValue(Clock->BurningType);
            return Capacity > 0.f ? FMath::Clamp(Clock->FuelCharge / Capacity,0.f,1.f) : 0.f;
        })]];
    return SNew(SBorder).Tag(bHUD ? TEXT("VehicleBoostHUD") : TEXT("VehicleBoostFuelSlot"))
        .BorderImage(White).BorderBackgroundColor(FLinearColor(.025f,.023f,.021f,bHUD ? .8f : .97f))
        .Padding(bHUD ? FMargin(5) : FMargin(0,0,10,0)).VAlign(VAlign_Center)
        [SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Socket]
        + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(bHUD ? 5 : 14,0,0,0)[Info]];
}

FReply UVehicleFuelSlot::NativeOnMouseButtonDown(const FGeometry& G, const FPointerEvent& Event)
{
    if (!bHUD && (Event.GetEffectingButton()==EKeys::LeftMouseButton || Event.GetEffectingButton()==EKeys::RightMouseButton))
    {
        auto* PC = Cast<AFGPlayerController>(GetOwningPlayer());
        auto* Remote = PC ? PC->GetRemoteCallObjectOfClass<UVehicleClockRemote>() : nullptr;
        if (Remote && Clock.IsValid() && Clock->FuelCount > 0)
            Remote->TransferFuel(Vehicle.Get(), INDEX_NONE, Clock->FuelType, true);
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonDown(G, Event);
}

bool UVehicleFuelSlot::NativeOnDragOver(const FGeometry&, const FDragDropEvent&, UDragDropOperation* Op)
{
    return Page.IsValid() && Op && Page->TransferFrom(Op->Payload, false);
}

bool UVehicleFuelSlot::NativeOnDrop(const FGeometry&, const FDragDropEvent&, UDragDropOperation* Op)
{
    return Page.IsValid() && Op && Page->TransferFrom(Op->Payload, true);
}

void UVehicleClockHUD::UpdateFuelHUD(APlayerController* PC, AFGVehicle* Driven, bool InMenu)
{
    auto* HUD = Cast<AFGHUD>(PC->GetHUD());
    auto* PawnHUD = VehicleClock::SupportsBooster(Driven) && HUD ? HUD->GetPawnHUD() : nullptr;
    auto* Canvas = PawnHUD ? Cast<UCanvasPanel>(PawnHUD->GetWidgetFromName(TEXT("mVehicleHUDCanvas"))) : nullptr;
    auto* Speed = Canvas ? PawnHUD->GetWidgetFromName(TEXT("CurrentSpeedSlot")) : nullptr;
    if (FuelHUD && (Driven != HUDVehicle.Get() || Canvas != FuelHUD->GetParent()))
    {
        FuelHUD->RemoveFromParent();
        FuelHUD = nullptr;
    }
    if (!Speed) return;
    if (!FuelHUD)
    {
        FuelHUD = CreateWidget<UVehicleFuelSlot>(PC);
        FuelHUD->Configure(Driven, nullptr);
        auto* Placement = Canvas->AddChildToCanvas(FuelHUD);
        Placement->SetAlignment(FVector2D(0,1));
        Placement->SetSize(FVector2D(108,54));
        Placement->SetZOrder(1);
        HUDVehicle = Driven;
    }
    bool Visible = !InMenu && !HUD->GetHiddenHUDMode() && !HUD->GetPumpiMode() && !HUD->GetPartialPumpiMode();
    for (UWidget* Widget = Speed; Visible && Widget; Widget = Widget->GetParent()) Visible = Widget->IsVisible();
    const auto& G = Speed->GetCachedGeometry();
    Visible &= G.GetLocalSize().X > 0.f;
    FuelHUD->SetVisibility(Visible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (Visible)
    {
        const FVector2D Corner = G.LocalToAbsolute(G.GetLocalSize());
        const FVector2D Pos = Canvas->GetCachedGeometry().AbsoluteToLocal(Corner);
        CastChecked<UCanvasPanelSlot>(FuelHUD->Slot)->SetPosition(Pos + FVector2D(12,9));
    }
}
