#include "RRGameMode.h"

#include "RRExplorer.h"
#include "RRHUD.h"

#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

ARRGameMode::ARRGameMode()
{
	DefaultPawnClass = ARRExplorer::StaticClass();
	HUDClass = ARRHUD::StaticClass();
	static ConstructorHelpers::FClassFinder<APlayerController> PcF(
		TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController"));
	if (PcF.Succeeded()) PlayerControllerClass = PcF.Class;
}
