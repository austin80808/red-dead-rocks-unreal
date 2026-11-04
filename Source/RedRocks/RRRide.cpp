#include "RRRide.h"

#include "RRExplorer.h"
#include "RRWeather.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float G = 980.f;

	UStaticMeshComponent* Part(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh,
		const FVector& Loc, const FRotator& Rot, const FVector& Scale)
	{
		UStaticMeshComponent* P = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		P->SetupAttachment(Parent);
		P->SetStaticMesh(Mesh);
		P->SetRelativeLocation(Loc);
		P->SetRelativeRotation(Rot);
		P->SetRelativeScale3D(Scale);
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetGenerateOverlapEvents(false);
		return P;
	}
}

ARRRide::ARRRide()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
	Visual = CreateDefaultSubobject<USceneComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Root);
	Seat = CreateDefaultSubobject<USceneComponent>(TEXT("Seat"));
	Seat->SetupAttachment(Visual);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeF(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylF(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cube = CubeF.Object;
	UStaticMesh* Cyl = CylF.Object;
	const FRotator Axle(0.f, 0.f, 90.f);

	// Shopping cart: X forward, origin on the ground under its middle.
	CartParts.Add(Part(this, Visual, TEXT("CartFloor"), Cube, FVector(0, 0, 55), FRotator::ZeroRotator, FVector(0.92f, 0.56f, 0.03f)));
	CartParts.Add(Part(this, Visual, TEXT("CartSideL"), Cube, FVector(0, -28, 82), FRotator::ZeroRotator, FVector(0.92f, 0.02f, 0.52f)));
	CartParts.Add(Part(this, Visual, TEXT("CartSideR"), Cube, FVector(0, 28, 82), FRotator::ZeroRotator, FVector(0.92f, 0.02f, 0.52f)));
	CartParts.Add(Part(this, Visual, TEXT("CartFront"), Cube, FVector(46, 0, 80), FRotator(-8, 0, 0), FVector(0.02f, 0.56f, 0.5f)));
	CartParts.Add(Part(this, Visual, TEXT("CartBack"), Cube, FVector(-46, 0, 86), FRotator::ZeroRotator, FVector(0.02f, 0.56f, 0.6f)));
	CartParts.Add(Part(this, Visual, TEXT("CartTray"), Cube, FVector(0, 0, 22), FRotator::ZeroRotator, FVector(0.72f, 0.46f, 0.02f)));
	CartParts.Add(Part(this, Visual, TEXT("CartHandle"), Cyl, FVector(-62, 0, 118), FRotator(0, 0, 90), FVector(0.045f, 0.045f, 0.62f)));
	CartParts.Add(Part(this, Visual, TEXT("CartGripL"), Cube, FVector(-54, -26, 104), FRotator(-22, 0, 0), FVector(0.025f, 0.025f, 0.34f)));
	CartParts.Add(Part(this, Visual, TEXT("CartGripR"), Cube, FVector(-54, 26, 104), FRotator(-22, 0, 0), FVector(0.025f, 0.025f, 0.34f)));
	for (int32 i = 0; i < 4; ++i)
	{
		const float X = (i < 2) ? 38.f : -38.f;
		const float Y = (i % 2) ? 23.f : -23.f;
		CartParts.Add(Part(this, Visual, *FString::Printf(TEXT("CartLeg%d"), i), Cube, FVector(X, Y, 32), FRotator::ZeroRotator, FVector(0.03f, 0.03f, 0.46f)));
		CartParts.Add(Part(this, Visual, *FString::Printf(TEXT("CartWheel%d"), i), Cyl, FVector(X, Y, 8), Axle, FVector(0.16f, 0.16f, 0.05f)));
	}

	// Sled: a red toboggan with a curled front.
	SledParts.Add(Part(this, Visual, TEXT("SledDeck"), Cube, FVector(0, 0, 12), FRotator::ZeroRotator, FVector(1.35f, 0.48f, 0.04f)));
	SledParts.Add(Part(this, Visual, TEXT("SledCurl"), Cyl, FVector(70, 0, 24), Axle, FVector(0.26f, 0.26f, 0.48f)));
	SledParts.Add(Part(this, Visual, TEXT("SledRailL"), Cube, FVector(0, -21, 5), FRotator::ZeroRotator, FVector(1.25f, 0.03f, 0.05f)));
	SledParts.Add(Part(this, Visual, TEXT("SledRailR"), Cube, FVector(0, 21, 5), FRotator::ZeroRotator, FVector(1.25f, 0.03f, 0.05f)));
	SledParts.Add(Part(this, Visual, TEXT("SledRopeBar"), Cube, FVector(52, 0, 40), FRotator::ZeroRotator, FVector(0.03f, 0.5f, 0.03f)));
}

void ARRRide::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const bool bCart = Kind == ERRRideKind::Cart;
	for (UStaticMeshComponent* P : CartParts) P->SetVisibility(bCart);
	for (UStaticMeshComponent* P : SledParts) P->SetVisibility(!bCart);
	// The rider stands in the basket, or kneels on the sled.
	Seat->SetRelativeLocation(bCart ? FVector(-6, 0, 58 + 92) : FVector(-15, 0, 16 + 70));
}

void ARRRide::BeginPlay()
{
	Super::BeginPlay();
	OnConstruction(GetActorTransform());
	for (UStaticMeshComponent* P : CartParts)
	{
		if (UMaterialInstanceDynamic* M = P->CreateDynamicMaterialInstance(0))
		{
			const bool bRed = P->GetName().Contains(TEXT("Handle")) || P->GetName().Contains(TEXT("Grip"));
			const bool bWheel = P->GetName().Contains(TEXT("Wheel"));
			M->SetVectorParameterValue(TEXT("Color"), bRed ? FLinearColor(0.8f, 0.05f, 0.03f)
				: bWheel ? FLinearColor(0.02f, 0.02f, 0.02f) : FLinearColor(0.62f, 0.64f, 0.66f));
		}
	}
	for (UStaticMeshComponent* P : SledParts)
	{
		if (UMaterialInstanceDynamic* M = P->CreateDynamicMaterialInstance(0))
		{
			const bool bRail = P->GetName().Contains(TEXT("Rail")) || P->GetName().Contains(TEXT("Rope"));
			M->SetVectorParameterValue(TEXT("Color"), bRail ? FLinearColor(0.15f, 0.15f, 0.17f) : FLinearColor(0.75f, 0.04f, 0.03f));
		}
	}
	Heading = GetActorRotation().Yaw;
	SnapToGround();
	Home = GetActorTransform();
}

void ARRRide::ResetToHome()
{
	if (Rider) Dismount(false);
	SetActorTransform(Home);
	Heading = Home.Rotator().Yaw;
	Velocity = FVector::ZeroVector;
	SnapToGround();
}

FString ARRRide::GetRideName() const { return Kind == ERRRideKind::Cart ? TEXT("Shopping cart") : TEXT("Sled"); }

FString ARRRide::GetPrompt() const
{
	return Kind == ERRRideKind::Cart ? TEXT("climb into the shopping cart") : TEXT("hop on the sled");
}

FString ARRRide::GetBoardMessage() const
{
	if (Kind == ERRRideKind::Cart)
	{
		return DestinationName.IsEmpty() ? FString(TEXT("Hold on."))
			: FString::Printf(TEXT("Next stop: %s.  Stay on the road."), *DestinationName);
	}
	const ARRWeather* W = ARRWeather::Find(this);
	return (W && W->GetSnow() > 0.3f) ? FString(TEXT("Fresh powder.  Point it downhill."))
		: FString(TEXT("Sleds need snow.  Press N to make it snow."));
}

FString ARRRide::GetRideHint() const
{
	FString H = TEXT("W push   S brake   A/D steer   SPACE hop   E get off");
	const float D = GetDistanceToDestinationM();
	if (D >= 0.f && !bArrived)
	{
		H += FString::Printf(TEXT("\n%s  %.0f m"), *DestinationName, D);
	}
	return H;
}

float ARRRide::GetDistanceToDestinationM() const
{
	if (Destination.IsNearlyZero()) return -1.f;
	return FVector::Dist2D(Destination, GetActorLocation()) / 100.f;
}

void ARRRide::Mount(ARRExplorer* InRider)
{
	Rider = InRider;
	bArrived = false;
	RouteIndex = 0;
	if (Route.Num() > 1)
	{
		float Best = TNumericLimits<float>::Max();
		for (int32 i = 0; i < Route.Num(); ++i)
		{
			const float D = FVector::DistSquared2D(Route[i], GetActorLocation());
			if (D < Best) { Best = D; RouteIndex = i; }
		}
	}
	TopSpeed = 0.f;
	MaxAirTime = 0.f;
	Rider->SetActorEnableCollision(false);
	Rider->GetCharacterMovement()->StopMovementImmediately();
	Rider->GetCharacterMovement()->DisableMovement();
	Rider->AttachToComponent(Seat, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	if (AController* C = Rider->GetController())
	{
		C->SetControlRotation(FRotator(-12.f, Heading, 0.f));
	}
}

void ARRRide::Dismount(bool bCrashed)
{
	if (!Rider) return;
	ARRExplorer* Was = Rider;
	Rider = nullptr;
	Was->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Was->SetActorEnableCollision(true);
	const FVector Right = FRotationMatrix(FRotator(0, Heading, 0)).GetUnitAxis(EAxis::Y);
	FVector Eject = Velocity * 0.85f + FVector(0, 0, 450.f);
	if (!bCrashed)
	{
		Was->SetActorLocation(GetActorLocation() + Right * 110.f + FVector(0, 0, 120.f), false, nullptr, ETeleportType::TeleportPhysics);
		Was->SetActorRotation(FRotator(0, Heading, 0));
	}
	else
	{
		Was->SetActorLocation(GetActorLocation() + FVector(0, 0, 200.f), false, nullptr, ETeleportType::TeleportPhysics);
	}
	Was->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	Was->OnRideEnded(bCrashed, Eject);
	if (bCrashed) Velocity *= 0.3f;
	else if (TopSpeed > 800.f)
	{
		Was->Toast(FString::Printf(TEXT("Top speed %.0f km/h, longest air %.1f s"), TopSpeed * 0.036f, MaxAirTime), 4.f);
	}
}

void ARRRide::Hop()
{
	if (bGrounded)
	{
		Velocity += GroundNormal * (Kind == ERRRideKind::Cart ? 420.f : 380.f);
		bGrounded = false;
	}
}

float ARRRide::RollingResistance() const
{
	const ARRWeather* W = ARRWeather::Find(this);
	const float Snow = W ? W->GetSnow() : 0.f;
	if (Kind == ERRRideKind::Sled)
	{
		// Snow makes a sled fly; bare red dirt barely lets it creep.
		return FMath::Lerp(0.55f, 0.035f, FMath::Clamp(Snow * 1.4f, 0.f, 1.f));
	}
	// Cart: casters on asphalt roll; off the road they bog down. Snow evens it out a bit.
	const float Base = bOnRoad ? 0.018f : 0.16f;
	return FMath::Lerp(Base, FMath::Min(Base, 0.06f), Snow);
}

float ARRRide::LateralGrip() const
{
	const ARRWeather* W = ARRWeather::Find(this);
	const float Snow = W ? W->GetSnow() : 0.f;
	if (Kind == ERRRideKind::Sled) return 2.6f;
	return FMath::Lerp(9.f, 2.2f, Snow); // carts drift on snow
}

bool ARRRide::RouteFrame(const FVector& P, FVector& OutDir, float& OutLateral) const
{
	if (Route.Num() < 2) return false;
	// Search a window around the last index (the cart only moves forward along it).
	float Best = TNumericLimits<float>::Max();
	int32 BestI = RouteIndex;
	const int32 Lo = FMath::Max(0, RouteIndex - 3);
	const int32 Hi = FMath::Min(Route.Num() - 2, RouteIndex + 12);
	FVector BestPoint = Route[0];
	for (int32 i = Lo; i <= Hi; ++i)
	{
		const FVector A(Route[i].X, Route[i].Y, 0.f), B(Route[i + 1].X, Route[i + 1].Y, 0.f);
		const FVector Q = FMath::ClosestPointOnSegment(FVector(P.X, P.Y, 0.f), A, B);
		const float D = FVector::DistSquared(Q, FVector(P.X, P.Y, 0.f));
		if (D < Best) { Best = D; BestI = i; BestPoint = Q; }
	}
	const_cast<ARRRide*>(this)->RouteIndex = BestI;
	// Aim a little ahead so curves are taken smoothly.
	const int32 AheadI = FMath::Min(BestI + 2, Route.Num() - 1);
	OutDir = FVector(Route[AheadI].X - BestPoint.X, Route[AheadI].Y - BestPoint.Y, 0.f).GetSafeNormal();
	if (OutDir.IsNearlyZero()) OutDir = FVector(Route[BestI + 1] - Route[BestI]).GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, OutDir);
	OutLateral = FVector::DotProduct(FVector(P.X, P.Y, 0.f) - BestPoint, Right);
	return FMath::Sqrt(Best) < 3000.f; // within 30 m counts as on the road
}

bool ARRRide::TraceGround(const FVector& At, float Below, FHitResult& Out) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRideGround), true, this);
	if (Rider) Params.AddIgnoredActor(Rider);
	return GetWorld()->LineTraceSingleByChannel(Out, At + FVector(0, 0, 150.f), At - FVector(0, 0, Below), ECC_Visibility, Params);
}

void ARRRide::SnapToGround()
{
	FHitResult Hit;
	if (TraceGround(GetActorLocation() + FVector(0, 0, 1500.f), 20000.f, Hit))
	{
		SetActorLocation(Hit.ImpactPoint);
		GroundNormal = Hit.ImpactNormal;
		bGrounded = true;
	}
	ApplyPose(1.f);
}

void ARRRide::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Rider && Velocity.IsNearlyZero(1.f) && bGrounded) return;
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(Dt / (1.f / 120.f)), 1, 8);
	for (int32 i = 0; i < Steps; ++i) Step(Dt / Steps);
	ApplyPose(Dt);

	if (Rider)
	{
		TopSpeed = FMath::Max(TopSpeed, Velocity.Size());
		// Keep the chase camera looking down the road unless the mouse is moving it.
		if (AController* C = Rider->GetController())
		{
			FRotator R = C->GetControlRotation();
			const float Speed = Velocity.Size2D();
			if (Speed > 150.f)
			{
				const float Travel = Velocity.Rotation().Yaw;
				R.Yaw = FMath::FixedTurn(R.Yaw, Travel, Dt * FMath::Min(Speed / 8.f, 200.f));
			}
			R.Pitch = FMath::FInterpTo(FRotator::NormalizeAxis(R.Pitch), -14.f, Dt, 1.5f);
			C->SetControlRotation(R);
		}
		const float D = GetDistanceToDestinationM();
		if (!bArrived && D >= 0.f && D < 120.f)
		{
			bArrived = true;
			Rider->Toast(FString::Printf(TEXT("Welcome to %s!  Top speed %.0f km/h."), *DestinationName, TopSpeed * 0.036f), 6.f);
		}
	}
	if (GetActorLocation().Z < -20000.f) ResetToHome();
}

void ARRRide::Step(float Dt)
{
	const FVector2D In = Rider ? Rider->GetMoveInput() : FVector2D::ZeroVector;
	const float Steer = In.X;
	const float Throttle = FMath::Max(0.f, In.Y);
	const float Brake = FMath::Max(0.f, -In.Y);
	const FVector Gv(0, 0, -G);

	FVector HeadingVec = FRotationMatrix(FRotator(0, Heading, 0)).GetUnitAxis(EAxis::X);
	if (bGrounded)
	{
		const FVector N = GroundNormal;
		const FVector Fwd = FVector::VectorPlaneProject(HeadingVec, N).GetSafeNormal();
		const FVector Right = FVector::CrossProduct(N, Fwd).GetSafeNormal();
		const FVector Slope = Gv - FVector::DotProduct(Gv, N) * N;
		float Vf = FVector::DotProduct(Velocity, Fwd);
		float Vr = FVector::DotProduct(Velocity, Right);

		float A = FVector::DotProduct(Slope, Fwd);
		// Push off like a kid with a cart (or paddle the sled) up to a running pace.
		const float PushMax = Kind == ERRRideKind::Cart ? 650.f : 380.f;
		if (Throttle > 0.f && Vf < PushMax) A += Throttle * (Kind == ERRRideKind::Cart ? 420.f : 260.f);
		Vf += A * Dt;
		// Resistances oppose motion and never reverse it.
		const float Normal = FMath::Max(0.f, -FVector::DotProduct(Gv, N));
		const float Resist = RollingResistance() * Normal + Brake * (Kind == ERRRideKind::Cart ? 650.f : 450.f)
			+ 0.000045f * Vf * Vf;
		const float Dv = Resist * Dt;
		Vf = FMath::Abs(Vf) <= Dv ? 0.f : Vf - FMath::Sign(Vf) * Dv;

		Vr += FVector::DotProduct(Slope, Right) * Dt;
		Vr *= FMath::Exp(-LateralGrip() * Dt);

		// Steering: casters turn harder at low speed, sleds are lazy.
		const float SpeedAbs = FMath::Abs(Vf);
		const float Rate = (Kind == ERRRideKind::Cart ? 95.f : 60.f) * FMath::Clamp(SpeedAbs / 250.f, 0.f, 1.f)
			/ (1.f + SpeedAbs / 3500.f);
		Heading += Steer * Rate * Dt * (Vf >= 0.f ? 1.f : -1.f);

		// Cartoon road assist: the cart knows the way to town.
		FVector RoadDir;
		float Lateral = 0.f;
		bOnRoute = Kind == ERRRideKind::Cart && bRoadAssist && Rider && RouteFrame(GetActorLocation(), RoadDir, Lateral);
		if (bOnRoute)
		{
			// Lane keeping, unless the rider is steering hard.
			if (FMath::Abs(Steer) < 0.35f)
			{
				const float Want = RoadDir.Rotation().Yaw - FMath::Clamp(Lateral / 600.f, -1.f, 1.f) * 25.f;
				Heading = FMath::FixedTurn(Heading, Want, 140.f * Dt);
			}
			// Hold a fun speed: a gravity boost on the gentle grades, a soft brake on the steep ones.
			const float Target = CruiseSpeed * (Brake > 0.f ? 0.3f : 1.f) * (Throttle > 0.f ? 1.35f : 1.f);
			Vf += FMath::Clamp(Target - Vf, -350.f, 450.f) * Dt;
			// Bumps and kerbs don't throw it sideways.
			Vr *= FMath::Exp(-6.f * Dt);
		}
		Velocity = Fwd * Vf + Right * Vr;
		AirTime = 0.f;
	}
	else
	{
		Velocity += Gv * Dt;
		Heading += Steer * 40.f * Dt;
		AirTime += Dt;
		MaxAirTime = FMath::Max(MaxAirTime, AirTime);
	}

	// Sweep the body for walls, rocks, trees and buildings.
	FVector Pos = GetActorLocation();
	FVector Delta = Velocity * Dt;
	const FVector BoxHalf = Kind == ERRRideKind::Cart ? FVector(55, 32, 40) : FVector(70, 28, 22);
	const FVector Lift(0, 0, BoxHalf.Z + (Kind == ERRRideKind::Cart ? 35.f : 22.f));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRideSweep), false, this);
	if (Rider) Params.AddIgnoredActor(Rider);
	FHitResult Wall;
	const FQuat Q = FRotator(0, Heading, 0).Quaternion();
	if (!Delta.IsNearlyZero() && GetWorld()->SweepSingleByChannel(Wall, Pos + Lift, Pos + Lift + Delta, Q, ECC_Visibility,
		FCollisionShape::MakeBox(BoxHalf), Params) && Wall.bBlockingHit && !Wall.bStartPenetrating && Wall.ImpactNormal.Z < 0.55f)
	{
		const FVector N = FVector(Wall.ImpactNormal.X, Wall.ImpactNormal.Y, 0.f).GetSafeNormal();
		const float Into = -FVector::DotProduct(Velocity, N);
		if (Into > 0.f)
		{
			if (Into > (bOnRoute ? 3000.f : 1500.f) && Rider)
			{
				Pos += Delta * Wall.Time;
				SetActorLocation(Pos);
				Dismount(true);
				return;
			}
			Velocity += N * Into * 1.3f;
			Delta = Velocity * Dt * (1.f - Wall.Time);
		}
	}
	Pos += Delta;

	// Follow the ground, or fly off it.
	FHitResult Ground;
	const float Snap = bGrounded ? 70.f : 10.f;
	if (TraceGround(Pos, Snap, Ground) && Ground.ImpactNormal.Z > 0.25f)
	{
		const bool bAbove = Pos.Z > Ground.ImpactPoint.Z + 2.f;
		const float AwayFromGround = FVector::DotProduct(Velocity, Ground.ImpactNormal);
		if (!bGrounded && bAbove)
		{
			// Still airborne this step.
		}
		else if (bGrounded && bAbove && AwayFromGround > 250.f)
		{
			bGrounded = false; // launched off a crest
		}
		else
		{
			Pos.Z = Ground.ImpactPoint.Z;
			GroundNormal = FMath::VInterpNormalRotationTo(GroundNormal, Ground.ImpactNormal, Dt, 720.f);
			const float IntoGround = FVector::DotProduct(Velocity, GroundNormal);
			if (IntoGround < 0.f) Velocity -= IntoGround * GroundNormal;
			bGrounded = true;
			const AActor* On = Ground.GetActor();
			bOnRoad = On && (On->ActorHasTag(TEXT("road")) || On->ActorHasTag(TEXT("parking")));
		}
	}
	else
	{
		bGrounded = false;
	}
	SetActorLocation(Pos);
}

void ARRRide::ApplyPose(float Dt)
{
	const FVector Fwd = FRotationMatrix(FRotator(0, Heading, 0)).GetUnitAxis(EAxis::X);
	const FVector Up = bGrounded ? GroundNormal : FMath::VInterpNormalRotationTo(GetActorUpVector(), FVector::UpVector, Dt, 60.f);
	const FVector X = FVector::VectorPlaneProject(Fwd, Up).GetSafeNormal();
	const FRotator Want = FRotationMatrix::MakeFromXZ(X, Up).Rotator();
	SetActorRotation(Dt >= 1.f ? Want : FMath::RInterpTo(GetActorRotation(), Want, Dt, 12.f));
}
