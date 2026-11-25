#include "VehicleClockUI.h"

#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "FGVehicle.h"
#include "Resources/FGItemDescriptor.h"
#include "Blueprint/DragDropOperation.h"
#include "Components/EditableText.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/DefaultValueHelper.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"

namespace
{
    template<class T>
    T* Part(UUserWidget* Parent, const FName Name)
    {
        return Cast<T>(Parent->GetWidgetFromName(Name));
    }

    void SetCost(UUserWidget* CostSlot, int32 Index, int32 Fitted)
    {
        UFunction* Setup = CostSlot->FindFunctionChecked(TEXT("Setup CostIcon"));
        FStructOnScope Arguments(Setup);
        uint8* Data = Arguments.GetStructMemory();
        *FindFieldChecked<FStructProperty>(Setup,TEXT("ItemAmount"))->ContainerPtrToValuePtr<FItemAmount>(Data)
            = FItemAmount(VehicleClock::Shard(),1);
        FindFieldChecked<FIntProperty>(Setup,TEXT("SlotIdx"))->SetPropertyValue_InContainer(Data,Index);
        FindFieldChecked<FIntProperty>(Setup,TEXT("CurrentNumInSlot"))->SetPropertyValue_InContainer(Data,Index<Fitted ? 1 : 0);
        FindFieldChecked<FBoolProperty>(Setup,TEXT("OverwriteComponentNumInSlot"))->SetPropertyValue_InContainer(Data,true);
        CostSlot->ProcessEvent(Setup,Data);
        // Keep each quantity badge legible. Only the shard artwork fades.
        if (auto* Icon = Part<UUserWidget>(CostSlot,TEXT("Widget_InventorySlot")))
            Icon->SetRenderOpacity(Index>=Fitted ? 0.22f : (Index==Fitted-1 ? 1.f : 0.45f));
    }
}

void UVehicleClockShardSlot::Configure(UVehicleClockPage* InPage, int32 InIndex)
{
    Page = InPage;
    Index = InIndex;
    SetVisibility(ESlateVisibility::Visible);
}

TSharedRef<SWidget> UVehicleClockShardSlot::RebuildWidget()
{
    return SNew(SButton).Tag(FName(*FString::Printf(TEXT("VehicleShard%d"),Index)))
        .ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0).Cursor(EMouseCursor::Hand)
        .ToolTipText_Lambda([this] {
            return FText::FromString(Page.IsValid() && Index<Page->Slots() ?
                TEXT("Remove one Power Shard") : TEXT("Fit Power Shards from your inventory"));
        })
        .OnClicked_Lambda([this] {
            if (!Page.IsValid()) return FReply::Unhandled();
            if (Index<Page->Slots()) return Page->Edit(EClockEdit::Remove);
            return Page->Edit(EClockEdit::SetPercent,VehicleClock::Limit(Index+1));
        })
        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .Visibility(EVisibility::HitTestInvisible).BorderBackgroundColor_Lambda([this] {
                return FLinearColor(1.f,0.55f,0.15f,IsHovered()?0.13f:0.f);
            })];
}

FReply UVehicleClockShardSlot::NativeOnMouseButtonDown(const FGeometry& G, const FPointerEvent& E)
{
    if (E.GetEffectingButton()==EKeys::RightMouseButton && Page.IsValid() && Index<Page->Slots())
        return Page->Edit(EClockEdit::Remove);
    return Super::NativeOnMouseButtonDown(G,E);
}

bool UVehicleClockPage::CanDropShard(UDragDropOperation* Operation) const
{
    auto* Source = Operation ? Operation->Payload.Get() : nullptr;
    auto* Player = VehicleClock::Operator(GetOwningPlayer(),Vehicle);
    if (!Source || !Player || !Player->GetInventory()) return false;
    auto* Inventory = FindFProperty<FObjectPropertyBase>(Source->GetClass(),TEXT("mCachedInventoryComponent"));
    auto* SlotIndex = FindFProperty<FIntProperty>(Source->GetClass(),TEXT("mSlotIdx"));
    if (!Inventory || !SlotIndex || Inventory->GetObjectPropertyValue_InContainer(Source)!=Player->GetInventory()) return false;
    FInventoryStack Stack;
    return Player->GetInventory()->GetStackFromIndex(SlotIndex->GetPropertyValue_InContainer(Source),Stack)
        && Stack.NumItems>0 && Stack.Item.GetItemClass()==VehicleClock::Shard();
}

bool UVehicleClockShardSlot::NativeOnDragOver(const FGeometry&,const FDragDropEvent&,UDragDropOperation* Op)
{
    return Page.IsValid() && Index>=Page->Slots() && Page->CanDropShard(Op);
}

bool UVehicleClockShardSlot::NativeOnDrop(const FGeometry& G,const FDragDropEvent& E,UDragDropOperation* Op)
{
    if (!NativeOnDragOver(G,E,Op)) return false;
    Page->Edit(EClockEdit::Insert);
    return true;
}

TSharedRef<SWidget> UVehicleClockPage::BuildMachineControls()
{
    static const TSoftClassPtr<UUserWidget> Stock(FSoftObjectPath(
        TEXT("/Game/FactoryGame/Interface/UI/InGame/-Shared/Widget_Overclock.Widget_Overclock_C")));
    Machine = CreateWidget<UUserWidget>(GetOwningPlayer(),Stock.LoadSynchronous());
    if (!Machine) return SNullWidget::NullWidget;
    // Mount only the presentation subtree. The outer blueprint's Construct and Tick
    // drive a factory's potential inventory and should never run against a vehicle.
    auto* Module = Part<UOverlay>(Machine,TEXT("mOverClockPanel"));
    ClockSlider = Part<USlider>(Machine,TEXT("MainSlider"));
    PercentInput = Part<UEditableText>(Machine,TEXT("mPercentText"));
    SpeedInput = Part<UEditableText>(Machine,TEXT("mPartsPerMinutesInput"));
    if (!Module || !ClockSlider || !PercentInput || !SpeedInput) return SNullWidget::NullWidget;
    Part<UTextBlock>(Machine,TEXT("mTargetProductionRateText"))->SetText(FText::FromString(TEXT("Rated vehicle speed:")));
    Part<UTextBlock>(Machine,TEXT("mPartsPerMinuteText"))->SetText(FText::FromString(TEXT(" × stock")));
    ClockSlider->SetToolTipText(FText::FromString(TEXT("Click or drag to set clock speed. Higher speeds fit shards from your inventory.")));
    ClockSlider->OnMouseCaptureBegin.Clear(); ClockSlider->OnMouseCaptureEnd.Clear();
    ClockSlider->OnControllerCaptureBegin.Clear(); ClockSlider->OnControllerCaptureEnd.Clear();
    ClockSlider->OnValueChanged.Clear();
    ClockSlider->OnMouseCaptureBegin.AddDynamic(this,&UVehicleClockPage::BeginClockDrag);
    ClockSlider->OnMouseCaptureEnd.AddDynamic(this,&UVehicleClockPage::EndClockDrag);
    ClockSlider->OnControllerCaptureBegin.AddDynamic(this,&UVehicleClockPage::BeginClockDrag);
    ClockSlider->OnControllerCaptureEnd.AddDynamic(this,&UVehicleClockPage::EndClockDrag);
    ClockSlider->OnValueChanged.AddDynamic(this,&UVehicleClockPage::ChangeClock);
    for (UEditableText* Input : {PercentInput.Get(),SpeedInput.Get()})
    {
        Input->OnTextChanged.Clear(); Input->OnTextCommitted.Clear();
        Input->SetSelectAllTextWhenFocused(true);
        Input->SetRevertTextOnEscape(true);
    }
    PercentInput->OnTextCommitted.AddDynamic(this,&UVehicleClockPage::CommitPercent);
    SpeedInput->OnTextCommitted.AddDynamic(this,&UVehicleClockPage::CommitSpeed);
    for (int32 I=0; I<VehicleClock::SlotCount; ++I)
    {
        auto* Cost = Part<UUserWidget>(Machine,*FString::Printf(TEXT("CostSlot_%d"),I));
        auto* Overlay = CastChecked<UOverlay>(Cost->GetParent());
        // The game's cost slots retain their images, number badges and typography.
        // Route input through the vehicle RPC instead of the factory handlers.
        Cost->SetVisibility(ESlateVisibility::HitTestInvisible);
        for (UWidget* Child : Overlay->GetAllChildren())
            if (Child!=Cost) Child->SetVisibility(ESlateVisibility::Collapsed);
        auto* Target = CreateWidget<UVehicleClockShardSlot>(GetOwningPlayer());
        Target->Configure(this,I);
        auto* Placement = Overlay->AddChildToOverlay(Target);
        Placement->SetHorizontalAlignment(HAlign_Fill);
        Placement->SetVerticalAlignment(VAlign_Fill);
    }
    TSharedRef<SWidget> Presentation = Module->TakeWidget();
    RefreshControls(true);
    return Presentation;
}

void UVehicleClockPage::RefreshControls(bool Force)
{
    if (!ClockSlider || !PercentInput || !SpeedInput) return;
    const int32 P = bDragging ? DragPercent : Clock();
    const int32 N = Slots();
    if (!Force && P==ShownClock && N==ShownShards) return;
    if (Force || N!=ShownShards)
        for (int32 I=0; I<VehicleClock::SlotCount; ++I)
            SetCost(Part<UUserWidget>(Machine,*FString::Printf(TEXT("CostSlot_%d"),I)),I,N);
    ShownClock=P; ShownShards=N;
    const float Ratio=P/100.f;
    if (!bDragging)
    {
        TGuardValue<bool> Updating(bUpdatingSlider,true);
        ClockSlider->SetValue(P/250.f);
    }
    if (Force || !PercentInput->HasKeyboardFocus())
        PercentInput->SetText(FText::FromString(FString::Printf(TEXT("%d%%"),P)));
    if (Force || !SpeedInput->HasKeyboardFocus())
        SpeedInput->SetText(FText::FromString(FString::Printf(TEXT("%.2f"),VehicleClock::Speed(Ratio))));
    auto* Bar=Part<UImage>(Machine,TEXT("mProgressBar"))->GetDynamicMaterial();
    if (Bar)
    {
        Bar->SetScalarParameterValue(TEXT("NumShards"),N);
        Bar->SetScalarParameterValue(TEXT("Value"),Ratio);
    }
    FindFieldChecked<FDoubleProperty>(Machine->GetClass(),TEXT("mOverclockAmount"))->SetPropertyValue_InContainer(Machine,Ratio);
    FindFieldChecked<FIntProperty>(Machine->GetClass(),TEXT("mNumShards"))->SetPropertyValue_InContainer(Machine,N);
    Machine->ProcessEvent(Machine->FindFunctionChecked(TEXT("UpdateBarTexts")),nullptr);
}

void UVehicleClockPage::BeginClockDrag()
{
    DragPercent=Clock(); bDragging=true;
}

void UVehicleClockPage::ChangeClock(float Value)
{
    if (bUpdatingSlider) return;
    DragPercent=FMath::Clamp(FMath::RoundToInt(Value*250.f),100,250);
    if (bDragging) RefreshControls();
    else Edit(EClockEdit::SetPercent,DragPercent);
}

void UVehicleClockPage::EndClockDrag()
{
    if (!bDragging) return;
    bDragging=false;
    Edit(EClockEdit::SetPercent,DragPercent);
    RefreshControls(true);
}

void UVehicleClockPage::CommitPercent(const FText& Text,ETextCommit::Type How)
{
    FString Number=Text.ToString().TrimStartAndEnd();
    Number.RemoveFromEnd(TEXT("%")); float Value;
    if (How!=ETextCommit::OnCleared && FDefaultValueHelper::ParseFloat(Number,Value) && FMath::IsFinite(Value))
        Edit(EClockEdit::SetPercent,FMath::RoundToInt(FMath::Clamp(Value,100.f,250.f)));
    RefreshControls(true);
}

void UVehicleClockPage::CommitSpeed(const FText& Text,ETextCommit::Type How)
{
    float Value;
    if (How!=ETextCommit::OnCleared && FDefaultValueHelper::ParseFloat(Text.ToString(),Value) && FMath::IsFinite(Value))
        Edit(EClockEdit::SetPercent,FMath::RoundToInt(100.f+(FMath::Clamp(Value,1.f,1.9f)-1.f)*100.f/0.6f));
    RefreshControls(true);
}
