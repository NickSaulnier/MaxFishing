// Copyright MaxFishing Project

#include "MaxFishingWaterBootstrapSubsystem.h"
#include "MaxFishingFishSpawn.h"
#include "MaxFishingWaterPlacement.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/WeakObjectPtr.h"

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
		[WeakThis = TWeakObjectPtr<UMaxFishingWaterBootstrapSubsystem>(this), WeakWorld = TWeakObjectPtr<UWorld>(World)]()
		{
			UMaxFishingWaterBootstrapSubsystem* Sub = WeakThis.Get();
			if (UWorld* W = WeakWorld.Get())
			{
				MaxFishingPlaceDefaultWater(W);
				if (Sub)
				{
					W->GetTimerManager().SetTimer(Sub->DemoTroutSpawnTimer,
						FTimerDelegate::CreateLambda([WeakW = TWeakObjectPtr<UWorld>(W)]()
							{
								if (UWorld* WW = WeakW.Get())
								{
									MaxFishingSpawnDemoTroutInLake(WW);
								}
							}),
						0.4f,
						false);
				}
			}
		}));
}
