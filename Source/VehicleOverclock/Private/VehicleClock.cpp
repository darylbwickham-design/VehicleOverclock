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

float VehicleClock::Speed(float Clock, const AFGVehicle* Vehicle)
{
    const float Base = 1.f + 0.6f * (Clock - 1.f);
    return VehicleFamily(Vehicle) == EVehicleFamily::Explorer && Clock > 2.f ? 1.6f + 0.3f * (Clock - 2.f) : Base;
}

float VehicleClock::ClockForSpeed(float Rate, const AFGVehicle* Vehicle)
{
    Rate = FMath::Clamp(Rate, 1.f, Speed(2.5f, Vehicle));
    if (VehicleFamily(Vehicle) == EVehicleFamily::Explorer && Rate > 1.6f) return 2.f + (Rate - 1.6f) / 0.3f;
    return 1.f + (Rate - 1.f) / 0.6f;
}

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
    DOREPLIFETIME(AVehicleClock, FuelType);
    DOREPLIFETIME(AVehicleClock, BurningType);
    DOREPLIFETIME(AVehicleClock, FuelCount);
    DOREPLIFETIME(AVehicleClock, FuelCharge);
    DOREPLIFETIME(AVehicleClock, ThrustDirection);
    DOREPLIFETIME(AVehicleClock, bDriftHeld);
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
    if (FuelCount > 0) GetBoosterInventory();
    Vehicle->OnDestroyed.AddUniqueDynamic(this, &AVehicleClock::VehicleDestroyed);
    Vehicle->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &AVehicleClock::ControllerChanged);
    if (Installer)
    {
        ShardOwner = Installer;
        bWaitingForDriver = Vehicle->GetDriver() == nullptr;
    }
    else if (Shards && !ShardOwner && !bRefundPending) ShardOwner = Vehicle->GetDriver();
}

void AVehicleClock::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority() && IsValid(Vehicle)) FitTo(Vehicle);
}

bool AVehicleClock::ShouldSave_Implementation() const { return IsValid(Vehicle) && (Shards > 0 || HasBoosterFuel()); }

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
    if (!Shards)
    {
        ClearShardOwner();
        UpdateDrive();
        if (!HasBoosterFuel()) Destroy();
    }
}

void AVehicleClock::ControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
    if (HasAuthority() && OldController != NewController) RequestDrift(false);
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
    if (Shards && !ShardOwner) ShardOwner = Driver;
    if (Driver) bWaitingForDriver = false;
    return true;
}

void AVehicleClock::VehicleDestroyed(AActor* DestroyedVehicle)
{
    if (!HasAuthority() || IsActorBeingDestroyed() || GetWorld()->bIsTearingDown) return;
    // Vanilla handles all vehicle inventory stacks during dismantle.
    FuelCount = 0;
    FuelCharge = 0.f;
    ReturnShards(DestroyedVehicle);
}

void AVehicleClock::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Reason == EEndPlayReason::Destroyed && IsValid(Vehicle) && Applied != 1.f)
    {
        Percent = VehicleClock::Stock;
        UpdateDrive();
    }
    if (Reason == EEndPlayReason::Destroyed && IsValid(Vehicle))
    {
        SetBallast(0);
        SetSteering(0);
        SetWheelHandling(0.f, 0.f);
    }
    if (Reason == EEndPlayReason::Destroyed && IsValid(BoosterInventory)) BoosterInventory->DestroyComponent();
    Super::EndPlay(Reason);
}

void AVehicleClock::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UntilResearchCheck -= DeltaSeconds;
    UntilDriverCheck -= DeltaSeconds;
    if(UntilDriverCheck <= 0.f)
    {
        UntilDriverCheck=0.2f;
        if (HasAuthority() && FuelCount > 0 && !IsValid(BoosterInventory)) GetBoosterInventory();
        SyncDriver();
        if (!IsActorBeingDestroyed()) UpdateDrive();
    }
    if (IsActorBeingDestroyed()) return;
    if (IsValid(Vehicle) && !Vehicle->GetDriver() && LastRearSlip > 0.f)
    {
        DriftBlend = 0.f;
        SetWheelHandling(0.f, 0.f);
    }
    if ((Applied > 1.f || bDriftHeld || ThrustDirection || DriftBlend > 0.f || LastRearSlip > 0.f) && IsValid(Vehicle) && Vehicle->GetDriver())
        UpdateHandling(DeltaSeconds);
    if (HasAuthority() && !HasBoosterFuel() && !Shards && (!bDriftHeld || !IsValid(Vehicle) || !Vehicle->GetDriver()) && DriftBlend <= 0.f)
        Destroy();
}

void AVehicleClock::UpdateDrive()
{
    if (!IsValid(Vehicle) || !Vehicle->HasActorBegunPlay()) return;
    auto* Loco = Cast<AFGLocomotive>(Vehicle);
    const bool Manual = Vehicle->GetDriver() && !Vehicle->IsSimulated() &&
        (!Loco || (Loco->GetTrain() && !Loco->GetTrain()->IsSelfDrivingEnabled() &&
        (!Loco->GetTrain()->GetMultipleUnitMaster() || Loco->GetTrain()->GetMultipleUnitMaster() == Loco)));
    if (UntilResearchCheck <= 0.f || (Manual && Applied == 1.f && Percent > VehicleClock::Stock))
    {
        Handling = FVehicleHandlingRanks::Read(GetWorld(), Vehicle);
        UntilResearchCheck = 1.f;
    }
    const float Scale = Manual && Handling.bCalibrated ? FMath::Clamp(Percent, 100, VehicleClock::Limit(Shards)) / 100.f : 1.f;
    if (Scale != Applied || (Scale > 1.f && AppliedEfficiency != Handling.Efficiency))
    {
        if (Loco) SetRailClock(Scale);
        else SetWheelClock(Scale);
        AppliedEfficiency = Handling.Efficiency;
    }
    SetActorTickInterval(Manual && !Loco && (Applied > 1.f || bDriftHeld || ThrustDirection) ? 0.f : .2f);
    if (Loco && Applied > 1.f) SetTrainSpeedLimit();
}

void AVehicleClock::SetWheelClock(float Clock)
{
    const float Scale = VehicleClock::Speed(Clock, Vehicle);
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
    }
    // Taller gearing raises the wheel speed at redline. Retain wheel torque
    // and compensate for quadratic air drag at the new cruising speed.
    Move->TransmissionSetup.FinalRatio = StockFinalDrive / Scale;
    Move->EngineSetup.MaxTorque = StockTorque * Scale;
    Move->DragCoefficient = StockDrag / FMath::Square(Scale);
    if (VehicleClock::NeedsFuel(Wheels))
        Fuel->SetPropertyValue_InContainer(Wheels, StockFuel * VehicleClock::Energy(Clock, Handling.Efficiency));
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
    if(Clock==1.f){SetBallast(0);SetSteering(0);SetWheelHandling(0.f,0.f);}
}

void AVehicleClock::SetSteering(int32 Rank)
{
    if (Rank == SteeringRank) return;
    auto* Car = Cast<AFGWheeledVehicle>(Vehicle);
    auto* Move = Car ? Car->GetVehicleMovementComponent() : nullptr;
    if (!Move || !Move->HasValidPhysicsState()) return;
    for (int32 I = 0; I < Move->Wheels.Num(); ++I)
    {
        const auto* Wheel = Move->Wheels[I].Get();
        if (Wheel && Wheel->bAffectedBySteering)
            Move->SetWheelMaxSteerAngle(I, Wheel->MaxSteerAngle * (1.f + .10f * Rank));
    }
    SteeringRank = Rank;
}

void AVehicleClock::SetBallast(int32 Rank)
{
    if (Rank == BallastRank) return;
    auto* Car = Cast<AFGWheeledVehicle>(Vehicle);
    auto* Move = Car ? Car->GetVehicleMovementComponent() : nullptr;
    auto* Mesh = Car ? Car->GetMesh() : nullptr;
    auto* Body = Mesh ? Mesh->GetBodyInstance() : nullptr;
    if (!Move || !Body || !Body->IsValidBodyInstance()) return;
    if (!BallastRank)
    {
        StockMass = Move->Mass;
        StockCOMNudge = Body->COMNudge;
    }
    static constexpr float MassBonus[] = {0.f, .06f, .10f, .14f};
    Move->Mass = StockMass * (1.f + MassBonus[Rank]);
    // Chaos reapplies the movement component's chassis mass whenever COM changes.
    Mesh->SetCenterOfMass(StockCOMNudge - FVector(0, 0, 5.f * Rank));
    BallastRank = Rank;
}

void AVehicleClock::ClearShardOwner()
{
    ShardOwner = nullptr;
    bRefundPending = false;
    bWaitingForDriver = false;
}

void AVehicleClock::RequestDrift(bool Held)
{
    bDriftHeld = Held;
    UntilResearchCheck = 0.f;
    UntilDriverCheck = 0.f;
    if (!Held)
    {
        DriftEntrySpeed = 0.f;
        ThrustDirection = 0;
        DriftBlend = 0.f;
        if (LastRearSlip > 0.f) SetWheelHandling(LastGripBonus, LastBrakeBonus);
    }
    SetActorTickInterval(Held || Applied > 1.f ? 0.f : .2f);
    ForceNetUpdate();
}

float AVehicleClock::MeterBooster(float Seconds)
{
    constexpr float BurnMW = 100.f;
    const float Demand = BurnMW * Seconds;
    if (Demand <= 0.f) return 0.f;
    const float EnergyPerItem = FuelCount && FuelType ? UFGItemDescriptor::GetEnergyValue(FuelType) : 0.f;
    while (FuelCharge < Demand && FuelCount > 0 && EnergyPerItem > 0.f)
    {
        auto* Reserve = GetBoosterInventory();
        if (!Reserve || Reserve->GetNumItems(FuelType) <= 0) break;
        FuelCharge += EnergyPerItem;
        BurningType = FuelType;
        Reserve->RemoveFromIndex(0, 1);
        RefreshBoosterStack();
    }
    const float Used = FMath::Min(Demand, FuelCharge);
    FuelCharge -= Used;
    if (!FuelCharge) BurningType = nullptr;
    return Used / Demand;
}

void AVehicleClock::UpdateBoost(float Seconds, float Steering, bool Grounded)
{
    auto* Car = CastChecked<AFGWheeledVehicle>(Vehicle);
    auto* Mesh = Car->GetMesh();
    const FVector Forward = Car->GetActorForwardVector().GetSafeNormal2D();
    const FVector Right(-Forward.Y, Forward.X, 0.f);
    const FVector Velocity = Mesh->GetPhysicsLinearVelocity();
    const float Speed = FVector::DotProduct(Velocity, Forward);
    float Duty = 1.f;
    if (HasAuthority())
    {
        int8 Direction = 0;
        if (bDriftHeld && Grounded && !Car->IsSimulated() && Car->GetDriver() && HasBoosterFuel())
        {
            if (Handling.bRocket && Speed < 4200.f) Direction = 2;
            else if (Handling.bDrift && Speed > 800.f && FMath::Abs(Steering) > .15f &&
                Car->GetVehicleMovementComponent()->GetModifiedBrakeInput() < .1f &&
                !Car->GetVehicleMovementComponent()->GetHandbrakeInput())
                Direction = Steering > 0.f ? 1 : -1;
        }
        if (Direction && (Duty = MeterBooster(Seconds)) <= 0.f) Direction = 0;
        if (Direction != ThrustDirection) { ThrustDirection = Direction; ForceNetUpdate(); }
    }
    DriftBlend = FMath::Abs(ThrustDirection) == 1 ? Duty : 0.f;
    if (!ThrustDirection || !Grounded)
    {
        DriftEntrySpeed = 0.f;
        return;
    }
    if (ThrustDirection == 2)
    {
        const float Acceleration = FMath::Clamp((4200.f - Speed) * 2.5f, 0.f, 1450.f);
        Mesh->AddForce(Forward * Acceleration * Duty, NAME_None, true);
        return;
    }
    const float Side = float(ThrustDirection);
    if (DriftEntrySpeed <= 0.f) DriftEntrySpeed = Speed;
    // Maintain the entry pace through tyre scrub. Releasing Boost or braking ends the assist.
    const float TargetSlip = FMath::Clamp(Speed * .22f, 240.f, 560.f);
    const float Slip = -FVector::DotProduct(Velocity, Right) * Side;
    const float SideForce = FMath::Clamp((TargetSlip - Slip) * 4.f, 0.f, 1350.f);
    const float ForwardForce = FMath::Clamp((DriftEntrySpeed * 1.09f - Speed) * 6.f, 0.f, 2250.f);
    // Direct part of the side thrust through the rear of the chassis to swing the tail.
    const FVector Lateral = -Right * Side * SideForce * Duty;
    Mesh->AddForce(Forward * ForwardForce * Duty + Lateral * .6f, NAME_None, true);
    const FVector Rear = Mesh->GetCenterOfMass() - Forward * (VehicleFamily(Car) == EVehicleFamily::Truck ? 150.f : 100.f);
    Mesh->AddForceAtLocation(Lateral * (.4f * Mesh->GetMass()), Rear);
    const float Yaw = Mesh->GetPhysicsAngularVelocityInRadians().Z;
    const float TargetYaw = Side * FMath::Lerp(.38f, 1.05f, FMath::Abs(Steering));
    const float YawAssist = FMath::Clamp((TargetYaw - Yaw) * 5.f, -3.8f, 3.8f);
    Mesh->AddTorqueInRadians(FVector(0,0,YawAssist * Duty), NAME_None, true);
}

void AVehicleClock::SetWheelHandling(float GripBonus,float BrakeBonus,float RearSlip)
{
    if(FMath::IsNearlyEqual(GripBonus,LastGripBonus,0.0001f) && FMath::IsNearlyEqual(BrakeBonus,LastBrakeBonus,0.0001f) && FMath::IsNearlyEqual(RearSlip,LastRearSlip,.001f))return;
    auto* Car=Cast<AFGWheeledVehicle>(Vehicle);
    auto* Move=Car?Car->GetVehicleMovementComponent():nullptr;
    if(!Move || !Move->HasValidPhysicsState())return;
    for(int32 I=0;I<Move->Wheels.Num();++I)
    {
        const auto* Wheel=Move->Wheels[I].Get();
        if(!Wheel)continue;
        const bool Rear = Wheel->AxleType == EAxleType::Rear || (Wheel->AxleType == EAxleType::Undefined && !Wheel->bAffectedBySteering);
        Move->SetWheelFrictionMultiplier(I,Wheel->FrictionForceMultiplier*(1.f+GripBonus)*(Rear ? 1.f-.46f*RearSlip : 1.f));
        if(Wheel->bAffectedByBrake)Move->SetWheelMaxBrakeTorque(I,Wheel->MaxBrakeTorque*(1.f+BrakeBonus));
    }
    LastGripBonus=GripBonus; LastBrakeBonus=BrakeBonus; LastRearSlip=RearSlip;
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
    if(!Move || !Mesh || !Move->HasValidPhysicsState() || !Mesh->IsSimulatingPhysics() ||
        (!HasAuthority() && !Car->IsLocallyControlled())) return;
    const FVector Velocity=Mesh->GetPhysicsLinearVelocity();
    const FVector2D Horizontal(Velocity.X,Velocity.Y);
    const float Speed=Horizontal.Size();
    SetBallast(Applied > 1.f ? Handling.Ballast : 0);
    SetSteering(Applied > 1.f ? Handling.Steering : 0);
    const float Blend=Applied > 1.f && StockRoadSpeed > 0.f ? FMath::SmoothStep(0.55f,1.15f,Speed/StockRoadSpeed) : 0.f;
    // Quantise only wheel config writes. Continuous chassis forces stay smooth.
    const float WheelBlend=FMath::RoundToFloat(Blend*32.f)/32.f;

    int32 Contacts=0;
    for(int32 I=0;I<Move->Wheels.Num();++I)if(Move->GetWheelState(I).bInContact)++Contacts;
    const bool Grounded = Contacts >= 2 && Car->GetActorUpVector().Z > .45f;
    const float Steering = Move->GetModifiedSteeringInput();
    UpdateBoost(DeltaSeconds, Steering, Grounded);
    SetWheelHandling(.12f*Handling.Grip*WheelBlend,.15f*Handling.Brakes*WheelBlend,FMath::RoundToFloat(DriftBlend*32.f)/32.f);
    if (!Grounded || (Blend <= 0.f && DriftBlend <= 0.f)) return;
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
        const float Damping = BallastRank == 3 ? 1.25f : .65f;
        const float SideLimit = BallastRank == 3 ? .36f : .18f;
        Acceleration -= Right * FMath::Clamp(SideSpeed*Damping,-SideLimit*Gravity,SideLimit*Gravity) * (1.f - .8f*DriftBlend);
        AngularAccel-=Forward*FMath::Clamp(FVector::DotProduct(Angular,Forward)*2.0,-3.0,3.0);
    }
    if (BallastRank >= 2)
    {
        const float RollRate = FVector::DotProduct(Angular,Forward);
        const float RollAngle = FVector::DotProduct(Car->GetActorUpVector(),Right);
        AngularAccel += Forward * FMath::Clamp(RollAngle*6.f - RollRate*2.f,-4.f,4.f);
        Acceleration.Z -= FMath::Clamp(float(Velocity.Z)*.65f,0.f,160.f);
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
        StockEffort = Curve;
    }
    Speed->SetPropertyValue_InContainer(Move, StockRailSpeed * Scale);
    Drag->SetPropertyValue_InContainer(Move, StockDrag / FMath::Square(Scale));
    const float DemandRate = VehicleClock::Energy(Clock, Handling.Efficiency);
    Power = FFloatInterval(StockRailPower.Min * DemandRate, StockRailPower.Max * DemandRate);
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
    if (Operation != EClockEdit::Remove && !FVehicleHandlingRanks::OverclockUnlocked(GetWorld()))
    {
        Feedback(Target, TEXT("Research Vehicle overclocking in the MAM first."));
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
            if (!Clock->Shards) Clock->FitTo(Target, Person);
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
    if (!Clock->Shards)
    {
        Clock->ClearShardOwner();
        if (!Clock->HasBoosterFuel()) Clock->Destroy();
    }
}

void UVehicleClockRemote::Drift_Implementation(AFGVehicle* ExpectedVehicle, bool Held)
{
    if (!ExpectedVehicle || ExpectedVehicle != VehicleClock::DrivenBy(GetOwnerPlayerController())) return;
    auto* Clock = AVehicleClock::Find(ExpectedVehicle);
    if (Held && (!Clock || !Clock->HasBoosterFuel())) return;
    if (Clock) Clock->RequestDrift(Held);
}

void UVehicleClockRemote::Feedback_Implementation(AFGVehicle* ExpectedVehicle, const FString& Message)
{
    FeedbackVehicle = ExpectedVehicle;
    LastMessage = Message;
}





