// Copyright MaxFishing Project

#include "MaxFishingWaterPlacement.h"
#include "GerstnerWaterWaves.h"
#include "WaterBodyActor.h"
#include "WaterBodyComponent.h"
#include "WaterBodyLakeActor.h"
#include "WaterBodyLakeComponent.h"
#include "WaterRuntimeSettings.h"
#include "WaterSplineComponent.h"
#include "WaterZoneActor.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "WorldCollision.h"
#include "UObject/SoftObjectPath.h"

#if WITH_EDITOR
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"
#include "LandscapeDataAccess.h"

#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#endif

namespace MaxFishingWaterBootstrap
{
	static const FName RuntimeWaterTag(TEXT("MaxFishingRuntimeWater"));
}

namespace
{
	/**
	 * Small lift above sampled ground Z so the water surface is not coplanar / inside landscape collision
	 * (0 made the lake vanish; 300 looked obviously floating).
	 */
	static constexpr float WaterSurfaceAboveLandscapeCm = 12.f;
	/** Lake spline extent from actor origin (see MaxFishingConfigureLakeSplineAndBody). */
	static constexpr float DefaultLakeSplineRadiusCm = 15000.f;
	/** Horizontal gap between lake edge and PlayerStart so spawn is on land, not in water. */
	static constexpr float PlayerStartShoreStandoffCm = 1500.f;

	/** ~6 ft — max depth at lake center (cm). */
	static constexpr float LakeCenterDepthCm = 183.f;
	/** ~2 ft — depth near the shoreline (cm). */
	static constexpr float LakeShoreDepthCm = 61.f;

#if WITH_EDITOR
	static void MaxFishingAppendAgentLog(
		const TCHAR* HypothesisId,
		const TCHAR* Location,
		const TCHAR* Message,
		const FString& DataJson,
		const TCHAR* RunId)
	{
		static int32 GLogCount = 0;
		if (GLogCount >= 25)
		{
			return;
		}
		++GLogCount;

		const FString DebugLogPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("debug-950d46.log"));
		const int64 Timestamp = FDateTime::UtcNow().ToUnixTimestamp();
		const FString Id = FString::Printf(TEXT("log_%d"), GLogCount);

		const FString Line = FString::Printf(
			TEXT("{\"sessionId\":\"%s\",\"id\":\"%s\",\"timestamp\":%lld,\"location\":\"%s\",\"message\":\"%s\",\"runId\":\"%s\",\"hypothesisId\":\"%s\",\"data\":{%s}}"),
			TEXT("950d46"),
			*Id,
			Timestamp,
			Location,
			Message,
			RunId,
			HypothesisId,
			*DataJson);

		FFileHelper::SaveStringToFile(
			Line + TEXT("\n"),
			*DebugLogPath,
			FFileHelper::EEncodingOptions::AutoDetect,
			&IFileManager::Get(),
			EFileWrite::FILEWRITE_Append);
	}
#endif // WITH_EDITOR

	static void MaxFishingRebuildAllWaterZones(UWorld* World)
	{
		if (!World)
		{
			return;
		}
		for (TActorIterator<AWaterZone> ZIt(World); ZIt; ++ZIt)
		{
			ZIt->MarkForRebuild(EWaterZoneRebuildFlags::All, *ZIt);
			ZIt->Update();
		}
	}

	static TOptional<float> MaxFishingTryLandscapeZViaCollisionTrace(UWorld* World, float X, float Y)
	{
		if (!World)
		{
			return TOptional<float>();
		}

		FHitResult Hit;
		const FVector Start(X, Y, 200000.f);
		const FVector End(X, Y, -200000.f);
		FCollisionQueryParams Params(NAME_None, false);
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
		{
			if (Hit.Component.IsValid() && Hit.Component->IsA<ULandscapeHeightfieldCollisionComponent>())
			{
				return Hit.ImpactPoint.Z;
			}
		}
		return TOptional<float>();
	}

	static TOptional<float> MaxFishingTryLandscapeSurfaceZ(UWorld* World, float X, float Y)
	{
		if (!World)
		{
			return TOptional<float>();
		}

		// Prefer collision height: matches what geometry the water can end up "inside" when using heightfield-only Z.
		if (TOptional<float> TraceZ = MaxFishingTryLandscapeZViaCollisionTrace(World, X, Y))
		{
			return TraceZ;
		}

		const FVector Query(X, Y, 0.f);
		const bool bEditorLike = (World->WorldType == EWorldType::Editor) || (World->WorldType == EWorldType::EditorPreview);

		for (TActorIterator<ALandscapeProxy> LIt(World); LIt; ++LIt)
		{
			if (bEditorLike)
			{
				if (TOptional<float> Z = LIt->GetHeightAtLocation(Query, EHeightfieldSource::Editor))
				{
					return Z;
				}
			}
			if (TOptional<float> Z = LIt->GetHeightAtLocation(Query, EHeightfieldSource::Complex))
			{
				return Z;
			}
			if (TOptional<float> Z = LIt->GetHeightAtLocation(Query, EHeightfieldSource::Simple))
			{
				return Z;
			}
		}

		return TOptional<float>();
	}

	static void MaxFishingAlignLakeActorZAboveLandscape(UWorld* World, AWaterBodyLake* Lake)
	{
		if (!World || !Lake)
		{
			return;
		}

		FVector SampleXY = Lake->GetActorLocation();
		if (UWaterSplineComponent* Spline = Lake->GetWaterSpline())
		{
			SampleXY = Spline->Bounds.Origin;
		}

		const TOptional<float> TerrainZ = MaxFishingTryLandscapeSurfaceZ(World, SampleXY.X, SampleXY.Y);
		if (!TerrainZ.IsSet())
		{
			return;
		}

		const float NewZ = TerrainZ.GetValue() + WaterSurfaceAboveLandscapeCm;
		FVector Loc = Lake->GetActorLocation();
		Loc.Z = NewZ;
		Lake->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
	}

#if WITH_EDITOR
	static bool MaxFishingCircleOverlapsBoxXY(const FVector2D& Center, float Radius, const FBox& B)
	{
		if (!B.IsValid)
		{
			return false;
		}
		const float ClosestX = FMath::Clamp(Center.X, B.Min.X, B.Max.X);
		const float ClosestY = FMath::Clamp(Center.Y, B.Min.Y, B.Max.Y);
		const float D2 = FVector2D::DistSquared(FVector2D(ClosestX, ClosestY), Center);
		return D2 <= Radius * Radius;
	}

	/**
	 * Sculpt a bowl-shaped lake bed so depth is ~LakeCenterDepthCm at center
	 * and ~LakeShoreDepthCm toward the shoreline.
	 *
	 * NOTE: Editor/PIE only; requires LandscapeEdit API.
	 */
	static void MaxFishingEditorSculptGradedLakeBed(UWorld* World, AWaterBodyLake* Lake)
	{
		if (!World || !Lake || !Lake->Tags.Contains(MaxFishingWaterBootstrap::RuntimeWaterTag))
		{
			return;
		}

		static const FName SculptedTag(TEXT("MaxFishingLakeBedSculpted"));
		static bool bLoggedOnce = false;
		if (!bLoggedOnce)
		{
			bLoggedOnce = true;

			const FVector LakeCenter = Lake->GetActorLocation();
			const float SurfaceZ = LakeCenter.Z;
			const FVector2D CenterXY(LakeCenter.X, LakeCenter.Y);
			const FVector2D EdgeXY(CenterXY.X + DefaultLakeSplineRadiusCm * 0.98f, CenterXY.Y);

			const TOptional<float> CenterLandZ = MaxFishingTryLandscapeSurfaceZ(World, CenterXY.X, CenterXY.Y);
			const TOptional<float> EdgeLandZ = MaxFishingTryLandscapeSurfaceZ(World, EdgeXY.X, EdgeXY.Y);
			const float CenterDepthBefore = CenterLandZ.IsSet() ? FMath::Max(0.f, SurfaceZ - CenterLandZ.GetValue()) : -1.f;
			const float EdgeDepthBefore = EdgeLandZ.IsSet() ? FMath::Max(0.f, SurfaceZ - EdgeLandZ.GetValue()) : -1.f;

			// #region agent log
			{
				const bool bHasSculptedTag = Lake->Tags.Contains(SculptedTag);
				MaxFishingAppendAgentLog(
					TEXT("H_S1"),
					TEXT("MaxFishingWaterPlacement.cpp:MaxFishingEditorSculptGradedLakeBed(before)"),
					TEXT("Call + pre-sculpt depth samples"),
					FString::Printf(
						TEXT("\"hasRuntimeTag\":1,\"hasSculptedTag\":%d,\"surfaceZ\":%.2f,\"centerDepthBeforeCm\":%.2f,\"edgeDepthBeforeCm\":%.2f"),
						bHasSculptedTag ? 1 : 0,
						SurfaceZ,
						CenterDepthBefore,
						EdgeDepthBefore),
					TEXT("debug_sculpt_iter0"));
			}
			// #endregion

			if (Lake->Tags.Contains(SculptedTag))
			{
				return;
			}
		}

		if (Lake->Tags.Contains(SculptedTag))
		{
			return;
		}

		ALandscape* LandscapeActor = nullptr;
		for (TActorIterator<ALandscape> It(World); It; ++It)
		{
			LandscapeActor = *It;
			break;
		}
		if (!LandscapeActor)
		{
			return;
		}

		ULandscapeInfo* Info = LandscapeActor->GetLandscapeInfo();
		if (!Info || Info->XYtoComponentMap.IsEmpty())
		{
			return;
		}

		const FVector LakeCenter = Lake->GetActorLocation();
		const float SurfaceZ = LakeCenter.Z;
		const float Radius = DefaultLakeSplineRadiusCm;
		const FVector2D CenterXY(LakeCenter.X, LakeCenter.Y);

		FLandscapeEditDataInterface LandscapeEdit(Info);
		FLandscapeDoNotDirtyScope IgnorePackageDirty(LandscapeEdit, true);
		LandscapeEdit.SetShouldDirtyPackage(false);

		bool bAnyHeightChange = false;
		int32 NumComponentsTouched = 0;

		Info->ForAllLandscapeComponents(
			[&](ULandscapeComponent* Comp)
			{
				if (!Comp)
				{
					return;
				}

				const FBox Bounds = Comp->Bounds.GetBox();
				if ((Bounds.Max - Bounds.Min).SquaredLength() < KINDA_SMALL_NUMBER
					|| !MaxFishingCircleOverlapsBoxXY(CenterXY, Radius + 256.f, Bounds))
				{
					return;
				}

				int32 X1 = Comp->GetSectionBase().X;
				int32 Y1 = Comp->GetSectionBase().Y;
				const int32 Quads = Comp->ComponentSizeQuads;
				int32 X2 = X1 + Quads;
				int32 Y2 = Y1 + Quads;
				const int32 Stride = Quads + 1;

				TArray<uint16> Data;
				Data.SetNumUninitialized(Stride * Stride);
				LandscapeEdit.GetHeightData(X1, Y1, X2, Y2, Data.GetData(), Stride);

				FLandscapeComponentDataInterface CDI(Comp, 0, true);
				const FTransform CompTM = Comp->GetComponentTransform();
				bool bComponentChanged = false;

				for (int32 Ly = 0; Ly <= Quads; ++Ly)
				{
					for (int32 Lx = 0; Lx <= Quads; ++Lx)
					{
						const int32 Idx = Lx + Ly * Stride;
						const FVector WorldV = CDI.GetWorldVertex(Lx, Ly);

						const float Dist2d = FVector::Dist2D(
							FVector(WorldV.X, WorldV.Y, 0.f),
							FVector(CenterXY.X, CenterXY.Y, 0.f));
						if (Dist2d > Radius)
						{
							continue;
						}

						// Avoid sculpting at/above the surface plane.
						if (WorldV.Z >= SurfaceZ - 1.5f)
						{
							continue;
						}

						const float T = FMath::Clamp(Dist2d / Radius, 0.f, 1.f);
						const float SmoothT = T * T * (3.f - 2.f * T); // SmoothStep
						const float DesiredDepth = FMath::Lerp(LakeCenterDepthCm, LakeShoreDepthCm, SmoothT);
						const float TargetBedZ = SurfaceZ - DesiredDepth;

						if (WorldV.Z > TargetBedZ)
						{
							const FVector LocalTarget = CompTM.InverseTransformPosition(FVector(WorldV.X, WorldV.Y, TargetBedZ));
							Data[Idx] = LandscapeDataAccess::GetTexHeight(LocalTarget.Z);
							bComponentChanged = true;
						}
					}
				}

				if (bComponentChanged)
				{
					LandscapeEdit.SetHeightData(
						X1, Y1, X2, Y2,
						Data.GetData(),
						Stride,
						true,
						nullptr, nullptr, nullptr,
						false,
						nullptr, nullptr,
						true, true, true);
					bAnyHeightChange = true;
					++NumComponentsTouched;

					// Update collision so subsequent collision-traces / water depth queries
					// observe the freshly edited heightmap this frame.
					Comp->UpdateCollisionData(true);
					Comp->UpdateCachedBounds();
				}
			});

		LandscapeEdit.Flush();

		if (bAnyHeightChange)
		{
			Lake->Tags.AddUnique(SculptedTag);
		}

		// #region agent log
		{
			const FVector LakeCenterAfter = Lake->GetActorLocation();
			const float SurfaceZAfter = LakeCenterAfter.Z;
			const FVector2D CenterXYAfter(LakeCenterAfter.X, LakeCenterAfter.Y);
			const FVector2D EdgeXYAfter(CenterXYAfter.X + DefaultLakeSplineRadiusCm * 0.98f, CenterXYAfter.Y);

			const TOptional<float> CenterLandZAfter = MaxFishingTryLandscapeSurfaceZ(World, CenterXYAfter.X, CenterXYAfter.Y);
			const TOptional<float> EdgeLandZAfter = MaxFishingTryLandscapeSurfaceZ(World, EdgeXYAfter.X, EdgeXYAfter.Y);
			const float CenterDepthAfter = CenterLandZAfter.IsSet() ? FMath::Max(0.f, SurfaceZAfter - CenterLandZAfter.GetValue()) : -1.f;
			const float EdgeDepthAfter = EdgeLandZAfter.IsSet() ? FMath::Max(0.f, SurfaceZAfter - EdgeLandZAfter.GetValue()) : -1.f;

			MaxFishingAppendAgentLog(
				TEXT("H_S2"),
				TEXT("MaxFishingWaterPlacement.cpp:MaxFishingEditorSculptGradedLakeBed(after)"),
				TEXT("Sculpt result + post-sculpt depth samples"),
				FString::Printf(
					TEXT("\"bAnyHeightChange\":%d,\"numComponentsTouched\":%d,\"surfaceZ\":%.2f,\"centerDepthAfterCm\":%.2f,\"edgeDepthAfterCm\":%.2f"),
					bAnyHeightChange ? 1 : 0,
					NumComponentsTouched,
					SurfaceZAfter,
					CenterDepthAfter,
					EdgeDepthAfter),
				TEXT("debug_sculpt_iter0"));
		}
		// #endregion
	}
#endif // WITH_EDITOR

	static void MaxFishingEnsureLakeRootMobility(UWaterBodyComponent* Body)
	{
		if (!Cast<UWaterBodyLakeComponent>(Body))
		{
			return;
		}
		Body->SetMobility(EComponentMobility::Movable);
	}

	static void MaxFishingApplyDefaultLakeMaterialsIfNeeded(UWaterBodyComponent* Body)
	{
		if (!Cast<UWaterBodyLakeComponent>(Body))
		{
			return;
		}

		if (Body->GetWaterMaterial() && Body->GetWaterStaticMeshMaterial())
		{
			return;
		}

		static const FSoftObjectPath LakeWaterPath(TEXT("/Water/Materials/WaterSurface/Water_Material_Lake.Water_Material_Lake"));
		static const FSoftObjectPath LakeLodPath(TEXT("/Water/Materials/WaterSurface/LODs/Water_Material_Lake_LOD.Water_Material_Lake_LOD"));
		static const FSoftObjectPath UnderwaterPath(TEXT("/Water/Materials/PostProcessing/M_UnderWater_PostProcess_Volume.M_UnderWater_PostProcess_Volume"));
		static const FSoftObjectPath HlodPath(TEXT("/Water/Materials/HLOD/HLODWater.HLODWater"));

		if (!Body->GetWaterMaterial())
		{
			if (UMaterialInterface* M = Cast<UMaterialInterface>(LakeWaterPath.TryLoad()))
			{
				Body->SetWaterMaterial(M);
			}
		}
		if (!Body->GetWaterStaticMeshMaterial())
		{
			if (UMaterialInterface* M = Cast<UMaterialInterface>(LakeLodPath.TryLoad()))
			{
				Body->SetWaterStaticMeshMaterial(M);
			}
		}
		if (UMaterialInterface* Underwater = Cast<UMaterialInterface>(UnderwaterPath.TryLoad()))
		{
			Body->SetUnderwaterPostProcessMaterial(Underwater);
		}
		if (const UWaterRuntimeSettings* WaterSettings = GetDefault<UWaterRuntimeSettings>())
		{
			if (UMaterialInterface* Info = WaterSettings->GetDefaultWaterInfoMaterial())
			{
				Body->SetWaterInfoMaterial(Info);
			}
		}
		if (UMaterialInterface* Hlod = Cast<UMaterialInterface>(HlodPath.TryLoad()))
		{
			Body->SetHLODMaterial(Hlod);
		}

		Body->UpdateMaterialInstances();
	}

	/** Procedural calm-lake Gerstner (no editor asset); only for lakes tagged at runtime bootstrap. */
	static void MaxFishingApplyCalmGerstnerWavesToRuntimeLake(AWaterBodyLake* Lake)
	{
		if (!Lake || !Lake->Tags.Contains(MaxFishingWaterBootstrap::RuntimeWaterTag))
		{
			return;
		}
		if (const UWaterBodyComponent* Body = Lake->GetWaterBodyComponent())
		{
			if (Body->HasWaves())
			{
				return;
			}
		}

		UGerstnerWaterWaves* Waves = NewObject<UGerstnerWaterWaves>(
			Lake,
			MakeUniqueObjectName(Lake, UGerstnerWaterWaves::StaticClass(), TEXT("MaxFishingGerstnerWaves")),
			RF_Transactional);

		if (UGerstnerWaterWaveGeneratorSimple* Gen = Cast<UGerstnerWaterWaveGeneratorSimple>(Waves->GerstnerWaveGenerator))
		{
			Gen->NumWaves = 8;
			Gen->Randomness = 0.12f;
			Gen->MinWavelength = 220.f;
			Gen->MaxWavelength = 2200.f;
			Gen->WavelengthFalloff = 2.f;
			Gen->MinAmplitude = 1.2f;
			Gen->MaxAmplitude = 14.f;
			Gen->AmplitudeFalloff = 2.f;
			Gen->WindAngleDeg = 40.f;
			Gen->DirectionAngularSpreadDeg = 50.f;
			Gen->SmallWaveSteepness = 0.32f;
			Gen->LargeWaveSteepness = 0.12f;
			Gen->SteepnessFalloff = 1.f;
		}

		Waves->RecomputeWaves(false);
		Lake->SetWaterWaves(Waves);
	}

	static void MaxFishingRefreshExistingWater(UWorld* World)
	{
		if (!World)
		{
			return;
		}
		for (TActorIterator<AWaterBody> It(World); It; ++It)
		{
			if (AWaterBodyLake* LakeActor = Cast<AWaterBodyLake>(*It))
			{
				MaxFishingAlignLakeActorZAboveLandscape(World, LakeActor);
#if WITH_EDITOR
				// #region agent log
				{
					static bool bLoggedRefreshOnce = false;
					if (!bLoggedRefreshOnce)
					{
						bLoggedRefreshOnce = true;

						static const FName SculptedTag(TEXT("MaxFishingLakeBedSculpted"));
						const bool bHasRuntimeTag = LakeActor->Tags.Contains(MaxFishingWaterBootstrap::RuntimeWaterTag);
						const bool bHasSculptedTag = LakeActor->Tags.Contains(SculptedTag);

						MaxFishingAppendAgentLog(
							TEXT("H_S0"),
							TEXT("MaxFishingWaterPlacement.cpp:MaxFishingRefreshExistingWater(call)"),
							TEXT("Whether sculpt entry conditions are met"),
							FString::Printf(
								TEXT("\"hasRuntimeTag\":%d,\"hasSculptedTag\":%d"),
								bHasRuntimeTag ? 1 : 0,
								bHasSculptedTag ? 1 : 0),
							TEXT("debug_sculpt_iter0"));
					}
				}
				// #endregion
				// Sculpt the landscape bowl so depth varies from center (~6 ft) to shore (~2 ft).
				// Editor/PIE only (requires LandscapeEdit API).
				MaxFishingEditorSculptGradedLakeBed(World, LakeActor);
#endif
				MaxFishingApplyCalmGerstnerWavesToRuntimeLake(LakeActor);
			}
			if (UWaterBodyComponent* Body = It->GetWaterBodyComponent())
			{
				MaxFishingEnsureLakeRootMobility(Body);
				MaxFishingApplyDefaultLakeMaterialsIfNeeded(Body);
				FOnWaterBodyChangedParams Params;
				Params.bShapeOrPositionChanged = true;
				Params.bUserTriggered = true;
				Body->UpdateAll(Params);
			}
		}
		MaxFishingRebuildAllWaterZones(World);
	}

	static FVector MaxFishingComputeDefaultWaterOrigin(UWorld* World)
	{
		FVector WaterOrigin = FVector(0.0, 0.0, 100.0);
		for (TActorIterator<APlayerStart> PSIt(World); PSIt; ++PSIt)
		{
			const FVector PS = PSIt->GetActorLocation();
			// Lake was centered on the player; default lake extends ~DefaultLakeSplineRadiusCm from center,
			// so spawn was underwater. Offset the lake west so PlayerStart sits on shore (+X = land).
			const float LakeWestOffsetX = DefaultLakeSplineRadiusCm + PlayerStartShoreStandoffCm;
			WaterOrigin.X = PS.X - LakeWestOffsetX;
			WaterOrigin.Y = PS.Y;
			WaterOrigin.Z = PS.Z - 20.0;
			break;
		}
		if (TOptional<float> TerrainZ = MaxFishingTryLandscapeSurfaceZ(World, WaterOrigin.X, WaterOrigin.Y))
		{
			WaterOrigin.Z = TerrainZ.GetValue() + WaterSurfaceAboveLandscapeCm;
		}
		return WaterOrigin;
	}

	static AWaterZone* MaxFishingSpawnWaterZone(UWorld* World, const FVector& WaterOrigin)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AWaterZone* WaterZone = World->SpawnActor<AWaterZone>(AWaterZone::StaticClass(), WaterOrigin, FRotator::ZeroRotator, SpawnParams);
		if (WaterZone)
		{
			WaterZone->Tags.Add(MaxFishingWaterBootstrap::RuntimeWaterTag);
			WaterZone->SetActorLocation(WaterOrigin);
			WaterZone->SetZoneExtent(FVector2D(200000.0, 200000.0));
			WaterZone->MarkForRebuild(EWaterZoneRebuildFlags::All, WaterZone);
			WaterZone->Update();
		}
		return WaterZone;
	}

	static void MaxFishingConfigureLakeSplineAndBody(AWaterBodyLake* Lake)
	{
		if (!Lake)
		{
			return;
		}

		Lake->Tags.Add(MaxFishingWaterBootstrap::RuntimeWaterTag);

		if (UWaterSplineComponent* Spline = Lake->GetWaterSpline())
		{
			const float Radius = DefaultLakeSplineRadiusCm;
			Spline->ClearSplinePoints(false);
			Spline->AddSplinePoint(FVector(Radius, 0.0f, 0.0f), ESplineCoordinateSpace::Local, false);
			Spline->AddSplinePoint(FVector(0.0f, Radius, 0.0f), ESplineCoordinateSpace::Local, false);
			Spline->AddSplinePoint(FVector(-Radius, 0.0f, 0.0f), ESplineCoordinateSpace::Local, false);
			Spline->AddSplinePoint(FVector(0.0f, -Radius, 0.0f), ESplineCoordinateSpace::Local, false);
			Spline->SetClosedLoop(true, false);
			Spline->UpdateSpline();
			Spline->K2_SynchronizeAndBroadcastDataChange();
		}

		if (UWaterBodyComponent* Body = Lake->GetWaterBodyComponent())
		{
			MaxFishingEnsureLakeRootMobility(Body);
			MaxFishingApplyDefaultLakeMaterialsIfNeeded(Body);
		}

		MaxFishingApplyCalmGerstnerWavesToRuntimeLake(Lake);

		if (UWaterBodyComponent* Body = Lake->GetWaterBodyComponent())
		{
			FOnWaterBodyChangedParams Params;
			Params.bShapeOrPositionChanged = true;
			Params.bUserTriggered = true;
			Body->UpdateAll(Params);
		}
	}

	static AWaterBodyLake* MaxFishingSpawnWaterLake(UWorld* World, const FVector& WaterOrigin)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AWaterBodyLake* Lake = World->SpawnActor<AWaterBodyLake>(AWaterBodyLake::StaticClass(), WaterOrigin, FRotator::ZeroRotator, SpawnParams);
		MaxFishingConfigureLakeSplineAndBody(Lake);
		return Lake;
	}
}

void MaxFishingPlaceDefaultWater(UWorld* World)
{
	if (!World)
	{
		return;
	}

	bool bHasZone = false;
	for (TActorIterator<AWaterZone> ZoneIt(World); ZoneIt; ++ZoneIt)
	{
		bHasZone = true;
		break;
	}

	bool bHasLake = false;
	for (TActorIterator<AWaterBodyLake> LakeIt(World); LakeIt; ++LakeIt)
	{
		bHasLake = true;
		break;
	}

	if (bHasZone && bHasLake)
	{
		MaxFishingRefreshExistingWater(World);
		return;
	}

	const FVector WaterOrigin = MaxFishingComputeDefaultWaterOrigin(World);

	if (!bHasZone)
	{
		MaxFishingSpawnWaterZone(World, WaterOrigin);
	}

	if (!bHasLake)
	{
		MaxFishingSpawnWaterLake(World, WaterOrigin);
	}

	if (!bHasZone && bHasLake)
	{
		MaxFishingRefreshExistingWater(World);
		return;
	}

	MaxFishingRebuildAllWaterZones(World);
}
