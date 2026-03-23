// Copyright MaxFishing Project

#include "AIFishCharacter.h"
#include "FishAIController.h"

AAIFishCharacter::AAIFishCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	AIControllerClass = AFishAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}
