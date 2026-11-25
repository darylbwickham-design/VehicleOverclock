#include "VehicleHandlingResearch.h"

#include "UObject/StrongObjectPtr.h"

#include "FGSchematicManager.h"
#include "Resources/FGItemDescriptor.h"
#include "Registry/ModContentRegistry.h"
#include "Engine/Texture2D.h"
#include "UObject/UnrealType.h"

namespace
{
    // Runtime-built children of native class defaults must survive world travel and garbage collection.
    TArray<TStrongObjectPtr<UObject>>& ResearchObjects()
    {
        static TArray<TStrongObjectPtr<UObject>> Objects;
        return Objects;
    }

    struct FUpgrade
    {
        UClass* Class;
        const TCHAR* Title;
        const TCHAR* Description;
        const TCHAR* PartA; int32 AmountA;
        const TCHAR* PartB; int32 AmountB;
        float Seconds;
        FIntPoint Position;
        TArray<int32> Parents;
    };

    const TArray<FUpgrade>& Upgrades()
    {
        static const TArray<FUpgrade> All = {
            {UVehicleCalibration::StaticClass(),TEXT("Chassis calibration"),TEXT("Enables speed-sensitive handling on manually overclocked road vehicles. Dampens excessive yaw when the tyres are in contact."),TEXT("IronPlate"),20,TEXT("Wire"),30,15,{2,0},{}},
            {UVehicleGripI::StaticClass(),TEXT("Tyre grip I"),TEXT("Increases high-speed tyre grip by up to 12%."),TEXT("IronScrew"),40,TEXT("IronPlate"),8,20,{0,2},{0}},
            {UVehicleGripII::StaticClass(),TEXT("Tyre grip II"),TEXT("Raises the high-speed grip bonus to 24%."),TEXT("Rotor"),4,TEXT("SteelPipe"),12,35,{0,4},{1}},
            {UVehicleGripIII::StaticClass(),TEXT("Tyre grip III"),TEXT("Raises the high-speed grip bonus to 36%."),TEXT("Rubber"),30,TEXT("Motor"),3,60,{0,6},{2}},
            {UVehicleAeroI::StaticClass(),TEXT("Downforce I"),TEXT("Adds up to 8% of the vehicle's weight as extra tyre loading near the road."),TEXT("CopperSheet"),12,TEXT("IronRod"),16,20,{2,2},{0}},
            {UVehicleAeroII::StaticClass(),TEXT("Downforce II"),TEXT("Raises additional high-speed tyre loading to 16% of vehicle weight."),TEXT("IronPlateReinforced"),6,TEXT("SteelPipe"),12,35,{2,4},{4}},
            {UVehicleAeroIII::StaticClass(),TEXT("Downforce III"),TEXT("Raises additional high-speed tyre loading to 24% of vehicle weight."),TEXT("ModularFrame"),6,TEXT("Rubber"),24,60,{2,6},{5}},
            {UVehicleBrakesI::StaticClass(),TEXT("Braking I"),TEXT("Adds up to 15% service-brake torque plus 0.18 m/s² of lift-off deceleration. Also improves locomotive air brakes."),TEXT("IronPlate"),12,TEXT("Cable"),10,20,{4,2},{0}},
            {UVehicleBrakesII::StaticClass(),TEXT("Braking II"),TEXT("Raises braking assistance to 30% and lift-off deceleration to 0.36 m/s²."),TEXT("Stator"),4,TEXT("SteelPipe"),12,35,{4,4},{7}},
            {UVehicleBrakesIII::StaticClass(),TEXT("Braking III"),TEXT("Raises braking assistance to 45% and lift-off deceleration to 0.54 m/s²."),TEXT("Motor"),3,TEXT("Rubber"),24,60,{4,6},{8}},
            {UVehicleStability::StaticClass(),TEXT("Electronic stability control"),TEXT("Uses the steering input and sideways slip to damp excessive yaw and roll in high-speed road turns."),TEXT("CircuitBoard"),8,TEXT("Motor"),4,90,{2,8},{3,6,9}}
        };
        return All;
    }

    UClass* Part(const TCHAR* Name)
    {
        const FString Path=FString::Printf(TEXT("/Game/FactoryGame/Resource/Parts/%s/Desc_%s.Desc_%s_C"),Name,Name,Name);
        return LoadClass<UFGItemDescriptor>(nullptr,*Path);
    }

    FProperty* Member(UStruct* Type, const TCHAR* Name)
    {
        const FString Prefix=FString(Name)+TEXT("_");
        for(TFieldIterator<FProperty> It(Type);It;++It)
            if(It->GetName()==Name || It->GetName().StartsWith(Prefix))return *It;
        return nullptr;
    }

    void Point(const FStructProperty* Field, void* Data, FIntPoint P)
    {
        auto* X=CastFieldChecked<FIntProperty>(Member(Field->Struct,TEXT("X")));
        auto* Y=CastFieldChecked<FIntProperty>(Member(Field->Struct,TEXT("Y")));
        X->SetPropertyValue_InContainer(Data,P.X);
        Y->SetPropertyValue_InContainer(Data,P.Y);
    }
}

void UVehicleHandlingPrerequisite::Set(const TArray<TSubclassOf<UFGSchematic>>& Parents)
{
    mSchematics=Parents;
    mRequireAllSchematicsToBePurchased=true;
}

// Every native class shares a script package, so use class names for distinct, stable asset IDs.
FPrimaryAssetId UVehicleHandlingSchematic::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(UFGSchematic::StaticClass()->GetFName(), GetClass()->GetFName());
}

FPrimaryAssetId UVehicleHandlingTree::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(UFGResearchTree::StaticClass()->GetFName(), GetClass()->GetFName());
}

void UVehicleHandlingUnlock::Set(const FText& Title,const FText& Description,UTexture2D* Image)
{
    mUnlockName=Title; mUnlockDescription=Description;
    mUnlockIconBig=Image; mUnlockIconSmall=Image; mUnlockIconCategory=Image;
}

void UVehicleHandlingSchematic::Configure(const TCHAR* Title,const TCHAR* Description,float Seconds,
    const TArray<FItemAmount>& Cost,const TArray<TSubclassOf<UFGSchematic>>& Parents,UTexture2D* Icon)
{
    mType=ESchematicType::EST_MAM; mDisplayName=FText::FromString(Title); mDescription=FText::FromString(Description);
    mCost=Cost; mTimeToComplete=Seconds; mIsPlayerSpecific=false; mSmallSchematicIcon=Icon;
    mSchematicIcon.SetResourceObject(Icon); mSchematicIcon.ImageSize=FVector2D(128,128);
    mSchematicDependencies.Reset(); mUnlocks.Reset();
    if(!Parents.IsEmpty())
    {
        auto* Required=NewObject<UVehicleHandlingPrerequisite>(this);
        Required->Set(Parents); ResearchObjects().Emplace(Required); mSchematicDependencies.Add(Required); mDependenciesBlocksSchematicAccess=true;
    }
    auto* Unlock=NewObject<UVehicleHandlingUnlock>(this);
    Unlock->Set(mDisplayName,mDescription,Icon); ResearchObjects().Emplace(Unlock); mUnlocks.Add(Unlock);
}

bool UVehicleHandlingTree::Configure()
{
    if(!mNodes.IsEmpty())return mNodes.Num()==Upgrades().Num();
    mPreUnlockDisplayName=mDisplayName=FText::FromString(TEXT("Vehicle Handling"));
    mPreUnlockDescription=mPostUnlockDescription=FText::FromString(TEXT("Research high-speed handling for manually overclocked vehicles."));
    auto* TreeIcon=UFGItemDescriptor::GetBigIcon(Part(TEXT("Motor")));
    mResearchTreeIcon.SetResourceObject(TreeIcon);mResearchTreeIcon.ImageSize=FVector2D(128,128);
    auto* NodeClass=LoadClass<UFGResearchTreeNode>(nullptr,TEXT("/Game/FactoryGame/Schematics/Research/BPD_ResearchTreeNode.BPD_ResearchTreeNode_C"));
    auto* Data=NodeClass?FindFProperty<FStructProperty>(NodeClass,TEXT("mNodeDataStruct")):nullptr;
    if(!Data)return false;
    auto* Coord=CastField<FStructProperty>(Member(Data->Struct,TEXT("Coordinates")));
    auto* Schematic=CastField<FClassProperty>(Member(Data->Struct,TEXT("Schematic")));
    auto* Parents=CastField<FArrayProperty>(Member(Data->Struct,TEXT("Parents")));
    auto* Roads=CastField<FMapProperty>(Member(Data->Struct,TEXT("ChildrenAndRoads")));
    if(!Coord||!Schematic||!Parents||!Roads)return false;
    const auto& All=Upgrades();
    TArray<UFGResearchTreeNode*> LocalNodes;
    for(const FUpgrade& U: All)
    {
        UClass* A=Part(U.PartA); UClass* B=Part(U.PartB);
        if(!A||!B){mNodes.Reset();return false;}
        TArray<TSubclassOf<UFGSchematic>> Required;
        for(int32 P:U.Parents)Required.Add(All[P].Class);
        U.Class->GetDefaultObject<UVehicleHandlingSchematic>()->Configure(U.Title,U.Description,U.Seconds,
            {FItemAmount(A,U.AmountA),FItemAmount(B,U.AmountB)},Required,UFGItemDescriptor::GetBigIcon(B));
        auto* N=NewObject<UFGResearchTreeNode>(this,NodeClass);
        void* V=Data->ContainerPtrToValuePtr<void>(N);
        Schematic->SetObjectPropertyValue_InContainer(V,U.Class);
        Point(Coord,Coord->ContainerPtrToValuePtr<void>(V),U.Position);
        FScriptArrayHelper ParentList(Parents,Parents->ContainerPtrToValuePtr<void>(V));
        for(int32 P:U.Parents)
        {
            const FIntPoint From=All[P].Position;
            Point(CastFieldChecked<FStructProperty>(Parents->Inner),ParentList.GetRawPtr(ParentList.AddValue()),From);
            void* ParentData=Data->ContainerPtrToValuePtr<void>(LocalNodes[P]);
            FScriptMapHelper Map(Roads,Roads->ContainerPtrToValuePtr<void>(ParentData));
            const int32 Entry=Map.AddDefaultValue_Invalid_NeedsRehash();
            Point(CastFieldChecked<FStructProperty>(Roads->KeyProp),Map.GetKeyPtr(Entry),U.Position);
            auto* Value=CastFieldChecked<FStructProperty>(Roads->ValueProp);
            auto* Points=CastField<FArrayProperty>(Member(Value->Struct,TEXT("Points")));
            if(Points)
            {
                FScriptArrayHelper Bends(Points,Points->ContainerPtrToValuePtr<void>(Map.GetValuePtr(Entry)));
                auto* Type=CastFieldChecked<FStructProperty>(Points->Inner);
                if(From.X!=U.Position.X)
                {
                    Point(Type,Bends.GetRawPtr(Bends.AddValue()),{From.X,(From.Y+U.Position.Y)/2});
                    Point(Type,Bends.GetRawPtr(Bends.AddValue()),{U.Position.X,(From.Y+U.Position.Y)/2});
                }
                Point(Type,Bends.GetRawPtr(Bends.AddValue()),U.Position);
            }
            Map.Rehash();
        }
        LocalNodes.Add(N);
        ResearchObjects().Emplace(N);
        ResearchObjects().Emplace(A); ResearchObjects().Emplace(B);
    }
    mNodes.Reset();
    for(auto* N:LocalNodes)mNodes.Add(N);
    return true;
}

void UVehicleHandlingResearch::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    const FString Map=GetWorld()->GetMapName();
    if(!GetWorld()->IsGameWorld() || Map.Contains(TEXT("Menu")) || Map.StartsWith(TEXT("Untitled")))return;
    Collection.InitializeDependency<UModContentRegistry>();
    auto* Registry=UModContentRegistry::Get(GetWorld());
    if(Registry && GetMutableDefault<UVehicleHandlingTree>()->Configure())
        Registry->RegisterResearchTree(TEXT("VehicleOverclock"),UVehicleHandlingTree::StaticClass());
}

FVehicleHandlingRanks FVehicleHandlingRanks::Read(UWorld* World)
{
    FVehicleHandlingRanks R;
    auto* Manager=AFGSchematicManager::Get(World); if(!Manager)return R;
    const auto& All=Upgrades();
    auto Bought=[&](int I){return Manager->IsSchematicPurchased(All[I].Class);};
    R.bCalibrated=Bought(0); if(!R.bCalibrated)return R;
    auto Rank=[&](int First){
        int32 Level=0;
        while(Level<3 && Bought(First+Level))++Level;
        return Level;
    };
    R.Grip=Rank(1); R.Aero=Rank(4); R.Brakes=Rank(7);
    R.bStability=R.Grip==3 && R.Aero==3 && R.Brakes==3 && Bought(10);
    return R;
}




