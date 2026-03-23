// Copyright MaxFishing Project

#include "FishAIController.h"
#include "FishBlackboardKeys.h"
#include "MaxFishingCharacter.h"
#include "FishingRodComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"

AFishAIController::AFishAIController()
{
	bWantsPlayerState = false;
	PrimaryActorTick.bCanEverTick = true;
}

void AFishAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (BehaviorTreeAsset)
	{
		RunBehaviorTree(BehaviorTreeAsset);
	}
}

void AFishAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SyncBlackboardFromWorld();
}

void AFishAIController::SyncBlackboardFromWorld()
{
	if (!GetWorld())
	{
		return;
	}

	UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB)
	{
		return;
	}

	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		BB->SetValueAsVector(FishBlackboard::PlayerLocation, PlayerPawn->GetActorLocation());

		if (AMaxFishingCharacter* Fisher = Cast<AMaxFishingCharacter>(PlayerPawn))
		{
			if (Fisher->FishingRod)
			{
				BB->SetValueAsVector(FishBlackboard::LureLocation, Fisher->FishingRod->GetLureWorldLocation());
			}
		}
	}
}
