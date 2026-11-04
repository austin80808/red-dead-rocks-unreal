#include "RRConcert.h"

#include "RRWeather.h"

#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"

namespace
{
	constexpr int32 NumHeads = 8;
	constexpr int32 NumBeams = 6;
	const FLinearColor Palette[] = {
		FLinearColor(1.f, 0.1f, 0.6f), FLinearColor(0.1f, 0.4f, 1.f), FLinearColor(1.f, 0.55f, 0.05f),
		FLinearColor(0.2f, 1.f, 0.5f), FLinearColor(0.7f, 0.15f, 1.f), FLinearColor(1.f, 0.95f, 0.85f)};
}

ARRConcert::ARRConcert()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	// The actor stands on the stage under the roof, X pointing up the bowl at the crowd.
	for (int32 i = 0; i < NumHeads; ++i)
	{
		USpotLightComponent* L = CreateDefaultSubobject<USpotLightComponent>(*FString::Printf(TEXT("Head%d"), i));
		L->SetupAttachment(Root);
		L->SetRelativeLocation(FVector(200.f, -1100.f + 2200.f * i / (NumHeads - 1), 1050.f));
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(0.f);
		L->SetAttenuationRadius(12000.f);
		L->SetInnerConeAngle(3.f);
		L->SetOuterConeAngle(9.f);
		L->SetCastShadows(false);
		L->SetVolumetricScatteringIntensity(4.f);
		L->SetVisibility(false);
		Heads.Add(L);
	}
	for (int32 i = 0; i < NumBeams; ++i)
	{
		USpotLightComponent* L = CreateDefaultSubobject<USpotLightComponent>(*FString::Printf(TEXT("Beam%d"), i));
		L->SetupAttachment(Root);
		L->SetRelativeLocation(FVector(-300.f, -900.f + 1800.f * i / (NumBeams - 1), 300.f));
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(0.f);
		L->SetAttenuationRadius(30000.f);
		L->SetInnerConeAngle(1.5f);
		L->SetOuterConeAngle(3.f);
		L->SetCastShadows(false);
		L->SetVolumetricScatteringIntensity(8.f);
		L->SetVisibility(false);
		Beams.Add(L);
	}
	Wash = CreateDefaultSubobject<UPointLightComponent>(TEXT("Wash"));
	Wash->SetupAttachment(Root);
	Wash->SetRelativeLocation(FVector(0.f, 0.f, 600.f));
	Wash->SetIntensityUnits(ELightUnits::Candelas);
	Wash->SetIntensity(0.f);
	Wash->SetAttenuationRadius(4000.f);
	Wash->SetVisibility(false);
}

void ARRConcert::Apply(bool bNowOn)
{
	bOn = bNowOn;
	for (USpotLightComponent* L : Heads) L->SetVisibility(bOn);
	for (USpotLightComponent* L : Beams) L->SetVisibility(bOn);
	Wash->SetVisibility(bOn);
}

void ARRConcert::Tick(float Dt)
{
	Super::Tick(Dt);
	const ARRWeather* W = ARRWeather::Find(this);
	const bool bNight = W && W->IsNight();
	if (bNight != bOn) Apply(bNight);
	if (!bOn) return;

	Clock += Dt;
	const float Beat = Clock * Bpm / 60.f;
	const int32 Bar = FMath::FloorToInt(Beat / 4.f);
	const float InBeat = Beat - FMath::FloorToFloat(Beat);
	const float Pulse = FMath::Lerp(0.35f, 1.f, FMath::Pow(1.f - InBeat, 3.f));
	const int32 NumColours = UE_ARRAY_COUNT(Palette);

	for (int32 i = 0; i < Heads.Num(); ++i)
	{
		// Sweep across the rows, mirrored left and right, a new colour every bar.
		const float Side = (i < Heads.Num() / 2) ? 1.f : -1.f;
		const float Yaw = Side * (12.f + 22.f * FMath::Sin(Clock * 0.9f + i * 0.7f));
		const float Pitch = -14.f + 8.f * FMath::Sin(Clock * 1.3f + i);
		Heads[i]->SetRelativeRotation(FRotator(Pitch, Yaw, 0.f));
		Heads[i]->SetLightColor(Palette[(Bar + i / 2) % NumColours]);
		Heads[i]->SetIntensity(Brightness * Pulse);
	}
	for (int32 i = 0; i < Beams.Num(); ++i)
	{
		// Fan up into the sky (they light the rock faces behind the bowl), crossing on the downbeat.
		const float Lean = 18.f * FMath::Sin(Clock * 0.6f + i * 1.1f);
		Beams[i]->SetRelativeRotation(FRotator(70.f, (i - (Beams.Num() - 1) / 2.f) * 10.f + Lean, 0.f));
		Beams[i]->SetLightColor(Palette[(Bar + 3) % NumColours]);
		Beams[i]->SetIntensity(Brightness * 2.f * (InBeat < 0.12f ? 1.f : 0.45f));
	}
	Wash->SetLightColor(Palette[(Bar + 1) % NumColours]);
	Wash->SetIntensity(Brightness * 0.05f * Pulse);
}
