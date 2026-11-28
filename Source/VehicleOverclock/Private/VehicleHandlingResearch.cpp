#include "VehicleHandlingResearch.h"
#include "VehicleDriftInput.h"

#include "UObject/StrongObjectPtr.h"
#include "FGVehicle.h"
#include "FGLocomotive.h"
#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"

#include "FGSchematicManager.h"
#include "Resources/FGItemDescriptor.h"
#include "Registry/ModContentRegistry.h"
#include "Engine/Texture2D.h"
#include "UObject/UnrealType.h"
#include "Algo/Reverse.h"

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
        const TCHAR* Icon;
    };

    const TArray<FUpgrade>& Upgrades()
    {
        static const TArray<FUpgrade> All = {
            {UVehicleCalibration::StaticClass(), TEXT("Vehicle overclocking"),
                TEXT("Unlock Power Shard tuning for every vehicle with a control panel. Each shard adds 50% clock capacity. Select a chassis below to research its handling."),
                TEXT("IronPlate"), 20, TEXT("Wire"), 30, 15, {3,0}, {}, TEXT("clock")},
            {UTractorWorks::StaticClass(), TEXT("Tractor chassis"),
                TEXT("Open the Tractor workshop. Choose tyre grip, downforce and braking in any order, then combine all three for stability control."),
                TEXT("IronPlate"), 8, TEXT("IronRod"), 12, 10, {1,2}, {0}, TEXT("tractor")},
            {UVehicleGripI::StaticClass(), TEXT("Tractor · Tyre grip I"),
                TEXT("Increase high-speed tyre grip by 12%."),
                TEXT("IronScrew"), 40, TEXT("IronPlate"), 8, 20, {0,3}, {1}, TEXT("tractor_grip1")},
            {UVehicleGripII::StaticClass(), TEXT("Tractor · Tyre grip II"),
                TEXT("Increase high-speed tyre grip to 24%."),
                TEXT("Rotor"), 4, TEXT("SteelPipe"), 12, 35, {0,4}, {2}, TEXT("tractor_grip2")},
            {UVehicleGripIII::StaticClass(), TEXT("Tractor · Tyre grip III"),
                TEXT("Increase high-speed tyre grip to 36%."),
                TEXT("Rubber"), 30, TEXT("Motor"), 3, 60, {0,5}, {3}, TEXT("tractor_grip3")},
            {UVehicleAeroI::StaticClass(), TEXT("Tractor · Downforce I"),
                TEXT("Add up to 8% of chassis weight as downforce at speed."),
                TEXT("CopperSheet"), 12, TEXT("IronRod"), 16, 20, {1,3}, {1}, TEXT("tractor_aero1")},
            {UVehicleAeroII::StaticClass(), TEXT("Tractor · Downforce II"),
                TEXT("Increase high-speed downforce to 16% of chassis weight."),
                TEXT("IronPlateReinforced"), 6, TEXT("SteelPipe"), 12, 35, {1,4}, {5}, TEXT("tractor_aero2")},
            {UVehicleAeroIII::StaticClass(), TEXT("Tractor · Downforce III"),
                TEXT("Increase high-speed downforce to 24% of chassis weight."),
                TEXT("ModularFrame"), 6, TEXT("Rubber"), 24, 60, {1,5}, {6}, TEXT("tractor_aero3")},
            {UVehicleBrakesI::StaticClass(), TEXT("Tractor · Brakes I"),
                TEXT("Add 15% brake torque and 0.18 m/s² of lift-off deceleration at speed."),
                TEXT("IronPlate"), 12, TEXT("Cable"), 10, 20, {2,3}, {1}, TEXT("tractor_brake1")},
            {UVehicleBrakesII::StaticClass(), TEXT("Tractor · Brakes II"),
                TEXT("Increase brake assistance to 30% and lift-off deceleration to 0.36 m/s²."),
                TEXT("Stator"), 4, TEXT("SteelPipe"), 12, 35, {2,4}, {8}, TEXT("tractor_brake2")},
            {UVehicleBrakesIII::StaticClass(), TEXT("Tractor · Brakes III"),
                TEXT("Increase brake assistance to 45% and lift-off deceleration to 0.54 m/s²."),
                TEXT("Motor"), 3, TEXT("Rubber"), 24, 60, {2,5}, {9}, TEXT("tractor_brake3")},
            {UVehicleStability::StaticClass(), TEXT("Tractor · Stability control"),
                TEXT("Combine grip, downforce and braking to settle body roll and catch a rear slide in fast corners."),
                TEXT("CircuitBoard"), 8, TEXT("Motor"), 4, 90, {1,6}, {4,7,10}, TEXT("tractor_stability")},
            {UExplorerWorks::StaticClass(), TEXT("Explorer chassis"),
                TEXT("Open the Explorer workshop. All-terrain tyres improve grip and a small amount of downforce keeps the wheels planted."),
                TEXT("Rotor"), 2, TEXT("Rubber"), 8, 15, {3,8}, {0}, TEXT("explorer")},
            {UExplorerBallast::StaticClass(), TEXT("Explorer · Belly ballast"),
                TEXT("Mount ballast under the floor. The overclocked Explorer gains 6% sprung mass and a lower centre of gravity."),
                TEXT("ModularFrame"), 3, TEXT("SteelPipe"), 12, 25, {1,9}, {12}, TEXT("explorer_ballast")},
            {UExplorerDampers::StaticClass(), TEXT("Explorer · Roll dampers"),
                TEXT("Control roll and rebound on uneven ground. Increase low-mounted ballast to 10% and improve high-speed tyre loading."),
                TEXT("Stator"), 3, TEXT("Rubber"), 16, 35, {1,10}, {13}, TEXT("explorer_spring")},
            {UExplorerBrakes::StaticClass(), TEXT("Explorer · Rally brakes"),
                TEXT("Fit the full brake package for 45% more brake torque and stronger lift-off deceleration at high speed."),
                TEXT("Motor"), 2, TEXT("CopperSheet"), 16, 45, {1,11}, {14}, TEXT("explorer_brake")},
            {UExplorerStability::StaticClass(), TEXT("Explorer · Rally stabiliser"),
                TEXT("Bring low-mounted ballast to 14%. Catch excessive yaw and sideways slip, and settle the chassis after a hard turn."),
                TEXT("CircuitBoard"), 4, TEXT("Motor"), 3, 60, {1,12}, {15}, TEXT("explorer_stability")},
            {UTruckWorks::StaticClass(), TEXT("Truck chassis"),
                TEXT("Open the Truck workshop. Heavy-haul research applies to the Truck and Fluid Freight Truck."),
                TEXT("SteelPipe"), 10, TEXT("IronPlate"), 15, 15, {5,2}, {0}, TEXT("truck")},
            {UTruckGrip::StaticClass(), TEXT("Truck · Haul tyres"),
                TEXT("Fit high-load tyres and add speed-sensitive downforce for loaded turns."),
                TEXT("Rubber"), 20, TEXT("SteelPipe"), 12, 25, {5,3}, {17}, TEXT("truck_grip")},
            {UTruckBrakes::StaticClass(), TEXT("Truck · Brake servo"),
                TEXT("Improve service braking and lift-off deceleration at speed."),
                TEXT("Stator"), 4, TEXT("CopperSheet"), 12, 35, {5,4}, {18}, TEXT("truck_brake")},
            {UTruckStability::StaticClass(), TEXT("Truck · Load stabiliser"),
                TEXT("Complete the truck grip, downforce and brake package. Control roll and yaw in sweeping turns."),
                TEXT("Motor"), 3, TEXT("CircuitBoard"), 4, 55, {5,5}, {19}, TEXT("truck_stability")},
            {UVehicleDrift::StaticClass(), TEXT("Controlled drift"),
                TEXT("Drive above 29 km/h with the tyres on the ground. Steer and hold Boost to push the rear outward while burning sulphur-based booster fuel. Set Boost hotkey in the mod options (default Left Ctrl). Fits Explorers and Trucks."),
                TEXT("Rotor"), 4, TEXT("Rubber"), 12, 30, {3,2}, {0}, TEXT("drift")},
            {UTractorRocket::StaticClass(), TEXT("Tractor · Forward rocket"),
                TEXT("Fit a forward thruster to the Tractor. Load compacted coal or packaged turbo, rocket or ionized fuel in the Overclocking tab. Hold Boost to accelerate on the ground. Release to cut thrust."),
                TEXT("SteelPipe"), 12, TEXT("Rotor"), 4, 30, {0,1}, {1}, TEXT("tractor_rocket")},
            {UFleetEfficiencyI::StaticClass(), TEXT("Fleet · Efficiency I"),
                TEXT("Reduce fuel and rail power demand at full overclock to 3.5× stock across the fleet. Partial overclocks also cost less."),
                TEXT("CopperSheet"), 10, TEXT("IronPlateReinforced"), 2, 20, {6,2}, {0}, TEXT("fleet_1")},
            {UFleetEfficiencyII::StaticClass(), TEXT("Fleet · Efficiency II"),
                TEXT("Reduce fuel and rail power demand at full overclock to 3× stock."),
                TEXT("SteelPipe"), 12, TEXT("Rotor"), 4, 35, {6,3}, {23}, TEXT("fleet_2")},
            {UFleetEfficiencyIII::StaticClass(), TEXT("Fleet · Efficiency III"),
                TEXT("Reduce fuel and rail power demand at full overclock to 2.5× stock."),
                TEXT("Rubber"), 20, TEXT("Stator"), 4, 45, {6,4}, {24}, TEXT("fleet_3")},
            {UFleetEfficiencyIV::StaticClass(), TEXT("Fleet · Efficiency IV"),
                TEXT("Reduce fuel and rail power demand at full overclock to 2× stock. Driving at stock clock still uses the standard rate."),
                TEXT("CircuitBoard"), 6, TEXT("Motor"), 4, 60, {6,5}, {25}, TEXT("fleet_4")},
            {UExplorerSteeringI::StaticClass(), TEXT("Explorer · Steering rack I"),
                TEXT("Add 10% steering lock to the overclocked Explorer for tighter turns."),
                TEXT("IronRod"), 16, TEXT("Rotor"), 2, 20, {2,9}, {12}, TEXT("explorer_steer1")},
            {UExplorerSteeringII::StaticClass(), TEXT("Explorer · Steering rack II"),
                TEXT("Add 20% steering lock to the overclocked Explorer."),
                TEXT("SteelPipe"), 12, TEXT("Rubber"), 10, 35, {2,10}, {27}, TEXT("explorer_steer2")},
            {UExplorerSteeringIII::StaticClass(), TEXT("Explorer · Steering rack III"),
                TEXT("Add 30% steering lock to the overclocked Explorer. The stock speed-sensitive steering curve still moderates input at speed."),
                TEXT("Motor"), 3, TEXT("CircuitBoard"), 4, 55, {2,11}, {28}, TEXT("explorer_steer3")},
            {UExplorerDriftI::StaticClass(), TEXT("Explorer · Drift control I"),
                TEXT("Build rear slip progressively over 0.6 seconds. Steering into the drift tightens the arc; countersteering widens it without reversing the drift."),
                TEXT("Rubber"), 12, TEXT("Rotor"), 3, 25, {3,9}, {12,21}, TEXT("explorer_drift1")},
            {UExplorerDriftII::StaticClass(), TEXT("Explorer · Drift control II"),
                TEXT("Improve drift response to 0.4 seconds and carry entry speed through the corner. Countersteer to straighten the slide."),
                TEXT("Stator"), 4, TEXT("Rubber"), 20, 40, {3,10}, {30}, TEXT("explorer_drift2")},
            {UExplorerDriftExit::StaticClass(), TEXT("Explorer · Corner exit"),
                TEXT("Recover rear grip over 0.45 seconds after releasing Boost. Settle yaw and align the nose with the travel direction for a clean exit."),
                TEXT("Motor"), 3, TEXT("CircuitBoard"), 4, 55, {3,11}, {31}, TEXT("explorer_exit")},
            {UExplorerAirI::StaticClass(), TEXT("Explorer · Air control I"),
                TEXT("Unlock air steering. Hold Air control (default Left Alt) to change steering to roll and throttle/brake to pitch. Release the modifier for yaw steering. Orientation uses no fuel."),
                TEXT("Rotor"), 4, TEXT("SteelPipe"), 12, 25, {4,9}, {12}, TEXT("explorer_air1")},
            {UExplorerAirII::StaticClass(), TEXT("Explorer · Air control II"),
                TEXT("Increase airborne rotation speed and damping for quicker, more precise jump corrections."),
                TEXT("Stator"), 4, TEXT("Rubber"), 16, 40, {4,10}, {33}, TEXT("explorer_air2")},
            {UExplorerAirIII::StaticClass(), TEXT("Explorer · Air control III"),
                TEXT("Complete the air-control package. Hold the modifier and pitch or roll for full flips. Strong damping catches rotation when the controls are released."),
                TEXT("Motor"), 3, TEXT("CircuitBoard"), 4, 55, {4,11}, {34}, TEXT("explorer_air3")},
            {UExplorerAirThruster::StaticClass(), TEXT("Explorer · Air thrusters"),
                TEXT("Hold Boost in the air with steering or throttle/brake to correct the flight path horizontally. With no direction input, thrust is forward. Burns sulphur-based booster fuel at 100 MJ/s."),
                TEXT("SteelPipe"), 16, TEXT("CompactedCoal"), 10, 45, {4,12}, {35}, TEXT("explorer_thruster")},
            {UExplorerLandingI::StaticClass(), TEXT("Explorer · Landing recovery I"),
                TEXT("Reduce descent speed close to the landing surface, damp rebound and settle roll after touchdown. Requires the rally stabiliser and full air control."),
                TEXT("Rubber"), 16, TEXT("ModularFrame"), 4, 35, {2,12}, {16,35}, TEXT("explorer_land1")},
            {UExplorerLandingII::StaticClass(), TEXT("Explorer · Landing recovery II"),
                TEXT("Strengthen landing recovery. Hold Boost and steer before touchdown to flow into a researched, fuelled drift as the tyres regain contact."),
                TEXT("Motor"), 4, TEXT("Rubber"), 24, 55, {2,13}, {37}, TEXT("explorer_land2")},
            {UTractorAirI::StaticClass(), TEXT("Tractor · Air control I"),
                TEXT("Unlock yaw steering in the air. Hold Air control (default Left Alt) for A/D roll and W/S pitch. Rotation uses no booster fuel."),
                TEXT("Rotor"), 3, TEXT("SteelPipe"), 10, 25, {3,3}, {1}, TEXT("tractor_air1")},
            {UTractorAirII::StaticClass(), TEXT("Tractor · Air control II"),
                TEXT("Increase airborne rotation speed and response for jump corrections and flips."),
                TEXT("Stator"), 4, TEXT("Rubber"), 12, 40, {3,4}, {39}, TEXT("tractor_air2")},
            {UTractorAirIII::StaticClass(), TEXT("Tractor · Air control III"),
                TEXT("Complete the Tractor air-control package. Hold the modifier and pitch or roll for full flips, with strong rotation damping on release."),
                TEXT("Motor"), 3, TEXT("CircuitBoard"), 4, 55, {3,5}, {40}, TEXT("tractor_air3")}
        };
        return All;
    }

    TArray<FIntPoint> ResearchRoad(FIntPoint From, FIntPoint To, const TSet<FIntPoint>& Occupied, int32 LastRow)
    {
        // The native MAM displays seven staggered hex columns and scrolls vertically.
        // Route through adjacent cells so wires stay inside that viewport and avoid other studies.
        TArray<FIntPoint> Queue{From};
        TMap<FIntPoint, FIntPoint> Previous;
        Previous.Add(From, From);
        for (int32 Head = 0; Head < Queue.Num() && !Previous.Contains(To); ++Head)
        {
            const FIntPoint Here = Queue[Head];
            const int32 Diagonal = Here.X % 2 == 0 ? 1 : -1;
            const FIntPoint Neighbours[] = {
                Here + FIntPoint(0, 1), Here + FIntPoint(-1, 0), Here + FIntPoint(1, 0),
                Here + FIntPoint(-1, Diagonal), Here + FIntPoint(1, Diagonal), Here + FIntPoint(0, -1)
            };
            for (FIntPoint Next : Neighbours)
            {
                if (Next.X < 0 || Next.X > 6 || Next.Y < 0 || Next.Y > LastRow ||
                    Previous.Contains(Next) || (Next != To && Occupied.Contains(Next))) continue;
                Previous.Add(Next, Here);
                Queue.Add(Next);
            }
        }
        TArray<FIntPoint> Road;
        if (!Previous.Contains(To)) return Road;
        for (FIntPoint Cell = To; Cell != From; Cell = Previous.FindChecked(Cell)) Road.Add(Cell);
        Algo::Reverse(Road);
        return Road;
    }

    UTexture2D* ResearchIcon(const TCHAR* Name)
    {
        if (IsRunningDedicatedServer()) return nullptr;
        static TMap<FString, UTexture2D*> Cache;
        if (auto** Found = Cache.Find(Name)) return *Found;
        const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("vehiclephysics"));
        if (!Plugin) return nullptr;
        const FString File = Plugin->GetBaseDir() / TEXT("Resources/Research") / (FString(Name) + TEXT(".png"));
        UTexture2D* Image = FImageUtils::ImportFileAsTexture2D(File);
        if (Image)
        {
            Image->LODGroup = TEXTUREGROUP_UI;
            ResearchObjects().Emplace(Image);
            Cache.Add(Name, Image);
        }
        return Image;
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
    mPreUnlockDisplayName=mDisplayName=FText::FromString(TEXT("Vehicle Engineering"));
    mPreUnlockDescription=mPostUnlockDescription=FText::FromString(TEXT("Research fleet-wide overclocking and fuel efficiency. Tune each chassis, then master Explorer drifting, air control and landings."));
    auto* TreeIcon=ResearchIcon(TEXT("clock"));
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
    TSet<FIntPoint> Occupied;
    int32 LastRow = 0;
    for (const auto& Upgrade : All)
    {
        if (Upgrade.Position.X < 0 || Upgrade.Position.X > 6 || Occupied.Contains(Upgrade.Position)) return false;
        Occupied.Add(Upgrade.Position);
        LastRow = FMath::Max(LastRow, Upgrade.Position.Y);
    }
    TArray<UFGResearchTreeNode*> LocalNodes;
    for(const FUpgrade& U: All)
    {
        UClass* A=Part(U.PartA); UClass* B=Part(U.PartB);
        if(!A||!B){mNodes.Reset();return false;}
        TArray<TSubclassOf<UFGSchematic>> Required;
        for(int32 P:U.Parents)Required.Add(All[P].Class);
        U.Class->GetDefaultObject<UVehicleHandlingSchematic>()->Configure(U.Title,U.Description,U.Seconds,
            {FItemAmount(A,U.AmountA),FItemAmount(B,U.AmountB)},Required,ResearchIcon(U.Icon));
        auto* N=NewObject<UFGResearchTreeNode>(this,NodeClass);
        void* V=Data->ContainerPtrToValuePtr<void>(N);
        Schematic->SetObjectPropertyValue_InContainer(V,U.Class);
        Point(Coord,Coord->ContainerPtrToValuePtr<void>(V),U.Position);
        FScriptArrayHelper ParentList(Parents,Parents->ContainerPtrToValuePtr<void>(V));
        for(int32 P:U.Parents)
        {
            const FIntPoint From=All[P].Position;
            const TArray<FIntPoint> Road = ResearchRoad(From, U.Position, Occupied, LastRow + 1);
            if (Road.IsEmpty()) return false;
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
                for (FIntPoint Tile : Road)
                    Point(Type,Bends.GetRawPtr(Bends.AddValue()),Tile);
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
    VehicleDriftInput::Register();
    const FString Map=GetWorld()->GetMapName();
    if(!GetWorld()->IsGameWorld() || Map.Contains(TEXT("Menu")) || Map.StartsWith(TEXT("Untitled")))return;
    Collection.InitializeDependency<UModContentRegistry>();
    auto* Registry=UModContentRegistry::Get(GetWorld());
    if(Registry && GetMutableDefault<UVehicleHandlingTree>()->Configure())
        Registry->RegisterResearchTree(TEXT("vehiclephysics"),UVehicleHandlingTree::StaticClass());
}

EVehicleFamily VehicleFamily(const AFGVehicle* Vehicle)
{
    if (!Vehicle) return EVehicleFamily::Other;
    if (Vehicle->IsA<AFGLocomotive>()) return EVehicleFamily::Rail;
    static const FName Tractor(TEXT("BP_Tractor_C")), Explorer(TEXT("BP_Explorer_C"));
    static const FName Truck(TEXT("BP_Truck_C")), FluidTruck(TEXT("BP_FluidTruck_C"));
    for (auto* Type = Vehicle->GetClass(); Type; Type = Type->GetSuperClass())
    {
        const FName Name = Type->GetFName();
        if (Name == Tractor) return EVehicleFamily::Tractor;
        if (Name == Explorer) return EVehicleFamily::Explorer;
        if (Name == Truck || Name == FluidTruck) return EVehicleFamily::Truck;
    }
    return EVehicleFamily::Other;
}

bool FVehicleHandlingRanks::OverclockUnlocked(UWorld* World)
{
    auto* Manager = AFGSchematicManager::Get(World);
    return Manager && Manager->IsSchematicPurchased(UVehicleCalibration::StaticClass());
}

FVehicleHandlingRanks FVehicleHandlingRanks::Read(UWorld* World, const AFGVehicle* Vehicle)
{
    FVehicleHandlingRanks R;
    auto* Manager = AFGSchematicManager::Get(World);
    if (!Manager || !Manager->IsSchematicPurchased(UVehicleCalibration::StaticClass())) return R;
    R.bCalibrated = true;
    auto Bought = [Manager](UClass* Type) { return Manager->IsSchematicPurchased(Type); };
    auto Rank = [&](UClass* I, UClass* II, UClass* III) { return int(Bought(I)) + int(Bought(II)) + int(Bought(III)); };
    R.Efficiency = Rank(UFleetEfficiencyI::StaticClass(), UFleetEfficiencyII::StaticClass(), UFleetEfficiencyIII::StaticClass())
        + int(Bought(UFleetEfficiencyIV::StaticClass()));
    const auto Family = VehicleFamily(Vehicle);
    if (Family == EVehicleFamily::Tractor)
    {
        R.AirControl = Rank(UTractorAirI::StaticClass(), UTractorAirII::StaticClass(), UTractorAirIII::StaticClass());
        R.bRocket = Bought(UTractorRocket::StaticClass());
        R.Grip = Rank(UVehicleGripI::StaticClass(), UVehicleGripII::StaticClass(), UVehicleGripIII::StaticClass());
        R.Aero = Rank(UVehicleAeroI::StaticClass(), UVehicleAeroII::StaticClass(), UVehicleAeroIII::StaticClass());
        R.Brakes = Rank(UVehicleBrakesI::StaticClass(), UVehicleBrakesII::StaticClass(), UVehicleBrakesIII::StaticClass());
        R.bStability = Bought(UVehicleStability::StaticClass());
    }
    else if (Family == EVehicleFamily::Explorer)
    {
        R.Steering = Rank(UExplorerSteeringI::StaticClass(), UExplorerSteeringII::StaticClass(), UExplorerSteeringIII::StaticClass());
        R.DriftControl = int(Bought(UExplorerDriftI::StaticClass())) + int(Bought(UExplorerDriftII::StaticClass()));
        R.bDriftExit = Bought(UExplorerDriftExit::StaticClass());
        R.AirControl = Rank(UExplorerAirI::StaticClass(), UExplorerAirII::StaticClass(), UExplorerAirIII::StaticClass());
        R.bAirThruster = Bought(UExplorerAirThruster::StaticClass());
        R.Landing = int(Bought(UExplorerLandingI::StaticClass())) + int(Bought(UExplorerLandingII::StaticClass()));
        R.Ballast = Rank(UExplorerBallast::StaticClass(), UExplorerDampers::StaticClass(), UExplorerStability::StaticClass());
        R.bStability = Bought(UExplorerStability::StaticClass());
        R.Grip = Rank(UExplorerWorks::StaticClass(), UExplorerDampers::StaticClass(), UExplorerStability::StaticClass());
        R.Aero = R.Grip;
        R.Brakes = Bought(UExplorerBrakes::StaticClass()) ? 3 : 0;
    }
    else if (Family == EVehicleFamily::Truck)
    {
        R.bStability = Bought(UTruckStability::StaticClass());
        R.Grip = (Bought(UTruckGrip::StaticClass()) ? 2 : 0) + int(R.bStability);
        R.Aero = R.Grip;
        R.Brakes = (Bought(UTruckBrakes::StaticClass()) ? 2 : 0) + int(R.bStability);
    }
    R.bDrift = (Family == EVehicleFamily::Explorer || Family == EVehicleFamily::Truck)
        && Bought(UVehicleDrift::StaticClass());
    return R;
}




