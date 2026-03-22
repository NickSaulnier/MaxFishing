// Copyright MaxFishing Project

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Fish/FishTypes.h"
#include "BTTask_FishSetState.generated.h"

UCLASS()
class MAXFISHING_API UBTTask_FishSetState : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FishSetState();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector FishStateKey;

	UPROPERTY(EditAnywhere, Category = "Fish")
	EFishAIState NewState = EFishAIState::Idle;
};
