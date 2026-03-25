// Copyright MaxFishing Project

#include "MaxFishingGameMode.h"
#include "MaxFishingCharacter.h"
#include "MaxFishingWaterPlacement.h"
#include "Engine/World.h"

AMaxFishingGameMode::AMaxFishingGameMode()
{
	DefaultPawnClass = AMaxFishingCharacter::StaticClass();
}

void AMaxFishingGameMode::StartPlay()
{
	Super::StartPlay();

	if (UWorld* World = GetWorld())
	{
		if (World->GetNetMode() != NM_Client)
		{
			MaxFishingPlaceDefaultWater(World);
		}
	}
}
