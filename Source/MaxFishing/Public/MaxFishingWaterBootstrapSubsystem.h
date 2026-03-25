// Copyright MaxFishing Project

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "MaxFishingWaterBootstrapSubsystem.generated.h"

#ifndef MAXFISHING_API
#define MAXFISHING_API
#endif

/** Runs runtime water placement/refresh for game worlds (PIE, standalone) without relying on a specific GameMode. */
UCLASS()
class MAXFISHING_API UMaxFishingWaterBootstrapSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
	FTimerHandle DemoTroutSpawnTimer;
};
