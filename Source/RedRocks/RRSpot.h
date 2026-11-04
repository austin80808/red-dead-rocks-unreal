// A named place the number keys teleport to (the stage, the top of Creation
// Rock, the cart lot, Morrison ...). Placed by unreal/setup_explore.py.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRSpot.generated.h"

UCLASS()
class REDROCKS_API ARRSpot : public AActor
{
	GENERATED_BODY()

public:
	ARRSpot();

	/** Key order: 1 is the first spot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") int32 Order = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") FString Title;
	/** One line telling the player what to do here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") FString Tip;
	/** On BeginPlay, drop onto whatever is below, tracing from this far above (cm). 0 keeps the spot as placed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") float SnapFromAbove = 3000.f;
	/** When set, walk toward this point until something blocks (a rock face) and stand just in front of it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") FVector FaceTarget = FVector::ZeroVector;

	/** When > 0, look this far around (cm) for the highest ground and stand there facing the steepest drop: a launch point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") float SeekSummitRadius = 0.f;

	virtual void BeginPlay() override;

private:
	void SnapDown(float From);
	void SeekSummit();
	bool GroundAt(const FVector2D& XY, float Top, FVector& Out) const;
};
