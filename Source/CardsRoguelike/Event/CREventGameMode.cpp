#include "CREventGameMode.h"

#include "../Run/CRRunSubsystem.h"
#include "CREventDefinition.h"
#include "CREventHUD.h"
#include "CREventPlayerController.h"
#include "CREventRoomActor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogCREvent, Log, All);

ACREventGameMode::ACREventGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ACREventPlayerController::StaticClass();
	HUDClass = ACREventHUD::StaticClass();
}

UCRRunSubsystem* ACREventGameMode::GetRun() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UCRRunSubsystem>() : nullptr;
}

void ACREventGameMode::StartPlay()
{
	Super::StartPlay();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Room = GetWorld()->SpawnActor<ACREventRoomActor>(ACREventRoomActor::StaticClass(), FTransform::Identity, Params);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetViewTarget(Room);
	}

	// Only a real, unresolved event room of an active run is interactive. A direct sandbox open never
	// touches run data (no event is drawn, nothing is created).
	UCRRunSubsystem* Run = GetRun();
	if (!Run || !Run->IsInEventRoom())
	{
		InvalidReason = TEXT("Нет активной комнаты события");
		UE_LOG(LogCREvent, Log, TEXT("Event sandbox opened without an active event room"));
		SetupPresentation();
		return;
	}

	EventNodeId = Run->GetRunState().CurrentNodeId;
	const FCREventNodeState* State = Run->EnsureEventState(EventNodeId);
	Event = State ? State->SelectedEvent.LoadSynchronous() : nullptr;
	if (!Event)
	{
		InvalidReason = TEXT("Данные события не найдены (проверьте пул событий узла)");
		UE_LOG(LogCREvent, Warning, TEXT("Event room %s: could not load an event definition"), *EventNodeId.ToString());
		SetupPresentation();
		return;
	}

	bValidEvent = true;
	SetupPresentation();
	UE_LOG(LogCREvent, Log, TEXT("Event room %s opened: %s (%d choices, %s)"), *EventNodeId.ToString(), *Event->EventId.ToString(),
		Event->Choices.Num(), State->bEffectsCommitted ? TEXT("result already committed") : TEXT("undecided"));
}

void ACREventGameMode::SetupPresentation()
{
	if (!Room)
	{
		return;
	}

	// A custom scene actor on the definition replaces the built-in graybox preset.
	if (Event && !Event->PresentationActorClass.IsNull())
	{
		if (UClass* CustomClass = Event->PresentationActorClass.LoadSynchronous())
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GetWorld()->SpawnActor<AActor>(CustomClass, Room->GetStageTransform(), Params);
			return;
		}
	}
	Room->BuildPresentation(Event ? Event->Presentation : ECREventPresentation::None);
}

void ACREventGameMode::RestartPlayer(AController* NewPlayer)
{
	// No pawn in the event room: the view is the room's fixed camera.
}

const FCREventNodeState* ACREventGameMode::GetEventState() const
{
	const UCRRunSubsystem* Run = GetRun();
	return (Run && bValidEvent) ? Run->FindEventState(EventNodeId) : nullptr;
}

ECREventScreen ACREventGameMode::GetScreen() const
{
	const FCREventNodeState* State = GetEventState();
	if (!bValidEvent || !State)
	{
		return ECREventScreen::Invalid;
	}
	if (State->bEffectsCommitted)
	{
		return ECREventScreen::Result;
	}
	return PendingChoice != INDEX_NONE ? ECREventScreen::CardSelect : ECREventScreen::Choices;
}

FString ACREventGameMode::GetChoiceBlockReason(int32 ChoiceIndex) const
{
	const UCRRunSubsystem* Run = GetRun();
	if (!Run || !Event || !Event->Choices.IsValidIndex(ChoiceIndex))
	{
		return TEXT("Недоступно");
	}
	return Run->GetEventChoiceBlockReason(Event->Choices[ChoiceIndex]);
}

void ACREventGameMode::ChooseOption(int32 ChoiceIndex)
{
	if (GetScreen() != ECREventScreen::Choices || !Event->Choices.IsValidIndex(ChoiceIndex))
	{
		return;
	}

	// Disabled choices stay visible but do nothing.
	const FString Reason = GetChoiceBlockReason(ChoiceIndex);
	if (!Reason.IsEmpty())
	{
		UE_LOG(LogCREvent, Log, TEXT("Choice %d is disabled"), ChoiceIndex + 1);
		ShowMessage(Reason);
		return;
	}

	// A sacrifice needs the card first; nothing is applied until it is picked.
	if (UCRRunSubsystem::ChoiceNeedsCardSelection(Event->Choices[ChoiceIndex]))
	{
		PendingChoice = ChoiceIndex;
		return;
	}

	UCRRunSubsystem* Run = GetRun();
	if (!Run || !Run->CommitEventChoice(EventNodeId, Event, ChoiceIndex, INDEX_NONE))
	{
		UE_LOG(LogCREvent, Warning, TEXT("Choice %d could not be committed"), ChoiceIndex + 1);
		ShowMessage(TEXT("Сейчас этот выбор невозможен"));
	}
}

void ACREventGameMode::SelectSacrifice(int32 DeckIndex)
{
	if (GetScreen() != ECREventScreen::CardSelect)
	{
		return;
	}

	UCRRunSubsystem* Run = GetRun();
	if (Run && Run->CommitEventChoice(EventNodeId, Event, PendingChoice, DeckIndex))
	{
		PendingChoice = INDEX_NONE;
	}
	else
	{
		UE_LOG(LogCREvent, Warning, TEXT("Sacrifice of deck card %d could not be committed"), DeckIndex);
		ShowMessage(TEXT("Эту карту нельзя пожертвовать"));
	}
}

void ACREventGameMode::CancelCardSelection()
{
	if (GetScreen() == ECREventScreen::CardSelect)
	{
		PendingChoice = INDEX_NONE;
	}
}

void ACREventGameMode::Continue()
{
	// One commit only: completes the room once, then returns to the run map.
	if (GetScreen() != ECREventScreen::Result || bContinuing)
	{
		return;
	}

	UCRRunSubsystem* Run = GetRun();
	if (!Run)
	{
		return;
	}
	if (Run->IsRunFailed())
	{
		// The choice killed the hamster: the run is over, the room is never completed. Show the run map's
		// failure state (which leads back to the sanctuary).
		bContinuing = true;
		UE_LOG(LogCREvent, Log, TEXT("Event %s was lethal; returning to the failed run map"), *EventNodeId.ToString());
		UGameplayStatics::OpenLevel(this, FName(CRRun::RunMapPath()));
		return;
	}
	if (!Run->ContinueFromEvent(EventNodeId))
	{
		return;
	}
	bContinuing = true;
	UE_LOG(LogCREvent, Log, TEXT("Continue from event %s"), *EventNodeId.ToString());
	UGameplayStatics::OpenLevel(this, FName(CRRun::RunMapPath()));
}

const TArray<FName>& ACREventGameMode::GetDeck() const
{
	static const TArray<FName> Empty;
	const UCRRunSubsystem* Run = GetRun();
	return Run ? Run->GetRunState().DeckCardIds : Empty;
}

bool ACREventGameMode::IsHamsterDead() const
{
	const UCRRunSubsystem* Run = GetRun();
	return bValidEvent && Run && Run->IsRunFailed();
}

FString ACREventGameMode::GetHamsterName() const
{
	const UCRRunSubsystem* Run = GetRun();
	return Run ? Run->GetRunState().Hamster.Name : FString();
}

int32 ACREventGameMode::GetHP() const
{
	const UCRRunSubsystem* Run = GetRun();
	return Run ? Run->GetRunState().Hamster.CurrentHP : 0;
}

int32 ACREventGameMode::GetMaxHP() const
{
	const UCRRunSubsystem* Run = GetRun();
	return Run ? Run->GetRunState().Hamster.MaxHP : 0;
}

int32 ACREventGameMode::GetSilver() const
{
	const UCRRunSubsystem* Run = GetRun();
	return Run ? Run->GetRunState().Carried.Silver : 0;
}

int32 ACREventGameMode::GetFood() const
{
	const UCRRunSubsystem* Run = GetRun();
	return Run ? Run->GetRunState().Carried.Food : 0;
}

int32 ACREventGameMode::GetWood() const
{
	const UCRRunSubsystem* Run = GetRun();
	return Run ? Run->GetRunState().Carried.Wood : 0;
}

void ACREventGameMode::ShowMessage(const FString& InMessage)
{
	// Player-facing (Russian) toast; callers log their own English line.
	Message = InMessage;
	MessageTime = GetWorld()->GetRealTimeSeconds();
}

FString ACREventGameMode::GetActiveMessage() const
{
	return GetWorld()->GetRealTimeSeconds() - MessageTime < 2.5 ? Message : FString();
}
