// Copyright Zeus Server Engine. All rights reserved.

#include "ZeusMapTools.h"

#include "Misc/Attribute.h"
#include "ToolMenus.h"
#include "ToolMenuEntry.h"
#include "Styling/AppStyle.h"
#include "ZeusCollisionExporter.h"

#define LOCTEXT_NAMESPACE "FZeusMapToolsModule"

DEFINE_LOG_CATEGORY(LogZeusMapTools);

namespace ZeusMapToolsMenu
{
	static FSlateIcon MainToolsIcon()
	{
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.StaticMeshComponent"));
	}

	static FSlateIcon ExportIcon()
	{
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Import"));
	}

	static FSlateIcon SaveLevelIcon()
	{
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Save"));
	}

	static FSlateIcon PreviewIcon()
	{
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Visible"));
	}

	static FSlateIcon ValidateIcon()
	{
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Info"));
	}
}

void FZeusMapToolsModule::StartupModule()
{
	UE_LOG(LogZeusMapTools, Log, TEXT("ZeusMapTools module starting up."));

	if (UToolMenus::IsToolMenuUIEnabled())
	{
		RegisterMenus();
	}
	else
	{
		ToolMenusStartupHandle = UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FZeusMapToolsModule::RegisterMenus));
	}
}

void FZeusMapToolsModule::ShutdownModule()
{
	UE_LOG(LogZeusMapTools, Log, TEXT("ZeusMapTools module shutting down."));

	UnregisterMenus();
}

void FZeusMapToolsModule::RegisterMenus()
{
	using namespace ZeusMapToolsMenu;

	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	if (!Menu)
	{
		return;
	}

	FToolMenuSection& Section = Menu->FindOrAddSection("ZeusMapTools");
	Section.Label = LOCTEXT("ZeusMapToolsSectionLabel", "Zeus");

	const TAttribute<FSlateIcon> SubMenuIconAttr = MakeAttributeLambda([]()
	{
		return MainToolsIcon();
	});

	FToolMenuEntry ZeusMapToolsEntry = FToolMenuEntry::InitSubMenu(
		FName(TEXT("ZeusMapToolsSubMenu")),
		LOCTEXT("ZeusMapToolsLabel", "Zeus Map Tools"),
		LOCTEXT("ZeusMapToolsTooltip", "Export collision and debug Zeus map data."),
		FNewToolMenuChoice(FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
		{
			FToolMenuSection& Actions = InMenu->FindOrAddSection("Actions");
			Actions.Label = LOCTEXT("ZeusMapToolsActionsLabel", "Actions");

			Actions.AddMenuEntry(
				"ZeusMapTools_ExportSelected",
				LOCTEXT("ExportSelectedLabel", "Export Selected Collision"),
				LOCTEXT("ExportSelectedTooltip", "Export simple collision of the selected actors to JSON + ZSM."),
				ExportIcon(),
				FUIAction(FExecuteAction::CreateLambda([]()
				{
					FZeusCollisionExporter Exporter;
					FZeusExportOptions Options;
					Options.bIncludeSelectedOnly = true;
					Options.bWriteJson = true;
					Options.bWriteZsm = true;
					Options.bDrawDebug = true;
					const FZeusExportResult Result = Exporter.ExportFromEditor(Options);
					UE_LOG(LogZeusMapTools, Log,
						TEXT("[ZeusMapTools] Export Selected: entities=%d shapes=%d warnings=%d"),
						Result.Stats.EntityCount, Result.Stats.ShapeCount, Result.Stats.WarningCount);
				})));

			Actions.AddMenuEntry(
				"ZeusMapTools_ExportLevel",
				LOCTEXT("ExportLevelLabel", "Export Current Level Collision"),
				LOCTEXT("ExportLevelTooltip", "Export simple collision of all StaticMeshComponents in the current level."),
				SaveLevelIcon(),
				FUIAction(FExecuteAction::CreateLambda([]()
				{
					FZeusCollisionExporter Exporter;
					FZeusExportOptions Options;
					Options.bIncludeSelectedOnly = false;
					Options.bWriteJson = true;
					Options.bWriteZsm = true;
					Options.bDrawDebug = false;
					const FZeusExportResult Result = Exporter.ExportFromEditor(Options);
					UE_LOG(LogZeusMapTools, Log,
						TEXT("[ZeusMapTools] Export Level: entities=%d shapes=%d warnings=%d"),
						Result.Stats.EntityCount, Result.Stats.ShapeCount, Result.Stats.WarningCount);
				})));

			Actions.AddMenuEntry(
				"ZeusMapTools_PreviewSelected",
				LOCTEXT("PreviewSelectedLabel", "Preview Selected Collision"),
				LOCTEXT("PreviewSelectedTooltip", "Draw the simple collision that would be exported (no files written)."),
				PreviewIcon(),
				FUIAction(FExecuteAction::CreateLambda([]()
				{
					FZeusCollisionExporter Exporter;
					FZeusExportOptions Options;
					Options.bIncludeSelectedOnly = true;
					Options.bWriteJson = false;
					Options.bWriteZsm = false;
					Options.bDrawDebug = true;
					Exporter.ExportFromEditor(Options);
				})));

			Actions.AddMenuEntry(
				"ZeusMapTools_ValidateSelected",
				LOCTEXT("ValidateSelectedLabel", "Validate Selected Collision"),
				LOCTEXT("ValidateSelectedTooltip", "Check the selected actors for missing or invalid simple collision."),
				ValidateIcon(),
				FUIAction(FExecuteAction::CreateLambda([]()
				{
					FZeusCollisionExporter Exporter;
					FZeusExportOptions Options;
					Options.bIncludeSelectedOnly = true;
					Options.bWriteJson = false;
					Options.bWriteZsm = false;
					Options.bDrawDebug = false;
					Options.bValidateOnly = true;
					const FZeusExportResult Result = Exporter.ExportFromEditor(Options);
					UE_LOG(LogZeusMapTools, Log,
						TEXT("[ZeusMapTools] Validate Selected: entities=%d shapes=%d warnings=%d"),
						Result.Stats.EntityCount, Result.Stats.ShapeCount, Result.Stats.WarningCount);
					for (const FString& Warning : Result.Warnings)
					{
						UE_LOG(LogZeusMapTools, Warning, TEXT("[ZeusMapTools]   %s"), *Warning);
					}
				})));
		})),
		false,
		SubMenuIconAttr);
	Section.AddEntry(ZeusMapToolsEntry);
}

void FZeusMapToolsModule::UnregisterMenus()
{
	if (UObjectInitialized() && UToolMenus::Get())
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FZeusMapToolsModule, ZeusMapTools)
