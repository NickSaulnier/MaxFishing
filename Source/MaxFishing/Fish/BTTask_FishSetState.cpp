// Copyright MaxFishing Project

#include "BTTask_FishSetState.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_FishSetState::UBTTask_FishSetState()
{
	NodeName = TEXT("Fish Set State");
}

EBTNodeResult::Type UBTTask_FishSetState::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB || FishStateKey.SelectedKeyName.IsNone())
	{
		return EBTNodeResult::Failed;
	}

	BB->SetValueAsEnum(FishStateKey.SelectedKeyName, static_cast<uint8>(NewState));
	return EBTNodeResult::Succeeded;
}
