// Game mode for Lvl_RedRocks: the explorer pawn, its HUD, and the FirstPerson
// template's player controller (which owns the camera manager and touch input).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RRGameMode.generated.h"

UCLASS()
class REDROCKS_API ARRGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARRGameMode();
};
