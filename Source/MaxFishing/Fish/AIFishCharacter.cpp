// Copyright MaxFishing Project

#include "Fish/AIFishCharacter.h"
#include "Fish/FishAIController.h"

AAIFishCharacter::AAIFishCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	AIControllerClass = AFishAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}
