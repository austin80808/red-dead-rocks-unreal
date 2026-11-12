#include "RRConcert.h"

#include "RRWeather.h"

#include "Components/LocalFogVolumeComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr int32 NumHeads = 8;
	constexpr int32 NumBeams = 6;
	constexpr int32 NumLasers = 9;
	constexpr int32 NumBlinders = 4;
	constexpr int32 NumPars = 8;
	constexpr float ShaftLength = 4500.f;   // cm: the drawn shaft of a moving head, fading out
	constexpr float LaserLength = 16000.f;  // cm: over the bowl to the top rows
	const TCHAR* BeamMaterialPath = TEXT("/Game/RedRocks/Materials/M_ShowBeam.M_ShowBeam");
	const FLinearColor Palette[] = {
		FLinearColor(1.f, 0.1f, 0.6f), FLinearColor(0.1f, 0.4f, 1.f), FLinearColor(1.f, 0.55f, 0.05f),
		FLinearColor(0.2f, 1.f, 0.5f), FLinearColor(0.7f, 0.15f, 1.f), FLinearColor(1.f, 0.95f, 0.85f)};
	// Laser colours: the green of a 532 nm diode, then cyan and red.
	const FLinearColor LaserColours[] = {
		FLinearColor(0.15f, 1.f, 0.1f), FLinearColor(0.05f, 0.9f, 1.f), FLinearColor(1.f, 0.05f, 0.05f)};
}

ARRConcert::ARRConcert()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	RodMesh = Cylinder.Object;

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
		Shafts.Add(MakeRod(*FString::Printf(TEXT("Shaft%d"), i), L, ShaftLength, 160.f));
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

	// A laser bank at the back of the deck, just under the roof, fanning out over the crowd.
	for (int32 i = 0; i < NumLasers; ++i)
	{
		USceneComponent* P = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("LaserPivot%d"), i));
		P->SetupAttachment(Root);
		P->SetRelativeLocation(FVector(-500.f, -60.f + 120.f * i / (NumLasers - 1), 900.f));
		LaserPivots.Add(P);
		Lasers.Add(MakeRod(*FString::Printf(TEXT("Laser%d"), i), P, LaserLength, 6.f));
	}
	// Strobes and blinders along the deck front, aimed at the crowd.
	for (int32 i = 0; i < NumBlinders; ++i)
	{
		UPointLightComponent* B = CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("Blinder%d"), i));
		B->SetupAttachment(Root);
		B->SetRelativeLocation(FVector(1250.f, -900.f + 1800.f * i / (NumBlinders - 1), 150.f));
		B->SetIntensityUnits(ELightUnits::Candelas);
		B->SetIntensity(0.f);
		B->SetAttenuationRadius(3500.f);
		B->SetLightColor(FLinearColor(1.f, 0.93f, 0.82f));
		B->SetCastShadows(false);
		B->SetVisibility(false);
		Blinders.Add(B);
	}
	// Floor PARs along the deck front, uplighting the rig and the roof.
	for (int32 i = 0; i < NumPars; ++i)
	{
		USpotLightComponent* L = CreateDefaultSubobject<USpotLightComponent>(*FString::Printf(TEXT("Par%d"), i));
		L->SetupAttachment(Root);
		L->SetRelativeLocation(FVector(1150.f, -1150.f + 2300.f * i / (NumPars - 1), 40.f));
		L->SetRelativeRotation(FRotator(70.f, 180.f, 0.f));  // up and back toward the roof
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(0.f);
		L->SetAttenuationRadius(2500.f);
		L->SetInnerConeAngle(12.f);
		L->SetOuterConeAngle(24.f);
		L->SetCastShadows(false);
		L->SetVisibility(false);
		Pars.Add(L);
	}
	// Stage smoke: a fog volume over the deck and the front rows, glowing in the show's colour.
	Smoke = CreateDefaultSubobject<ULocalFogVolumeComponent>(TEXT("Smoke"));
	Smoke->SetupAttachment(Root);
	Smoke->SetRelativeLocation(FVector(300.f, 0.f, 250.f));
	Smoke->SetRelativeScale3D(FVector(3.6f, 3.2f, 1.4f));  // the base volume is a 500 cm-radius sphere
	Smoke->SetVisibility(false);
}

UStaticMeshComponent* ARRConcert::MakeRod(const TCHAR* Name, USceneComponent* Parent, float Length, float Diameter)
{
	// The engine's cylinder is 100 cm tall and wide, centred, along Z. Turned so its +Z runs
	// along the parent's +X (pitch -90) and pushed half its length out, it starts at the parent.
	UStaticMeshComponent* M = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	M->SetupAttachment(Parent);
	M->SetStaticMesh(RodMesh);
	M->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	M->SetRelativeLocation(FVector(Length / 2.f, 0.f, 0.f));
	M->SetRelativeScale3D(FVector(Diameter / 100.f, Diameter / 100.f, Length / 100.f));
	M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M->SetCastShadow(false);
	M->SetVisibility(false);
	return M;
}

void ARRConcert::BeginPlay()
{
	Super::BeginPlay();
	// M_ShowBeam is made by the level build (setup_materials.py); without it the shafts stay hidden.
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, BeamMaterialPath);
	if (!Mat)
	{
		UE_LOG(LogTemp, Warning, TEXT("RRConcert: no %s; light shafts and lasers are off"), BeamMaterialPath);
		Shafts.Empty();
		Lasers.Empty();
		return;
	}
	for (UStaticMeshComponent* M : Shafts) ShaftMids.Add(M->CreateDynamicMaterialInstance(0, Mat));
	for (UStaticMeshComponent* M : Lasers) LaserMids.Add(M->CreateDynamicMaterialInstance(0, Mat));
	// Shafts: soft-edged haze columns; lasers: hard, thin lines.
	for (UMaterialInstanceDynamic* D : ShaftMids) D->SetScalarParameterValue(TEXT("EdgePower"), 4.f);
	for (UMaterialInstanceDynamic* D : LaserMids) D->SetScalarParameterValue(TEXT("EdgePower"), 0.5f);
}

void ARRConcert::Apply(bool bNowOn)
{
	bOn = bNowOn;
	for (USpotLightComponent* L : Heads) L->SetVisibility(bOn);
	for (USpotLightComponent* L : Beams) L->SetVisibility(bOn);
	for (UStaticMeshComponent* M : Shafts) M->SetVisibility(bOn && ShaftMids.Num() > 0);
	for (UPointLightComponent* B : Blinders) B->SetVisibility(bOn);
	for (USpotLightComponent* L : Pars) L->SetVisibility(bOn);
	Wash->SetVisibility(bOn);
	Smoke->SetVisibility(bOn);
	if (!bOn)
	{
		bLasers = false;
		for (UStaticMeshComponent* M : Lasers) M->SetVisibility(false);
	}
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
	const float InBar = Beat / 4.f - Bar;
	const float Pulse = FMath::Lerp(0.35f, 1.f, FMath::Pow(1.f - InBeat, 3.f));
	const int32 NumColours = UE_ARRAY_COUNT(Palette);
	const int32 Phrase = Bar / FMath::Max(PhraseBars, 2);
	const int32 InPhrase = Bar % FMath::Max(PhraseBars, 2);
	const bool bDrop = InPhrase == 0;                 // the first bar of every phrase
	const bool bBuild = InPhrase == PhraseBars - 1;   // the bar before it

	for (int32 i = 0; i < Heads.Num(); ++i)
	{
		// Sweep across the rows, mirrored left and right, a new colour every bar.
		const float Side = (i < Heads.Num() / 2) ? 1.f : -1.f;
		const float Yaw = Side * (12.f + 22.f * FMath::Sin(Clock * 0.9f + i * 0.7f));
		const float Pitch = -14.f + 8.f * FMath::Sin(Clock * 1.3f + i);
		const FLinearColor C = Palette[(Bar + i / 2) % NumColours];
		Heads[i]->SetRelativeRotation(FRotator(Pitch, Yaw, 0.f));
		Heads[i]->SetLightColor(C);
		Heads[i]->SetIntensity(Brightness * Pulse);
		if (ShaftMids.IsValidIndex(i))
		{
			ShaftMids[i]->SetVectorParameterValue(TEXT("Color"), C);
			ShaftMids[i]->SetScalarParameterValue(TEXT("Glow"), ShaftGlow * Pulse);
		}
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

	// Floor PARs: a colour chase along the deck front, one step a beat.
	const int32 Step = FMath::FloorToInt(Beat);
	for (int32 i = 0; i < Pars.Num(); ++i)
	{
		const bool bLit = ((i + Step) % 4) < 2;
		Pars[i]->SetLightColor(Palette[(Bar + 2 + (i % 2)) % NumColours]);
		Pars[i]->SetIntensity(bLit ? Brightness * 0.6f : Brightness * 0.04f);
	}

	// Strobes through the build-up bar (sixteenth notes), the blinders hit the crowd on the drop's downbeat.
	const float Sixteenth = Beat * 4.f - FMath::FloorToFloat(Beat * 4.f);
	float Blind = 0.f;
	if (bBuild) Blind = Sixteenth < 0.3f ? 1.f : 0.f;
	else if (bDrop) Blind = FMath::Max(0.f, 1.f - InBar * 3.f);
	// A blinder is a flash in the eyes, not a floodlight: kept short and dim enough
	// that the auto exposure does not open the whole bowl to white.
	for (UPointLightComponent* B : Blinders) B->SetIntensity(Brightness * 0.35f * Blind);

	// Smoke: a burst from the machines on every drop, settling over a few seconds.
	if (Bar != LastBar && bDrop) Puff = 1.f;
	LastBar = Bar;
	Puff = FMath::Max(0.f, Puff - Dt * 0.25f);
	const float Density = SmokeDensity * (1.f + 2.5f * Puff) * (0.85f + 0.15f * FMath::Sin(Clock * 0.37f));
	Smoke->SetRadialFogExtinction(Density);
	Smoke->SetHeightFogExtinction(Density * 1.5f);
	Smoke->SetFogEmissive(Palette[(Bar + 1) % NumColours] * SmokeGlow * (0.6f + 0.4f * Pulse + 1.5f * Blind));

	// Lasers, now and then: the second half of every other phrase.
	const bool bNowLasers = LaserMids.Num() > 0 && (Phrase % 2 == 1) && InPhrase >= PhraseBars / 2;
	if (bNowLasers != bLasers)
	{
		bLasers = bNowLasers;
		for (UStaticMeshComponent* M : Lasers) M->SetVisibility(bLasers);
	}
	if (bLasers)
	{
		// A fan over the crowd's heads, aimed up the rake; it sweeps, opens and closes on the bar,
		// and chops on the off-beats of every other bar.
		const FLinearColor C = LaserColours[(Bar / 2) % UE_ARRAY_COUNT(LaserColours)];
		const float Open = 4.f + 3.5f * FMath::Sin(Clock * 1.7f);
		const float Sweep = 14.f * FMath::Sin(Clock * 0.8f);
		const bool bChop = (Bar % 2 == 1) && InBeat > 0.5f;
		for (int32 i = 0; i < LaserPivots.Num(); ++i)
		{
			const float k = i - (LaserPivots.Num() - 1) / 2.f;
			LaserPivots[i]->SetRelativeRotation(FRotator(16.f + 2.f * FMath::Sin(Clock * 2.3f + i), Sweep + k * Open, 0.f));
			LaserMids[i]->SetVectorParameterValue(TEXT("Color"), C);
			LaserMids[i]->SetScalarParameterValue(TEXT("Glow"), bChop ? 0.f : LaserGlow);
		}
	}
}
