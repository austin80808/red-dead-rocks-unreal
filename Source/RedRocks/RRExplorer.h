// The Red Rocks explorer: one first-person character that walks, sprints,
// climbs any rock face, flies a wingsuit, pulls a parachute and rides
// shopping carts and sleds. Everything is plain C++ on top of
// UCharacterMovementComponent, so the level scripts only have to place things.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RRExplorer.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UInputAction;
class UInputMappingContext;
class ARRRide;
class ARRSpot;
struct FInputActionValue;

UENUM(BlueprintType)
enum class ERRMode : uint8
{
	Walk,
	Climb,
	Mantle,
	Wingsuit,
	Parachute,
	Ride
};

UCLASS()
class REDROCKS_API ARRExplorer : public ACharacter
{
	GENERATED_BODY()

public:
	ARRExplorer();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp,
		bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse,
		const FHitResult& Hit) override;
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;

	// --- State the HUD reads -------------------------------------------------
	UFUNCTION(BlueprintPure, Category = "RedRocks") ERRMode GetMode() const { return Mode; }
	UFUNCTION(BlueprintPure, Category = "RedRocks") FString GetModeName() const;
	/** Speed in metres per second. */
	UFUNCTION(BlueprintPure, Category = "RedRocks") float GetSpeedMps() const;
	/** Height above the ground under the player, metres (capped at 2 km). */
	UFUNCTION(BlueprintPure, Category = "RedRocks") float GetHeightAboveGroundM() const { return HeightAboveGround / 100.f; }
	UFUNCTION(BlueprintPure, Category = "RedRocks") FString GetHint() const { return Hint; }
	UFUNCTION(BlueprintPure, Category = "RedRocks") ARRRide* GetRide() const { return Ride; }
	/** What the last climb check saw (for play-tests). */
	UFUNCTION(BlueprintPure, Category = "RedRocks|Test") FString GetClimbProbe() const { return ClimbProbe; }
	const TArray<ARRSpot*>& GetSpots() const { return Spots; }

	/** Show a message in the middle of the screen for a few seconds. */
	UFUNCTION(BlueprintCallable, Category = "RedRocks") void Toast(const FString& Message, float Seconds = 3.f);
	FString GetToast() const { return ToastTimeLeft > 0.f ? ToastText : FString(); }

	bool bShowHelp = true;

	// --- Console commands and test hooks -------------------------------------
	/** Teleport to spot N (1-based, the number keys do the same). */
	UFUNCTION(Exec, BlueprintCallable, Category = "RedRocks") void RRGo(int32 SpotNumber);
	/** Toggle snow (N). */
	UFUNCTION(Exec, BlueprintCallable, Category = "RedRocks") void RRSnow();
	/** Toggle night and the concert (T). */
	UFUNCTION(Exec, BlueprintCallable, Category = "RedRocks") void RRNight();
	/** Scripted input for play-tests: Move.X right, Move.Y forward, held until changed. */
	UFUNCTION(BlueprintCallable, Category = "RedRocks|Test") void ScriptMove(FVector2D Move, bool bSprint);
	/** Scripted key press: Jump, Interact, Drop, Respawn, Camera, Snow, Help. */
	UFUNCTION(BlueprintCallable, Category = "RedRocks|Test") void ScriptPress(FName Action);
	/** Point the view: yaw and pitch in degrees. */
	UFUNCTION(BlueprintCallable, Category = "RedRocks|Test") void ScriptLook(float Yaw, float Pitch);
	/** Hand control back to the keyboard. */
	UFUNCTION(BlueprintCallable, Category = "RedRocks|Test") void ScriptRelease();

	// Called by ARRRide.
	void OnRideEnded(bool bCrashed, const FVector& EjectVelocity);
	FVector2D GetMoveInput() const { return MoveInput; }
	bool IsSprinting() const { return bSprint; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "RedRocks") TObjectPtr<USpringArmComponent> Boom;
	UPROPERTY(VisibleAnywhere, Category = "RedRocks") TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, Category = "RedRocks") TObjectPtr<USceneComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "RedRocks") TObjectPtr<USceneComponent> WingRig;
	UPROPERTY(VisibleAnywhere, Category = "RedRocks") TObjectPtr<USceneComponent> ChuteRig;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BodyParts;

	UPROPERTY() TObjectPtr<UInputAction> MoveAction;
	UPROPERTY() TObjectPtr<UInputAction> LookAction;
	UPROPERTY() TObjectPtr<UInputAction> MouseLookAction;
	UPROPERTY() TObjectPtr<UInputAction> JumpAction;
	UPROPERTY() TObjectPtr<UInputMappingContext> DefaultContext;
	UPROPERTY() TObjectPtr<UInputMappingContext> MouseLookContext;

	// Tuning, in centimetres and seconds.
	UPROPERTY(EditAnywhere, Category = "RedRocks|Walk") float WalkSpeed = 520.f;
	UPROPERTY(EditAnywhere, Category = "RedRocks|Walk") float SprintSpeed = 1050.f;
	UPROPERTY(EditAnywhere, Category = "RedRocks|Climb") float ClimbSpeed = 170.f;
	UPROPERTY(EditAnywhere, Category = "RedRocks|Climb") float ClimbSprintSpeed = 320.f;
	UPROPERTY(EditAnywhere, Category = "RedRocks|Wingsuit") float WingsuitMinHeight = 500.f;
	UPROPERTY(EditAnywhere, Category = "RedRocks|Wingsuit") float WingsuitDrag = 1.6e-5f;
	UPROPERTY(EditAnywhere, Category = "RedRocks|Wingsuit") float WingsuitSink = 180.f;
	UPROPERTY(EditAnywhere, Category = "RedRocks|Wingsuit") float WingsuitCrashSpeed = 2600.f;

private:
	void OnMove(const FInputActionValue& Value);
	void OnMoveDone(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnJumpPressed();
	void OnJumpReleased();

	void PollKeys();
	void HandleAction(FName Action);
	void SetMode(ERRMode NewMode);
	void UpdateGroundHeight();
	void UpdateCamera(float Dt);
	void UpdateBodyPose(float Dt);
	void UpdateHint();

	// Climbing
	bool FindWall(const FVector& Dir, FVector& OutPoint, FVector& OutNormal, int32& OutHits) const;
	bool TryStartClimb();
	void TickClimb(float Dt);
	bool TryMantle(const FVector& WallNormal);
	void TickMantle(float Dt);
	void LeapOffWall();

	// Air
	void StartWingsuit();
	void TickWingsuit(float Dt);
	void StartParachute();
	void TickParachute(float Dt);
	void Crash(const FString& What);

	// Rides and spots
	ARRRide* FindNearbyRide(float MaxDist) const;
	void BoardRide(ARRRide* NewRide);
	void CollectSpots();
	void WarpTo(const FVector& Location, float Yaw);

	ERRMode Mode = ERRMode::Walk;
	FVector2D MoveInput = FVector2D::ZeroVector;
	bool bSprint = false;
	bool bScripted = false;
	FVector2D ScriptedMove = FVector2D::ZeroVector;
	bool bScriptedSprint = false;
	bool bChaseCam = false;

	FVector WallNormal = FVector::ForwardVector;
	float NoGrabTime = 0.f;
	FVector MantleStart, MantleEnd;
	float MantleAlpha = 0.f;

	FVector GlideDir = FVector::ForwardVector;
	float GlideSpeed = 0.f;
	float FallTime = 0.f;

	float HeightAboveGround = 0.f;
	FString Hint;
	FString ClimbProbe;
	FString ToastText;
	float ToastTimeLeft = 0.f;

	UPROPERTY() TObjectPtr<ARRRide> Ride;
	UPROPERTY() TArray<TObjectPtr<ARRSpot>> Spots;
	int32 LastSpot = 0;
};
