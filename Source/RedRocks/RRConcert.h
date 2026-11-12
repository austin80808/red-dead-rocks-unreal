// The stage at night: moving coloured spotlights over the crowd and up onto the
// rocks, pulsing on the beat, with stage smoke, strobes and blinders on the drops,
// floor PARs chasing along the deck front and, now and then, lasers fanning over
// the crowd. On only while ARRWeather says it is night.
//
// The night has no volumetric fog (it turns the sky light into a white haze), so
// light shafts are drawn: each moving head and laser carries a glowing rod in
// M_ShowBeam (additive, unlit; unreal/setup_materials.py builds it), and the
// smoke is a local fog volume that glows in the show's colour.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRConcert.generated.h"

class USpotLightComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class ULocalFogVolumeComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;

UCLASS()
class REDROCKS_API ARRConcert : public AActor
{
	GENERATED_BODY()

public:
	ARRConcert();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") float Bpm = 124.f;
	/** Candela of each moving head. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") float Brightness = 60000.f;
	/** Glow of the drawn light shafts (M_ShowBeam's Glow) and of the lasers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") float ShaftGlow = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") float LaserGlow = 60.f;
	/** The smoke's glow in the show's colour, and its density between puffs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") float SmokeGlow = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") float SmokeDensity = 1.2f;
	/** Bars in a phrase; the lasers play the second half of every other phrase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") int32 PhraseBars = 16;

	UFUNCTION(BlueprintPure, Category = "RedRocks") bool IsPlaying() const { return bOn; }
	/** True while the lasers play (for play-tests). */
	UFUNCTION(BlueprintPure, Category = "RedRocks") bool LasersOn() const { return bLasers; }
	/** Jump the show's clock, e.g. to a laser section, for play-tests and shots. */
	UFUNCTION(BlueprintCallable, Category = "RedRocks") void SetShowTime(float Seconds) { Clock = Seconds; }

private:
	UPROPERTY() TArray<TObjectPtr<USpotLightComponent>> Heads;
	UPROPERTY() TArray<TObjectPtr<USpotLightComponent>> Beams;
	UPROPERTY() TObjectPtr<UPointLightComponent> Wash;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Shafts;      // one per moving head
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> LaserPivots;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Lasers;
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Blinders;    // strobes and blinders at the deck front
	UPROPERTY() TArray<TObjectPtr<USpotLightComponent>> Pars;         // floor uplights along the deck front
	UPROPERTY() TObjectPtr<ULocalFogVolumeComponent> Smoke;
	UPROPERTY() TObjectPtr<UStaticMesh> RodMesh;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> ShaftMids;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> LaserMids;
	float Clock = 0.f;
	float Puff = 0.f;     // smoke-machine burst, decays
	int32 LastBar = -1;
	bool bOn = false;
	bool bLasers = false;
	void Apply(bool bNowOn);
	UStaticMeshComponent* MakeRod(const TCHAR* Name, USceneComponent* Parent, float Length, float Diameter);
};
