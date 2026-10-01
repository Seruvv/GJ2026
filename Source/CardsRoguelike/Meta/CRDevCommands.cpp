// Development console commands for manual M2.8 testing (death, graveyard, recruitment) in PIE. Each one goes
// through the same game code the UI uses; none exist in Shipping builds. Use a disposable profile.
//   CR.Dev.Status                    logs the active profile (living, dead, selection, candidates) and the run
//   CR.Dev.NewProfile <name>         creates and selects a profile, then opens the sanctuary
//   CR.Dev.StartRun                  same as В ПОХОД in the sanctuary
//   CR.Dev.Enter <Combat|Event|Shop> enters the first open room of that type and opens its map
//   CR.Dev.LoseCombat                the combat hamster falls (normal defeat flow)
//   CR.Dev.SetHP <n>                 sets the run hamster's current HP (e.g. before a lethal event choice)
//   CR.Dev.Choose <n>                event room: takes choice n (as key n); CR.Dev.Continue = ПРОДОЛЖИТЬ
//   CR.Dev.HubView <Graveyard|Recruitment|Sanctuary>   opens a sanctuary view (as its button)
//   CR.Dev.Recruit <n>               recruits saved candidate n (1-based, as ПРИНЯТЬ В УБЕЖИЩЕ)
//   CR.Dev.ReturnToHub               same as ВЕРНУТЬСЯ В УБЕЖИЩЕ on a finished run
//   CR.Dev.KillAllLiving <name>      kills every living hamster of the active profile (name must match)

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "../Combat/CRCombatGameMode.h"
#include "../Event/CREventGameMode.h"
#include "../Hub/CRHubHUD.h"
#include "../Run/CRRunSubsystem.h"
#include "CRProfileSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogCRDev, Log, All);

namespace
{
	UCRProfileSubsystem* DevProfiles(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UCRProfileSubsystem>() : nullptr;
	}

	UCRRunSubsystem* DevRun(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UCRRunSubsystem>() : nullptr;
	}

	FString DevJoinArgs(const TArray<FString>& Args)
	{
		return FString::Join(Args, TEXT(" ")).TrimStartAndEnd();
	}

	FAutoConsoleCommandWithWorldAndArgs GCRDevStatus(TEXT("CR.Dev.Status"), TEXT("Logs the active profile and the current run."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			const UCRProfileSubsystem* Profiles = DevProfiles(World);
			const UCRProfileSaveGame* Profile = Profiles ? Profiles->GetActiveProfile() : nullptr;
			if (!Profile)
			{
				UE_LOG(LogCRDev, Warning, TEXT("CR.Dev.Status: no active profile"));
				return;
			}
			UE_LOG(LogCRDev, Log, TEXT("Profile %s '%s' v%d: Silver %d Food %d Wood %d | runs %d, completed %d, failed %d, abandoned %d | selected %s"),
				*Profile->ProfileId, *Profile->DisplayName, Profile->SaveVersion, Profile->Resources.Silver, Profile->Resources.Food, Profile->Resources.Wood,
				Profile->Stats.RunsStarted, Profile->Stats.RunsCompleted, Profile->Stats.RunsFailed, Profile->Stats.RunsAbandoned, *Profile->SelectedHamsterId.ToString());
			for (const FCRHamsterPersistentState& Hamster : Profile->Hamsters)
			{
				if (Hamster.bAlive)
				{
					UE_LOG(LogCRDev, Log, TEXT("  alive  %s '%s' HP %d mana %d"), *Hamster.HamsterId.ToString(), *Hamster.DisplayName, Hamster.BaseMaxHP, Hamster.BaseManaPerTurn);
				}
				else
				{
					UE_LOG(LogCRDev, Log, TEXT("  DEAD   %s '%s' HP %d mana %d | %s, %s, seed %d, node %s (%s), rooms %d, lost %s | epitaph: %s"),
						*Hamster.HamsterId.ToString(), *Hamster.DisplayName, Hamster.BaseMaxHP, Hamster.BaseManaPerTurn,
						*CRRun::DeathCauseDisplayText(Hamster.Death.DeathCause), *Hamster.Death.DeathTimestamp.ToString(), Hamster.Death.RunSeed,
						*Hamster.Death.NodeId.ToString(), *CRRun::RoomTypeName(Hamster.Death.RoomType), Hamster.Death.RoomsVisited,
						*CRMeta::FormatResources(Hamster.Death.LostLoot), *CRMeta::GetRevealedEpitaph(Hamster));
				}
			}
			for (const FCRHamsterPersistentState& Candidate : Profile->RecruitCandidates)
			{
				UE_LOG(LogCRDev, Log, TEXT("  cand.  %s '%s' HP %d mana %d"), *Candidate.HamsterId.ToString(), *Candidate.DisplayName, Candidate.BaseMaxHP, Candidate.BaseManaPerTurn);
			}
			UE_LOG(LogCRDev, Log, TEXT("  living %d, graves %d (unseen %d), can recruit %s | last run: valid %d, reason %s, hamster '%s', delivered %s, carried %s"),
				Profiles->GetLivingHamsterCount(), CRMeta::GetGraveyard(*Profile).Num(), Profiles->GetUnseenGraveCount(), Profiles->CanRecruit() ? TEXT("yes") : TEXT("no"),
				Profile->LastRun.bValid ? 1 : 0, *UEnum::GetValueAsString(Profile->LastRun.Reason), *Profile->LastRun.HamsterName,
				*CRMeta::FormatResources(Profile->LastRun.Delivered), *CRMeta::FormatResources(Profile->LastRun.Carried));
			if (const UCRRunSubsystem* Run = DevRun(World); Run && Run->HasRun())
			{
				const FCRRunState& State = Run->GetRunState();
				UE_LOG(LogCRDev, Log, TEXT("  run %s: status %s, node %s, hamster %s HP %d/%d, carried S%d F%d W%d, end reported %d"), *State.RunId,
					*UEnum::GetValueAsString(State.Status), *State.CurrentNodeId.ToString(), *State.Hamster.Name, State.Hamster.CurrentHP, State.Hamster.MaxHP,
					State.Carried.Silver, State.Carried.Food, State.Carried.Wood, State.bEndReported ? 1 : 0);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GCRDevNewProfile(TEXT("CR.Dev.NewProfile"), TEXT("Creates and selects a profile, then opens the sanctuary."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UCRProfileSubsystem* Profiles = DevProfiles(World);
			const FString Name = DevJoinArgs(Args);
			if (!Profiles || Name.IsEmpty())
			{
				UE_LOG(LogCRDev, Warning, TEXT("CR.Dev.NewProfile <name>"));
				return;
			}
			UE_LOG(LogCRDev, Log, TEXT("CR.Dev.NewProfile: %s"), *Profiles->CreateProfile(Name));
			UGameplayStatics::OpenLevel(World, FName(CRMeta::HubMapPath()));
		}));

	FAutoConsoleCommandWithWorldAndArgs GCRDevStartRun(TEXT("CR.Dev.StartRun"), TEXT("Starts a run for the active profile (as В ПОХОД)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			UCRProfileSubsystem* Profiles = DevProfiles(World);
			if (!Profiles || !Profiles->StartRunFromHub())
			{
				UE_LOG(LogCRDev, Warning, TEXT("CR.Dev.StartRun: refused (no living hamster?)"));
				return;
			}
			UGameplayStatics::OpenLevel(World, FName(CRRun::RunMapPath()));
		}));

	FAutoConsoleCommandWithWorldAndArgs GCRDevEnter(TEXT("CR.Dev.Enter"), TEXT("Enters the first open room of a type: Combat, Event or Shop."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UCRRunSubsystem* Run = DevRun(World);
			const FString Type = Args.Num() > 0 ? Args[0] : FString();
			const ECRRoomType RoomType = Type.Equals(TEXT("Event"), ESearchCase::IgnoreCase) ? ECRRoomType::Event
				: (Type.Equals(TEXT("Shop"), ESearchCase::IgnoreCase) ? ECRRoomType::Shop : ECRRoomType::Combat);
			const FCRRunNodeData* Target = Run ? Run->GetRunState().Nodes.FindByPredicate([RoomType](const FCRRunNodeData& N)
			{
				return N.RoomType == RoomType && N.State == ECRRunNodeState::Available;
			}) : nullptr;
			if (!Target || !Run->EnterNode(Target->NodeId))
			{
				UE_LOG(LogCRDev, Warning, TEXT("CR.Dev.Enter: no open %s room"), *CRRun::RoomTypeName(RoomType));
				return;
			}
			UE_LOG(LogCRDev, Log, TEXT("CR.Dev.Enter: %s"), *Target->NodeId.ToString());
			const TCHAR* Map = RoomType == ECRRoomType::Event ? CRRun::EventMapPath() : (RoomType == ECRRoomType::Shop ? CRRun::ShopMapPath() : CRRun::CombatMapPath());
			UGameplayStatics::OpenLevel(World, FName(Map));
		}));

	FAutoConsoleCommandWithWorldAndArgs GCRDevLoseCombat(TEXT("CR.Dev.LoseCombat"), TEXT("The combat hamster falls (normal defeat flow)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ACRCombatGameMode* Combat = World ? World->GetAuthGameMode<ACRCombatGameMode>() : nullptr)
			{
				Combat->DevForceDefeat();
			}
			else
			{
				UE_LOG(LogCRDev, Warning, TEXT("CR.Dev.LoseCombat: not in a combat room"));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GCRDevSetHP(TEXT("CR.Dev.SetHP"), TEXT("Sets the run hamster's current HP."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UCRRunSubsystem* Run = DevRun(World);
			if (!Run || !Run->IsRunActive() || Args.Num() == 0 || !Args[0].IsNumeric())
			{
				UE_LOG(LogCRDev, Warning, TEXT("CR.Dev.SetHP <n> (needs an active run)"));
				return;
			}
			Run->SetHamsterHP(FCString::Atoi(*Args[0]));
			UE_LOG(LogCRDev, Log, TEXT("CR.Dev.SetHP: %d/%d"), Run->GetRunState().Hamster.CurrentHP, Run->GetRunState().Hamster.MaxHP);
		}));

	FAutoConsoleCommandWithWorldAndArgs GCRDevChoose(TEXT("CR.Dev.Choose"), TEXT("Event room: takes choice n (1-based), as pressing key n."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ACREventGameMode* Event = World ? World->GetAuthGameMode<ACREventGameMode>() : nullptr;
			if (!Event || Args.Num() == 0 || !Args[0].IsNumeric())
			{
				UE_LOG(LogCRDev, Warning, TEXT("CR.Dev.Choose <n> (in an event room)"));
				return;
			}
			Event->ChooseOption(FCString::Atoi(*Args[0]) - 1);
		}));

	FAutoConsoleCommandWithWorldAndArgs GCRDevContinue(TEXT("CR.Dev.Continue"), TEXT("Event room: ПРОДОЛЖИТЬ on the result screen."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ACREventGameMode* Event = World ? World->GetAuthGameMode<ACREventGameMode>() : nullptr)
			{
				Event->Continue();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GCRDevHubView(TEXT("CR.Dev.HubView"), TEXT("Sanctuary: opens Graveyard, Recruitment or Sanctuary."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			if (ACRHubHUD* HUD = PC ? Cast<ACRHubHUD>(PC->GetHUD()) : nullptr)
			{
				HUD->DevOpenView(Args.Num() > 0 ? Args[0] : FString());
			}
			else
			{
				UE_LOG(LogCRDev, Warning, TEXT("CR.Dev.HubView: not in the sanctuary"));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GCRDevRecruit(TEXT("CR.Dev.Recruit"), TEXT("Recruits saved candidate n (1-based)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UCRProfileSubsystem* Profiles = DevProfiles(World);
			const UCRProfileSaveGame* Profile = Profiles ? Profiles->GetActiveProfile() : nullptr;
			const int32 Index = Args.Num() > 0 && Args[0].IsNumeric() ? FCString::Atoi(*Args[0]) - 1 : 0;
			if (!Profile || !Profile->RecruitCandidates.IsValidIndex(Index))
			{
				UE_LOG(LogCRDev, Warning, TEXT("CR.Dev.Recruit <n>: no such candidate"));
				return;
			}
			FString Message;
			const bool bOk = Profiles->RecruitCandidate(Profile->RecruitCandidates[Index].HamsterId, Message);
			UE_LOG(LogCRDev, Log, TEXT("CR.Dev.Recruit: %s (%s)"), bOk ? TEXT("ok") : TEXT("refused"), *Message);
		}));

	FAutoConsoleCommandWithWorldAndArgs GCRDevReturnToHub(TEXT("CR.Dev.ReturnToHub"), TEXT("Leaves a finished run for the sanctuary."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			UCRRunSubsystem* Run = DevRun(World);
			if (!Run || Run->IsRunActive())
			{
				UE_LOG(LogCRDev, Warning, TEXT("CR.Dev.ReturnToHub: the run is still active"));
				return;
			}
			Run->AbandonRun();
			UGameplayStatics::OpenLevel(World, FName(CRMeta::HubMapPath()));
		}));

	FAutoConsoleCommandWithWorldAndArgs GCRDevKillAll(TEXT("CR.Dev.KillAllLiving"), TEXT("Kills every living hamster of the active profile (disposable profiles only)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UCRProfileSubsystem* Profiles = DevProfiles(World);
			const int32 Killed = Profiles ? Profiles->DevKillAllLiving(DevJoinArgs(Args)) : 0;
			UE_LOG(LogCRDev, Log, TEXT("CR.Dev.KillAllLiving: %d killed (the argument must be the active profile's name)"), Killed);
		}));
}

#endif // !UE_BUILD_SHIPPING
