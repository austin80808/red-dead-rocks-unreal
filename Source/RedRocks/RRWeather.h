// Snow: flakes fall around the camera, the ground whitens through the
// MPC_Weather "Snow" parameter every Red Rocks material reads, the sun dims
// and the fog thickens. Sleds only run well once it has snowed.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRWeather.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialParameterCollection;

UCLASS()
class REDROCKS_API ARRWeather : public AActor
{
	GENERATED_BODY()

public:
	ARRWeather();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	static ARRWeather* Find(const UObject* WorldContext);

	UFUNCTION(BlueprintCallable, Category = "RedRocks") void ToggleSnow() { SetSnowing(!bSnowing); }
	UFUNCTION(BlueprintCallable, Category = "RedRocks") void SetSnowing(bool bOn);
	/** Jump straight to full snow cover (or none): for screenshots and tests. */
	UFUNCTION(BlueprintCallable, Category = "RedRocks") void SetSnowNow(bool bOn);
	UFUNCTION(BlueprintPure, Category = "RedRocks") bool IsSnowing() const { return bSnowing; }
	/** Ground cover, 0 (bare) to 1 (white). */
	UFUNCTION(BlueprintPure, Category = "RedRocks") float GetSnow() const { return Cover; }

	/** Start the level snowed in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") bool bStartSnowing = false;
	/** Seconds for bare ground to turn white (and back). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") float CoverSeconds = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") int32 FlakeCount = 3000;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") TObjectPtr<UMaterialParameterCollection> Collection;
	/** Elevation above sea level of Z = 0 (the terrain minimum), metres: for the HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") float DatumElevationM = 1753.98f;

private:
	void ApplyCover();
	void TickFlakes(float Dt);

	UPROPERTY(VisibleAnywhere, Category = "RedRocks") TObjectPtr<UInstancedStaticMeshComponent> Flakes;

	bool bSnowing = false;
	float Cover = 0.f;
	float Falling = 0.f;
	float SunBase = -1.f;
	float FogBase = -1.f;
	float SkyBase = -1.f;
	TArray<FVector> FlakeOffsets;
	TArray<float> FlakeSpeeds;
	FVector LastCamera = FVector::ZeroVector;
	float Clock = 0.f;
};
