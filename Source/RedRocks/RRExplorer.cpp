#include "RRExplorer.h"

#include "RRRide.h"
#include "RRSpot.h"
#include "RRWeather.h"

#include "Camera/CameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "GroomComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float Gravity = 980.f;
	// The sasquatch: a 2.56 m capsule, eyes 2.28 m up (100 cm over its centre).
	constexpr float CapsuleRadius = 42.f;
	constexpr float CapsuleHalfHeight = 128.f;
	constexpr float EyeHeight = 100.f;
	// The climb probes were tuned on a 92 cm half-height capsule; they scale with his.
	constexpr float ProbeScale = CapsuleHalfHeight / 92.f;

	const TCHAR* SasquatchMesh = TEXT("/Game/Sasquatch/SKM_Sasquatch.SKM_Sasquatch");
	const TCHAR* SasquatchAnim = TEXT("/Game/Sasquatch/Anims/ABP_Sasquatch.ABP_Sasquatch_C");
	const TCHAR* BodyGroom = TEXT("/Game/Sasquatch/GR_Sasquatch_Body.GR_Sasquatch_Body");
	const TCHAR* BodyBinding = TEXT("/Game/Sasquatch/GB_Sasquatch_Body.GB_Sasquatch_Body");
	const TCHAR* HeadGroom = TEXT("/Game/Sasquatch/GR_Sasquatch_Head.GR_Sasquatch_Head");
	const TCHAR* HeadBinding = TEXT("/Game/Sasquatch/GB_Sasquatch_Head.GB_Sasquatch_Head");
	// A floor is walkable up to this steepness (normal Z); anything steeper can be climbed.
	constexpr float ClimbMaxNormalZ = 0.74f;
	constexpr float ClimbMinNormalZ = -0.35f;

	UStaticMeshComponent* AddPart(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh,
		const FVector& Loc, const FRotator& Rot, const FVector& Scale)
	{
		UStaticMeshComponent* Part = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Parent);
		Part->SetStaticMesh(Mesh);
		Part->SetRelativeLocation(Loc);
		Part->SetRelativeRotation(Rot);
		Part->SetRelativeScale3D(Scale);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(true);
		Part->SetGenerateOverlapEvents(false);
		Part->CanCharacterStepUpOn = ECB_No;
		return Part;
	}

	FLinearColor PartColor(const FName& Name)
	{
		const FString S = Name.ToString();
		if (S.StartsWith(TEXT("Wing"))) return FLinearColor(0.95f, 0.30f, 0.04f);
		if (S.StartsWith(TEXT("Chute"))) return FLinearColor(0.10f, 0.45f, 0.95f);
		if (S.StartsWith(TEXT("Head"))) return FLinearColor(0.85f, 0.62f, 0.48f);
		if (S.StartsWith(TEXT("Leg"))) return FLinearColor(0.10f, 0.12f, 0.20f);
		return FLinearColor(0.12f, 0.35f, 0.30f); // jacket
	}
}

ARRExplorer::ARRExplorer()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = WalkSpeed;
	Move->JumpZVelocity = 520.f;
	Move->AirControl = 0.4f;
	// Bench rows are 0.45 m tall: let the player step up them instead of jumping each one.
	Move->MaxStepHeight = 52.f;
	Move->SetWalkableFloorAngle(46.f);
	Move->MaxFlySpeed = 20000.f;
	Move->BrakingDecelerationFlying = 0.f;
	Move->bOrientRotationToMovement = false;
	Move->GravityScale = 1.f;

	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(GetCapsuleComponent());
	Boom->SetRelativeLocation(FVector(0.f, 0.f, EyeHeight));
	Boom->TargetArmLength = 0.f;
	Boom->bUsePawnControlRotation = true;
	Boom->bDoCollisionTest = true;
	Boom->ProbeSize = 16.f;
	Boom->bEnableCameraLag = false;
	Boom->CameraLagSpeed = 8.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Boom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->SetFieldOfView(90.f);

	// A blocky figure, only seen from the chase camera (rides, wingsuit, V); the
	// sasquatch replaces it when his assets are in the project (SetupSasquatch).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeF(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereF(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylF(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cube = CubeF.Object;
	UStaticMesh* Sphere = SphereF.Object;
	UStaticMesh* Cyl = CylF.Object;

	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(GetCapsuleComponent());
	BodyParts.Add(AddPart(this, Body, TEXT("Torso"), Cyl, FVector(0, 0, 18), FRotator::ZeroRotator, FVector(0.38f, 0.30f, 0.62f)));
	BodyParts.Add(AddPart(this, Body, TEXT("Head"), Sphere, FVector(0, 0, 66), FRotator::ZeroRotator, FVector(0.26f)));
	BodyParts.Add(AddPart(this, Body, TEXT("ArmL"), Cube, FVector(0, -24, 20), FRotator::ZeroRotator, FVector(0.11f, 0.11f, 0.55f)));
	BodyParts.Add(AddPart(this, Body, TEXT("ArmR"), Cube, FVector(0, 24, 20), FRotator::ZeroRotator, FVector(0.11f, 0.11f, 0.55f)));
	BodyParts.Add(AddPart(this, Body, TEXT("LegL"), Cube, FVector(0, -10, -55), FRotator::ZeroRotator, FVector(0.13f, 0.13f, 0.75f)));
	BodyParts.Add(AddPart(this, Body, TEXT("LegR"), Cube, FVector(0, 10, -55), FRotator::ZeroRotator, FVector(0.13f, 0.13f, 0.75f)));

	// The sasquatch: the character's skeletal mesh under Body (so the wingsuit and
	// ride poses tilt him), feet on the capsule's base, facing +X. Mesh, fur and
	// animation are loaded in BeginPlay so a project without them still plays.
	USkeletalMeshComponent* Sk = GetMesh();
	Sk->SetupAttachment(Body);
	Sk->SetRelativeLocation(FVector(0.f, 0.f, -CapsuleHalfHeight));
	Sk->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Sk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sk->SetVisibility(false);
	BodyFur = CreateDefaultSubobject<UGroomComponent>(TEXT("BodyFur"));
	BodyFur->SetupAttachment(Sk);
	HeadFur = CreateDefaultSubobject<UGroomComponent>(TEXT("HeadFur"));
	HeadFur->SetupAttachment(Sk);

	// Wingsuit membranes between the arms and legs, shown only in flight.
	WingRig = CreateDefaultSubobject<USceneComponent>(TEXT("WingRig"));
	WingRig->SetupAttachment(Body);
	BodyParts.Add(AddPart(this, WingRig, TEXT("WingL"), Cube, FVector(0, -45, 0), FRotator::ZeroRotator, FVector(0.02f, 0.75f, 1.2f)));
	BodyParts.Add(AddPart(this, WingRig, TEXT("WingR"), Cube, FVector(0, 45, 0), FRotator::ZeroRotator, FVector(0.02f, 0.75f, 1.2f)));
	BodyParts.Add(AddPart(this, WingRig, TEXT("WingTail"), Cube, FVector(0, 0, -55), FRotator::ZeroRotator, FVector(0.02f, 0.32f, 0.6f)));

	ChuteRig = CreateDefaultSubobject<USceneComponent>(TEXT("ChuteRig"));
	ChuteRig->SetupAttachment(Body);
	BodyParts.Add(AddPart(this, ChuteRig, TEXT("ChuteCanopy"), Sphere, FVector(0, 0, 420), FRotator::ZeroRotator, FVector(2.2f, 7.5f, 0.9f)));
	BodyParts.Add(AddPart(this, ChuteRig, TEXT("ChuteLineL"), Cube, FVector(0, -150, 230), FRotator(0, 0, -36), FVector(0.01f, 0.01f, 4.2f)));
	BodyParts.Add(AddPart(this, ChuteRig, TEXT("ChuteLineR"), Cube, FVector(0, 150, 230), FRotator(0, 0, 36), FVector(0.01f, 0.01f, 4.2f)));

	static ConstructorHelpers::FObjectFinder<UInputAction> MoveF(TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
	static ConstructorHelpers::FObjectFinder<UInputAction> LookF(TEXT("/Game/Input/Actions/IA_Look.IA_Look"));
	static ConstructorHelpers::FObjectFinder<UInputAction> MouseLookF(TEXT("/Game/Input/Actions/IA_MouseLook.IA_MouseLook"));
	static ConstructorHelpers::FObjectFinder<UInputAction> JumpF(TEXT("/Game/Input/Actions/IA_Jump.IA_Jump"));
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> ImcF(TEXT("/Game/Input/IMC_Default.IMC_Default"));
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> ImcMouseF(TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"));
	MoveAction = MoveF.Object;
	LookAction = LookF.Object;
	MouseLookAction = MouseLookF.Object;
	JumpAction = JumpF.Object;
	DefaultContext = ImcF.Object;
	MouseLookContext = ImcMouseF.Object;
}

void ARRExplorer::BeginPlay()
{
	Super::BeginPlay();
	for (UStaticMeshComponent* Part : BodyParts)
	{
		if (UMaterialInstanceDynamic* Mid = Part->CreateDynamicMaterialInstance(0))
		{
			Mid->SetVectorParameterValue(TEXT("Color"), PartColor(Part->GetFName()));
		}
	}
	SetupSasquatch();
	CollectSpots();
	SetMode(ERRMode::Walk);
	Toast(TEXT("Welcome to Red Rocks.  H shows the controls."), 5.f);
}

void ARRExplorer::PawnClientRestart()
{
	Super::PawnClientRestart();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultContext && !Sub->HasMappingContext(DefaultContext)) Sub->AddMappingContext(DefaultContext, 0);
			if (MouseLookContext && !Sub->HasMappingContext(MouseLookContext)) Sub->AddMappingContext(MouseLookContext, 0);
		}
	}
}

void ARRExplorer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC) return;
	if (MoveAction)
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ARRExplorer::OnMove);
		EIC->BindAction(MoveAction, ETriggerEvent::Completed, this, &ARRExplorer::OnMoveDone);
	}
	if (LookAction) EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ARRExplorer::OnLook);
	if (MouseLookAction) EIC->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ARRExplorer::OnLook);
	if (JumpAction)
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ARRExplorer::OnJumpPressed);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ARRExplorer::OnJumpReleased);
	}
}

void ARRExplorer::OnMove(const FInputActionValue& Value)
{
	if (!bScripted) MoveInput = Value.Get<FVector2D>();
}

void ARRExplorer::OnMoveDone(const FInputActionValue& Value)
{
	if (!bScripted) MoveInput = FVector2D::ZeroVector;
}

void ARRExplorer::OnLook(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	AddControllerYawInput(V.X);
	AddControllerPitchInput(V.Y);
}

void ARRExplorer::OnJumpPressed() { HandleAction(TEXT("Jump")); }
void ARRExplorer::OnJumpReleased() { StopJumping(); }

// ---------------------------------------------------------------------------
// Tick

void ARRExplorer::Tick(float Dt)
{
	Super::Tick(Dt);
	PollKeys();
	if (bScripted)
	{
		MoveInput = ScriptedMove;
		bSprint = bScriptedSprint;
	}
	NoGrabTime = FMath::Max(0.f, NoGrabTime - Dt);
	ToastTimeLeft = FMath::Max(0.f, ToastTimeLeft - Dt);
	UpdateGroundHeight();

	UCharacterMovementComponent* Move = GetCharacterMovement();
	switch (Mode)
	{
	case ERRMode::Walk:
	{
		Move->MaxWalkSpeed = bSprint ? SprintSpeed : WalkSpeed;
		const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), MoveInput.Y);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), MoveInput.X);
		FallTime = Move->IsFalling() ? FallTime + Dt : 0.f;
		TryStartClimb();
		break;
	}
	case ERRMode::Climb: TickClimb(Dt); break;
	case ERRMode::Mantle: TickMantle(Dt); break;
	case ERRMode::Wingsuit: TickWingsuit(Dt); break;
	case ERRMode::Parachute: TickParachute(Dt); break;
	case ERRMode::Ride: break; // ARRRide moves us; it reads GetMoveInput().
	}

	UpdateCamera(Dt);
	UpdateBodyPose(Dt);
	UpdateSasquatchAnim();
	UpdateHint();
}

void ARRExplorer::PollKeys()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;
	if (!bScripted) bSprint = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);
	if (PC->WasInputKeyJustPressed(EKeys::E)) HandleAction(TEXT("Interact"));
	if (PC->WasInputKeyJustPressed(EKeys::Q)) HandleAction(TEXT("Drop"));
	if (PC->WasInputKeyJustPressed(EKeys::R)) HandleAction(TEXT("Respawn"));
	if (PC->WasInputKeyJustPressed(EKeys::V)) HandleAction(TEXT("Camera"));
	if (PC->WasInputKeyJustPressed(EKeys::N)) HandleAction(TEXT("Snow"));
	if (PC->WasInputKeyJustPressed(EKeys::H)) HandleAction(TEXT("Help"));
	if (PC->WasInputKeyJustPressed(EKeys::T)) HandleAction(TEXT("Night"));
	static const FKey Numbers[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
		EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
	for (int32 i = 0; i < UE_ARRAY_COUNT(Numbers); ++i)
	{
		if (PC->WasInputKeyJustPressed(Numbers[i])) RRGo(i + 1);
	}
}

void ARRExplorer::HandleAction(FName Action)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (Action == TEXT("Jump"))
	{
		switch (Mode)
		{
		case ERRMode::Walk:
			if (Move->IsFalling() && HeightAboveGround > WingsuitMinHeight) StartWingsuit();
			else Jump();
			break;
		case ERRMode::Climb: LeapOffWall(); break;
		case ERRMode::Wingsuit: StartParachute(); break;
		case ERRMode::Ride: if (Ride) Ride->Hop(); break;
		default: break;
		}
	}
	else if (Action == TEXT("Interact"))
	{
		if (Mode == ERRMode::Ride && Ride) Ride->Dismount(false);
		else if (Mode == ERRMode::Walk)
		{
			if (ARRRide* Near = FindNearbyRide(400.f)) BoardRide(Near);
		}
	}
	else if (Action == TEXT("Drop"))
	{
		if (Mode == ERRMode::Climb)
		{
			NoGrabTime = 0.6f;
			SetMode(ERRMode::Walk);
			Move->SetMovementMode(MOVE_Falling);
		}
		else if (Mode == ERRMode::Parachute)
		{
			SetMode(ERRMode::Walk);
			Move->SetMovementMode(MOVE_Falling);
			Toast(TEXT("Cut away!"));
		}
	}
	else if (Action == TEXT("Respawn")) RRGo(LastSpot > 0 ? LastSpot : 1);
	else if (Action == TEXT("Camera")) bChaseCam = !bChaseCam;
	else if (Action == TEXT("Snow")) RRSnow();
	else if (Action == TEXT("Night")) RRNight();
	else if (Action == TEXT("Help")) bShowHelp = !bShowHelp;
}

void ARRExplorer::SetMode(ERRMode NewMode)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const ERRMode Old = Mode;
	Mode = NewMode;
	bUseControllerRotationYaw = (NewMode == ERRMode::Walk || NewMode == ERRMode::Wingsuit || NewMode == ERRMode::Parachute);
	Move->BrakingFrictionFactor = (NewMode == ERRMode::Walk) ? 2.f : 0.f;
	switch (NewMode)
	{
	case ERRMode::Walk:
		Move->GravityScale = 1.f;
		if (Old != ERRMode::Walk && Move->MovementMode == MOVE_Flying) Move->SetMovementMode(MOVE_Falling);
		break;
	case ERRMode::Climb:
	case ERRMode::Mantle:
	case ERRMode::Wingsuit:
	case ERRMode::Parachute:
		Move->SetMovementMode(MOVE_Flying);
		break;
	case ERRMode::Ride:
		break;
	}
	WingRig->SetVisibility(NewMode == ERRMode::Wingsuit, true);
	ChuteRig->SetVisibility(NewMode == ERRMode::Parachute, true);
}

void ARRExplorer::UpdateGroundHeight()
{
	FHitResult Hit;
	const FVector Start = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRGround), false, this);
	if (Ride) Params.AddIgnoredActor(Ride);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0, 0, 200000.f), ECC_Visibility, Params))
	{
		HeightAboveGround = Hit.Distance;
	}
	else
	{
		HeightAboveGround = 200000.f;
	}
}

// ---------------------------------------------------------------------------
// Climbing

bool ARRExplorer::FindWall(const FVector& Dir, FVector& OutPoint, FVector& OutNormal, int32& OutHits) const
{
	const FVector Center = GetActorLocation();
	const FVector Flat = FVector(Dir.X, Dir.Y, 0.f).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Flat).GetSafeNormal();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRWall), true, this);
	FVector NormalSum = FVector::ZeroVector;
	FVector Nearest = FVector::ZeroVector;
	float NearestDist = TNumericLimits<float>::Max();
	OutHits = 0;
	static const float Heights[] = {-55.f * ProbeScale, -10.f * ProbeScale, 40.f * ProbeScale};
	static const float Sides[] = {-28.f, 0.f, 28.f};
	for (float H : Heights)
	{
		for (float S : Sides)
		{
			const FVector From = Center + FVector(0, 0, H) + Right * S;
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByChannel(Hit, From, From + Dir * 110.f, ECC_Visibility, Params))
			{
				++OutHits;
				NormalSum += Hit.ImpactNormal;
				if (Hit.Distance < NearestDist)
				{
					NearestDist = Hit.Distance;
					Nearest = Hit.ImpactPoint;
				}
			}
		}
	}
	if (OutHits < 3) return false;
	OutNormal = NormalSum.GetSafeNormal();
	OutPoint = Nearest;
	return true;
}

bool ARRExplorer::TryStartClimb()
{
	if (NoGrabTime > 0.f || MoveInput.Y < 0.5f) return false;
	FVector Point, Normal;
	int32 Hits = 0;
	const FVector Fwd = FRotationMatrix(FRotator(0, GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::X);
	const bool bFound = FindWall(Fwd, Point, Normal, Hits);
	const float Dist = bFound ? FVector::DistXY(Point, GetActorLocation()) : -1.f;
	// Pushing into something and not getting anywhere: anything short of a floor counts as a wall.
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	const bool bBlocked = Move->IsMovingOnGround() && Move->Velocity.Size2D() < 120.f;
	const float MaxZ = bBlocked ? 0.86f : ClimbMaxNormalZ;
	ClimbProbe = FString::Printf(TEXT("hits %d nz %.2f dist %.0f blocked %d"), Hits, bFound ? Normal.Z : -9.f, Dist, bBlocked ? 1 : 0);
	if (!bFound || Hits < 4) return false;
	if (Normal.Z > MaxZ || Normal.Z < ClimbMinNormalZ) return false;
	if (Dist > GetCapsuleComponent()->GetScaledCapsuleRadius() + 45.f) return false;
	// Not a bench riser or a kerb: the face must reach above the head (a sloping face recedes, so reach further).
	FHitResult High;
	const FVector HeadFrom = GetActorLocation() + FVector(0, 0, 95.f * ProbeScale);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRWallHigh), true, this);
	if (!GetWorld()->LineTraceSingleByChannel(High, HeadFrom, HeadFrom + Fwd * 220.f, ECC_Visibility, Params)) return false;

	WallNormal = Normal;
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	SetMode(ERRMode::Climb);
	Toast(TEXT("Climbing.  Space leaps off, Q lets go."), 2.5f);
	return true;
}

void ARRExplorer::TickClimb(float Dt)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	FVector Point, Normal;
	int32 Hits = 0;
	if (!FindWall(-WallNormal, Point, Normal, Hits))
	{
		if (MoveInput.Y > 0.f && TryMantle(WallNormal)) return;
		SetMode(ERRMode::Walk);
		Move->SetMovementMode(MOVE_Falling);
		NoGrabTime = 0.4f;
		return;
	}
	WallNormal = FMath::VInterpNormalRotationTo(WallNormal, Normal, Dt, 240.f);

	// Ran out of rock above: pull up over the edge.
	if (MoveInput.Y > 0.f)
	{
		FHitResult Top;
		const FVector From = GetActorLocation() + FVector(0, 0, 80.f);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RRTop), true, this);
		if (!GetWorld()->LineTraceSingleByChannel(Top, From, From - WallNormal * 120.f, ECC_Visibility, Params))
		{
			if (TryMantle(WallNormal)) return;
		}
	}
	// Climbed down onto walkable ground.
	if (MoveInput.Y < 0.f && Move->IsMovingOnGround() == false)
	{
		FHitResult Floor;
		const FVector Feet = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RRFloor), true, this);
		if (GetWorld()->LineTraceSingleByChannel(Floor, Feet, Feet - FVector(0, 0, 25.f), ECC_Visibility, Params) &&
			Floor.ImpactNormal.Z > ClimbMaxNormalZ)
		{
			SetMode(ERRMode::Walk);
			Move->SetMovementMode(MOVE_Walking);
			return;
		}
	}

	const FVector Facing = -WallNormal;
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Facing).GetSafeNormal();
	const FVector Up = FVector::CrossProduct(Facing, Right).GetSafeNormal();
	const float Speed = bSprint ? ClimbSprintSpeed : ClimbSpeed;
	FVector Vel = (Right * MoveInput.X + Up * MoveInput.Y) * Speed;
	// Hold the capsule a hand's width off the rock.
	const float Want = GetCapsuleComponent()->GetScaledCapsuleRadius() + 6.f;
	const float Gap = FVector::DotProduct(GetActorLocation() - Point, WallNormal);
	Vel += Facing * FMath::Clamp((Gap - Want) * 8.f, -300.f, 300.f);
	Move->Velocity = Vel;

	const FRotator FaceRot(0.f, Facing.Rotation().Yaw, 0.f);
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), FaceRot, Dt, 8.f));
}

bool ARRExplorer::TryMantle(const FVector& Normal)
{
	const FVector Facing = FVector(-Normal.X, -Normal.Y, 0.f).GetSafeNormal();
	const float HalfH = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRMantle), true, this);
	for (float Reach : {70.f, 110.f, 150.f})
	{
		const FVector From = GetActorLocation() + FVector(0, 0, HalfH + 120.f) + Facing * Reach;
		FHitResult Ledge;
		if (!GetWorld()->LineTraceSingleByChannel(Ledge, From, From - FVector(0, 0, HalfH * 2.f + 200.f), ECC_Visibility, Params))
			continue;
		if (Ledge.ImpactNormal.Z < 0.6f || Ledge.bStartPenetrating) continue;
		const FVector Target = Ledge.ImpactPoint + FVector(0, 0, HalfH + 4.f);
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfH);
		if (GetWorld()->OverlapBlockingTestByChannel(Target, FQuat::Identity, ECC_Pawn, Shape, Params)) continue;
		MantleStart = GetActorLocation();
		MantleEnd = Target;
		MantleAlpha = 0.f;
		SetMode(ERRMode::Mantle);
		return true;
	}
	return false;
}

void ARRExplorer::TickMantle(float Dt)
{
	MantleAlpha = FMath::Min(1.f, MantleAlpha + Dt / 0.45f);
	// Up first, then over.
	const float Up = FMath::Clamp(MantleAlpha * 1.6f, 0.f, 1.f);
	const float Over = FMath::Clamp((MantleAlpha - 0.35f) / 0.65f, 0.f, 1.f);
	FVector P;
	P.Z = FMath::Lerp(MantleStart.Z, MantleEnd.Z, FMath::InterpEaseOut(0.f, 1.f, Up, 2.f));
	P.X = FMath::Lerp(MantleStart.X, MantleEnd.X, Over);
	P.Y = FMath::Lerp(MantleStart.Y, MantleEnd.Y, Over);
	SetActorLocation(P, false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	if (MantleAlpha >= 1.f)
	{
		SetMode(ERRMode::Walk);
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		NoGrabTime = 0.3f;
		if (HeightAboveGround < 300.f && GetActorLocation().Z > 4000.f)
		{
			Toast(TEXT("Topped out!"), 2.f);
		}
	}
}

void ARRExplorer::LeapOffWall()
{
	const FVector Facing = -WallNormal;
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Facing).GetSafeNormal();
	FVector Launch = WallNormal * 650.f + FVector(0, 0, 420.f) + Right * MoveInput.X * 300.f;
	NoGrabTime = 0.8f;
	SetMode(ERRMode::Walk);
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	// Turn around to face away from the rock: ready to fly.
	if (AController* C = GetController())
	{
		C->SetControlRotation(FRotator(-10.f, WallNormal.Rotation().Yaw, 0.f));
	}
	LaunchCharacter(Launch, true, true);
}

// ---------------------------------------------------------------------------
// Wingsuit and parachute

void ARRExplorer::StartWingsuit()
{
	const FVector V = GetCharacterMovement()->Velocity;
	const FVector Look = GetControlRotation().Vector();
	GlideSpeed = FMath::Max(V.Size(), 1400.f);
	GlideDir = (V.SizeSquared() > 100.f ? V.GetSafeNormal() : Look);
	// Start already leaning into the look direction so the first second flies.
	GlideDir = (GlideDir + Look).GetSafeNormal();
	SetMode(ERRMode::Wingsuit);
	Toast(TEXT("Wingsuit!  Mouse steers, W dives, S flares, Space pulls the chute."), 3.5f);
}

void ARRExplorer::TickWingsuit(float Dt)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	AController* C = GetController();
	if (C && FMath::Abs(MoveInput.X) > 0.05f)
	{
		FRotator R = C->GetControlRotation();
		R.Yaw += MoveInput.X * 70.f * Dt;
		C->SetControlRotation(R);
	}
	FRotator Aim = GetControlRotation();
	Aim.Pitch = FRotator::NormalizeAxis(Aim.Pitch);
	Aim.Pitch += MoveInput.Y > 0.f ? -28.f * MoveInput.Y : -14.f * MoveInput.Y; // W dives, S flares
	Aim.Pitch = FMath::Clamp(Aim.Pitch, -80.f, 25.f);
	const FVector Target = Aim.Vector();

	// The suit turns the airflow toward where you point it; faster flies more crisply.
	const float Authority = FMath::Clamp(GlideSpeed / 3000.f, 0.25f, 1.2f) * 2.2f;
	GlideDir = FMath::VInterpNormalRotationTo(GlideDir, Target, Dt, 90.f * Authority);
	// Energy: gravity along the path, quadratic drag.
	GlideSpeed += (-Gravity * GlideDir.Z - WingsuitDrag * GlideSpeed * GlideSpeed) * Dt;
	if (GlideSpeed < 1100.f)
	{
		// Stall: the nose drops until there is airspeed again.
		GlideDir = FMath::VInterpNormalRotationTo(GlideDir, FVector(GlideDir.X, GlideDir.Y, -1.2f).GetSafeNormal(), Dt, 120.f);
		GlideSpeed = FMath::Max(GlideSpeed, 500.f);
	}
	Move->Velocity = GlideDir * GlideSpeed - FVector(0, 0, WingsuitSink);

	if (HeightAboveGround < 140.f)
	{
		if (GlideSpeed > WingsuitCrashSpeed) Crash(TEXT("the ground"));
		else
		{
			SetMode(ERRMode::Walk);
			Move->SetMovementMode(MOVE_Falling);
			Move->Velocity *= 0.25f;
			Toast(TEXT("Landed it."), 2.f);
		}
	}
}

void ARRExplorer::StartParachute()
{
	SetMode(ERRMode::Parachute);
	Toast(TEXT("Canopy open.  A/D turn, W/S speed, Q cuts away."), 3.f);
}

void ARRExplorer::TickParachute(float Dt)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (AController* C = GetController())
	{
		FRotator R = C->GetControlRotation();
		R.Yaw += MoveInput.X * 50.f * Dt;
		C->SetControlRotation(R);
	}
	const FVector Fwd = FRotationMatrix(FRotator(0, GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::X);
	const float Forward = 900.f + 450.f * MoveInput.Y;
	const float Sink = 420.f - 120.f * MoveInput.Y * -1.f;
	const FVector Want = Fwd * Forward - FVector(0, 0, Sink);
	Move->Velocity = FMath::VInterpTo(Move->Velocity, Want, Dt, 1.6f);
	if (HeightAboveGround < 120.f)
	{
		SetMode(ERRMode::Walk);
		Move->SetMovementMode(MOVE_Falling);
		Move->Velocity *= 0.2f;
		Toast(TEXT("Touchdown."), 2.f);
	}
}

void ARRExplorer::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp,
	bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
	if (Mode != ERRMode::Wingsuit) return;
	const FVector V = GlideDir * GlideSpeed;
	const float Into = -FVector::DotProduct(V, HitNormal);
	if (Into > WingsuitCrashSpeed * 0.7f)
	{
		Crash(Other ? Other->GetActorNameOrLabel() : FString(TEXT("the rock")));
	}
	else if (HitNormal.Z > ClimbMaxNormalZ && GlideSpeed < WingsuitCrashSpeed)
	{
		SetMode(ERRMode::Walk);
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		Toast(TEXT("Belly landing."), 2.f);
	}
	else
	{
		// Glance off: keep flying along the surface.
		GlideDir = FVector::VectorPlaneProject(GlideDir, HitNormal).GetSafeNormal();
		GlideSpeed *= 0.8f;
	}
}

void ARRExplorer::Crash(const FString& What)
{
	const float Kmh = GlideSpeed * 0.036f;
	SetMode(ERRMode::Walk);
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->SetMovementMode(MOVE_Falling);
	Move->Velocity = FVector::ZeroVector;
	FString Name = What;
	if (Name.StartsWith(TEXT("Scan_ml0w")) || Name.Contains(TEXT("Rock"))) Name = TEXT("the rock");
	else if (Name.Contains(TEXT("Terrain"))) Name = TEXT("the hillside");
	Toast(FString::Printf(TEXT("SPLAT.  Met %s at %.0f km/h.  R to try again."), *Name, Kmh), 4.f);
}

void ARRExplorer::FellOutOfWorld(const UDamageType& DmgType)
{
	if (Ride) Ride->Dismount(false);
	RRGo(LastSpot > 0 ? LastSpot : 1);
	Toast(TEXT("You fell off the edge of the world."), 3.f);
}

// ---------------------------------------------------------------------------
// Camera and figure

void ARRExplorer::RRPhoto(float Yaw, float Pitch, float Arm)
{
	bPhoto = Arm > 0.f;
	PhotoArm = Arm;
	// The boom points from him to the camera's back: a camera in front of him
	// (Yaw 0) looks back along -X of his facing.
	PhotoRot = FRotator(Pitch, GetActorRotation().Yaw + 180.f + Yaw, 0.f);
	Boom->bUsePawnControlRotation = !bPhoto;
	Boom->bEnableCameraLag = false;
	Boom->bEnableCameraRotationLag = false;
	if (bPhoto)
	{
		bUseControllerRotationYaw = false;
		Boom->SetWorldRotation(PhotoRot);
		Boom->TargetArmLength = PhotoArm;
		Boom->SocketOffset = FVector::ZeroVector;
	}
	else
	{
		SetMode(Mode);
	}
}

void ARRExplorer::UpdateCamera(float Dt)
{
	if (bPhoto)
	{
		Boom->SetWorldRotation(PhotoRot);
		Boom->TargetArmLength = PhotoArm;
		if (bSasquatch)
		{
			HeadFur->SetOwnerNoSee(false);
		}
		else
		{
			Body->SetVisibility(true, true);
			WingRig->SetVisibility(Mode == ERRMode::Wingsuit, true);
			ChuteRig->SetVisibility(Mode == ERRMode::Parachute, true);
		}
		return;
	}
	float WantArm = 0.f;
	FVector WantOffset = FVector::ZeroVector;
	bool bLag = false;
	switch (Mode)
	{
	case ERRMode::Wingsuit: WantArm = 650.f; WantOffset = FVector(0, 0, 90); bLag = true; break;
	case ERRMode::Parachute: WantArm = 900.f; WantOffset = FVector(0, 0, 250); bLag = true; break;
	case ERRMode::Ride:
		// Tubes sit low in a canyon: look down the creek from higher and closer.
		if (Ride && Ride->Kind == ERRRideKind::Tube) { WantArm = 380.f; WantOffset = FVector(0, 0, 260); }
		else { WantArm = 520.f; WantOffset = FVector(0, 0, 110); }
		bLag = true;
		break;
	default:
		if (bChaseCam) { WantArm = ChaseArm; WantOffset = FVector(0, 0, 45); }
		break;
	}
	Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, WantArm, Dt, 5.f);
	if (WantArm == 0.f && Boom->TargetArmLength < 5.f) Boom->TargetArmLength = 0.f;
	Boom->SocketOffset = FMath::VInterpTo(Boom->SocketOffset, WantOffset, Dt, 5.f);
	Boom->bEnableCameraLag = bLag;
	Boom->bEnableCameraRotationLag = bLag;
	Boom->CameraRotationLagSpeed = 10.f;

	// Speed widens the view.
	const float Speed = GetSpeedMps();
	const float WantFov = 90.f + FMath::Clamp((Speed - 12.f) * 0.45f, 0.f, 22.f);
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, WantFov, Dt, 3.f));

	if (bSasquatch)
	{
		// He is always there. In first person the camera sits inside his head
		// (its faces point away, so they cull) and his head fur is hidden from
		// you, so you look out past the brow and down at his chest, arms and hands.
		HeadFur->SetOwnerNoSee(!IsThirdPerson());
		WingRig->SetVisibility(Mode == ERRMode::Wingsuit, true);
		ChuteRig->SetVisibility(Mode == ERRMode::Parachute, true);
		return;
	}
	const bool bShowBody = Boom->TargetArmLength > 60.f;
	Body->SetVisibility(bShowBody, true);
	if (bShowBody)
	{
		WingRig->SetVisibility(Mode == ERRMode::Wingsuit, true);
		ChuteRig->SetVisibility(Mode == ERRMode::Parachute, true);
	}
}

bool ARRExplorer::IsThirdPerson() const
{
	return Boom && Boom->TargetArmLength > 60.f;
}

void ARRExplorer::SetupSasquatch()
{
	USkeletalMesh* MeshAsset = LoadObject<USkeletalMesh>(nullptr, SasquatchMesh);
	UClass* AnimClass = LoadObject<UClass>(nullptr, SasquatchAnim);
	if (!MeshAsset)
	{
		bSasquatch = false;
		return;
	}
	USkeletalMeshComponent* Sk = GetMesh();
	Sk->SetSkeletalMesh(MeshAsset);
	if (AnimClass)
	{
		Sk->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		Sk->SetAnimInstanceClass(AnimClass);
	}
	Sk->SetVisibility(true);
	Sk->SetCastShadow(true);
	BodyFur->SetGroomAsset(LoadObject<UGroomAsset>(nullptr, BodyGroom), LoadObject<UGroomBindingAsset>(nullptr, BodyBinding));
	HeadFur->SetGroomAsset(LoadObject<UGroomAsset>(nullptr, HeadGroom), LoadObject<UGroomBindingAsset>(nullptr, HeadBinding));

	// The blocky figure goes; the wingsuit and parachute stay, grown to his size.
	for (int32 i = BodyParts.Num() - 1; i >= 0; --i)
	{
		UStaticMeshComponent* Part = BodyParts[i];
		if (Part && Part->GetAttachParent() == Body)
		{
			Part->DestroyComponent();
			BodyParts.RemoveAt(i);
		}
	}
	WingRig->SetRelativeScale3D(FVector(SasquatchScale));
	ChuteRig->SetRelativeScale3D(FVector(SasquatchScale));
	Body->SetVisibility(true, false);
	bSasquatch = true;
}

void ARRExplorer::UpdateSasquatchAnim()
{
	if (!bSasquatch) return;
	// The mannequin's cycles on his longer legs cover SasquatchScale times the
	// ground, so they play slower (a big animal's heavier cadence) and speed up
	// past the blend space's top speed instead of sliding.
	const float Speed = GetVelocity().Size2D();
	const float Over = (Mode == ERRMode::Walk) ? FMath::Max(1.f, Speed / AnimTopSpeed) : 1.f;
	GetMesh()->GlobalAnimRateScale = Over / SasquatchScale;
}

void ARRExplorer::UpdateBodyPose(float Dt)
{
	// Lie flat along the flight path in the wingsuit, upright otherwise.
	FRotator Want = FRotator::ZeroRotator;
	if (Mode == ERRMode::Wingsuit)
	{
		const FRotator Path = GlideDir.Rotation();
		Want = FRotator(Path.Pitch - 85.f, Path.Yaw - GetActorRotation().Yaw, -MoveInput.X * 35.f);
	}
	else if (Mode == ERRMode::Ride)
	{
		Want = FRotator(-12.f, 0.f, 0.f);
	}
	Body->SetRelativeRotation(FMath::RInterpTo(Body->GetRelativeRotation(), Want, Dt, 6.f));
}

void ARRExplorer::UpdateHint()
{
	switch (Mode)
	{
	case ERRMode::Walk:
		if (GetCharacterMovement()->IsFalling() && HeightAboveGround > WingsuitMinHeight)
			Hint = TEXT("SPACE  wingsuit");
		else if (ARRRide* Near = FindNearbyRide(400.f))
			Hint = FString::Printf(TEXT("E  %s"), *Near->GetPrompt());
		else
			Hint = FString();
		break;
	case ERRMode::Climb: Hint = TEXT("WASD climb   SHIFT faster   SPACE leap off   Q let go"); break;
	case ERRMode::Mantle: Hint = FString(); break;
	case ERRMode::Wingsuit: Hint = TEXT("MOUSE steer   A/D bank   W dive   S flare   SPACE parachute"); break;
	case ERRMode::Parachute: Hint = TEXT("A/D turn   W faster   S slower   Q cut away"); break;
	case ERRMode::Ride: Hint = Ride ? Ride->GetRideHint() : FString(); break;
	}
}

// ---------------------------------------------------------------------------
// Rides, spots, weather

ARRRide* ARRExplorer::FindNearbyRide(float MaxDist) const
{
	ARRRide* Best = nullptr;
	float BestD = MaxDist;
	for (TActorIterator<ARRRide> It(GetWorld()); It; ++It)
	{
		if (It->IsRidden()) continue;
		const float D = FVector::Dist(It->GetActorLocation(), GetActorLocation() - FVector(0, 0, 90.f));
		if (D < BestD)
		{
			BestD = D;
			Best = *It;
		}
	}
	return Best;
}

void ARRExplorer::BoardRide(ARRRide* NewRide)
{
	Ride = NewRide;
	SetMode(ERRMode::Ride);
	Ride->Mount(this);
	Toast(Ride->GetBoardMessage(), 3.f);
}

void ARRExplorer::OnRideEnded(bool bCrashed, const FVector& EjectVelocity)
{
	Ride = nullptr;
	SetMode(ERRMode::Walk);
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->SetMovementMode(MOVE_Falling);
	if (bCrashed)
	{
		LaunchCharacter(EjectVelocity, true, true);
		Toast(FString::Printf(TEXT("WIPEOUT at %.0f km/h!"), EjectVelocity.Size() * 0.036f), 3.f);
	}
}

void ARRExplorer::CollectSpots()
{
	Spots.Reset();
	for (TActorIterator<ARRSpot> It(GetWorld()); It; ++It) Spots.Add(*It);
	Spots.Sort([](const ARRSpot& A, const ARRSpot& B) { return A.Order < B.Order; });
}

void ARRExplorer::WarpTo(const FVector& Location, float Yaw)
{
	if (Ride) Ride->Dismount(false);
	SetMode(ERRMode::Walk);
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->Velocity = FVector::ZeroVector;
	Move->SetMovementMode(MOVE_Falling);
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	if (AController* C = GetController()) C->SetControlRotation(FRotator(0.f, Yaw, 0.f));
}

void ARRExplorer::RRGo(int32 SpotNumber)
{
	if (Spots.Num() == 0) CollectSpots();
	if (!Spots.IsValidIndex(SpotNumber - 1)) return;
	ARRSpot* Spot = Spots[SpotNumber - 1];
	LastSpot = SpotNumber;
	WarpTo(Spot->GetActorLocation(), Spot->GetActorRotation().Yaw);
	Toast(FString::Printf(TEXT("%s\n%s"), *Spot->Title, *Spot->Tip), 5.f);
}

void ARRExplorer::RRSnow()
{
	if (ARRWeather* W = ARRWeather::Find(this))
	{
		W->ToggleSnow();
		Toast(W->IsSnowing() ? TEXT("It's snowing.  Sleds are fast now.") : TEXT("The snow is melting."), 3.f);
	}
	else
	{
		Toast(TEXT("No weather in this level."), 2.f);
	}
}

void ARRExplorer::RRNight()
{
	if (ARRWeather* W = ARRWeather::Find(this))
	{
		W->ToggleNight();
		Toast(W->IsNight() ? TEXT("Night falls.  The band is on: press 1 for the stage.") : TEXT("Morning."), 3.f);
	}
}

// ---------------------------------------------------------------------------
// HUD helpers and test hooks

FString ARRExplorer::GetModeName() const
{
	switch (Mode)
	{
	case ERRMode::Walk: return GetCharacterMovement()->IsFalling() ? TEXT("Falling") : (bSprint ? TEXT("Running") : TEXT("Walking"));
	case ERRMode::Climb: return TEXT("Climbing");
	case ERRMode::Mantle: return TEXT("Pulling up");
	case ERRMode::Wingsuit: return TEXT("Wingsuit");
	case ERRMode::Parachute: return TEXT("Parachute");
	case ERRMode::Ride: return Ride ? Ride->GetRideName() : TEXT("Riding");
	}
	return FString();
}

float ARRExplorer::GetSpeedMps() const
{
	if (Mode == ERRMode::Ride && Ride) return Ride->GetSpeed() / 100.f;
	if (Mode == ERRMode::Wingsuit) return GlideSpeed / 100.f;
	return GetCharacterMovement()->Velocity.Size() / 100.f;
}

void ARRExplorer::Toast(const FString& Message, float Seconds)
{
	ToastText = Message;
	ToastTimeLeft = Seconds;
	UE_LOG(LogTemp, Log, TEXT("RRToast: %s"), *Message.Replace(TEXT("\n"), TEXT(" | ")));
}

void ARRExplorer::ScriptMove(FVector2D InMove, bool bInSprint)
{
	bScripted = true;
	ScriptedMove = InMove;
	bScriptedSprint = bInSprint;
}

void ARRExplorer::ScriptPress(FName Action)
{
	HandleAction(Action);
}

void ARRExplorer::ScriptLook(float Yaw, float Pitch)
{
	if (AController* C = GetController()) C->SetControlRotation(FRotator(Pitch, Yaw, 0.f));
}

void ARRExplorer::ScriptRelease()
{
	bScripted = false;
	ScriptedMove = FVector2D::ZeroVector;
	MoveInput = FVector2D::ZeroVector;
}
