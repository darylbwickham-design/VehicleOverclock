#include "VehicleClock.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "FGItemPickup_Spawnable.h"
#include "FGLocomotive.h"
#include "FGLocomotiveMovementComponent.h"
#include "FGPlayerController.h"
#include "FGTrain.h"
#include "Resources/FGItemDescriptor.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodyInstance.h"
#include "ChaosVehicleWheel.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UnrealType.h"
#include "UI/FGInteractWidget.h"

TSubclassOf<UFGItemDescriptor> VehicleClock::Shard()
{
    static const TSoftClassPtr<UFGItemDescriptor> Class(FSoftObjectPath(
        TEXT("/Game/FactoryGame/Resource/Environment/Crystal/Desc_CrystalShard.Desc_CrystalShard_C")));
    return Class.LoadSynchronous();
}

AFGVehicle* VehicleClock::DrivenBy(APlayerController* Controller)
{
    auto* Vehicle = Controller ? Cast<AFGVehicle>(Controller->GetPawn()) : nullptr;
    return Vehicle && Vehicle->GetDriver() &&
        (Vehicle->IsA<AFGWheeledVehicle>() || Vehicle->IsA<AFGLocomotive>()) ? Vehicle : nullptr;
}

bool VehicleClock::NeedsFuel(const AFGVehicle* Vehicle)
{
    const auto* Wheels = Cast<AFGWheeledVehicle>(Vehicle);
    static const auto* NeedsFuel = FindFieldChecked<FBoolProperty>(
        AFGWheeledVehicle::StaticClass(), TEXT("mNeedsFuelToDrive"));
    return Wheels && NeedsFuel->GetPropertyValue_InContainer(Wheels);
}

UClass* VehicleClock::MenuClass(const AFGVehicle* Vehicle)
{
    if (!IsValid(Vehicle)) return nullptr;
    const FName Field = Vehicle->IsA<AFGLocomotive>() ? TEXT("mLocomotiveMenuWidgetClass") : TEXT("mInteractWidget");
    const FProperty* P = Vehicle->GetClass()->FindPropertyByName(Field);
    UClass* Class = nullptr;
    if (const auto* Soft = CastField<FSoftClassProperty>(P))
        Class = Cast<UClass>(Soft->GetPropertyValue_InContainer(Vehicle).LoadSynchronous());
    else if (const auto* Hard = CastField<FClassProperty>(P))
        Class = Cast<UClass>(Hard->GetObjectPropertyValue_InContainer(Vehicle));
    return Class && Class->IsChildOf(UFGInteractWidget::StaticClass()) ? Class : nullptr;
}

AFGCharacterPlayer* VehicleClock::Operator(APlayerController* Player, AFGVehicle* Vehicle)
{
    if (!Player || !MenuClass(Vehicle)) return nullptr;
    if (Vehicle == DrivenBy(Player)) return Vehicle->GetDriver();
    auto* Person = Cast<AFGCharacterPlayer>(Player->GetPawn());
    if (!Person || Vehicle->GetDriver() || Vehicle->GetWorld() != Person->GetWorld()) return nullptr;
    const FBox Bounds = Vehicle->GetComponentsBoundingBox(true);
    return Bounds.ComputeSquaredDistanceToPoint(Person->GetActorLocation()) <= FMath::Square(Person->GetUseDistance())
        ? Person : nullptr;
}

AVehicleClock::AVehicleClock()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(false);
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.20f;
}

void AVehicleClock::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AVehicleClock, Vehicle);
    DOREPLIFETIME(AVehicleClock, Shards);
    DOREPLIFETIME(AVehicleClock, Percent);
}

AVehicleClock* AVehicleClock::Find(const AFGVehicle* Target)
{
    if (IsValid(Target))
    {
        for (TActorIterator<AVehicleClock> It(Target->GetWorld()); It; ++It)
        {
            if (It->Vehicle == Target && !It->IsActorBeingDestroyed()) return *It;
        }
    }
    return nullptr;
}

void AVehicleClock::FitTo(AFGVehicle* InVehicle, AFGCharacterPlayer* Installer)
{
    Vehicle = InVehicle;
    Vehicle->OnDestroyed.AddUniqueDynamic(this, &AVehicleClock::VehicleDestroyed);
    Vehicle->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &AVehicleClock::ControllerChanged);
    if (Installer)
    {
        ShardOwner = Installer;
        bWaitingForDriver = Vehicle->GetDriver() == nullptr;
    }
    else if (!ShardOwner && !bRefundPending) ShardOwner = Vehicle->GetDriver();
}

void AVehicleClock::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority() && IsValid(Vehicle)) FitTo(Vehicle);
}

bool AVehicleClock::ShouldSave_Implementation() const { return IsValid(Vehicle) && Shards > 0; }

void AVehicleClock::GatherDependencies_Implementation(TArray<UObject*>& Out)
{
    if (IsValid(Vehicle)) Out.Add(Vehicle);
    if (IsValid(ShardOwner)) Out.Add(ShardOwner);
}

void AVehicleClock::ReturnShards(AActor* DropAt)
{
    if (!HasAuthority() || IsActorBeingDestroyed() || GetWorld()->bIsTearingDown) return;
    bRefundPending = true;
    Percent = VehicleClock::Stock;
    const auto Item = VehicleClock::Shard();
    if (Item && Shards > 0)
    {
        if (auto* Inventory = IsValid(ShardOwner) ? ShardOwner->GetInventory() : nullptr)
            Shards -= Inventory->AddStack(FInventoryStack(Shards, Item), true);
        if (Shards > 0 && DropAt)
        {
            const FVector Location = DropAt->GetActorLocation() + FVector(0, 0, 80);
            if (AFGItemPickup_Spawnable::CreateItemDrop(GetWorld(), FInventoryStack(Shards, Item),
                Location, FRotator::ZeroRotator)) Shards = 0;
        }
    }
    ForceNetUpdate();
    if (Shards == 0) Destroy();
}

void AVehicleClock::ControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
    if (OldController && OldController != NewController && OldController->IsPlayerController() && Shards > 0)
        ReturnShards(Pawn);
}

bool AVehicleClock::SyncDriver()
{
    if (!HasAuthority() || !IsValid(Vehicle)) return true;
    auto* Driver = Vehicle->GetDriver();
    if (bRefundPending || !VehicleClock::MenuClass(Vehicle) ||
        (ShardOwner && ShardOwner != Driver && (Driver || !bWaitingForDriver)))
    {
        ReturnShards(Vehicle);
        return false;
    }
    // Older saves have the shard count but no owner. Adopt the first driver
    // so those shards can be recovered on the next exit.
    if (!ShardOwner) ShardOwner = Driver;
    if (Driver) bWaitingForDriver = false;
    return true;
}

void AVehicleClock::VehicleDestroyed(AActor* DestroyedVehicle)
{
    ReturnShards(DestroyedVehicle);
}

void AVehicleClock::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Reason == EEndPlayReason::Destroyed && IsValid(Vehicle) && Applied != 1.f)
    {
        Percent = VehicleClock::Stock;
        UpdateDrive();
    }
    Super::EndPlay(Reason);
}

void AVehicleClock::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UntilDriverCheck -= DeltaSeconds;
    if(UntilDriverCheck <= 0.f)
    {
        UntilDriverCheck=0.2f;
        SyncDriver();
        if (!IsActorBeingDestroyed()) UpdateDrive();
    }
    if (!IsActorBeingDestroyed() && Applied>1.f) UpdateHandling(DeltaSeconds);
}

void AVehicleClock::UpdateDrive()
{
    if (!IsValid(Vehicle) || !Vehicle->HasActorBegunPlay()) return;
    auto* Loco = Cast<AFGLocomotive>(Vehicle);
    const bool Manual = Vehicle->GetDriver() && !Vehicle->IsSimulated() &&
        (!Loco || (Loco->GetTrain() && !Loco->GetTrain()->IsSelfDrivingEnabled() &&
        (!Loco->GetTrain()->GetMultipleUnitMaster() || Loco->GetTrain()->GetMultipleUnitMaster() == Loco)));
    const float Scale = Manual ? FMath::Clamp(Percent, 100, VehicleClock::Limit(Shards)) / 100.f : 1.f;
    if (Scale != Applied)
    {
        if (Loco) SetRailClock(Scale);
        else SetWheelClock(Scale);
    }
    if (Loco && Applied > 1.f) SetTrainSpeedLimit();
}

void AVehicleClock::SetWheelClock(float Clock)
{
    const float Scale = VehicleClock::Speed(Clock);
    auto* Wheels = CastChecked<AFGWheeledVehicle>(Vehicle);
    auto* Move = Wheels->GetVehicleMovementComponent();
    if (!Move) return;
    static const auto* Fuel = FindFieldChecked<FFloatProperty>(
        AFGWheeledVehicle::StaticClass(), TEXT("mManualFuelConsumption"));
    if (Applied == 1.f)
    {
        StockFinalDrive = Move->TransmissionSetup.FinalRatio;
        StockTorque = Move->EngineSetup.MaxTorque;
        StockDrag = Move->DragCoefficient;
        StockFuel = Wheels->GetManualFuelConsumption();
        const float TopGear=Move->TransmissionSetup.ForwardGearRatios.IsEmpty()?1.f:Move->TransmissionSetup.ForwardGearRatios.Last();
        const float Radius=Move->Wheels.Num() && Move->Wheels[0]?Move->Wheels[0]->WheelRadius:40.f;
        // Stock redline in top gear, in cm/s. Wheel size and gearing distinguish a cart from a truck.
        StockRoadSpeed=FMath::Clamp(Move->EngineSetup.MaxRPM*2.f*PI*Radius /
            (60.f*FMath::Max(0.1f,TopGear*StockFinalDrive)),550.f,4600.f);
        UntilResearchCheck=0.f;
    }
    // Taller gearing raises the wheel speed at redline. Retain wheel torque
    // and compensate for quadratic air drag at the new cruising speed.
    Move->TransmissionSetup.FinalRatio = StockFinalDrive / Scale;
    Move->EngineSetup.MaxTorque = StockTorque * Scale;
    Move->DragCoefficient = StockDrag / FMath::Square(Scale);
    if (VehicleClock::NeedsFuel(Wheels))
        Fuel->SetPropertyValue_InContainer(Wheels, StockFuel * VehicleClock::Energy(Clock));
    if (auto* Primitive = Move->UpdatedPrimitive.Get())
    {
        if (auto* Body = Primitive->GetBodyInstance(); Body && Body->IsValidBodyInstance())
        {
            // Chaos retains the per-vehicle config by reference. Update it under
            // the chassis lock without rebuilding the wheels while driving.
            FPhysicsCommand::ExecuteWrite(Body->GetPhysicsActor(), [&](const FPhysicsActorHandle&) {
                Move->TransmissionSetup.GetPhysicsTransmissionConfig();
            });
            if (Move->bMechanicalSimEnabled) Move->SetMaxEngineTorque(Move->EngineSetup.MaxTorque);
            Move->SetDragCoefficient(Move->DragCoefficient);
        }
    }
    Applied = Clock;
    if(Clock==1.f){SetWheelHandling(0.f,0.f);SetActorTickInterval(0.2f);}
}

void AVehicleClock::SetWheelHandling(float GripBonus,float BrakeBonus)
{
    if(FMath::IsNearlyEqual(GripBonus,LastGripBonus,0.0001f) && FMath::IsNearlyEqual(BrakeBonus,LastBrakeBonus,0.0001f))return;
    auto* Car=Cast<AFGWheeledVehicle>(Vehicle);
    auto* Move=Car?Car->GetVehicleMovementComponent():nullptr;
    if(!Move || !Move->HasValidPhysicsState())return;
    for(int32 I=0;I<Move->Wheels.Num();++I)
    {
        const auto* Wheel=Move->Wheels[I].Get();
        if(!Wheel)continue;
        Move->SetWheelFrictionMultiplier(I,Wheel->FrictionForceMultiplier*(1.f+GripBonus));
        if(Wheel->bAffectedByBrake)Move->SetWheelMaxBrakeTorque(I,Wheel->MaxBrakeTorque*(1.f+BrakeBonus));
    }
    LastGripBonus=GripBonus; LastBrakeBonus=BrakeBonus;
}

void AVehicleClock::SetRailBraking(float Bonus)
{
    auto* Loco=Cast<AFGLocomotive>(Vehicle);
    auto* Move=Loco?Loco->GetRailroadVehicleMovementComponent():nullptr;
    if(!Move)return;
    auto* Air=FindFProperty<FFloatProperty>(Move->GetClass(),TEXT("mMaxAirBrakingEffort"));
    if(Air)Air->SetPropertyValue_InContainer(Move,StockRailAirBrake*(1.f+Bonus));
}

void AVehicleClock::UpdateHandling(float DeltaSeconds)
{
    if(!IsValid(Vehicle))return;
    UntilResearchCheck-=DeltaSeconds;
    if(UntilResearchCheck<=0.f)
    {
        Handling=FVehicleHandlingRanks::Read(GetWorld());
        UntilResearchCheck=1.f;
        SetActorTickInterval(Handling.bCalibrated && Vehicle->IsA<AFGWheeledVehicle>()?0.f:0.2f);
    }
    if(!Handling.bCalibrated)return;
    if(auto* Loco=Cast<AFGLocomotive>(Vehicle))
    {
        auto* Move=Loco->GetRailroadVehicleMovementComponent();
        if(Move && StockRailSpeed>0.f)
            SetRailBraking(0.15f*Handling.Brakes*FMath::SmoothStep(0.55f,1.15f,FMath::Abs(Move->GetForwardSpeed())/StockRailSpeed));
        return;
    }
    auto* Car=Cast<AFGWheeledVehicle>(Vehicle);
    auto* Move=Car?Car->GetVehicleMovementComponent():nullptr;
    auto* Mesh=Car?Car->GetMesh():nullptr;
    if(!Move || !Mesh || !Move->HasValidPhysicsState() || !Mesh->IsSimulatingPhysics() || StockRoadSpeed<=0.f ||
        (!HasAuthority() && !Car->IsLocallyControlled())) return;
    const FVector Velocity=Mesh->GetPhysicsLinearVelocity();
    const FVector2D Horizontal(Velocity.X,Velocity.Y);
    const float Speed=Horizontal.Size();
    const float Blend=FMath::SmoothStep(0.55f,1.15f,Speed/StockRoadSpeed);
    // Quantise only wheel config writes. Continuous chassis forces stay smooth.
    const float WheelBlend=FMath::RoundToFloat(Blend*32.f)/32.f;
    SetWheelHandling(0.12f*Handling.Grip*WheelBlend,0.15f*Handling.Brakes*WheelBlend);
    if(Blend<=0.f || Car->GetActorUpVector().Z<0.35f) return;
    int32 Contacts=0;
    for(int32 I=0;I<Move->Wheels.Num();++I)if(Move->GetWheelState(I).bInContact)++Contacts;
    if(Contacts<2)return;
    const float Gravity=FMath::Abs(GetWorld()->GetGravityZ());
    const FVector Forward=Car->GetActorForwardVector().GetSafeNormal2D();
    const FVector Right(-Forward.Y,Forward.X,0.f);
    const float Throttle=FMath::Abs(Move->GetModifiedThrottleInput());
    const float Coast=(1.f-FMath::Clamp(Throttle*5.f,0.f,1.f))*(1.f-Move->GetModifiedBrakeInput());
    FVector Acceleration=FVector(0,0,-Gravity*(0.08f*Handling.Aero));
    Acceleration-=Velocity.GetSafeNormal2D()*(18.f*Handling.Brakes*Coast);
    const FVector Angular=Mesh->GetPhysicsAngularVelocityInRadians();
    const float YawLimit=0.35f+0.75f*FMath::Abs(Move->GetModifiedSteeringInput());
    const float ExcessYaw=Angular.Z-FMath::Clamp(float(Angular.Z),-YawLimit,YawLimit);
    FVector AngularAccel(0,0,-ExcessYaw*(Handling.bStability?3.5f:0.85f));
    if(Handling.bStability)
    {
        const float SideSpeed=FVector::DotProduct(Velocity,Right);
        Acceleration-=Right*FMath::Clamp(SideSpeed*0.65f,-0.18f*Gravity,0.18f*Gravity);
        AngularAccel-=Forward*FMath::Clamp(FVector::DotProduct(Angular,Forward)*2.0,-3.0,3.0);
    }
    Mesh->AddForce(Acceleration*Blend,NAME_None,true);
    Mesh->AddTorqueInRadians(AngularAccel.GetClampedToMaxSize(6.f)*Blend,NAME_None,true);
}

void AVehicleClock::SetRailClock(float Clock)
{
    const float Scale = VehicleClock::Speed(Clock);
    auto* Loco = CastChecked<AFGLocomotive>(Vehicle);
    auto* Move = Cast<UFGLocomotiveMovementComponent>(Loco->GetRailroadVehicleMovementComponent());
    if (!Move) return;
    static const auto* Speed = FindFieldChecked<FFloatProperty>(Move->StaticClass(), TEXT("mMaxVelocity"));
    static const auto* Demand = FindFieldChecked<FStructProperty>(Loco->StaticClass(), TEXT("mPowerConsumption"));
    static const auto* Effort = FindFieldChecked<FStructProperty>(Move->StaticClass(), TEXT("mTractiveEffortCurve"));
    static const auto* Drag = FindFieldChecked<FFloatProperty>(Move->StaticClass(), TEXT("mDragCoefficient"));
    auto& Power = *Demand->ContainerPtrToValuePtr<FFloatInterval>(Loco);
    auto& Curve = *Effort->ContainerPtrToValuePtr<FRuntimeFloatCurve>(Move);
    if (Applied == 1.f)
    {
        StockDrag = Drag->GetPropertyValue_InContainer(Move);
        StockRailSpeed = Speed->GetPropertyValue_InContainer(Move);
        StockRailPower = Power;
        if(auto* Air=FindFProperty<FFloatProperty>(Move->GetClass(),TEXT("mMaxAirBrakingEffort")))
            StockRailAirBrake=Air->GetPropertyValue_InContainer(Move);
        UntilResearchCheck=0.f;
        StockEffort = Curve;
    }
    Speed->SetPropertyValue_InContainer(Move, StockRailSpeed * Scale);
    Drag->SetPropertyValue_InContainer(Move, StockDrag / FMath::Square(Scale));
    Power = FFloatInterval(StockRailPower.Min * VehicleClock::Energy(Clock),
        StockRailPower.Max * VehicleClock::Energy(Clock));
    Curve = StockEffort;
    if (Scale > 1.f)
    {
        // Local keys keep the shared locomotive curve asset immutable.
        auto Keys = StockEffort.GetRichCurveConst()->GetCopyOfKeys();
        Curve.ExternalCurve = nullptr;
        for (auto& Key : Keys)
        {
            Key.Time *= Scale;
            // Preserve peak effort so native throttle-to-power normalization still holds.
            Key.ArriveTangent /= Scale;
            Key.LeaveTangent /= Scale;
        }
        Curve.EditorCurveData.SetKeys(Keys);
    }
    Applied = Clock;
    if (Scale == 1.f)
    {
        SetRailBraking(0.f);
        if (auto* OldTrain = TunedTrain.Get()) OldTrain->RecalculateConsistData();
        TunedTrain.Reset();
    }
}

void AVehicleClock::SetTrainSpeedLimit()
{
    auto* Loco = CastChecked<AFGLocomotive>(Vehicle);
    AFGTrain* Train = Loco->GetTrain();
    if (TunedTrain.Get() != Train)
    {
        if (auto* Previous = TunedTrain.Get()) Previous->RecalculateConsistData();
        TunedTrain = Train;
    }
    if (!Train || Train->mConsistData.Vehicles.IsEmpty()) return;
    // Vanilla caches the slowest car's speed. Keep the entire coupled train
    // together and derive the limit again if its consist changes.
    float Limit = TNumericLimits<float>::Max();
    for (AFGRailroadVehicle* Car : Train->mConsistData.Vehicles)
    {
        if (!IsValid(Car)) return;
        auto* Move = Car->GetRailroadVehicleMovementComponent();
        if (!Move) return;
        const float Speed = Move->GetMaxForwardSpeed();
        Limit = FMath::Min(Limit, Car == Loco ? Speed : Speed * VehicleClock::Speed(Applied));
    }
    Train->mConsistData.MaxSpeed = Limit;
}

void UVehicleClockRemote::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UVehicleClockRemote, bReady);
}

void UVehicleClockRemote::Edit_Implementation(AFGVehicle* ExpectedVehicle, EClockEdit Operation, int32 Value)
{
    AFGVehicle* Target = ExpectedVehicle;
    auto* Person = VehicleClock::Operator(GetOwnerPlayerController(), Target);
    if (!Person) return;
    UFGInventoryComponent* Inventory = Person->GetInventory();
    const auto Shard = VehicleClock::Shard();
    if (!Inventory || !Shard) return;
    AVehicleClock* Clock = AVehicleClock::Find(Target);
    if (Clock && !Clock->SyncDriver()) return;
    if (Clock && !Clock->CanEdit(Person))
    {
        Feedback(Target, TEXT("Another pioneer's Power Shards are fitted."));
        return;
    }
    FString Message;
    if (Operation == EClockEdit::Insert || Operation == EClockEdit::SetPercent)
    {
        const int32 Fitted = Clock ? Clock->Shards : 0;
        if (Operation == EClockEdit::Insert && Fitted == VehicleClock::SlotCount) return;
        const int32 Requested = Operation == EClockEdit::Insert ? VehicleClock::Limit(Fitted + 1) :
            FMath::Clamp(Value, VehicleClock::Stock, VehicleClock::Limit(VehicleClock::SlotCount));
        const int32 Required = FMath::DivideAndRoundUp(Requested - VehicleClock::Stock, 50);
        const int32 Needed = FMath::Max(0, Required - Fitted);
        const int32 Added = FMath::Min(Needed, Inventory->GetNumItems(Shard));
        if (Added < Needed)
            Message = TEXT("More Power Shards are needed for that clock speed.");
        if (!Clock && Added == 0)
        {
            Feedback(Target, Message);
            return;
        }
        if (!Clock)
        {
            Clock = GetWorld()->SpawnActor<AVehicleClock>();
            if (!Clock) return;
            Clock->FitTo(Target, Person);
        }
        if (Added > 0)
        {
            Inventory->Remove(Shard, Added);
            Clock->Shards += Added;
        }
        Clock->Percent = FMath::Min(Requested, VehicleClock::Limit(Clock->Shards));
    }
    else if (Operation == EClockEdit::Remove && Clock && Clock->Shards > 0)
    {
        if (Inventory->AddStack(FInventoryStack(1, Shard)) != 1)
        {
            Feedback(Target, TEXT("Make room in your inventory to remove a shard."));
            return;
        }
        --Clock->Shards;
        Clock->Percent = FMath::Min(Clock->Percent, VehicleClock::Limit(Clock->Shards));
    }
    else return;

    Clock->UpdateDrive();
    Clock->ForceNetUpdate();
    Feedback(Target, Message);
    if (!Clock->Shards) Clock->Destroy();
}

void UVehicleClockRemote::Feedback_Implementation(AFGVehicle* ExpectedVehicle, const FString& Message)
{
    FeedbackVehicle = ExpectedVehicle;
    LastMessage = Message;
}





