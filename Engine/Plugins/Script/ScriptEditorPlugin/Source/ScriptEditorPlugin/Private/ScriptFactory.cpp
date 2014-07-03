// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved. 
#include "ScriptEditorPluginPrivatePCH.h"

/**
 * Class Picker filter
 */
class FScriptFactoryFilter : public IClassViewerFilter
{
public:
	TSet< const UClass* > AllowedChildOfClasses;

	bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs)
	{
		return AllowedChildOfClasses.Contains(InClass);
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return true;
	}
};

//////////////////////////////////////////////////////////////////////////


UScriptFactory::UScriptFactory(const FPostConstructInitializeProperties& PCIP)
	: Super( PCIP )
{
	SupportedClass = UScriptBlueprint::StaticClass();
	ParentClass = AScriptActor::StaticClass();

	Formats.Add(TEXT("txt;Script"));
	Formats.Add(TEXT("lua;Script"));

	bCreateNew = false;
	bEditorImport = true;
	bText = true;
	bEditAfterNew = true;	
}

void UScriptFactory::PostInitProperties()
{
	Super::PostInitProperties();
}

bool UScriptFactory::ConfigureProperties()
{
	// Null the parent class so we can check for selection later
	ParentClass = NULL;

	// Load the class viewer module to display a class picker
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.DisplayMode = EClassViewerDisplayMode::ListView;
	Options.bShowObjectRootClass = false;
	Options.bIsBlueprintBaseOnly = false;
	Options.bShowUnloadedBlueprints = false;

	// Only allow script-enabled classes
	TSharedPtr<FScriptFactoryFilter> Filter = MakeShareable(new FScriptFactoryFilter);
	Options.ClassFilter = Filter;
	Filter->AllowedChildOfClasses.Add(AScriptActor::StaticClass());
	Filter->AllowedChildOfClasses.Add(UScriptComponent::StaticClass());

	const FText TitleText = NSLOCTEXT("EditorFactories", "CreateScriptOptions", "Pick Parent Class");
	UClass* ChosenClass = NULL;
	const bool bPressedOk = SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, UScriptBlueprintGeneratedClass::StaticClass());
	if (bPressedOk)
	{
		ParentClass = ChosenClass;
	}

	return bPressedOk;
}


bool UScriptFactory::DoesSupportClass(UClass* Class)
{
	return Class == UScriptBlueprint::StaticClass();
}

UObject* UScriptFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	GEditor->SelectNone(true, true, false);

	UScriptBlueprint* NewBlueprint = CastChecked<UScriptBlueprint>(FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, InName, BPTYPE_Normal, UScriptBlueprint::StaticClass(), UScriptBlueprintGeneratedClass::StaticClass(), "UScriptFactory"));
	NewBlueprint->SourceFilePath = FReimportManager::SanitizeImportFilename(CurrentFilename, NewBlueprint);
	NewBlueprint->SourceCode = Buffer;
	NewBlueprint->SourceFileTimestamp = IFileManager::Get().GetTimeStamp(*NewBlueprint->SourceFilePath).ToString();

	// Need to make sure we compile with the new source code
	FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

	FEditorDelegates::OnAssetPostImport.Broadcast(this, NewBlueprint);

	return NewBlueprint;
}


/** UReimportScriptFactory */

UReimportScriptFactory::UReimportScriptFactory(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
}

bool UReimportScriptFactory::ConfigureProperties()
{
	return UFactory::ConfigureProperties();
}

bool UReimportScriptFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UScriptBlueprint* ScriptClass = Cast<UScriptBlueprint>(Obj);
	if (ScriptClass)
	{
		OutFilenames.Add(FReimportManager::ResolveImportFilename(ScriptClass->SourceFilePath, ScriptClass));
		return true;
	}
	return false;
}

void UReimportScriptFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UScriptBlueprint* ScriptClass = Cast<UScriptBlueprint>(Obj);
	if (ScriptClass && ensure(NewReimportPaths.Num() == 1))
	{
		ScriptClass->SourceFilePath = FReimportManager::SanitizeImportFilename(NewReimportPaths[0], Obj);
	}
}

/**
* Reimports specified texture from its source material, if the meta-data exists
*/
EReimportResult::Type UReimportScriptFactory::Reimport(UObject* Obj)
{
	UScriptBlueprint* ScriptClass = Cast<UScriptBlueprint>(Obj);
	if (!ScriptClass)
	{
		return EReimportResult::Failed;
	}

	TGuardValue<UScriptBlueprint*> OriginalScriptGuardValue(OriginalScript, ScriptClass);

	const FString ResolvedSourceFilePath = FReimportManager::ResolveImportFilename(ScriptClass->SourceFilePath, ScriptClass);
	if (!ResolvedSourceFilePath.Len())
	{
		return EReimportResult::Failed;
	}

	UE_LOG(LogScriptEditorPlugin, Log, TEXT("Performing atomic reimport of [%s]"), *ResolvedSourceFilePath);

	// Ensure that the file provided by the path exists
	if (IFileManager::Get().FileSize(*ResolvedSourceFilePath) == INDEX_NONE)
	{
		UE_LOG(LogScriptEditorPlugin, Warning, TEXT("Cannot reimport: source file cannot be found."));
		return EReimportResult::Failed;
	}

	if (UFactory::StaticImportObject(ScriptClass->GetClass(), ScriptClass->GetOuter(), *ScriptClass->GetName(), RF_Public | RF_Standalone, *ResolvedSourceFilePath, NULL, this))
	{
		UE_LOG(LogScriptEditorPlugin, Log, TEXT("Imported successfully"));
		// Try to find the outer package so we can dirty it up
		if (ScriptClass->GetOuter())
		{
			ScriptClass->GetOuter()->MarkPackageDirty();
		}
		else
		{
			ScriptClass->MarkPackageDirty();
		}
	}
	else
	{
		UE_LOG(LogScriptEditorPlugin, Warning, TEXT("-- import failed"));
	}

	return EReimportResult::Succeeded;
}
