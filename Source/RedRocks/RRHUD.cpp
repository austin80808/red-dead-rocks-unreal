#include "RRHUD.h"

#include "RRExplorer.h"
#include "RRRide.h"
#include "RRSpot.h"
#include "RRWeather.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"

void ARRHUD::Text(const FString& S, float X, float Y, const FLinearColor& Color, float Scale, bool bCenter)
{
	if (!Canvas || S.IsEmpty()) return;
	UFont* Font = GEngine->GetLargeFont();
	TArray<FString> Lines;
	S.ParseIntoArrayLines(Lines, false);
	const float LineH = Font->GetMaxCharHeight() * Scale * 1.15f;
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		FCanvasTextItem Item(FVector2D(X, Y + i * LineH), FText::FromString(Lines[i]), Font, Color);
		Item.Scale = FVector2D(Scale);
		Item.bCentreX = bCenter;
		Item.EnableShadow(FLinearColor(0, 0, 0, 0.85f), FVector2D(2, 2));
		Canvas->DrawItem(Item);
	}
}

void ARRHUD::DrawHUD()
{
	Super::DrawHUD();
	ARRExplorer* P = Cast<ARRExplorer>(GetOwningPawn());
	if (!P || !Canvas) return;
	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	const float S = FMath::Clamp(H / 1080.f, 0.6f, 2.f);
	const FLinearColor Sand(1.f, 0.86f, 0.70f);
	const FLinearColor White(1.f, 1.f, 1.f);

	// Status, top left.
	const ARRWeather* Weather = ARRWeather::Find(this);
	FString Status = FString::Printf(TEXT("%s\n%.0f km/h"), *P->GetModeName(), P->GetSpeedMps() * 3.6f);
	if (P->GetHeightAboveGroundM() > 3.f && P->GetHeightAboveGroundM() < 1999.f)
		Status += FString::Printf(TEXT("   %.0f m up"), P->GetHeightAboveGroundM());
	Status += FString::Printf(TEXT("\nElevation %.0f m"), (Weather ? Weather->DatumElevationM : 0.f) + P->GetActorLocation().Z / 100.f);
	if (Weather && Weather->IsNight()) Status += TEXT("\nNight");
	if (Weather && Weather->GetSnow() > 0.01f) Status += FString::Printf(TEXT("\nSnow %.0f%%"), Weather->GetSnow() * 100.f);
	Text(Status, 28 * S, 24 * S, Sand, 1.1f * S);

	// Hint, bottom centre.
	Text(P->GetHint(), W * 0.5f, H - 120 * S, White, 1.0f * S, true);

	// Toast, upper middle.
	Text(P->GetToast(), W * 0.5f, H * 0.22f, FLinearColor(1.f, 0.95f, 0.6f), 1.35f * S, true);

	// Help, right side.
	if (P->bShowHelp)
	{
		FString Help = TEXT("CONTROLS   (H hides)\n")
			TEXT("WASD move   SHIFT run   SPACE jump\n")
			TEXT("Walk into rock to climb\n")
			TEXT("SPACE in the air: wingsuit, again: chute\n")
			TEXT("E ride a cart or sled\n")
			TEXT("N snow   T night + concert\n")
			TEXT("V first / third person   R respawn   C credits\n\nGO TO\n");
		const TArray<ARRSpot*>& Spots = P->GetSpots();
		for (int32 i = 0; i < Spots.Num() && i < 9; ++i)
		{
			Help += FString::Printf(TEXT("%d  %s\n"), i + 1, *Spots[i]->Title);
		}
		Text(Help, W - 470 * S, 24 * S, FLinearColor(0.95f, 0.95f, 0.95f, 0.92f), 0.8f * S);
	}
}
