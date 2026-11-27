#include "VehicleClock.h"

#include "Components/SkeletalMeshComponent.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "Engine/World.h"

void AVehicleClock::RequestFlight(FVector2D Axes, bool Modifier)
{
    FlightAxes = Axes;
    bAirModifier = Modifier;
    FlightInputAge = 0.f;
    SetActorTickInterval(0.f);
}

void UVehicleClockRemote::Flight_Implementation(AFGVehicle* ExpectedVehicle, FVector2D Axes, bool Modifier)
{
    if (!ExpectedVehicle || ExpectedVehicle != VehicleClock::DrivenBy(GetOwnerPlayerController()) ||
        (VehicleFamily(ExpectedVehicle) != EVehicleFamily::Explorer && VehicleFamily(ExpectedVehicle) != EVehicleFamily::Tractor) ||
        !FMath::IsFinite(Axes.X) || !FMath::IsFinite(Axes.Y)) return;
    if (!FVehicleHandlingRanks::Read(GetWorld(), ExpectedVehicle).AirControl) return;
    auto* Clock = AVehicleClock::Find(ExpectedVehicle);
    if (!Clock)
    {
        Clock = GetWorld()->SpawnActor<AVehicleClock>();
        if (!Clock) return;
        Clock->FitTo(ExpectedVehicle);
        Clock->UpdateDrive();
    }
    Clock->RequestFlight(FVector2D(FMath::Clamp(Axes.X, -1., 1.), FMath::Clamp(Axes.Y, -1., 1.)), Modifier);
}

void AVehicleClock::UpdateBoost(float Seconds, float Steering, bool Grounded)
{
    auto* Car = CastChecked<AFGWheeledVehicle>(Vehicle);
    auto* Move = Car->GetVehicleMovementComponent();
    auto* Mesh = Car->GetMesh();
    const FVector Forward = Car->GetActorForwardVector().GetSafeNormal2D();
    const FVector Right(-Forward.Y, Forward.X, 0.f);
    const FVector Velocity = Mesh->GetPhysicsLinearVelocity();
    const float Speed = FVector::DotProduct(Velocity, Forward);
    if (!Grounded)
    {
        DriftBlend = FMath::FInterpConstantTo(DriftBlend, 0.f, Seconds, 4.f);
        if (HasAuthority() && ThrustDirection != 3) ThrustDirection = 0;
        return;
    }
    if (bWasAirborne && Handling.Landing >= 2 && bDriftHeld)
        DriftBlend = FMath::Max(DriftBlend, .3f);
    float Duty = 1.f;
    if (HasAuthority())
    {
        int8 Direction = 0;
        const bool Braking = Move->GetModifiedBrakeInput() >= .1f || Move->GetHandbrakeInput();
        if (bDriftHeld && Grounded && !Car->IsSimulated() && HasBoosterFuel() && !Braking)
        {
            if (Handling.bRocket && Speed < 4200.f) Direction = 2;
            else if (Handling.bDrift && Speed > 800.f)
            {
                // Keep the original drift side through countersteer.
                if (FMath::Abs(ThrustDirection) == 1) Direction = ThrustDirection;
                else if (FMath::Abs(Steering) > .15f) Direction = Steering > 0.f ? 1 : -1;
            }
        }
        if (Direction && (Duty = MeterBooster(Seconds)) <= 0.f) Direction = 0;
        if (Direction != ThrustDirection)
        {
            ThrustDirection = Direction;
            ForceNetUpdate();
        }
    }
    const bool Drifting = FMath::Abs(ThrustDirection) == 1 && Grounded;
    const float EntrySeconds = .8f - .2f * Handling.DriftControl;
    const float ExitSeconds = Handling.bDriftExit ? .45f : .25f;
    DriftBlend = FMath::FInterpConstantTo(DriftBlend, Drifting ? 1.f : 0.f, Seconds,
        1.f / (Drifting ? EntrySeconds : ExitSeconds));
    if (ThrustDirection == 2)
    {
        const float Acceleration = FMath::Clamp((4200.f - Speed) * 2.5f, 0.f, 1450.f);
        Mesh->AddForce(Forward * Acceleration * Duty, NAME_None, true);
        return;
    }
    const float Yaw = Mesh->GetPhysicsAngularVelocityInRadians().Z;
    if (!Drifting)
    {
        if (Handling.bDriftExit && DriftBlend > 0.f && Velocity.Size2D() > 400.f)
        {
            // Rotate toward the existing momentum. The lateral correction is
            // perpendicular to travel, so release does not award free thrust.
            const FVector Travel = Velocity.GetSafeNormal2D();
            const float Error = FMath::Atan2(FVector::CrossProduct(Forward, Travel).Z,
                FVector::DotProduct(Forward, Travel));
            const float Recovery = FMath::Clamp(Error * 4.f - Yaw * 2.f, -3.f, 3.f);
            Mesh->AddTorqueInRadians(FVector(0, 0, Recovery * DriftBlend), NAME_None, true);
            FVector Correction = -Right * FVector::DotProduct(Velocity, Right) * 2.f;
            Correction -= Travel * FVector::DotProduct(Correction, Travel);
            Mesh->AddForce(Correction.GetClampedToMaxSize(1000.f) * DriftBlend, NAME_None, true);
        }
        if (DriftBlend <= 0.f) DriftEntrySpeed = 0.f;
        return;
    }
    if (DriftEntrySpeed <= 0.f) DriftEntrySpeed = Velocity.Size2D();
    const float Side = float(ThrustDirection);
    const float Turn = FMath::Clamp(Steering * Side, -1.f, 1.f);
    const float Strength = FMath::SmoothStep(0.f, 1.f, DriftBlend);
    const float TargetSlip = Side * FMath::Clamp(Speed * (.12f + .06f * Turn), 90.f, 480.f) * Strength;
    const float SideSpeed = FVector::DotProduct(Velocity, Right);
    const float SideForce = FMath::Clamp((-TargetSlip - SideSpeed) * 3.f, -1100.f, 1100.f);
    const float EntryTarget = FMath::Min(DriftEntrySpeed * (1.f + .025f * Handling.DriftControl), 4600.f);
    const float ForwardForce = FMath::Clamp((EntryTarget - Velocity.Size2D()) * 5.f, 0.f, 2100.f);
    Mesh->AddForce((Forward * ForwardForce + Right * SideForce) * Strength * Duty, NAME_None, true);
    const float TargetYaw = Side * FMath::Lerp(.12f, .85f + .1f * Handling.DriftControl, (Turn + 1.f) * .5f);
    const float YawAssist = FMath::Clamp((TargetYaw - Yaw) * (4.f + Handling.DriftControl), -3.5f, 3.5f);
    Mesh->AddTorqueInRadians(FVector(0, 0, YawAssist * Strength * Duty), NAME_None, true);
}

void AVehicleClock::UpdateAirControl(float Seconds, int32 Contacts)
{
    if (!Handling.AirControl || Contacts > 0)
    {
        if (HasAuthority() && ThrustDirection == 3) ThrustDirection = 0;
        return;
    }
    auto* Car = CastChecked<AFGWheeledVehicle>(Vehicle);
    auto* Mesh = Car->GetMesh();
    AirTime += Seconds;
    if (AirTime < .10f) return;
    bWasAirborne = true;
    const FVector Forward = Car->GetActorForwardVector();
    const FVector Right = Car->GetActorRightVector();
    const FVector Angular = Mesh->GetPhysicsAngularVelocityInRadians();
    static constexpr float RotationRates[] = {0.f, .95f, 1.8f, 3.4f};
    const float Rate = RotationRates[Handling.AirControl];
    const FVector Target = bAirModifier
        ? ((Right * FlightAxes.Y - Forward * FlightAxes.X) * Rate).GetClampedToMaxSize(Rate)
        : FVector::UpVector * FlightAxes.X * (.55f + .3f * Handling.AirControl);
    // Pitch and roll have no angle limit, allowing complete flips and rolls.
    const FVector Torque = ((Target - Angular) * (5.f + Handling.AirControl)).GetClampedToMaxSize(12.f);
    Mesh->AddTorqueInRadians(Torque, NAME_None, true);

    float Duty = 1.f;
    if (HasAuthority())
    {
        Duty = bDriftHeld && Handling.bAirThruster && HasBoosterFuel() ? MeterBooster(Seconds) : 0.f;
        const int8 Direction = Duty > 0.f ? 3 : 0;
        if (Direction != ThrustDirection)
        {
            ThrustDirection = Direction;
            ForceNetUpdate();
        }
        // Meter once, on the server. Orientation control has no fuel demand.
    }
    if (ThrustDirection != 3) return;
    const FVector Heading = FRotator(0.f, Car->GetActorRotation().Yaw, 0.f).Vector();
    const FVector Side(-Heading.Y, Heading.X, 0);
    FVector Direction = Heading * FlightAxes.Y + Side * FlightAxes.X;
    if (Direction.IsNearlyZero()) Direction = Heading;
    Direction = Direction.GetSafeNormal();
    const float Along = FVector::DotProduct(Mesh->GetPhysicsLinearVelocity(), Direction);
    const float Acceleration = FMath::Clamp((4600.f - Along) * 2.f, 0.f, 650.f);
    Mesh->AddForce(Direction * Acceleration * Duty, NAME_None, true);
}

void AVehicleClock::UpdateLanding(float Seconds, bool Grounded)
{
    auto* Car = CastChecked<AFGWheeledVehicle>(Vehicle);
    auto* Mesh = Car->GetMesh();
    if (Grounded)
    {
        if (bWasAirborne) LandingTime = Handling.Landing ? .5f + .25f * Handling.Landing : 0.f;
        bWasAirborne = false;
        AirTime = 0.f;
    }
    if (!Handling.Landing) return;
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(VehicleLanding), false, Car);
    const FVector Location = Mesh->GetCenterOfMass();
    const bool Surface = GetWorld()->LineTraceSingleByChannel(Hit, Location, Location - FVector(0, 0, 240),
        ECC_Visibility, Query) && Hit.ImpactNormal.Z > .45f;
    const FVector Velocity = Mesh->GetPhysicsLinearVelocity();
    if (!Grounded && bWasAirborne && Surface && Velocity.Z < -300.f)
    {
        // Slow descent only within suspension approach distance of a surface.
        const float Cushion = FMath::Clamp((-Velocity.Z - 300.f) * Handling.Landing, 0.f, 1600.f);
        Mesh->AddForce(FVector(0, 0, Cushion), NAME_None, true);
    }
    if (!Grounded || LandingTime <= 0.f) return;
    LandingTime = FMath::Max(0.f, LandingTime - Seconds);
    const FVector Normal = Surface ? Hit.ImpactNormal : FVector::UpVector;
    const FVector Angular = Mesh->GetPhysicsAngularVelocityInRadians();
    const FVector Tilt = FVector::CrossProduct(Car->GetActorUpVector(), Normal);
    // Preserve the driver's yaw and drift as pitch and roll settle onto the road.
    const FVector PlanarAngular = Angular - Normal * FVector::DotProduct(Angular, Normal);
    const FVector Torque = (Tilt * (5.f + Handling.Landing) - PlanarAngular * 3.f).GetClampedToMaxSize(6.f);
    Mesh->AddTorqueInRadians(Torque, NAME_None, true);
    if (Velocity.Z > 0.f)
        Mesh->AddForce(FVector(0, 0, -FMath::Min(float(Velocity.Z) * Handling.Landing * 2.f, 1400.f)), NAME_None, true);
}
