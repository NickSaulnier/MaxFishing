// Copyright MaxFishing Project

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MaxFishingGameMode.generated.h"

UCLASS()
class MAXFISHING_API AMaxFishingGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMaxFishingGameMode();

	virtual void StartPlay() override;

private:
	void EnsureRuntimeWater();
};
