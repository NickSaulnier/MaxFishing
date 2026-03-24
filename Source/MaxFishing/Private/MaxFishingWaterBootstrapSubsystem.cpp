// Copyright MaxFishing Project

#include "MaxFishingWaterBootstrapSubsystem.h"
#include "MaxFishingWaterPlacement.h"
#include "Engine/World.h"
#include "TimerManager.h"

bool UMaxFishingWaterBootstrapSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && !World->IsPreviewWorld();
}

void UMaxFishingWaterBootstrapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
		[WeakWorld = TWeakObjectPtr<UWorld>(World)]()
		{
			if (UWorld* W = WeakWorld.Get())
			{
				MaxFishingPlaceDefaultWater(W);
			}
		}));
}
