// Copyright MaxFishing Project

#pragma once

#include "CoreMinimal.h"
#include "FishTypes.generated.h"

UENUM(BlueprintType)
enum class EFishAIState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Investigate UMETA(DisplayName = "Investigate"),
	Bite UMETA(DisplayName = "Bite"),
	Flee UMETA(DisplayName = "Flee")
};
