// The stage at night: moving coloured spotlights over the crowd and up onto the
// rocks, pulsing on the beat. On only while ARRWeather says it is night.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRConcert.generated.h"

class USpotLightComponent;
class UPointLightComponent;

UCLASS()
class REDROCKS_API ARRConcert : public AActor
{
	GENERATED_BODY()

public:
	ARRConcert();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") float Bpm = 124.f;
	/** Candela of each moving head. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") float Brightness = 60000.f;

	UFUNCTION(BlueprintPure, Category = "RedRocks") bool IsPlaying() const { return bOn; }

private:
	UPROPERTY() TArray<TObjectPtr<USpotLightComponent>> Heads;
	UPROPERTY() TArray<TObjectPtr<USpotLightComponent>> Beams;
	UPROPERTY() TObjectPtr<UPointLightComponent> Wash;
	float Clock = 0.f;
	bool bOn = false;
	void Apply(bool bNowOn);
};
