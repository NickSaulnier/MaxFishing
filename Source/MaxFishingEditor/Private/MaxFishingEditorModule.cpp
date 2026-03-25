// Copyright MaxFishing Project

#include "MaxFishingWaterPlacement.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "IAssetTools.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"

namespace MaxFishingEditorWater
{
	/** Scan /Water so Content Browser shows plugin assets without a manual folder force-load; load core materials into memory. */
	static void PreloadWaterPluginContent()
	{
		if (!FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
		{
			return;
		}

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
		TArray<FString> Paths;
		Paths.Add(TEXT("/Water"));
		AssetRegistry.ScanPathsSynchronous(Paths, /*bForceRescan*/ false);

		static const TCHAR* ObjectPaths[] = {
			TEXT("/Water/Materials/WaterSurface/Water_Material_Lake.Water_Material_Lake"),
			TEXT("/Water/Materials/WaterSurface/LODs/Water_Material_Lake_LOD.Water_Material_Lake_LOD"),
			TEXT("/Water/Materials/PostProcessing/M_UnderWater_PostProcess_Volume.M_UnderWater_PostProcess_Volume"),
			TEXT("/Water/Materials/HLOD/HLODWater.HLODWater"),
			TEXT("/Water/Materials/MPC/MPC_Water.MPC_Water"),
			TEXT("/Water/Materials/WaterInfo/DrawWaterInfo.DrawWaterInfo"),
		};
		for (const TCHAR* Path : ObjectPaths)
		{
			FSoftObjectPath(Path).TryLoad();
		}
	}
}

namespace MaxFishingEditorTrout
{
	static bool GImportAttemptedThisEditorSession = false;

	/**
	 * If base.obj exists under Content but no UStaticMesh asset yet, import once so PIE can load /Game/Fish/RainbowTrout/base.
	 */
	static void TryImportBaseObjIfMissing()
	{
		const FString ObjFullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Fish/RainbowTrout/base.obj"));
		if (!FPaths::FileExists(ObjFullPath))
		{
			return;
		}
		if (LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Fish/RainbowTrout/base.base")))
		{
			return;
		}

		UAssetImportTask* Task = NewObject<UAssetImportTask>(GetTransientPackage(), NAME_None, RF_Transient);
		Task->Filename = ObjFullPath;
		Task->DestinationPath = TEXT("/Game/Fish/RainbowTrout");
		Task->DestinationName = TEXT("base");
		Task->bSave = true;
		Task->bAutomated = true;
		Task->bReplaceExisting = true;
		Task->bAsync = false;

		if (!FModuleManager::Get().IsModuleLoaded("AssetTools"))
		{
			return;
		}

		TArray<UAssetImportTask*> Tasks;
		Tasks.Add(Task);
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().ImportAssetTasks(Tasks);
	}
}

class FMaxFishingEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		MaxFishingEditorWater::PreloadWaterPluginContent();

		OnMapOpenedHandle = FEditorDelegates::OnMapOpened.AddRaw(this, &FMaxFishingEditorModule::HandleMapOpened);
		TryPlaceWaterInEditorWorld();
	}

	virtual void ShutdownModule() override
	{
		FEditorDelegates::OnMapOpened.Remove(OnMapOpenedHandle);
		OnMapOpenedHandle = FDelegateHandle();
	}

private:
	void HandleMapOpened(const FString& /*Filename*/, bool /*bLoadAsTemplate*/)
	{
		TryPlaceWaterInEditorWorld();
	}

	void TryPlaceWaterInEditorWorld()
	{
		if (!GEditor)
		{
			return;
		}

		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (!World || !World->IsEditorWorld() || World->IsPreviewWorld())
		{
			return;
		}

		// Next tick: AssetTools is not safe during StartupModule; same tick as water so landscape/collision are ready.
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
			[WeakWorld = TWeakObjectPtr<UWorld>(World)]()
			{
				if (!MaxFishingEditorTrout::GImportAttemptedThisEditorSession)
				{
					MaxFishingEditorTrout::GImportAttemptedThisEditorSession = true;
					MaxFishingEditorTrout::TryImportBaseObjIfMissing();
				}
				if (UWorld* W = WeakWorld.Get())
				{
					MaxFishingPlaceDefaultWater(W);
					if (GEditor)
					{
						GEditor->RedrawAllViewports();
					}
				}
			}));
	}

	FDelegateHandle OnMapOpenedHandle;
};

IMPLEMENT_MODULE(FMaxFishingEditorModule, MaxFishingEditor);
