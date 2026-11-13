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
class ARRWeather;
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
	/** What is underfoot, from the floor's physical material: Concrete, Wood, Asphalt, Rock, Soil... ("" in the air). */
	UFUNCTION(BlueprintPure, Category = "RedRocks") FString GetSurface() const { return Surface; }
	/** Footing, 1 on dry concrete: the floor's friction (less under snow) scales grip, pick-up and stopping. */
	UFUNCTION(BlueprintPure, Category = "RedRocks") float GetGrip() const { return Grip; }
	const TArray<ARRSpot*>& GetSpots() const { return Spots; }

	/** Show a message in the middle of the screen for a few seconds. */
	UFUNCTION(BlueprintCallable, Category = "RedRocks") void Toast(const FString& Message, float Seconds = 3.f);
	FString GetToast() const { return ToastTimeLeft > 0.f ? ToastText : FString(); }

	bool bShowHelp = true;

	/** True while a game controller was the last thing used; hints and the help panel then name its buttons. */
	UFUNCTION(BlueprintPure, Category = "RedRocks") bool UsingPad() const { return bUsingPad; }
	/** Kb when the keyboard was used last, Pad when a controller was. */
	FString Keys(const TCHAR* Kb, const TCHAR* Pad) const { return bUsingPad ? FString(Pad) : FString(Kb); }
	/** Go to the next (+1) or previous (-1) spot after the last one, wrapping (D-pad right / left). */
	UFUNCTION(Exec, BlueprintCallable, Category = "RedRocks") void RRCycleSpot(int32 Step);

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
	/** The credits menu's action (setup_credits.py maps C to it); a controller's Menu button fires it too. */
	UPROPERTY() TObjectPtr<UInputAction> CreditsAction;

	// Tuning, in centimetres and seconds.
	// A brisk walk (6.5 km/h) and, on Shift, a run (19 km/h): a person's pace, not a vehicle's.
	UPROPERTY(EditAnywhere, Category = "RedRocks|Walk") float WalkSpeed = 180.f;
	UPROPERTY(EditAnywhere, Category = "RedRocks|Walk") float SprintSpeed = 520.f;
	// Pick-up, stopping and ground friction on a floor of friction 0.7 (dry concrete); UpdateFooting scales them.
	UPROPERTY(EditAnywhere, Category = "RedRocks|Walk") float WalkAcceleration = 1000.f;
	UPROPERTY(EditAnywhere, Category = "RedRocks|Walk") float WalkBraking = 1400.f;
	UPROPERTY(EditAnywhere, Category = "RedRocks|Walk") float WalkFriction = 8.f;
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

	void PollKeys(float Dt);
	void HandleAction(FName Action);
	void SetMode(ERRMode NewMode);
	void UpdateGroundHeight();
	void UpdateFooting();
	/** Rock, ground and the scan can be climbed; anything tagged "built" (stage, walls, cars, poles, rails) cannot. */
	bool IsClimbable(const FHitResult& Hit) const;
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
	// Controller: L3 turns running on; it stays on until the stick rests for PadRunRestSeconds or you take off.
	bool bUsingPad = false;
	bool bPadRun = false;
	float PadStickRest = 0.f;
	static constexpr float PadRunRestSeconds = 0.5f;
	void PollPad(APlayerController* PC, float Dt);
	void NoteDevice(APlayerController* PC);
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
	FString Surface;
	float Grip = 1.f;
	FString Hint;
	FString ClimbProbe;
	FString ToastText;
	float ToastTimeLeft = 0.f;

	UPROPERTY() TObjectPtr<ARRRide> Ride;
	UPROPERTY() TObjectPtr<ARRWeather> Weather;
	UPROPERTY() TArray<TObjectPtr<ARRSpot>> Spots;
	int32 LastSpot = 0;
};
