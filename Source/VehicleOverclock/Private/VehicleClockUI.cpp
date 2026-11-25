#include "VehicleClockUI.h"
#include "FGCharacterPlayer.h"
#include "FGHUD.h"
#include "FGLocomotive.h"
#include "FGPlayerController.h"
#include "FGTrain.h"
#include "FGVehicle.h"
#include "UI/FGGameUI.h"
#include "UI/FGInteractWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/NamedSlot.h"
#include "Engine/Texture2D.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "UObject/UnrealType.h"

namespace
{
    const FLinearColor Amber(1.f, 0.42f, 0.046f);

    auto Label(const FString& Text, int32 Size, const FLinearColor Color = FLinearColor::White)
    {
        return SNew(STextBlock).Text(FText::FromString(Text))
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", Size)).ColorAndOpacity(Color);
    }

    template<class T>
    T* ObjectField(UObject* Owner, FName Name)
    {
        const auto* P = Owner ? FindFProperty<FObjectPropertyBase>(Owner->GetClass(), Name) : nullptr;
        return P ? Cast<T>(P->GetObjectPropertyValue_InContainer(Owner)) : nullptr;
    }

    void SetObjectField(UObject* Owner, FName Name, UObject* Value)
    {
        if (auto* P = FindFProperty<FObjectPropertyBase>(Owner->GetClass(), Name))
            P->SetObjectPropertyValue_InContainer(Owner, Value);
    }

    void Call(UObject* Object, FName Name)
    {
        if (UFunction* F = Object->FindFunction(Name)) Object->ProcessEvent(F, nullptr);
    }

    int32 Index(UUserWidget* Slider)
    {
        auto* P = FindFProperty<FIntProperty>(Slider->GetClass(), TEXT("mActiveIndex"));
        return P ? P->GetPropertyValue_InContainer(Slider) : 0;
    }

    FArrayProperty* Definitions(UUserWidget* Header)
    {
        return FindFProperty<FArrayProperty>(Header->GetClass(), TEXT("mButtons"));
    }

    void RebuildHeaders(UUserWidget* Header, UUserWidget* SlidingPages)
    {
        if (auto* Buttons=ObjectField<UCanvasPanel>(Header,TEXT("mContainer"))) Buttons->ClearChildren();
        if (SlidingPages) Call(SlidingPages,TEXT("ClearButtons"));
        Call(Header,TEXT("GenerateButtons"));
    }

    void AddDefinition(UUserWidget* Header, const FString& Title, UTexture2D* Icon)
    {
        auto* Array = Definitions(Header);
        FScriptArrayHelper Entries(Array, Array->ContainerPtrToValuePtr<void>(Header));
        uint8* Data = Entries.GetRawPtr(Entries.AddValue());
        auto* Structure = CastFieldChecked<FStructProperty>(Array->Inner)->Struct.Get();
        for (TFieldIterator<FProperty> It(Structure); It; ++It)
        {
            if (auto* Text = CastField<FTextProperty>(*It)) Text->SetPropertyValue_InContainer(Data, FText::FromString(Title));
            else if (auto* Texture = CastField<FObjectPropertyBase>(*It))
                if (Icon && Icon->IsA(Texture->PropertyClass)) Texture->SetObjectPropertyValue_InContainer(Data, Icon);
        }
    }
}

void UVehicleClockPage::Configure(AFGVehicle* InVehicle, bool ShowDetails)
{
    Vehicle = InVehicle;
    bDetails = ShowDetails;

    MachineBrush.SetResourceObject(LoadObject<UTexture2D>(nullptr,
        TEXT("/Game/FactoryGame/Interface/UI/Assets/Shared/ConstructorWindows/TXUI_VehicleStorageBG.TXUI_VehicleStorageBG")));
    MachineBrush.SetUVRegion(FBox2f(FVector2f(0,0),FVector2f(266.f/512.f,363.f/512.f)));
    MachineBrush.ImageSize=FVector2D(700,726);
}

int32 UVehicleClockPage::Slots() const
{
    auto* S = AVehicleClock::Find(Vehicle);
    return S ? S->Shards : 0;
}

void UVehicleClockPage::NativeTick(const FGeometry& Geometry,float DeltaSeconds)
{
    Super::NativeTick(Geometry,DeltaSeconds);
    RefreshControls();
    UntilRefresh-=DeltaSeconds;
    if(UntilRefresh<=0.f)
    {
        Learned=FVehicleHandlingRanks::Read(GetWorld());
        UntilRefresh=1.f;
    }
}

int32 UVehicleClockPage::Clock() const
{
    auto* S = AVehicleClock::Find(Vehicle);
    return S ? S->Percent : 100;
}

FReply UVehicleClockPage::Edit(EClockEdit Operation, int32 Value)
{
    if (auto* PC = Cast<AFGPlayerController>(GetOwningPlayer()))
        if (VehicleClock::Operator(PC, Vehicle))
            if (auto* Remote = PC->GetRemoteCallObjectOfClass<UVehicleClockRemote>())
                Remote->Edit(Vehicle, Operation, Value);
    return FReply::Handled();
}

TSharedRef<SWidget> UVehicleClockPage::RebuildWidget()
{
    auto Rows = SNew(SVerticalBox);
    if (!bDetails)
    {
        Rows->AddSlot().AutoHeight().HAlign(HAlign_Center)
        [SNew(SBox).WidthOverride(532).HeightOverride(181)[BuildMachineControls()]];
    }
    else
    {
        Rows->AddSlot().AutoHeight()[Label(TEXT("Vehicle performance"),18,Amber)];
        Rows->AddSlot().AutoHeight().Padding(0,8,0,0)
        [SNew(STextBlock).Text_Lambda([this] {
            return FText::FromString(FString::Printf(TEXT("Clock speed    %d%%       Power Shards    %d / 3"),Clock(),Slots()));
        })];
    }

    Rows->AddSlot().AutoHeight().Padding(12,16,12,0)
    [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",15)).Text_Lambda([this] {
        if (!Vehicle) return FText::GetEmpty();
        const float Rate=VehicleClock::Energy(Clock()/100.f);
        if (Vehicle->IsA<AFGLocomotive>())
            return FText::FromString(FString::Printf(TEXT("Rail power    %.2f×"),Rate));
        if (!VehicleClock::NeedsFuel(Vehicle)) return FText::FromString(TEXT("Fuel-free"));
        return FText::FromString(FString::Printf(TEXT("Fuel burn rate    %.2f×"),Rate));
    })];
    Rows->AddSlot().AutoHeight().Padding(12,20,12,5)[Label(TEXT("Vehicle Handling"),15,Amber)];
    Rows->AddSlot().AutoHeight().Padding(12,0)
    [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",13)).Text_Lambda([this] {
        if (!Learned.bCalibrated) return FText::FromString(TEXT("Chassis calibration available at the MAM"));
        if (Vehicle && Vehicle->IsA<AFGLocomotive>())
            return FText::FromString(FString::Printf(TEXT("Air braking    %d / 3"),Learned.Brakes));
        return FText::FromString(FString::Printf(TEXT("Tyre grip    %d / 3     Downforce    %d / 3     Braking    %d / 3\nStability control    %s"),
            Learned.Grip,Learned.Aero,Learned.Brakes,Learned.bStability?TEXT("Installed"):TEXT("Locked")));
    })];
    Rows->AddSlot().AutoHeight().Padding(12,14)[SNew(STextBlock).ColorAndOpacity(Amber).AutoWrapText(true)
        .Text_Lambda([this]{auto* PC=Cast<AFGPlayerController>(GetOwningPlayer());
            auto* R=PC?PC->GetRemoteCallObjectOfClass<UVehicleClockRemote>():nullptr;
            return FText::FromString(R && R->FeedbackVehicle==Vehicle ? R->LastMessage : FString());})];
    return SNew(SBox).WidthOverride(700).HeightOverride(726)
        [SNew(SOverlay)
        + SOverlay::Slot()[SNew(SImage).Visibility(EVisibility::HitTestInvisible).Image(&MachineBrush)]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0,112,0,0)
          [SNew(SBox).WidthOverride(540)[Rows]]];
}

void UVehicleClockHUD::RegenerateTabs(int32 Selected)
{
    Call(Slider, TEXT("ShowAllIndexes"));
    Slider->ForceLayoutPrepass();
    Call(Slider, TEXT("SetupIndexOffset"));
    RebuildHeaders(Headers, Slider);
    struct { int32 Index; bool Animate; } P{Selected,false};
    if (auto* Fn = Slider->FindFunction(TEXT("SetActiveIndex"))) Slider->ProcessEvent(Fn,&P);
    Call(Slider, TEXT("HideNonActiveIndexes"));
    Call(Slider, TEXT("SetActiveButton"));
}

bool UVehicleClockHUD::Attach(UFGInteractWidget* Host, AFGVehicle* Target)
{
    auto* PC = Host->GetOwningPlayer();
    auto* Window = Cast<UUserWidget>(Host->GetRootWidget());
    if (!Window) return false;
    auto* H = Cast<UUserWidget>(Window->GetWidgetFromName(TEXT("Widget_TabsContainer")));
    auto* B = Cast<UNamedSlot>(Window->GetWidgetFromName(TEXT("WindowBody")));
    if (!H || !B || !B->GetContent() || !Definitions(H)) return false;
    auto* S = ObjectField<UUserWidget>(H,TEXT("mSlidingTabsWidget"));
    auto* Row = ObjectField<UHorizontalBox>(S,TEXT("mHorizontalBox"));
    FScriptArrayHelper Entries(Definitions(H),Definitions(H)->ContainerPtrToValuePtr<void>(H));
    const int32 Existing = Entries.Num();
    if (Existing < 1 || (S && (!Row || Row->GetChildrenCount()!=Existing)) || (!S && Existing!=1)) return false;
    if (!S)
    {
        static const TSoftClassPtr<UUserWidget> Class(FSoftObjectPath(TEXT("/Game/FactoryGame/Interface/UI/InGame/-Shared/Widget_SlidingTabs.Widget_SlidingTabs_C")));
        S = CreateWidget<UUserWidget>(PC,Class.LoadSynchronous());
        if (!S) return false;
        OriginalBody = B->GetContent();
        B->ClearChildren();
        Row = NewObject<UHorizontalBox>(S);
        Row->AddChildToHorizontalBox(OriginalBody);
        S->SetContentForSlot(TEXT("mContent"),Row);
        B->SetContent(S);
        S->TakeWidget();
        SetObjectField(S,TEXT("mHorizontalBox"),Row);
        SetObjectField(H,TEXT("mSlidingTabsWidget"),S);
    }

    Headers=H; Slider=S; Pages=Row; Body=B; InitialDefinitions=Existing;
    InitialPageCount=Row->GetChildrenCount(); Menu=Host; Vehicle=Target;
    bOpenedWhileDriving = Target == VehicleClock::DrivenBy(PC);
    const int32 Selected=FMath::Clamp(Index(S),0,Existing-1);
    auto* Icon=LoadObject<UTexture2D>(nullptr,TEXT("/Game/FactoryGame/Interface/UI/Assets/Shared/TXUI_Overclock_256.TXUI_Overclock_256"));
    if(Existing==1)
    {
        auto* Details=CreateWidget<UVehicleClockPage>(PC);
        Details->Configure(Target,true);
        Row->AddChildToHorizontalBox(Details);
        AddDefinition(H,TEXT("Vehicle performance"),nullptr);
    }
    auto* Page=CreateWidget<UVehicleClockPage>(PC);
    Page->Configure(Target,false);
    Row->AddChildToHorizontalBox(Page);
    AddDefinition(H,TEXT("Overclocking"),Icon);
    RegenerateTabs(Selected);

    // Some trunks enable an extra craft bench. Keep its native page index, but
    // put Overclocking in the third visible header position.
    if (auto* Canvas=ObjectField<UCanvasPanel>(H,TEXT("mContainer")))
    {
        TArray<UCanvasPanelSlot*> Visible;
        for(int32 I=0;I<Canvas->GetChildrenCount();++I)
        {
            auto* Button=Canvas->GetChildAt(I);
            if (Button->IsVisible()) if(auto* Slot=Cast<UCanvasPanelSlot>(Button->Slot)) Visible.Add(Slot);
        }
        Visible.Sort([](const UCanvasPanelSlot& A,const UCanvasPanelSlot& B){return A.GetPosition().X<B.GetPosition().X;});
        if(Visible.Num()>3)
        {
            const FVector2D Third=Visible[2]->GetPosition();
            for(int32 I=2;I<Visible.Num()-1;++I) Visible[I]->SetPosition(Visible[I+1]->GetPosition());
            Visible.Last()->SetPosition(Third);
        }
    }
    return true;
}

void UVehicleClockHUD::Detach()
{
    if (Headers && Slider && Pages)
    {
        const int32 Selected=FMath::Clamp(Index(Slider),0,InitialPageCount-1);
        auto* Def=Definitions(Headers);
        FScriptArrayHelper Items(Def,Def->ContainerPtrToValuePtr<void>(Headers));
        while (Pages->GetChildrenCount()>InitialPageCount) Pages->RemoveChildAt(Pages->GetChildrenCount()-1);
        if(Items.Num()>InitialDefinitions) Items.RemoveValues(InitialDefinitions,Items.Num()-InitialDefinitions);
        if(OriginalBody && Body && Body->GetContent()==Slider)
        {
            OriginalBody->RemoveFromParent();
            Body->SetContent(OriginalBody);
            SetObjectField(Headers,TEXT("mSlidingTabsWidget"),nullptr);
            RebuildHeaders(Headers,nullptr);
        }
        else RegenerateTabs(Selected);
    }
    OriginalBody=nullptr; Body=nullptr; Pages=nullptr; Slider=nullptr; Headers=nullptr; Vehicle.Reset(); Menu.Reset();
}

void UVehicleClockHUD::Tick(float)
{
    auto* PC=Cast<AFGPlayerController>(GetWorld()->GetFirstPlayerController());
    auto* UI=PC && PC->IsLocalController()? PC->GetGameUI():nullptr;
    if (!UI) { if(Headers) Detach(); return; }
    const auto Stack=UI->GetInteractWidgetStack();
    if (Headers && bOpenedWhileDriving && VehicleClock::DrivenBy(PC)!=Vehicle.Get() && Stack.Contains(Menu.Get()))
    {
        auto* Leaving=Menu.Get();
        Detach();
        if (Leaving) UI->PopWidget(Leaving);
        return;
    }
    if(Headers && (!Menu.IsValid() || !Vehicle.IsValid() || !Stack.Contains(Menu.Get()))) Detach();
    if(Headers || Stack.IsEmpty()) return;
    auto* Top=Stack.Last();
    if(!Top) return;
    auto* Object=ObjectField<UObject>(Top,TEXT("mInteractObject"));
    auto* Target=Cast<AFGVehicle>(Object);
    if(!Target) Target=ObjectField<AFGVehicle>(Top,TEXT("mOwningVehicle"));
    if(!Target) Target=ObjectField<AFGVehicle>(Top,TEXT("mGolfCart"));
    if(!Target)
    {
        auto* Loco=Cast<AFGLocomotive>(VehicleClock::DrivenBy(PC));
        if(Loco && Loco->GetTrain() && (Object==Loco->GetTrain() || ObjectField<AFGTrain>(Top,TEXT("mTrain"))==Loco->GetTrain())) Target=Loco;
    }
    UClass* Class=VehicleClock::MenuClass(Target);
    if(Class && Top->IsA(Class) && VehicleClock::Operator(PC,Target)) Attach(Top,Target);
}

void UVehicleClockHUD::Deinitialize()
{
    Detach();
    Super::Deinitialize();
}

