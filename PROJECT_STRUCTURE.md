# CardsRoguelike / «Хомячки в бутылке» — структура проекта и onboarding

**Актуально для:** M2.8 RC  
**Дата:** 2026-10-01  
**Engine:** Unreal Engine 5.8  
**Репозиторий:** https://github.com/Seruvv/GJ2026  
**Текущий принятый RC:** `feature/m2.8-death-graveyard-recruitment @ eb40200`  
**Важно:** на момент этого документа M2.8 ещё не влит в `main`; `main` остаётся на `0f1e4bf`.

Этот документ нужен новому участнику команды, чтобы быстро понять, как устроен проект, где находятся основные системы и как безопасно начать вносить изменения.

---

## 1. Что это за проект

«Хомячки в бутылке» — PC-прототип карточного 3D roguelike.

Игровой цикл:

`Main Menu → Profile → Sanctuary Hub → Select Hamster → Run Map → Combat / Event / Shop → Boss placeholder → Return / Death / Abandon → Profile update → Sanctuary`

Главные опоры текущей реализации:

1. Пошаговый карточный бой в физическом 3D-пространстве.
2. Процедурный маршрут забега.
3. Постоянные профили и мета-прогрессия Убежища.
4. Постоянные хомяки с HP/Mana, смертью, кладбищем и эпитафиями.
5. Recruitment, который не даёт профилю застрять без живых персонажей.

---

## 2. Технологический стек

- Unreal Engine 5.8.
- Основной gameplay написан на **C++**.
- Один Runtime module: `CardsRoguelike`.
- Blueprint gameplay сейчас практически не используется.
- UI в основном рисуется через **C++ Canvas HUD**.
- Slate используется только там, где нужен настоящий текстовый input (создание имени профиля).
- UMG / Widget Blueprints сейчас не являются частью основной архитектуры.
- Data-driven контент хранится в Data Assets.
- Save system использует Unreal `USaveGame`.
- Основной target — Windows.
- Основной render path: DX12 / SM6.
- Compatibility path: DX11 / SM5.

---

## 3. Главный архитектурный принцип

### Run не должен зависеть от Meta

`UCRRunSubsystem` владеет состоянием текущего забега.

`UCRProfileSubsystem` владеет долговременным состоянием профиля.

```text
Profile / Hub
    ↓
Build Run Start Config
    ↓
UCRRunSubsystem
    ↓
Combat / Event / Shop
    ↓
Run End
    ↓
OnRunEnded
    ↓
UCRProfileSubsystem
    ↓
Resources / Stats / Death / Save
```

**Важно:** Meta/Profile может слушать результат Run, но Run не должен импортировать и напрямую менять систему профиля.

---

## 4. Структура Source

```text
Source/
└── CardsRoguelike/
    ├── Combat/
    ├── Run/
    ├── Event/
    ├── Shop/
    ├── Meta/
    ├── Hub/
    ├── Menu/
    ├── UI/
    └── Tests/
```

### `Combat/`

Ответственность: один пошаговый бой.

Ключевые сущности:

- `ACRCombatGameMode`
- `ACRPlayerController`
- `ACRDebugHUD`
- `ACRArena`
- `ACRBoundarySegment`
- `ACRHamster`
- `ACREnemy`
- `ACRBarrel`
- `ACRPit`
- `CRCardLibrary`

Текущие карты:

- Push / ТОЛЧОК
- Blast / ВЗРЫВ
- Pull / ПРИТЯЖЕНИЕ
- Guard / ЗАЩИТА
- Mend / ЛЕЧЕНИЕ

Особенности:

- mana восстанавливается каждый player turn;
- весь текущий deck фактически является hand;
- полноценного draw/discard/reshuffle пока нет;
- физика арены является gameplay;
- победа возможна через яму/край/взрыв;
- после Victory открывается Reward.

Input:

- `1–9` — card slots;
- отсутствующий слот ничего не делает;
- `F10` — Debug View;
- `Shift+7/8/9` меняют типы границ только в Debug View;
- обычные `7/8/9` границы не меняют;
- right-click отменяет targeting.

### `Run/`

Ответственность: весь текущий забег.

Ключевые элементы:

- `FCRRunState`
- `UCRRunSubsystem`
- procedural generator
- `ACRRunMapGameMode`
- `ACRRunMapPlayerController`
- `ACRRunMapHUD`
- `ACRRunMapActor`
- `ACRRunNodeActor`

Run state хранит:

- seed;
- selected hamster snapshot;
- HP;
- deck;
- carried Silver/Food/Wood;
- current node;
- visited nodes;
- room states;
- run status;
- end reason;
- death cause;
- profile link;
- exactly-once end reporting.

**Run не сохраняется на диск.** Если приложение закрыто посреди run, незавершённый забег теряется.

### `Event/`

Ответственность: narrative rooms.

Ключевые элементы:

- `UCREventDefinition`
- `UCREventPool`
- `FCREventChoice`
- `FCREventEffect`
- `ACREventGameMode`
- `ACREventPlayerController`
- `ACREventHUD`
- `ACREventRoomActor`

Поддерживаются эффекты:

- ModifyHP
- ModifyResource
- AddRandomCard
- AddSpecificCard
- RemoveSelectedCard

Event choice коммитится один раз. Lethal choice разрешён: если HP становится `<= 0`, run завершается как Failed с `DeathCause = Event`.

### `Shop/`

Ответственность: торговая комната.

Ключевые элементы:

- `ACRShopGameMode`
- `ACRShopHUD`
- `ACRShopPlayerController`
- `ACRShopRoomActor`
- merchant presentation
- per-node `FCRShopState`

Shop хранит состояние конкретного run node: offers, purchased flags, heal-used flag и merchant line.

### `Meta/`

Ответственность: долговременный профиль.

Ключевые элементы:

- `UCRProfileSubsystem`
- `UCRProfileIndexSaveGame`
- `UCRProfileSaveGame`
- hamster persistent state
- resource structs
- death record
- recruitment rules
- save migration
- `CRDevCommands.cpp`

Текущая `Profile SaveVersion = 3`.

Профиль хранит ProfileId, display name, timestamps, ресурсы, building levels, profile stats, LastRun, hamster roster, SelectedHamsterId, death records и recruitment candidates.

### `Hub/`

Ответственность: Sanctuary / Бутылка.

Ключевые элементы:

- `ACRHubGameMode`
- `ACRHubHUD`
- `ACRHubRoomActor`

Hub содержит Heart, Workshop, Storage, living roster, Graveyard, Recruitment и кнопку `В ПОХОД`.

### `Menu/`

Main Menu, profile list, create/select/delete profile, Continue и Exit.

### `UI/`

Общие Canvas helpers и общий PlayerController.

### `Tests/`

Automation tests для procedural generation, profile/meta, migrations, hamster roster, death, graveyard, recruitment, shop, events, full route и profile isolation.

Текущий baseline: **29/29 passed**.

---

## 5. Структура Content

```text
Content/
└── Dev/
    ├── Events/
    │   ├── Definitions/
    │   └── Pools/
    ├── Hub/
    │   ├── Buildings/
    │   ├── DA_HubCatalog
    │   ├── DA_DefaultHamsterRoster
    │   └── DA_Recruitment
    └── TestMaps/
        ├── LV_MainMenu
        ├── LV_SanctuaryHub
        ├── LV_RunMapSandbox
        ├── LV_CombatSandbox
        ├── LV_EventSandbox
        └── LV_ShopSandbox
```

### Важные Data Assets

**`DA_HubCatalog`** — starting resources, building definitions, keep percentages, default roster и recruitment asset.

**`DA_DefaultHamsterRoster`** — стартовые персонажи:

| Hamster | Base HP | Mana |
|---|---:|---:|
| Бублик | 36 | 2 |
| Искра | 24 | 4 |
| Пуговка | 30 | 3 |
| Жёлудь | 32 | 3 |
| Сквозняк | 27 | 4 |
| Валун | 34 | 2 |

**`DA_Recruitment`** — 3 persistent candidates, target living roster = 6, name pool, epitaph pool, HP/Mana templates и tint options.

---

## 6. Карты и routing

- `LV_MainMenu` — game entrypoint и профили.
- `LV_SanctuaryHub` — meta hub.
- `LV_RunMapSandbox` — procedural run map.
- `LV_CombatSandbox` — combat room.
- `LV_EventSandbox` — narrative event room.
- `LV_ShopSandbox` — shop room.

Текущий graph run:

- 13 nodes;
- layers `1 / 2 / 3 / 3 / 2 / 1 / 1`;
- минимум 4 Combat;
- минимум 2 Event;
- минимум 1 Shop;
- каждый Start→Boss path содержит минимум 2 Combat;
- Shop→Shop запрещён;
- Boss → Return.

---

## 7. Save architecture

### Index save

Хранит summaries профилей, last selected profile и next profile number.

### Profile save

Отдельный slot на профиль.

`SaveVersion = 3`.

Migration v2 → v3 сохраняет существующие ресурсы, buildings, roster и selection, добавляя Recruitment state.

### Что не сохраняется

Незавершённый `FCRRunState`.

---

## 8. Permanent Death

Если профильный хомяк погибает:

```text
Combat/Event death
    ↓
Run Failed
    ↓
OnRunEnded
    ↓
ProfileSubsystem
    ↓
bAlive = false
    ↓
DeathRecord
    ↓
0% carried loot delivered
    ↓
Save
```

Death Record хранит timestamp, Combat/Event cause, RunSeed, NodeId, room context, lost loot и rooms visited.

- Completed: hamster alive, 100% delivery.
- Abandoned: hamster alive, 0% delivery.
- Failed/death: hamster dead, 0% delivery.

---

## 9. Graveyard

Graveyard встроен в `LV_SanctuaryHub`.

Показывает dead hamsters newest-first и хранит:
- portrait/placeholder;
- имя;
- Base HP / Mana;
- причину смерти;
- дату;
- seed;
- node/room;
- потерянные ресурсы;
- эпитафию.

**Эпитафия назначается при создании хомяка, а не в момент смерти.** Пока персонаж жив, она скрыта.

---

## 10. Recruitment

Текущий prototype:

- target living roster = 6;
- 3 persistent candidates;
- recruitment бесплатный;
- candidates переживают restart и не reroll'ятся;
- новый recruit получает unique GUID-based ID;
- recruiting блокируется при 6 living;
- после найма кандидат заменяется новым;
- если roster пуст, первый recruit автоматически становится selected.

Это prototype safety-net, а не финальная экономика.

---

## 11. Hub buildings

### Heart / Сердце бутыли
- L1 start;
- L2: +3 Run Max HP;
- L3: ещё +3.

### Workshop / Мастерская
- L0 → L3;
- Guard;
- затем +5 starting Silver;
- затем Push.

### Storage / Склад
- L1 → L3;
- улучшает Silver→Wood;
- L3 добавляет Food→Wood.

---

## 12. Resource rules

Новый профиль:

- Silver: 60
- Food: 4
- Wood: 6

| Run result | Delivery |
|---|---:|
| Completed | 100% |
| Failed / death | 0% |
| Abandoned | 0% |

Combat Reward:
- +5 Silver
- +1 Food
- +1 Wood
- 3 card offers или Skip

Карты пока остаются только в текущем run. Persistent card collection не реализована.

---

## 13. Известные ограничения

- нет mid-run save;
- Boss — placeholder;
- один combat encounter layout;
- нет draw/discard/reshuffle;
- artifacts не реализованы;
- persistent card/artifact collection не реализована;
- corpse recovery не реализован;
- zombie hamster encounters не реализованы;
- injuries / traits / jobs / retirement пока не gameplay;
- Recruitment пока бесплатный;
- новые 3D Hub anchors требуют кода;
- Main Menu не рассчитан на очень большое число профилей;
- часть run randomness не seeded;
- финальный art direction ещё не применён.

---

## 14. Как начать работать новому разработчику

1. Склонировать репозиторий.
2. Проверить текущую milestone-ветку.
3. На момент этого документа accepted RC: `feature/m2.8-death-graveyard-recruitment @ eb40200`.
4. Открыть проект в Unreal Engine 5.8.
5. Собрать `CardsRoguelikeEditor Win64 Development`.
6. Запустить automation suite.
7. Проверить нужную карту в PIE.
8. Прочитать соответствующую системную папку и tests.
9. Делать минимальный diff.
10. Добавить/обновить regression test.

### Не делать без отдельного решения команды

- не переносить gameplay в Blueprint только ради удобства;
- не вводить новую глобальную framework-архитектуру;
- не создавать циклическую зависимость Run ↔ Meta;
- не менять Save schema без migration;
- не рефакторить unrelated systems в рамках feature-задачи;
- не менять engine/plugin/render config без необходимости.

---

## 15. Куда добавлять новую функциональность

### Новая combat card
- gameplay: `Combat/`
- tests: `Tests/`
- data-driven параметры не прятать в HUD.

### Новый Event
- asset: `/Game/Dev/Events/Definitions/`
- pool: `/Game/Dev/Events/Pools/`
- generic logic: `Event/`

### Новое Hub building
Нужны и data definition, и presentation. Текущие 3D building anchors частично hardcoded.

### Новая Meta mechanic
- persistent data и migration: `Meta/`
- apply at run end: Meta/Profile rules;
- UI: `Hub/`;
- regression: `Tests/`.

### Новый room type
Почти наверняка затронет Run node type/state, generator, routing, отдельный GameMode, completion contract, tests и cook list.

---

## 16. UI conventions

- Player-facing текст — русский.
- Internal class names/logs — английский.
- Canvas UI — текущий стандарт.
- Не добавлять UMG точечно без архитектурного решения.
- Для Cyrillic C++ source сохранять UTF-8 BOM.
- 16:9 — основная layout база.
- Graveyard readability уже проходила отдельный M2.8 RC polish.

---

## 17. Testing и packaging

Перед завершением системной задачи:

1. Automation tests.
2. Editor build.
3. PIE/system flow.
4. Save/reload, если затронут persistent state.
5. Full run regression, если затронут Run.
6. Package smoke, если milestone готов к RC.

Текущий baseline: **29/29 automation tests passed**.

M2.8 RC проверен:
- DX12 / SM6;
- DX11 / SM5;
- Main Menu;
- Sanctuary;
- Graveyard;
- Recruitment;
- Run Map;
- Combat;
- Event;
- Shop map load.

Cook должен включать:

```text
/Game/Dev/TestMaps/LV_MainMenu
/Game/Dev/TestMaps/LV_SanctuaryHub
/Game/Dev/TestMaps/LV_RunMapSandbox
/Game/Dev/TestMaps/LV_CombatSandbox
/Game/Dev/TestMaps/LV_EventSandbox
/Game/Dev/TestMaps/LV_ShopSandbox
/Game/Dev/Hub
/Game/Dev/Events
```

---

## 18. Development / QA commands

В Development builds есть `CR.Dev.*` команды для QA и automation workaround.

Конкретный актуальный список смотреть в `Meta/CRDevCommands.cpp`.

Они используются для:
- status profile/run;
- disposable profiles;
- enter room;
- force defeat;
- set HP;
- Event choose/continue;
- Hub Graveyard/Recruitment;
- recruit candidate;
- return to Hub;
- empty-roster tests.

Не использовать эти команды как player-facing gameplay.

---

## 19. Git workflow

```text
main
 ↓
feature milestone
 ↓
implementation
 ↓
automation
 ↓
manual acceptance
 ↓
RC package
 ↓
user acceptance
 ↓
merge
```

На момент документа:

- accepted M2.8 RC branch: `feature/m2.8-death-graveyard-recruitment`
- HEAD: `eb40200`
- `main`: `0f1e4bf`
- M2.8 ещё не merged.

### Особенность текущей машины владельца

Системный Git for Windows повреждён. На этой машине используется Visual Studio bundled Git:

```text
C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe
```

Это локальная особенность конкретного компьютера, не требование проекта.

---

## 20. Что читать перед большой задачей

1. Этот файл.
2. Актуальный GDD / Notion.
3. `git status`, branch и HEAD.
4. Нужную системную папку.
5. Связанные automation tests.
6. Data Assets, если система data-driven.

Если документация и код расходятся:

**репозиторий — источник фактической реализации, GDD — источник продуктового намерения.**

Расхождение нужно зафиксировать и согласовать, а не молча переписывать систему.

---

## 21. Следующие крупные направления после M2.8

Пока не реализованы:

- полноценный Boss;
- combat encounter variety;
- Artifacts;
- persistent loot/card collection;
- corpse recovery;
- zombie former hamsters;
- injuries / traits / veteran progression;
- recruitment economy;
- mid-run persistence;
- final jam theme / art direction.
