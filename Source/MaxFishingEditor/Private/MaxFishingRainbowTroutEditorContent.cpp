// Copyright MaxFishing Project

#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/EngineTypes.h"
#include "Engine/Texture2D.h"
#include "IAssetTools.h"
#include "Logging/LogMacros.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SceneTypes.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogMaxFishingTroutContent, Log, All);

namespace MaxFishingEditorTroutContent
{
	static void TryImportTexturePngIfMissing(const FString& FileName)
	{
		const FString Dir = FPaths::ProjectContentDir() / TEXT("Fish/RainbowTrout");
		const FString FullPath = FPaths::ConvertRelativePathToFull(Dir / FileName);
		if (!FPaths::FileExists(FullPath))
		{
			return;
		}

		const FString BaseName = FPaths::GetBaseFilename(FileName);
		const FString ObjectPath = FString::Printf(TEXT("/Game/Fish/RainbowTrout/%s.%s"), *BaseName, *BaseName);
		if (LoadObject<UTexture2D>(nullptr, *ObjectPath))
		{
			return;
		}

		if (!FModuleManager::Get().IsModuleLoaded("AssetTools"))
		{
			return;
		}

		UAssetImportTask* Task = NewObject<UAssetImportTask>(GetTransientPackage(), NAME_None, RF_Transient);
		Task->Filename = FullPath;
		Task->DestinationPath = TEXT("/Game/Fish/RainbowTrout");
		Task->DestinationName = BaseName;
		Task->bSave = true;
		Task->bAutomated = true;
		Task->bReplaceExisting = true;
		Task->bAsync = false;

		TArray<UAssetImportTask*> Tasks;
		Tasks.Add(Task);
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().ImportAssetTasks(Tasks);
	}

	void TryImportRainbowTroutTexturesIfMissing()
	{
		static const TCHAR* const PngFiles[] = {
			TEXT("texture_diffuse.png"),
			TEXT("texture_normal.png"),
			TEXT("texture_roughness.png"),
			TEXT("texture_metallic.png"),
		};
		for (const TCHAR* Name : PngFiles)
		{
			TryImportTexturePngIfMissing(Name);
		}
	}

	static void ScanTroutContentFolder()
	{
		if (!FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
		{
			return;
		}
		FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FString> Paths;
		Paths.Add(TEXT("/Game/Fish/RainbowTrout"));
		ARM.Get().ScanPathsSynchronous(Paths);
	}

	void TryCreateRainbowTroutMaterialIfMissing()
	{
		if (LoadObject<UMaterial>(nullptr, TEXT("/Game/Fish/RainbowTrout/M_RainbowTrout.M_RainbowTrout")))
		{
			return;
		}

		ScanTroutContentFolder();

		UTexture2D* Diffuse = Cast<UTexture2D>(
			StaticLoadObject(UTexture2D::StaticClass(), nullptr, TEXT("/Game/Fish/RainbowTrout/texture_diffuse.texture_diffuse")));
		if (!Diffuse)
		{
			UE_LOG(LogMaxFishingTroutContent, Verbose, TEXT("M_RainbowTrout: texture_diffuse asset not found (import PNGs first)."));
			return;
		}

		UTexture2D* Normal = Cast<UTexture2D>(
			StaticLoadObject(UTexture2D::StaticClass(), nullptr, TEXT("/Game/Fish/RainbowTrout/texture_normal.texture_normal")));
		UTexture2D* Roughness = Cast<UTexture2D>(
			StaticLoadObject(UTexture2D::StaticClass(), nullptr, TEXT("/Game/Fish/RainbowTrout/texture_roughness.texture_roughness")));
		UTexture2D* Metallic = Cast<UTexture2D>(
			StaticLoadObject(UTexture2D::StaticClass(), nullptr, TEXT("/Game/Fish/RainbowTrout/texture_metallic.texture_metallic")));

		const FString PackageName = TEXT("/Game/Fish/RainbowTrout/M_RainbowTrout");
		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			UE_LOG(LogMaxFishingTroutContent, Warning, TEXT("M_RainbowTrout: CreatePackage failed for %s"), *PackageName);
			return;
		}

		UMaterial* Mat = NewObject<UMaterial>(Package, FName(TEXT("M_RainbowTrout")), RF_Public | RF_Standalone | RF_Transactional);
		Mat->MaterialDomain = MD_Surface;
		Mat->SetShadingModel(MSM_DefaultLit);

		UMaterialExpressionTextureSampleParameter2D* ExprDiffuse = Cast<UMaterialExpressionTextureSampleParameter2D>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTextureSampleParameter2D::StaticClass(), -400, -150));
		if (!ExprDiffuse)
		{
			UE_LOG(LogMaxFishingTroutContent, Warning, TEXT("M_RainbowTrout: failed to create diffuse expression."));
			return;
		}
		ExprDiffuse->ParameterName = FName(TEXT("TroutDiffuse"));
		ExprDiffuse->Texture = Diffuse;
		ExprDiffuse->SamplerType = SAMPLERTYPE_Color;
		if (!UMaterialEditingLibrary::ConnectMaterialProperty(ExprDiffuse, TEXT("RGB"), MP_BaseColor))
		{
			UE_LOG(LogMaxFishingTroutContent, Warning, TEXT("M_RainbowTrout: ConnectMaterialProperty BaseColor failed."));
		}

		if (Normal)
		{
			UMaterialExpressionTextureSampleParameter2D* ExprNormal = Cast<UMaterialExpressionTextureSampleParameter2D>(
				UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTextureSampleParameter2D::StaticClass(), -400, 150));
			if (ExprNormal)
			{
				ExprNormal->ParameterName = FName(TEXT("TroutNormal"));
				ExprNormal->Texture = Normal;
				ExprNormal->SamplerType = SAMPLERTYPE_Normal;
				UMaterialEditingLibrary::ConnectMaterialProperty(ExprNormal, TEXT("RGB"), MP_Normal);
			}
		}

		if (Roughness)
		{
			UMaterialExpressionTextureSampleParameter2D* ExprRough = Cast<UMaterialExpressionTextureSampleParameter2D>(
				UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTextureSampleParameter2D::StaticClass(), -400, 350));
			if (ExprRough)
			{
				ExprRough->ParameterName = FName(TEXT("TroutRoughness"));
				ExprRough->Texture = Roughness;
				ExprRough->SamplerType = SAMPLERTYPE_LinearGrayscale;
				UMaterialEditingLibrary::ConnectMaterialProperty(ExprRough, TEXT("R"), MP_Roughness);
			}
		}

		if (Metallic)
		{
			UMaterialExpressionTextureSampleParameter2D* ExprMetal = Cast<UMaterialExpressionTextureSampleParameter2D>(
				UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTextureSampleParameter2D::StaticClass(), -400, 550));
			if (ExprMetal)
			{
				ExprMetal->ParameterName = FName(TEXT("TroutMetallic"));
				ExprMetal->Texture = Metallic;
				ExprMetal->SamplerType = SAMPLERTYPE_LinearGrayscale;
				UMaterialEditingLibrary::ConnectMaterialProperty(ExprMetal, TEXT("R"), MP_Metallic);
			}
		}

		UMaterialEditingLibrary::RecompileMaterial(Mat);

		FAssetRegistryModule::AssetCreated(Mat);

		Package->MarkPackageDirty();

		const FString PackageFilename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		const bool bSaved = UPackage::SavePackage(Package, Mat, *PackageFilename, SaveArgs);
		if (!bSaved)
		{
			UE_LOG(LogMaxFishingTroutContent, Warning, TEXT("M_RainbowTrout: SavePackage failed (%s). Check folder permissions."), *PackageFilename);
		}
		else
		{
			UE_LOG(LogMaxFishingTroutContent, Log, TEXT("M_RainbowTrout: saved to %s"), *PackageFilename);
		}
	}

	/**
	 * Minimal lit material with one TextureSampleParameter2D (FishDiffuse) for runtime MID binding.
	 * Does not replace M_RainbowTrout; game code loads this path instead of engine debug materials.
	 */
	void TryCreateFishRuntimeDiffuseMaterialIfMissing()
	{
		if (LoadObject<UMaterial>(nullptr, TEXT("/Game/Fish/RainbowTrout/M_FishRuntimeDiffuse.M_FishRuntimeDiffuse")))
		{
			return;
		}

		ScanTroutContentFolder();

		UTexture2D* Diffuse = Cast<UTexture2D>(
			StaticLoadObject(UTexture2D::StaticClass(), nullptr, TEXT("/Game/Fish/RainbowTrout/texture_diffuse.texture_diffuse")));
		if (!Diffuse)
		{
			UE_LOG(LogMaxFishingTroutContent, Verbose, TEXT("M_FishRuntimeDiffuse: texture_diffuse asset not found."));
			return;
		}

		const FString PackageName = TEXT("/Game/Fish/RainbowTrout/M_FishRuntimeDiffuse");
		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			UE_LOG(LogMaxFishingTroutContent, Warning, TEXT("M_FishRuntimeDiffuse: CreatePackage failed for %s"), *PackageName);
			return;
		}

		UMaterial* Mat = NewObject<UMaterial>(Package, FName(TEXT("M_FishRuntimeDiffuse")), RF_Public | RF_Standalone | RF_Transactional);
		Mat->MaterialDomain = MD_Surface;
		Mat->SetShadingModel(MSM_DefaultLit);

		UMaterialExpressionTextureSampleParameter2D* ExprDiffuse = Cast<UMaterialExpressionTextureSampleParameter2D>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTextureSampleParameter2D::StaticClass(), -400, -150));
		if (!ExprDiffuse)
		{
			UE_LOG(LogMaxFishingTroutContent, Warning, TEXT("M_FishRuntimeDiffuse: failed to create diffuse expression."));
			return;
		}
		ExprDiffuse->ParameterName = FName(TEXT("FishDiffuse"));
		ExprDiffuse->Texture = Diffuse;
		ExprDiffuse->SamplerType = SAMPLERTYPE_Color;
		if (!UMaterialEditingLibrary::ConnectMaterialProperty(ExprDiffuse, TEXT("RGB"), MP_BaseColor))
		{
			UE_LOG(LogMaxFishingTroutContent, Warning, TEXT("M_FishRuntimeDiffuse: ConnectMaterialProperty BaseColor failed."));
		}

		UMaterialEditingLibrary::RecompileMaterial(Mat);

		FAssetRegistryModule::AssetCreated(Mat);

		Package->MarkPackageDirty();

		const FString PackageFilename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		const bool bSaved = UPackage::SavePackage(Package, Mat, *PackageFilename, SaveArgs);
		if (!bSaved)
		{
			UE_LOG(LogMaxFishingTroutContent, Warning, TEXT("M_FishRuntimeDiffuse: SavePackage failed (%s)."), *PackageFilename);
		}
		else
		{
			UE_LOG(LogMaxFishingTroutContent, Log, TEXT("M_FishRuntimeDiffuse: saved to %s"), *PackageFilename);
		}
	}

	/** Remove auto-generated planar UV chains so we can rebuild (WP*mul*mask or WP*TP*mul*mask; not TextureCoordinate). */
	static bool TryRemoveTroutDiffuseAutoPlanarUVChain(UMaterial* Mat, UMaterialExpressionTextureSampleParameter2D* DiffuseExpr)
	{
		if (!DiffuseExpr->Coordinates.IsConnected())
		{
			return false;
		}
		UMaterialExpressionAppendVector* Append = Cast<UMaterialExpressionAppendVector>(DiffuseExpr->Coordinates.Expression);
		if (Append && Append->A.IsConnected() && Append->B.IsConnected())
		{
			UMaterialExpressionComponentMask* MaskX = Cast<UMaterialExpressionComponentMask>(Append->A.Expression);
			UMaterialExpressionComponentMask* MaskZ = Cast<UMaterialExpressionComponentMask>(Append->B.Expression);
			if (MaskX && MaskZ && MaskX->Input.Expression == MaskZ->Input.Expression)
			{
				UMaterialExpressionMultiply* Mul = Cast<UMaterialExpressionMultiply>(MaskX->Input.Expression);
				if (Mul && Mul->A.IsConnected())
				{
					if (UMaterialExpressionTransformPosition* TP = Cast<UMaterialExpressionTransformPosition>(Mul->A.Expression))
					{
						if (UMaterialExpressionWorldPosition* WP = Cast<UMaterialExpressionWorldPosition>(TP->Input.Expression))
						{
							UMaterialEditingLibrary::DeleteMaterialExpression(Mat, Append);
							UMaterialEditingLibrary::DeleteMaterialExpression(Mat, MaskX);
							UMaterialEditingLibrary::DeleteMaterialExpression(Mat, MaskZ);
							UMaterialEditingLibrary::DeleteMaterialExpression(Mat, Mul);
							UMaterialEditingLibrary::DeleteMaterialExpression(Mat, TP);
							UMaterialEditingLibrary::DeleteMaterialExpression(Mat, WP);
							DiffuseExpr->Coordinates.Expression = nullptr;
							DiffuseExpr->Coordinates.OutputIndex = 0;
							return true;
						}
					}
				}
			}
		}

		UMaterialExpressionComponentMask* Mask = Cast<UMaterialExpressionComponentMask>(DiffuseExpr->Coordinates.Expression);
		if (!Mask || !Mask->Input.IsConnected())
		{
			return false;
		}
		UMaterialExpressionMultiply* Mul = Cast<UMaterialExpressionMultiply>(Mask->Input.Expression);
		if (!Mul || !Mul->A.IsConnected())
		{
			return false;
		}
		if (UMaterialExpressionTransformPosition* TP = Cast<UMaterialExpressionTransformPosition>(Mul->A.Expression))
		{
			if (UMaterialExpressionWorldPosition* WP = Cast<UMaterialExpressionWorldPosition>(TP->Input.Expression))
			{
				UMaterialEditingLibrary::DeleteMaterialExpression(Mat, Mask);
				UMaterialEditingLibrary::DeleteMaterialExpression(Mat, Mul);
				UMaterialEditingLibrary::DeleteMaterialExpression(Mat, TP);
				UMaterialEditingLibrary::DeleteMaterialExpression(Mat, WP);
				DiffuseExpr->Coordinates.Expression = nullptr;
				DiffuseExpr->Coordinates.OutputIndex = 0;
				return true;
			}
			return false;
		}
		if (UMaterialExpressionWorldPosition* WP = Cast<UMaterialExpressionWorldPosition>(Mul->A.Expression))
		{
			UMaterialEditingLibrary::DeleteMaterialExpression(Mat, Mask);
			UMaterialEditingLibrary::DeleteMaterialExpression(Mat, Mul);
			UMaterialEditingLibrary::DeleteMaterialExpression(Mat, WP);
			DiffuseExpr->Coordinates.Expression = nullptr;
			DiffuseExpr->Coordinates.OutputIndex = 0;
			return true;
		}
		return false;
	}

	/**
	 * Mesh UV0 can be degenerate. Use world position -> local space, then planar projection on local X/Z (typical fish length along Z).
	 */
	void TryUpgradeRainbowTroutDiffuseToWorldPlaneUV()
	{
		UMaterial* Mat = Cast<UMaterial>(StaticLoadObject(
			UMaterial::StaticClass(),
			nullptr,
			TEXT("/Game/Fish/RainbowTrout/M_RainbowTrout.M_RainbowTrout"),
			nullptr,
			LOAD_NoWarn,
			nullptr));
		if (!Mat)
		{
			UE_LOG(LogMaxFishingTroutContent, Verbose, TEXT("M_RainbowTrout: planar-UV upgrade skipped (material asset not loaded yet)."));
			return;
		}

		UMaterialExpressionTextureSampleParameter2D* DiffuseExpr = nullptr;
		for (UMaterialExpression* E : Mat->GetExpressions())
		{
			if (UMaterialExpressionTextureSampleParameter2D* T2d = Cast<UMaterialExpressionTextureSampleParameter2D>(E))
			{
				if (T2d->ParameterName == FName(TEXT("TroutDiffuse")))
				{
					DiffuseExpr = T2d;
					break;
				}
			}
		}
		if (!DiffuseExpr)
		{
			UE_LOG(LogMaxFishingTroutContent, Verbose, TEXT("M_RainbowTrout: planar-UV upgrade skipped (no TroutDiffuse expression; count=%d)."), Mat->GetExpressions().Num());
			return;
		}

		if (DiffuseExpr->Coordinates.IsConnected())
		{
			if (!TryRemoveTroutDiffuseAutoPlanarUVChain(Mat, DiffuseExpr))
			{
				return;
			}
			UE_LOG(LogMaxFishingTroutContent, Log, TEXT("M_RainbowTrout: removed prior auto planar UV chain; applying local X/Z UVs."));
		}

		UMaterialExpressionWorldPosition* ExprWP = Cast<UMaterialExpressionWorldPosition>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionWorldPosition::StaticClass(), -820, -150));
		UMaterialExpressionTransformPosition* ExprTP = Cast<UMaterialExpressionTransformPosition>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTransformPosition::StaticClass(), -640, -150));
		UMaterialExpressionMultiply* ExprMul = Cast<UMaterialExpressionMultiply>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -460, -150));
		UMaterialExpressionComponentMask* ExprMaskX = Cast<UMaterialExpressionComponentMask>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionComponentMask::StaticClass(), -280, -220));
		UMaterialExpressionComponentMask* ExprMaskZ = Cast<UMaterialExpressionComponentMask>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionComponentMask::StaticClass(), -280, -80));
		UMaterialExpressionAppendVector* ExprAppend = Cast<UMaterialExpressionAppendVector>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionAppendVector::StaticClass(), -100, -150));
		if (!ExprWP || !ExprTP || !ExprMul || !ExprMaskX || !ExprMaskZ || !ExprAppend)
		{
			UE_LOG(LogMaxFishingTroutContent, Warning, TEXT("M_RainbowTrout: planar-UV upgrade failed (could not create expressions)."));
			return;
		}

		ExprWP->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
		ExprTP->TransformSourceType = TRANSFORMPOSSOURCE_World;
		ExprTP->TransformType = TRANSFORMPOSSOURCE_Local;
		ExprMul->ConstB = 2.0f;

		const bool bWpToTp = UMaterialEditingLibrary::ConnectMaterialExpressions(ExprWP, FString(), ExprTP, FString());
		const bool bTpToMul = UMaterialEditingLibrary::ConnectMaterialExpressions(ExprTP, FString(), ExprMul, TEXT("A"));
		const bool bMulToMaskX = UMaterialEditingLibrary::ConnectMaterialExpressions(ExprMul, FString(), ExprMaskX, FString());
		const bool bMulToMaskZ = UMaterialEditingLibrary::ConnectMaterialExpressions(ExprMul, FString(), ExprMaskZ, FString());
		ExprMaskX->R = true;
		ExprMaskX->G = false;
		ExprMaskX->B = false;
		ExprMaskX->A = false;
		ExprMaskZ->R = false;
		ExprMaskZ->G = false;
		ExprMaskZ->B = true;
		ExprMaskZ->A = false;
		const bool bMXtoApp = UMaterialEditingLibrary::ConnectMaterialExpressions(ExprMaskX, FString(), ExprAppend, TEXT("A"));
		const bool bMZtoApp = UMaterialEditingLibrary::ConnectMaterialExpressions(ExprMaskZ, FString(), ExprAppend, TEXT("B"));
		const bool bAppToDiffuse = UMaterialEditingLibrary::ConnectMaterialExpressions(ExprAppend, FString(), DiffuseExpr, TEXT("UVs"));

		if (!bWpToTp || !bTpToMul || !bMulToMaskX || !bMulToMaskZ || !bMXtoApp || !bMZtoApp || !bAppToDiffuse)
		{
			UE_LOG(LogMaxFishingTroutContent, Warning, TEXT("M_RainbowTrout: planar-UV connect failed (wp->tp=%d tp->mul=%d mul->masks=%d/%d app=%d/%d app->diffuse=%d)."),
				bWpToTp, bTpToMul, bMulToMaskX, bMulToMaskZ, bMXtoApp, bMZtoApp, bAppToDiffuse);
			return;
		}

		UMaterialEditingLibrary::RecompileMaterial(Mat);
		Mat->MarkPackageDirty();

		const FString PackageName = TEXT("/Game/Fish/RainbowTrout/M_RainbowTrout");
		const FString PackageFilename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(Mat->GetOutermost(), Mat, *PackageFilename, SaveArgs);

		UE_LOG(LogMaxFishingTroutContent, Log, TEXT("M_RainbowTrout: TroutDiffuse uses local-space X/Z planar UVs (degenerate mesh UV workaround)."));
	}
}
