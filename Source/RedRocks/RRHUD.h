// Canvas HUD: what you are doing, how fast, how high, the hint for this moment,
// toasts, the spot list and the controls (H).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RRHUD.generated.h"

UCLASS()
class REDROCKS_API ARRHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void Text(const FString& S, float X, float Y, const FLinearColor& Color, float Scale = 1.f, bool bCenter = false);
};
