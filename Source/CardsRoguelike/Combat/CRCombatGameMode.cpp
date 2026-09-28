#include "CRCombatGameMode.h"

#include "CRArena.h"
#include "CRCardLibrary.h"
#include "CRBarrel.h"
#include "CRDebugHUD.h"
#include "CREnemy.h"
#include "CRHamster.h"
#include "CRPit.h"
#include "CRPlayerController.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "../Run/CRRunSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogCRCombat, Log, All);

ACRCombatGameMode::ACRCombatGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ACRPlayerController::StaticClass();
	HUDClass = ACRDebugHUD::StaticClass();

	// Shared prototype catalog (also used by rewards and the shop).
	Cards = CRCardLibrary::GetPrototypeCards();

	MeleeStats.CombatType = ECRCombatType::Melee;
	MeleeStats.MaxHP = 8;
	MeleeStats.Damage = 4;
	MeleeStats.Speed = 600.f;
	MeleeStats.MassKg = 90.f;

	RangedStats.CombatType = ECRCombatType::Ranged;
	RangedStats.MaxHP = 6;
	RangedStats.Damage = 3;
	RangedStats.Speed = 500.f;
	RangedStats.MassKg = 60.f;

	auto MakeSpawn = [](ECRCombatType Type, float X, float Y)
	{
		FCREnemySpawn Spawn;
		Spawn.CombatType = Type;
		Spawn.Location = FVector2D(X, Y);
		return Spawn;
	};

	// Melee start in the outer zone (must walk in); one ranged starts inside the inner zone (must walk out).
	EnemySpawns = {
		MakeSpawn(ECRCombatType::Melee,  -900.f, -500.f),
		MakeSpawn(ECRCombatType::Melee,   200.f,  800.f),
		MakeSpawn(ECRCombatType::Ranged, -200.f,  150.f),
		MakeSpawn(ECRCombatType::Ranged,  900.f, -550.f),
	};

	BarrelLocations = { FVector2D(-600.f, -520.f), FVector2D(250.f, -600.f) };
}

void ACRCombatGameMode::StartPlay()
{
	Super::StartPlay();
	SpawnCombatScene();
	ApplyRunState();
	StartPlayerTurn();
}

void ACRCombatGameMode::ApplyRunState()
{
	// Integrated only when an active run stands in an unresolved Combat room. Opening this map
	// directly (no run) keeps the standalone prototype setup untouched.
	// The full prototype card set doubles as the reward pool.
	CardCatalog = Cards;

	UCRRunSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCRRunSubsystem>() : nullptr;
	bRunIntegrated = Run && Run->IsInCombatRoom();
	if (!bRunIntegrated)
	{
		LogEvent(TEXT("Standalone combat test"));
		return;
	}

	const FCRRunState& State = Run->GetRunState();
	ManaPerTurn = State.Hamster.ManaPerTurn;
	if (Hamster)
	{
		Hamster->InitHealth(State.Hamster.CurrentHP, State.Hamster.MaxHP);
	}

	// Each deck id becomes one hand card (duplicates allowed): Push -> PUSH definition, ...
	Cards.Reset();
	for (const FName CardId : State.DeckCardIds)
	{
		if (const FCRCardDef* Found = FindCardDef(CardId))
		{
			Cards.Add(*Found);
		}
		else
		{
			UE_LOG(LogCRCombat, Warning, TEXT("Unknown deck card id '%s' - skipped"), *CardId.ToString());
		}
	}

	LogEvent(FString::Printf(TEXT("Run combat in %s: %s HP %d/%d, mana %d, %d cards"), *State.CurrentNodeId.ToString(),
		*State.Hamster.Name, State.Hamster.CurrentHP, State.Hamster.MaxHP, ManaPerTurn, Cards.Num()));
}

void ACRCombatGameMode::OnCombatResolved(bool bVictory)
{
	if (!bRunIntegrated)
	{
		return;
	}

	UCRRunSubsystem* Run = GetGameInstance()->GetSubsystem<UCRRunSubsystem>();
	if (!Run)
	{
		return;
	}

	if (bVictory)
	{
		// HP is written back now. The room stays unresolved (next rooms Locked) until the reward is committed.
		Run->SetHamsterHP(Hamster ? Hamster->GetHP() : 0);
		GetWorldTimerManager().SetTimer(RewardTimer, this, &ACRCombatGameMode::EnterRewardState, FMath::Max(RewardDelay, 0.01f), false);
		return;
	}

	Run->SetHamsterHP(0);
	Run->FailCurrentRun();
	GetWorldTimerManager().SetTimer(ReturnToRunTimer, this, &ACRCombatGameMode::ReturnToRunMap, FMath::Max(RunReturnDelay, 0.01f), false);
}

void ACRCombatGameMode::ReturnToRunMap()
{
	UGameplayStatics::OpenLevel(this, FName(CRRun::RunMapPath()));
}

const FCRCardDef* ACRCombatGameMode::FindCardDef(FName CardId) const
{
	const TArray<FCRCardDef>& Source = CardCatalog.Num() > 0 ? CardCatalog : Cards;
	return Source.FindByPredicate([CardId](const FCRCardDef& Card)
	{
		return Card.Id == CardId || Card.Name.Equals(CardId.ToString(), ESearchCase::IgnoreCase);
	});
}

void ACRCombatGameMode::EnterRewardState()
{
	if (TurnState != ECRTurnState::Victory || !bRunIntegrated)
	{
		return;
	}

	GrantCombatResources();

	// Offers: distinct cards from the prototype catalog, simple uniform randomness.
	TArray<FName> Pool;
	for (const FCRCardDef& Card : CardCatalog)
	{
		Pool.AddUnique(Card.Id);
	}
	RewardOffers.Reset();
	while (RewardOffers.Num() < RewardOfferCount && Pool.Num() > 0)
	{
		const int32 Pick = FMath::RandRange(0, Pool.Num() - 1);
		RewardOffers.Add(Pool[Pick]);
		Pool.RemoveAt(Pick);
	}

	TurnState = ECRTurnState::Reward;
	FString OfferText;
	for (const FName Offer : RewardOffers)
	{
		OfferText += (OfferText.IsEmpty() ? TEXT("") : TEXT(", ")) + Offer.ToString();
	}
	LogEvent(FString::Printf(TEXT("Reward offers: %s"), *OfferText));
}

void ACRCombatGameMode::GrantCombatResources()
{
	if (bResourcesGranted || !bRunIntegrated)
	{
		return;
	}
	bResourcesGranted = true;
	if (UCRRunSubsystem* Run = GetGameInstance()->GetSubsystem<UCRRunSubsystem>())
	{
		Run->AddCarriedResources(RewardSilver, RewardFood, RewardWood);
		LogEvent(FString::Printf(TEXT("Reward: +%d Silver, +%d Food, +%d Wood"), RewardSilver, RewardFood, RewardWood));
	}
}

void ACRCombatGameMode::ChooseReward(int32 OfferIndex)
{
	if (TurnState == ECRTurnState::Reward && RewardOffers.IsValidIndex(OfferIndex))
	{
		CommitReward(RewardOffers[OfferIndex]);
	}
}

void ACRCombatGameMode::SkipReward()
{
	if (TurnState == ECRTurnState::Reward)
	{
		CommitReward(NAME_None);
	}
}

void ACRCombatGameMode::CommitReward(FName CardId)
{
	// One commit per combat: repeated clicks or keys after this are ignored.
	if (bRewardCommitted || !bRunIntegrated)
	{
		return;
	}
	bRewardCommitted = true;
	ChosenReward = CardId;

	UCRRunSubsystem* Run = GetGameInstance()->GetSubsystem<UCRRunSubsystem>();
	if (!Run)
	{
		return;
	}
	if (!CardId.IsNone())
	{
		Run->AddCardToDeck(CardId);
	}
	GrantCombatResources();
	Run->CompleteCurrentRoom();
	LogEvent(CardId.IsNone() ? FString(TEXT("Reward skipped")) : FString::Printf(TEXT("Reward chosen: %s"), *CardId.ToString()));

	GetWorldTimerManager().SetTimer(ReturnToRunTimer, this, &ACRCombatGameMode::ReturnToRunMap, FMath::Max(RunReturnDelay, 0.01f), false);
}

void ACRCombatGameMode::RestartPlayer(AController* NewPlayer)
{
	// The hamster is spawned and possessed by SpawnCombatScene; no default pawn.
}

void ACRCombatGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	if (Hamster && NewPlayer)
	{
		NewPlayer->Possess(Hamster);
	}
}

void ACRCombatGameMode::SpawnCombatScene()
{
	UWorld* World = GetWorld();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Reuse an arena placed in the level so polygons can be tweaked per map.
	for (TActorIterator<ACRArena> It(World); It; ++It)
	{
		Arena = *It;
		break;
	}
	if (!Arena)
	{
		Arena = World->SpawnActor<ACRArena>(ACRArena::StaticClass(), FTransform::Identity, Params);
	}

	const FVector ArenaOrigin = Arena->GetActorLocation();
	const FVector2D Center = Arena->GetCenter();

	Hamster = World->SpawnActor<ACRHamster>(ACRHamster::StaticClass(), FTransform(FVector(Center, 60.f)), Params);
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		PC->Possess(Hamster);
	}

	int32 MeleeCount = 0;
	int32 RangedCount = 0;
	for (const FCREnemySpawn& Spawn : EnemySpawns)
	{
		const bool bMelee = Spawn.CombatType == ECRCombatType::Melee;
		const FVector Location(FVector2D(ArenaOrigin) + Spawn.Location, bMelee ? 55.f : 65.f);
		ACREnemy* Enemy = World->SpawnActorDeferred<ACREnemy>(ACREnemy::StaticClass(), FTransform(Location), nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Enemy)
		{
			continue;
		}
		const FString Name = bMelee ? FString::Printf(TEXT("M%d"), ++MeleeCount) : FString::Printf(TEXT("R%d"), ++RangedCount);
		Enemy->InitEnemy(bMelee ? MeleeStats : RangedStats, Name);
		Enemy->FinishSpawning(FTransform(Location));
		Enemies.Add(Enemy);
	}

	for (const FVector2D& BarrelLocation : BarrelLocations)
	{
		const FVector Location(FVector2D(ArenaOrigin) + BarrelLocation, 55.f);
		if (ACRBarrel* Barrel = World->SpawnActor<ACRBarrel>(ACRBarrel::StaticClass(), FTransform(Location), Params))
		{
			Barrels.Add(Barrel);
		}
	}

	Pit = World->SpawnActor<ACRPit>(ACRPit::StaticClass(), FTransform(FVector(FVector2D(ArenaOrigin) + PitLocation, 0.f)), Params);
	if (Pit)
	{
		Pit->SetPitSize(PitSize);
	}

	LogEvent(FString::Printf(TEXT("Combat start: %d enemies, %d barrels"), Enemies.Num(), Barrels.Num()));
}

void ACRCombatGameMode::StartPlayerTurn()
{
	TurnState = ECRTurnState::PlayerTurn;
	++TurnNumber;
	Mana = ManaPerTurn;
	ClearTargeting();
	RefreshIntents();
	LogEvent(FString::Printf(TEXT("--- Player turn %d (mana %d) ---"), TurnNumber, Mana));
}

void ACRCombatGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Arena)
	{
		return;
	}

	EnforceArenaBounds();

	// Resolution and enemy-turn timers advance with simulated physics time, so a throttled
	// editor slows them down together with the bodies they are waiting on.
	const float SimDelta = CRProto::GetSimulatedDeltaSeconds(DeltaSeconds);

	switch (TurnState)
	{
	case ECRTurnState::PlayerTurn:
		RefreshIntents();
		break;

	case ECRTurnState::ResolvingCard:
		ResolveElapsed += SimDelta;
		if (UpdateSettle(SimDelta) && !CheckCombatEnd())
		{
			TurnState = ECRTurnState::PlayerTurn;
			RefreshIntents();
		}
		break;

	case ECRTurnState::EnemyTurn:
		TickEnemyTurn(SimDelta);
		break;

	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// Player commands

void ACRCombatGameMode::SelectCard(int32 Index)
{
	if (TurnState != ECRTurnState::PlayerTurn || !Cards.IsValidIndex(Index))
	{
		return;
	}

	const FCRCardDef& Card = Cards[Index];
	if (Card.ManaCost > Mana)
	{
		ShowMessage(FString::Printf(TEXT("Not enough mana for %s"), *Card.Name));
		return;
	}

	SelectedCard = Index;
	PendingTarget.Reset();

	if (Card.Targeting == ECRCardTargeting::None)
	{
		PlayCard(Index, nullptr, FVector::ZeroVector);
	}
}

void ACRCombatGameMode::CancelTargeting()
{
	if (SelectedCard != INDEX_NONE)
	{
		LogEvent(TEXT("Targeting cancelled"));
	}
	ClearTargeting();
}

void ACRCombatGameMode::ClearTargeting()
{
	SelectedCard = INDEX_NONE;
	PendingTarget.Reset();
}

void ACRCombatGameMode::HandleClick(AActor* HitActor, const FVector& WorldLocation)
{
	if (TurnState != ECRTurnState::PlayerTurn || !Cards.IsValidIndex(SelectedCard))
	{
		return;
	}

	const FCRCardDef& Card = Cards[SelectedCard];
	switch (Card.Targeting)
	{
	case ECRCardTargeting::PhysicsTarget:
		if (IsPhysicsTarget(HitActor))
		{
			PlayCard(SelectedCard, HitActor, WorldLocation);
		}
		else
		{
			ShowMessage(TEXT("Click an enemy or barrel"));
		}
		break;

	case ECRCardTargeting::PhysicsTargetThenPoint:
		if (!PendingTarget.IsValid())
		{
			if (IsPhysicsTarget(HitActor))
			{
				PendingTarget = HitActor;
			}
			else
			{
				ShowMessage(TEXT("Click an enemy or barrel first"));
			}
		}
		else
		{
			PlayCard(SelectedCard, PendingTarget.Get(), WorldLocation);
		}
		break;

	case ECRCardTargeting::GroundPoint:
		PlayCard(SelectedCard, nullptr, WorldLocation);
		break;

	default:
		break;
	}
}

void ACRCombatGameMode::RequestEndTurn()
{
	if (TurnState != ECRTurnState::PlayerTurn)
	{
		return;
	}

	ClearTargeting();
	RefreshIntents();
	TurnState = ECRTurnState::EnemyTurn;
	EnemyQueue.Reset();
	for (ACREnemy* Enemy : Enemies)
	{
		EnemyQueue.Add(Enemy);
	}
	EnemyIndex = 0;
	bEnemyActing = false;
	bEnemySettling = false;
	EnemyPhaseTimer = 0.f;
	LogEvent(TEXT("--- Enemy turn ---"));
}

void ACRCombatGameMode::CycleBoundaryType(int32 EdgeIndex)
{
	if (Arena && EdgeIndex < Arena->GetNumEdges() && TurnState == ECRTurnState::PlayerTurn)
	{
		Arena->CycleEdgeType(EdgeIndex);
		LogEvent(FString::Printf(TEXT("Edge %d is now %s"), EdgeIndex + 1, *CRProto::BoundaryTypeName(Arena->GetEdgeType(EdgeIndex))));
	}
}

// ---------------------------------------------------------------------------
// Cards

void ACRCombatGameMode::PlayCard(int32 Index, AActor* Target, const FVector& Point)
{
	const FCRCardDef Card = Cards[Index];
	UPrimitiveComponent* TargetBody = GetPhysicsBody(Target);

	switch (Card.Effect)
	{
	case ECRCardEffect::Push:
	{
		if (!TargetBody)
		{
			return;
		}
		const FVector2D Dir = FVector2D(Point) - FVector2D(Target->GetActorLocation());
		if (Dir.Size() < 30.f)
		{
			ShowMessage(TEXT("Aim farther from the target"));
			return;
		}
		Mana -= Card.ManaCost;
		TargetBody->AddImpulse(FVector(Dir.GetSafeNormal(), 0.f) * Card.Strength, NAME_None, true);
		LogEvent(FString::Printf(TEXT("PUSH %s"), *DescribeActor(Target)));
		break;
	}

	case ECRCardEffect::Pull:
	{
		if (!TargetBody)
		{
			return;
		}
		const FVector2D Dir = (Arena->GetCenter() - FVector2D(Target->GetActorLocation())).GetSafeNormal();
		Mana -= Card.ManaCost;
		TargetBody->AddImpulse(FVector(Dir, 0.f) * Card.Strength, NAME_None, true);
		LogEvent(FString::Printf(TEXT("PULL %s"), *DescribeActor(Target)));
		break;
	}

	case ECRCardEffect::Blast:
		Mana -= Card.ManaCost;
		ApplyRadialBlast(FVector(FVector2D(Point), 0.f), Card.Radius, Card.Damage, Card.Strength, bCardBlastHurtsHamster, TEXT("BLAST"));
		break;

	case ECRCardEffect::Guard:
		Mana -= Card.ManaCost;
		Hamster->AddArmor(Card.Amount);
		LogEvent(FString::Printf(TEXT("GUARD: +%d armor"), Card.Amount));
		break;

	case ECRCardEffect::Mend:
	{
		Mana -= Card.ManaCost;
		const int32 Healed = Hamster->Heal(Card.Amount);
		LogEvent(FString::Printf(TEXT("MEND: +%d HP"), Healed));
		break;
	}
	}

	ClearTargeting();

	if (Card.bIsPhysical)
	{
		BeginResolving();
	}
	else
	{
		CheckCombatEnd();
	}
}

bool ACRCombatGameMode::IsPhysicsTarget(const AActor* Actor) const
{
	if (const ACREnemy* Enemy = Cast<ACREnemy>(Actor))
	{
		return !Enemy->IsEliminated();
	}
	if (const ACRBarrel* Barrel = Cast<ACRBarrel>(Actor))
	{
		return !Barrel->IsGone();
	}
	return false;
}

FString ACRCombatGameMode::DescribeActor(const AActor* Actor) const
{
	if (const ACREnemy* Enemy = Cast<ACREnemy>(Actor))
	{
		return Enemy->GetDisplayName();
	}
	if (Cast<ACRBarrel>(Actor))
	{
		return TEXT("Barrel");
	}
	return Actor ? Actor->GetActorNameOrLabel() : TEXT("nothing");
}

UPrimitiveComponent* ACRCombatGameMode::GetPhysicsBody(AActor* Actor) const
{
	if (ACREnemy* Enemy = Cast<ACREnemy>(Actor))
	{
		return Enemy->GetBody();
	}
	if (ACRBarrel* Barrel = Cast<ACRBarrel>(Actor))
	{
		return Barrel->GetBody();
	}
	return nullptr;
}

void ACRCombatGameMode::ApplyRadialBlast(const FVector& Origin, float Radius, int32 Damage, float Impulse, bool bDamageHamster, const FString& Source)
{
	UWorld* World = GetWorld();
	DrawDebugSphere(World, Origin, Radius, 24, FColor(255, 140, 0), false, 1.0f, 0, 4.f);
	LogEvent(FString::Printf(TEXT("%s (radius %.0f, dmg %d)"), *Source, Radius, Damage));

	// Impulse first so bodies destroyed by the damage below are not touched afterwards.
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius));

	TSet<UPrimitiveComponent*> Pushed;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		UPrimitiveComponent* Component = Overlap.GetComponent();
		if (Component && Component->IsSimulatingPhysics() && !Pushed.Contains(Component))
		{
			Pushed.Add(Component);
			Component->AddRadialImpulse(Origin, Radius, Impulse, ERadialImpulseFalloff::RIF_Linear, true);
		}
	}

	const FVector2D Origin2D(Origin);
	const TArray<TObjectPtr<ACREnemy>> EnemiesCopy = Enemies;
	for (ACREnemy* Enemy : EnemiesCopy)
	{
		if (IsValid(Enemy) && FVector2D::Distance(FVector2D(Enemy->GetActorLocation()), Origin2D) <= Radius + Enemy->BodyRadius)
		{
			Enemy->ApplyCombatDamage(Damage, Source);
		}
	}

	const TArray<TObjectPtr<ACRBarrel>> BarrelsCopy = Barrels;
	for (ACRBarrel* Barrel : BarrelsCopy)
	{
		if (IsValid(Barrel) && FVector2D::Distance(FVector2D(Barrel->GetActorLocation()), Origin2D) <= Radius)
		{
			Barrel->ReceiveDamage(Damage);
		}
	}

	if (bDamageHamster && Hamster &&
		FVector2D::Distance(FVector2D(Hamster->GetActorLocation()), Origin2D) <= Radius + Hamster->GetBodyRadius())
	{
		Hamster->ApplyCombatDamage(Damage, Source);
	}
}

// ---------------------------------------------------------------------------
// Resolution

void ACRCombatGameMode::BeginResolving()
{
	TurnState = ECRTurnState::ResolvingCard;
	ResolveElapsed = 0.f;
	SettledTime = 0.f;
}

bool ACRCombatGameMode::UpdateSettle(float SimDeltaSeconds)
{
	if (ResolveElapsed >= ResolveTimeout)
	{
		LogEvent(TEXT("Physics resolve timed out"));
		return true;
	}

	float MaxSpeed = 0.f;
	for (ACREnemy* Enemy : Enemies)
	{
		if (IsValid(Enemy))
		{
			MaxSpeed = FMath::Max(MaxSpeed, Enemy->GetBody()->GetPhysicsLinearVelocity().Size());
		}
	}
	for (ACRBarrel* Barrel : Barrels)
	{
		if (IsValid(Barrel))
		{
			if (Barrel->IsExplosionPending())
			{
				SettledTime = 0.f;
				return false;
			}
			MaxSpeed = FMath::Max(MaxSpeed, Barrel->GetBody()->GetPhysicsLinearVelocity().Size());
		}
	}

	SettledTime = MaxSpeed < SettleSpeed ? SettledTime + SimDeltaSeconds : 0.f;
	return ResolveElapsed >= MinResolveTime && SettledTime >= SettleHoldTime;
}

void ACRCombatGameMode::TickEnemyTurn(float SimDeltaSeconds)
{
	EnemyPhaseTimer += SimDeltaSeconds;

	if (bEnemySettling)
	{
		ResolveElapsed += SimDeltaSeconds;
		if (UpdateSettle(SimDeltaSeconds) && !CheckCombatEnd())
		{
			StartPlayerTurn();
		}
		return;
	}

	if (EnemyIndex >= EnemyQueue.Num())
	{
		bEnemySettling = true;
		ResolveElapsed = 0.f;
		SettledTime = 0.f;
		return;
	}

	ACREnemy* Enemy = EnemyQueue[EnemyIndex].Get();
	if (!IsValid(Enemy) || Enemy->IsEliminated())
	{
		++EnemyIndex;
		bEnemyActing = false;
		return;
	}

	if (!bEnemyActing)
	{
		bEnemyActing = true;
		EnemyPhaseTimer = 0.f;
		Enemy->StartAction();
		if (CheckCombatEnd())
		{
			return;
		}
		return;
	}

	if ((Enemy->IsActionFinished() && EnemyPhaseTimer >= EnemyActionGap) || EnemyPhaseTimer >= EnemyActionTimeout)
	{
		Enemy->StopAction();
		++EnemyIndex;
		bEnemyActing = false;
		CheckCombatEnd();
	}
}

// ---------------------------------------------------------------------------
// Arena rules

void ACRCombatGameMode::EnforceArenaBounds()
{
	TArray<AActor*> Bodies;
	for (ACREnemy* Enemy : Enemies)
	{
		if (IsValid(Enemy))
		{
			Bodies.Add(Enemy);
		}
	}
	for (ACRBarrel* Barrel : Barrels)
	{
		if (IsValid(Barrel))
		{
			Bodies.Add(Barrel);
		}
	}

	for (AActor* Actor : Bodies)
	{
		const FVector Location = Actor->GetActorLocation();
		if (Location.Z < KillZ)
		{
			if (ACREnemy* Enemy = Cast<ACREnemy>(Actor))
			{
				Enemy->Eliminate(ECREliminationReason::Fell);
			}
			else if (ACRBarrel* Barrel = Cast<ACRBarrel>(Actor))
			{
				Barrel->RemoveSilently();
			}
			continue;
		}

		const FVector2D P(Location);
		if (Arena->IsInsideOuter(P))
		{
			continue;
		}

		// Crossed the outer polygon (fast body or non-blocking edge): apply that edge's rule.
		const int32 Edge = Arena->FindNearestOuterEdge(P);
		const ECRBoundaryType Type = Arena->GetEdgeType(Edge);
		if (Type == ECRBoundaryType::Void || Type == ECRBoundaryType::TeleportInner)
		{
			HandleBoundaryContact(Actor, Type);
		}
		else
		{
			// Tunnelled through a solid wall: put it back inside and kill outward velocity.
			const FVector2D Inside = Arena->ClampInsideEdge(P, Edge, 80.f);
			Actor->SetActorLocation(FVector(Inside, Location.Z), false, nullptr, ETeleportType::TeleportPhysics);
			if (UPrimitiveComponent* Body = GetPhysicsBody(Actor))
			{
				Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
			}
		}
	}
}

void ACRCombatGameMode::HandleBoundaryContact(AActor* Actor, ECRBoundaryType Type)
{
	if (Type == ECRBoundaryType::Void)
	{
		if (ACREnemy* Enemy = Cast<ACREnemy>(Actor))
		{
			Enemy->Eliminate(ECREliminationReason::Void);
		}
		else if (ACRBarrel* Barrel = Cast<ACRBarrel>(Actor))
		{
			Barrel->RemoveSilently();
		}
	}
	else if (Type == ECRBoundaryType::TeleportInner && IsPhysicsTarget(Actor))
	{
		TeleportToInner(Actor);
	}
}

void ACRCombatGameMode::TeleportToInner(AActor* Actor)
{
	TArray<FVector2D> Occupied;
	for (ACREnemy* Enemy : Enemies)
	{
		if (IsValid(Enemy) && Enemy != Actor)
		{
			Occupied.Add(FVector2D(Enemy->GetActorLocation()));
		}
	}
	for (ACRBarrel* Barrel : Barrels)
	{
		if (IsValid(Barrel) && Barrel != Actor)
		{
			Occupied.Add(FVector2D(Barrel->GetActorLocation()));
		}
	}

	const FVector2D Target = Arena->FindSafeInnerPoint(Occupied, 140.f);
	const float Z = Actor->GetActorLocation().Z;
	Actor->SetActorLocation(FVector(Target, FMath::Max(Z, 60.f)), false, nullptr, ETeleportType::TeleportPhysics);
	if (UPrimitiveComponent* Body = GetPhysicsBody(Actor))
	{
		Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
	}
	LogEvent(FString::Printf(TEXT("%s teleported into the inner zone"), *DescribeActor(Actor)));
}

void ACRCombatGameMode::RefreshIntents()
{
	for (ACREnemy* Enemy : Enemies)
	{
		if (IsValid(Enemy))
		{
			Enemy->UpdateIntent();
		}
	}
}

void ACRCombatGameMode::OnEnemyEliminated(ACREnemy* Enemy, ECREliminationReason Reason)
{
	Enemies.Remove(Enemy);
	if (PendingTarget.Get() == Enemy)
	{
		PendingTarget.Reset();
	}
	LogEvent(FString::Printf(TEXT("%s %s"), *Enemy->GetDisplayName(), *CRProto::EliminationReasonName(Reason)));
}

void ACRCombatGameMode::OnBarrelRemoved(ACRBarrel* Barrel)
{
	Barrels.Remove(Barrel);
	if (PendingTarget.Get() == Barrel)
	{
		PendingTarget.Reset();
	}
}

bool ACRCombatGameMode::CheckCombatEnd()
{
	if (TurnState == ECRTurnState::Victory || TurnState == ECRTurnState::Defeat || TurnState == ECRTurnState::Reward)
	{
		return true;
	}

	if (Hamster && Hamster->IsDead())
	{
		TurnState = ECRTurnState::Defeat;
		ClearTargeting();
		LogEvent(TEXT("DEFEAT: the hamster has fallen"));
		OnCombatResolved(false);
		return true;
	}
	if (Enemies.Num() == 0)
	{
		TurnState = ECRTurnState::Victory;
		ClearTargeting();
		LogEvent(TEXT("VICTORY: all enemies eliminated"));
		OnCombatResolved(true);
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// HUD helpers

FString ACRCombatGameMode::GetTurnStateName() const
{
	switch (TurnState)
	{
	case ECRTurnState::PlayerTurn:    return TEXT("PLAYER TURN");
	case ECRTurnState::ResolvingCard: return TEXT("RESOLVING CARD");
	case ECRTurnState::EnemyTurn:     return TEXT("ENEMY TURN");
	case ECRTurnState::Victory:       return TEXT("VICTORY");
	case ECRTurnState::Defeat:        return TEXT("DEFEAT");
	case ECRTurnState::Reward:        return TEXT("REWARD");
	}
	return TEXT("?");
}

FString ACRCombatGameMode::GetTargetingPrompt() const
{
	if (TurnState != ECRTurnState::PlayerTurn)
	{
		return FString();
	}
	if (!Cards.IsValidIndex(SelectedCard))
	{
		return TEXT("Drag a card to a target");
	}

	const FCRCardDef& Card = Cards[SelectedCard];
	switch (Card.Targeting)
	{
	case ECRCardTargeting::PhysicsTarget:
		return FString::Printf(TEXT("%s: click an enemy or barrel"), *Card.Name);
	case ECRCardTargeting::PhysicsTargetThenPoint:
		return PendingTarget.IsValid()
			? FString::Printf(TEXT("Aim %s: left click to confirm, right click to cancel"), *Card.Name)
			: FString::Printf(TEXT("%s: click an enemy or barrel"), *Card.Name);
	case ECRCardTargeting::GroundPoint:
		return FString::Printf(TEXT("%s: click a point on the arena"), *Card.Name);
	default:
		return Card.Name;
	}
}

bool ACRCombatGameMode::IsAwaitingPushDirection() const
{
	return TurnState == ECRTurnState::PlayerTurn && PendingTarget.IsValid() && Cards.IsValidIndex(SelectedCard)
		&& Cards[SelectedCard].Targeting == ECRCardTargeting::PhysicsTargetThenPoint;
}

bool ACRCombatGameMode::CanAffordCard(int32 Index) const
{
	return Cards.IsValidIndex(Index) && Cards[Index].ManaCost <= Mana;
}

bool ACRCombatGameMode::IsValidCardDrop(int32 Index, const AActor* HitActor, const FVector& WorldLocation, bool bHitWorld) const
{
	if (TurnState != ECRTurnState::PlayerTurn || !CanAffordCard(Index) || !bHitWorld)
	{
		return false;
	}

	switch (Cards[Index].Targeting)
	{
	case ECRCardTargeting::PhysicsTarget:
	case ECRCardTargeting::PhysicsTargetThenPoint:
		return IsPhysicsTarget(HitActor);
	case ECRCardTargeting::GroundPoint:
		return Arena && Arena->IsInsideOuter(FVector2D(WorldLocation));
	case ECRCardTargeting::None:
		// Self-target cards commit on any release over the combat field.
		return true;
	}
	return false;
}

bool ACRCombatGameMode::TryDropCard(int32 Index, AActor* HitActor, const FVector& WorldLocation, bool bHitWorld)
{
	if (!IsValidCardDrop(Index, HitActor, WorldLocation, bHitWorld))
	{
		if (TurnState == ECRTurnState::PlayerTurn && Cards.IsValidIndex(Index))
		{
			ShowMessage(CanAffordCard(Index) ? TEXT("Invalid target - card returned") : TEXT("Not enough mana"));
		}
		return false;
	}

	ClearTargeting();
	SelectedCard = Index;

	if (Cards[Index].Targeting == ECRCardTargeting::PhysicsTargetThenPoint)
	{
		// Stage 1 of PUSH: lock the target, nothing is spent until the direction is confirmed.
		PendingTarget = HitActor;
		return true;
	}

	const FVector Point = Cards[Index].Targeting == ECRCardTargeting::GroundPoint ? FVector(FVector2D(WorldLocation), 0.f) : WorldLocation;
	PlayCard(Index, HitActor, Point);
	return true;
}

void ACRCombatGameMode::ToggleDebugView()
{
	bDebugView = !bDebugView;
	LogEvent(bDebugView ? TEXT("Debug View on") : TEXT("Playtest View on"));
}

void ACRCombatGameMode::ShowMessage(const FString& Message)
{
	FlashMessage = Message;
	FlashMessageTime = GetWorld()->GetRealTimeSeconds();
	LogEvent(Message);
}

FString ACRCombatGameMode::GetActiveMessage() const
{
	return GetWorld()->GetRealTimeSeconds() - FlashMessageTime < 2.5 ? FlashMessage : FString();
}

void ACRCombatGameMode::LogEvent(const FString& Message)
{
	UE_LOG(LogCRCombat, Log, TEXT("%s"), *Message);
	EventLog.Add(Message);
	if (EventLog.Num() > 10)
	{
		EventLog.RemoveAt(0);
	}
}
