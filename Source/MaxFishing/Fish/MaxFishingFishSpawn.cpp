// Copyright MaxFishing Project

#include "MaxFishingFishSpawn.h"
#include "AIFishCharacter.h"
#include "WaterBodyLakeActor.h"
#include "WaterBodyComponent.h"
#include "WaterBodyTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RandomStream.h"

namespace MaxFishingFishSpawnPrivate
{
	static const FName RuntimeWaterTag(TEXT("MaxFishingRuntimeWater"));
	/** Shallow so default above-water camera can still see fish through the surface. */
	static constexpr float TroutDepthBelowSurfaceCm = 25.f;
	static constexpr float WaveQueryDepthCm = 10000.f;
	static constexpr int32 DemoTroutCount = 12;
	/** Minimum toward-lake pull; actual distance may be larger so XY stays inside the lake volume. */
	static constexpr float SpawnDistanceFromPlayerTowardLakeCm = 550.f;
	/** Spacing along the shore line (perpendicular to player-to-lake) so the school is easy to see. */
	static constexpr float TroutLineSpacingCm = 90.f;
	/** Matches runtime lake placement: player east of lake on +X when Y aligned (see MaxFishingWaterPlacement). */
	static constexpr float FallbackPlayerEastOfLakeXCm = 16500.f;
	/** Must match `DefaultLakeSplineRadiusCm` in MaxFishingWaterPlacement.cpp (circular spline lake). */
	static constexpr float LakeSplineRadiusCm = 15000.f;
	/** Past the shoreline along player->lake so XY is inside the water volume (550 cm alone leaves fish on dry land). */
	static constexpr float InsideLakeInsetCm = 350.f;
}

void MaxFishingSpawnDemoTroutInLake(UWorld* World)
{
	using namespace MaxFishingFishSpawnPrivate;

	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	AWaterBodyLake* Lake = nullptr;
	for (TActorIterator<AWaterBodyLake> It(World); It; ++It)
	{
		if (It->ActorHasTag(RuntimeWaterTag))
		{
			Lake = *It;
			break;
		}
	}
	if (!Lake)
	{
		for (TActorIterator<AWaterBodyLake> It(World); It; ++It)
		{
			Lake = *It;
			break;
		}
	}
	if (!Lake)
	{
		return;
	}

	APlayerStart* PlayerStart = nullptr;
	for (TActorIterator<APlayerStart> PSIt(World); PSIt; ++PSIt)
	{
		PlayerStart = *PSIt;
		break;
	}

	const FVector LakeLoc = Lake->GetActorLocation();

	FVector PlayerStartLoc = FVector::ZeroVector;
	if (PlayerStart)
	{
		PlayerStartLoc = PlayerStart->GetActorLocation();
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	FVector PawnLoc = FVector::ZeroVector;
	const bool bHasPawn = PlayerPawn != nullptr;
	if (bHasPawn)
	{
		PawnLoc = PlayerPawn->GetActorLocation();
	}

	FVector PlayerLoc;
	if (PlayerStart)
	{
		PlayerLoc = PlayerStartLoc;
	}
	else if (bHasPawn)
	{
		PlayerLoc = PawnLoc;
	}
	else
	{
		PlayerLoc = LakeLoc + FVector(FallbackPlayerEastOfLakeXCm, 0.f, 0.f);
	}

	FVector2D ToLake2D(LakeLoc.X - PlayerLoc.X, LakeLoc.Y - PlayerLoc.Y);
	if (ToLake2D.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		return;
	}
	ToLake2D.Normalize();
	const FVector TowardLake(ToLake2D.X, ToLake2D.Y, 0.f);
	const FVector AlongShore(-TowardLake.Y, TowardLake.X, 0.f);

	const float DistPlayerToLakeCenterXY = FVector::Dist2D(PlayerLoc, LakeLoc);
	const float MinTowardToBeInsideWater = DistPlayerToLakeCenterXY - LakeSplineRadiusCm + InsideLakeInsetCm;
	const float TowardLakeDistanceCm = FMath::Max(SpawnDistanceFromPlayerTowardLakeCm, MinTowardToBeInsideWater);
	const FVector BaseInWater = PlayerLoc + TowardLake * TowardLakeDistanceCm;
	UWaterBodyComponent* WB = Lake->GetWaterBodyComponent();

	for (int32 i = 0; i < DemoTroutCount; ++i)
	{
		const int32 Seed = static_cast<int32>(GetTypeHash(World)) ^ (i * 7919);
		FRandomStream Rng(Seed);
		const float LineOffset = (static_cast<float>(i) - 0.5f * static_cast<float>(DemoTroutCount - 1)) * TroutLineSpacingCm;
		const FVector Jitter(Rng.FRandRange(-40.f, 40.f), Rng.FRandRange(-40.f, 40.f), 0.f);
		FVector SpawnLocation = BaseInWater + AlongShore * LineOffset + Jitter;
		SpawnLocation.Z = LakeLoc.Z - TroutDepthBelowSurfaceCm;

		if (WB)
		{
			FWaveInfo WaveInfo;
			WaveInfo.Normal = FVector::UpVector;
			const FVector QueryPos(SpawnLocation.X, SpawnLocation.Y, LakeLoc.Z);
			if (WB->HasWaves() && WB->GetWaveInfoAtPosition(QueryPos, WaveQueryDepthCm, true, WaveInfo))
			{
				SpawnLocation.Z = LakeLoc.Z + WaveInfo.Height - TroutDepthBelowSurfaceCm;
			}
		}

		const FRotator SpawnRotation(0.f, Rng.FRandRange(0.f, 360.f), 0.f);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		World->SpawnActor<AAIFishCharacter>(AAIFishCharacter::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
	}
}
