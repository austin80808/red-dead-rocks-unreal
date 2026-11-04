// A shopping cart or a sled: a kinematic vehicle that rolls or slides
// downhill on whatever it stands on, steered by its rider.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRide.generated.h"

class ARRExplorer;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ERRRideKind : uint8
{
	Cart,
	Sled
};

UCLASS()
class REDROCKS_API ARRRide : public AActor
{
	GENERATED_BODY()

public:
	ARRRide();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") ERRRideKind Kind = ERRRideKind::Cart;
	/** Where the HUD counts down to while riding (Morrison for the carts). Zero means none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") FVector Destination = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") FString DestinationName;
	/** The road to follow (world cm, top to bottom). With it, the cart rides on cartoon physics:
	 *  it keeps to the road, holds a fun speed downhill and shrugs off the bumps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") TArray<FVector> Route;
	/** Speed the road assist holds on the way down, cm/s (about 55 km/h). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") float CruiseSpeed = 1550.f;
	/** Turn the road assist off for pure physics. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedRocks") bool bRoadAssist = true;

	void Mount(ARRExplorer* InRider);
	void Dismount(bool bCrashed);
	void Hop();
	bool IsRidden() const { return Rider != nullptr; }
	/** Centimetres per second. */
	float GetSpeed() const { return Velocity.Size(); }
	bool IsGrounded() const { return bGrounded; }
	float GetAirTime() const { return AirTime; }
	FString GetRideName() const;
	FString GetPrompt() const;
	FString GetBoardMessage() const;
	FString GetRideHint() const;
	/** Metres to Destination, or -1. */
	float GetDistanceToDestinationM() const;

	UFUNCTION(BlueprintPure, Category = "RedRocks") float GetSpeedKmh() const { return GetSpeed() * 0.036f; }
	UFUNCTION(BlueprintPure, Category = "RedRocks") bool HasRider() const { return Rider != nullptr; }
	/** Put the ride back where the level placed it. */
	UFUNCTION(BlueprintCallable, Category = "RedRocks") void ResetToHome();

private:
	void Step(float Dt);
	bool TraceGround(const FVector& At, float Below, FHitResult& Out) const;
	void SnapToGround();
	void ApplyPose(float Dt);
	float RollingResistance() const;
	/** Route point index nearest ahead of P, the lateral offset (cm, + right) and the road direction there. */
	bool RouteFrame(const FVector& P, FVector& OutDir, float& OutLateral) const;
	float LateralGrip() const;

	UPROPERTY(VisibleAnywhere, Category = "RedRocks") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "RedRocks") TObjectPtr<USceneComponent> Visual;
	UPROPERTY(VisibleAnywhere, Category = "RedRocks") TObjectPtr<USceneComponent> Seat;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> CartParts;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SledParts;
	UPROPERTY() TObjectPtr<ARRExplorer> Rider;

	FVector Velocity = FVector::ZeroVector;
	FVector GroundNormal = FVector::UpVector;
	float Heading = 0.f; // yaw, degrees
	bool bGrounded = true;
	bool bOnRoad = false;
	float AirTime = 0.f;
	float MaxAirTime = 0.f;
	float TopSpeed = 0.f;
	FTransform Home;
	bool bArrived = false;
	int32 RouteIndex = 0;
	bool bOnRoute = false;
};
