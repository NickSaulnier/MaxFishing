// Copyright MaxFishing Project

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "FishAIController.generated.h"

UCLASS()
class MAXFISHING_API AFishAIController : public AAIController
{
	GENERATED_BODY()

public:
	AFishAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

protected:
	void SyncBlackboardFromWorld();
};
