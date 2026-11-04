#include "RRWeather.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float FlakeBox = 3200.f; // cm, a cube of falling snow around the camera
}

ARRWeather::ARRWeather()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Flakes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Flakes"));
	Flakes->SetupAttachment(Root);
	Flakes->SetUsingAbsoluteLocation(true);
	Flakes->SetUsingAbsoluteRotation(true);
	Flakes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Flakes->SetCastShadow(false);
	Flakes->SetGenerateOverlapEvents(false);
	Moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	Moon->SetupAttachment(Root);
	Moon->SetUsingAbsoluteRotation(true);
	Moon->SetWorldRotation(FRotator(-38.f, 210.f, 0.f));
	Moon->SetIntensity(0.f);
	Moon->SetLightColor(FLinearColor(0.55f, 0.65f, 1.f));
	Moon->SetAtmosphereSunLightIndex(1);
	Moon->SetAtmosphereSunLight(true);
	Moon->SetCastShadows(true);
	Moon->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereF(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Flakes->SetStaticMesh(SphereF.Object);
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> MpcF(
		TEXT("/Game/RedRocks/Materials/MPC_Weather.MPC_Weather"));
	Collection = MpcF.Object;
}

ARRWeather* ARRWeather::Find(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	TActorIterator<ARRWeather> It(World);
	return It ? *It : nullptr;
}

void ARRWeather::BeginPlay()
{
	Super::BeginPlay();
	// Unlit white flakes (setup_snow.py makes M_Snowflake); lit spheres read as black specks in shade.
	if (UMaterialInterface* FlakeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RedRocks/Materials/M_Snowflake.M_Snowflake")))
	{
		Flakes->SetMaterial(0, FlakeMat);
		UE_LOG(LogTemp, Log, TEXT("RRWeather: flakes use %s"), *FlakeMat->GetPathName());
	}
	else if (UMaterialInstanceDynamic* M = Flakes->CreateDynamicMaterialInstance(0))
	{
		M->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 1.f, 1.f));
	}
	if (!Collection)
	{
		Collection = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/RedRocks/Materials/MPC_Weather.MPC_Weather"));
	}
	FlakeOffsets.SetNum(FlakeCount);
	FlakeSpeeds.SetNum(FlakeCount);
	FRandomStream R(1941);
	TArray<FTransform> Xf;
	Xf.Reserve(FlakeCount);
	for (int32 i = 0; i < FlakeCount; ++i)
	{
		FlakeOffsets[i] = FVector(R.FRandRange(0, FlakeBox), R.FRandRange(0, FlakeBox), R.FRandRange(0, FlakeBox));
		FlakeSpeeds[i] = R.FRandRange(90.f, 170.f);
		Xf.Add(FTransform(FQuat::Identity, FVector(0, 0, -1e6), FVector(0.025f)));
	}
	Flakes->AddInstances(Xf, false, true);
	Flakes->SetVisibility(false);

	if (ADirectionalLight* Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(this, ADirectionalLight::StaticClass())))
		SunBase = Sun->GetLightComponent()->Intensity;
	if (AExponentialHeightFog* Fog = Cast<AExponentialHeightFog>(UGameplayStatics::GetActorOfClass(this, AExponentialHeightFog::StaticClass())))
		FogBase = Fog->GetComponent()->FogDensity;
	if (ASkyLight* Sky = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(this, ASkyLight::StaticClass())))
		SkyBase = Sky->GetLightComponent()->Intensity;

	if (bStartSnowing) SetSnowNow(true);
	else ApplyCover();
}

void ARRWeather::SetSnowing(bool bOn) { bSnowing = bOn; }

void ARRWeather::SetNight(bool bOn)
{
	bNight = bOn;
	ADirectionalLight* Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(this, ADirectionalLight::StaticClass()));
	if (Sun)
	{
		if (!bHaveSunRot)
		{
			SunDayRot = Sun->GetActorRotation();
			bHaveSunRot = true;
		}
		// Below the horizon: the sky atmosphere goes to dusk-blue, then dark.
		FRotator R = SunDayRot;
		R.Pitch = bOn ? 12.f : SunDayRot.Pitch;
		Sun->SetActorRotation(R);
	}
	// The level's sun is stationary, so it can't be moved at runtime: switch it off instead.
	if (Sun && SunBase >= 0.f)
	{
		Sun->GetLightComponent()->SetIntensity(bOn ? 0.f : SunBase * FMath::Lerp(1.f, 0.22f, Falling));
	}
	Moon->SetVisibility(bOn);
	Moon->SetIntensity(bOn ? 0.6f : 0.f); // lux: a bright moon
	// Open the exposure for the dark, put it back by day.
	for (TActorIterator<APostProcessVolume> It(GetWorld()); It; ++It)
	{
		FPostProcessSettings& P = It->Settings;
		if (!bHaveExposure)
		{
			ExposureMin = P.AutoExposureMinBrightness;
			ExposureMax = P.AutoExposureMaxBrightness;
			bHaveExposure = true;
		}
		P.bOverride_AutoExposureMinBrightness = true;
		P.bOverride_AutoExposureMaxBrightness = true;
		// Moonlit ground is about EV100 -1; hold it there so the stage lights read as lights.
		// Eyes adjust in a couple of seconds, not half a minute.
		P.bOverride_AutoExposureSpeedDown = true;
		P.bOverride_AutoExposureSpeedUp = true;
		P.AutoExposureSpeedDown = 6.f;
		P.AutoExposureSpeedUp = 6.f;
		P.AutoExposureMinBrightness = bOn ? -1.5f : ExposureMin;
		P.AutoExposureMaxBrightness = bOn ? 0.5f : ExposureMax;
	}
	// Haze catches the stage beams at night.
	if (AExponentialHeightFog* Fog = Cast<AExponentialHeightFog>(UGameplayStatics::GetActorOfClass(this, AExponentialHeightFog::StaticClass())))
	{
		UExponentialHeightFogComponent* F = Fog->GetComponent();
		if (!bHaveFogColor)
		{
			FogDayColor = F->FogInscatteringLuminance;
			bHaveFogColor = true;
		}
		// The fog's own glow is a fixed daytime colour: darken it, or the night is a white haze.
		F->SetFogInscatteringColor(bOn ? FLinearColor(0.002f, 0.003f, 0.006f) : FogDayColor);
		// No volumetric fog at night: it scatters the sky light into a white haze over everything.
	}
	if (ASkyLight* Sky = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(this, ASkyLight::StaticClass())))
	{
		if (SkyBase >= 0.f) Sky->GetLightComponent()->SetIntensity(SkyBase * (bOn ? 0.35f : 1.f));
		Sky->GetLightComponent()->RecaptureSky();
	}
}

void ARRWeather::SetSnowNow(bool bOn)
{
	bSnowing = bOn;
	Cover = bOn ? 1.f : 0.f;
	Falling = Cover;
	ApplyCover();
}

void ARRWeather::Tick(float Dt)
{
	Super::Tick(Dt);
	Clock += Dt;
	const float Target = bSnowing ? 1.f : 0.f;
	Falling = FMath::FInterpConstantTo(Falling, Target, Dt, 0.5f);
	const float Old = Cover;
	Cover = FMath::FInterpConstantTo(Cover, Target, Dt, 1.f / FMath::Max(CoverSeconds, 0.1f));
	if (Cover != Old) ApplyCover();
	TickFlakes(Dt);
}

void ARRWeather::ApplyCover()
{
	if (Collection)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(this, Collection, TEXT("Snow"), Cover);
	}
	// Overcast: a dimmer sun, a brighter share of sky, thicker fog.
	if (SunBase >= 0.f)
		if (ADirectionalLight* Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(this, ADirectionalLight::StaticClass())))
			Sun->GetLightComponent()->SetIntensity(bNight ? 0.f : SunBase * FMath::Lerp(1.f, 0.22f, Falling));
	if (FogBase >= 0.f)
		if (AExponentialHeightFog* Fog = Cast<AExponentialHeightFog>(UGameplayStatics::GetActorOfClass(this, AExponentialHeightFog::StaticClass())))
			Fog->GetComponent()->SetFogDensity(FogBase * FMath::Lerp(1.f, 6.f, Falling));
	if (SkyBase >= 0.f)
		if (ASkyLight* Sky = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(this, ASkyLight::StaticClass())))
			Sky->GetLightComponent()->SetIntensity(SkyBase * FMath::Lerp(1.f, 1.6f, Falling) * (bNight ? 0.35f : 1.f));
}

void ARRWeather::TickFlakes(float Dt)
{
	const bool bShow = Falling > 0.02f;
	Flakes->SetVisibility(bShow);
	if (!bShow) return;
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam) return;
	const FVector Center = Cam->GetCameraLocation();
	const FVector Corner = Center - FVector(FlakeBox * 0.5f);
	const int32 Shown = FMath::RoundToInt(FlakeCount * Falling);
	const FVector Wind(70.f, 35.f, 0.f);
	TArray<FTransform> Xf;
	Xf.SetNum(FlakeCount);
	for (int32 i = 0; i < FlakeCount; ++i)
	{
		FVector& O = FlakeOffsets[i];
		const float Wobble = FMath::Sin(Clock * 1.7f + i) * 30.f;
		O += (Wind + FVector(Wobble, -Wobble * 0.5f, -FlakeSpeeds[i])) * Dt;
		// Wrap the flake into the box that follows the camera.
		FVector World = Corner + FVector(
			FMath::Fmod(FMath::Fmod(O.X - Corner.X, FlakeBox) + FlakeBox, FlakeBox),
			FMath::Fmod(FMath::Fmod(O.Y - Corner.Y, FlakeBox) + FlakeBox, FlakeBox),
			FMath::Fmod(FMath::Fmod(O.Z - Corner.Z, FlakeBox) + FlakeBox, FlakeBox));
		if (i >= Shown) World.Z -= 1e6f;
		Xf[i] = FTransform(FQuat::Identity, World, FVector(0.03f));
	}
	Flakes->BatchUpdateInstancesTransforms(0, Xf, true, true, false);
}
