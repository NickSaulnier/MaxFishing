// Copyright MaxFishing Project

#include "MaxFishingGameMode.h"
#include "Player/MaxFishingCharacter.h"

AMaxFishingGameMode::AMaxFishingGameMode()
{
	DefaultPawnClass = AMaxFishingCharacter::StaticClass();
}
