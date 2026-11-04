#include "RRSpot.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"

ARRSpot::ARRSpot()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	SetActorHiddenInGame(true);
}

void ARRSpot::BeginPlay()
{
	Super::BeginPlay();
	if (SnapFromAbove > 0.f) SnapDown(SnapFromAbove);
	if (SeekSummitRadius > 0.f) SeekSummit();
	if (!FaceTarget.IsNearlyZero())
	{
		// Find the face: walk toward the target at chest height until something blocks.
		const FVector From = GetActorLocation() + FVector(0, 0, 60.f);
		FVector Dir = FaceTarget - From;
		Dir.Z = 0.f;
		const float Len = Dir.Size();
		Dir = Dir.GetSafeNormal();
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RRSpotFace), true, this);
		if (Len > 1.f && GetWorld()->LineTraceSingleByChannel(Hit, From, From + Dir * Len, ECC_Visibility, Params))
		{
			SetActorLocation(Hit.ImpactPoint - Dir * 140.f + FVector(0, 0, 40.f));
			SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
			SnapDown(300.f);
		}
	}
}

void ARRSpot::SnapDown(float From)
{
	FHitResult Hit;
	const FVector Top = GetActorLocation() + FVector(0, 0, From);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRSpotSnap), true, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, From + 100000.f), ECC_Visibility, Params))
	{
		// Capsule centre a little over the ground: the player lands, not spawns inside it.
		SetActorLocation(Hit.ImpactPoint + FVector(0, 0, 110.f));
	}
}

bool ARRSpot::GroundAt(const FVector2D& XY, float Top, FVector& Out) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRSpotSeek), true, this);
	const FVector From(XY.X, XY.Y, Top);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, From, From - FVector(0, 0, 200000.f), ECC_Visibility, Params)) return false;
	Out = Hit.ImpactPoint;
	return true;
}

void ARRSpot::SeekSummit()
{
	const FVector C = GetActorLocation();
	const float Top = C.Z + 30000.f;
	FVector Best = C - FVector(0, 0, 110.f);
	bool bAny = false;
	const int32 N = 12;
	for (int32 i = -N; i <= N; ++i)
	{
		for (int32 j = -N; j <= N; ++j)
		{
			const FVector2D XY(C.X + SeekSummitRadius * i / N, C.Y + SeekSummitRadius * j / N);
			FVector G;
			if (GroundAt(XY, Top, G) && (!bAny || G.Z > Best.Z))
			{
				Best = G;
				bAny = true;
			}
		}
	}
	if (!bAny) return;
	// Face the way the ground falls away fastest, and step back from that edge a few metres.
	float Deepest = 0.f;
	FVector2D Dir(1, 0);
	for (int32 k = 0; k < 16; ++k)
	{
		const float A = 2.f * PI * k / 16.f;
		const FVector2D D(FMath::Cos(A), FMath::Sin(A));
		FVector G;
		if (GroundAt(FVector2D(Best.X, Best.Y) + D * 3500.f, Top, G) && Best.Z - G.Z > Deepest)
		{
			Deepest = Best.Z - G.Z;
			Dir = D;
		}
	}
	SetActorLocation(Best + FVector(0, 0, 110.f));
	SetActorRotation(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.f));
}
