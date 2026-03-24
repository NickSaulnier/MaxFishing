// Copyright MaxFishing Project

#pragma once

#include "CoreMinimal.h"

class UWorld;

#ifndef MAXFISHING_API
#define MAXFISHING_API
#endif

/**
 * Ensures the world has a visible Water Zone + Water Body Lake for the Water plugin:
 * - If both already exist: refreshes materials, mobility, landscape Z alignment, and zone rebuild.
 * - If only a zone exists (common partial save): spawns the default lake near the first Player Start.
 * - If only a lake exists: spawns a large zone and refreshes all water bodies.
 * - If neither exists: spawns zone + lake west of the first Player Start (spawn on shore), applies materials, calm Gerstner waves on runtime-tagged lakes, registers with the zone.
 */
MAXFISHING_API void MaxFishingPlaceDefaultWater(UWorld* World);
