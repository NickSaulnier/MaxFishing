// Copyright MaxFishing Project

#include "MaxFishingGameMode.h"
#include "MaxFishingCharacter.h"

AMaxFishingGameMode::AMaxFishingGameMode()
{
	DefaultPawnClass = AMaxFishingCharacter::StaticClass();
}
