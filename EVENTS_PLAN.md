# Native activity events — Bronze and Canterbury

This supplements the completed forty-cycle Canterbury plan. “Events” here means
native episodic activity codes: X chats with Y, X eats food, completed work,
delivered service, training, trade and birth. It does not introduce an independent
world-event clock or scheduled disaster system.

## 1. Inspect the existing event paths

- [x] Identify the existing event enum, episodic storage, descriptions and logging.
- [x] Confirm Bronze already records profession/trade/training memories and native
  social chat, but explicit cargo eating omits its eating memory.
- [x] Confirm Canterbury bypasses the island behaviour loop; its own eating and
  social actions need explicit native integration.

## 2. Implement guarded ApeSDK integration

- [x] Add BRONZE_SIM helpers that record actual eating and paired chats without
  granting food, energy, movement, stock or time.
- [x] Append named simulation food types without renumbering historical codes.
- [x] Record successful Bronze cargo consumption; retain existing native social,
  profession, trade and training pathways.
- [x] Provide bounded, copied, newest-first descriptions and show them in the
  selected Bronze actor inspector.
- [x] Verify ordinary builds remain unaffected and native memory/save layouts
  are unchanged.

## 3. Implement Canterbury actions and presentation

- [x] Record actual finite-meal consumption in native memory and personal history.
- [x] Add bounded local conversation opportunities for awake, not severely ill, co-located
  idle residents with unused native action budgets. Never infer positions or
  conversations for unplaced people.
- [x] Record paired chat memories and personal records with correct civil names.
- [x] Connect completed work and physically delivered services to native memories.
- [x] Show eating/conversation history in the selected resident inspector.
- [x] Test that failed/deferred actions create no success memories, pause/inspection
  does not generate events, and names/references survive compaction.

## 4. Persistence, replay and acceptance

- [x] Test actual food stock/energy effects alongside EVENT_EAT, including food
  descriptions, paired EVENT_CHAT and invalid/sleeping participant rejection.
- [x] Test Canterbury placement, chat eligibility, meal accounting, causal privacy,
  batching, save/load continuation, and bounded native retention.
- [x] Run Bronze and ordinary native regression suites plus Canterbury acceptance
  and native application builds.
- [x] Verify both inspectors show the activity events from real simulated actions.
- [x] Document the event/code mapping, bounded subjective memory versus diagnostic
  history, and the schematic local-chat opportunity policy.

No successful-looking record substitutes for an authoritative action. New enum
values are appended only. Native episodic storage and its ordinary forgetting
remain the source of an agent's subjective memories; causal city records remain
bounded observer history. Source pin/review and checkpoint compatibility gates
must continue to reject unreviewed or incompatible state.

## Completion evidence

Implemented and verified on 9 November 2026. The native profile qualification,
ordinary ApeSDK/Bronze regressions, Canterbury cycles 14–34, dedicated activity
acceptance, Canterbury Xcode Debug, and Bronze Xcode Debug/Release builds passed.
Both inspectors were observed showing actual food and conversation memories.
The final Canterbury text path explicitly decodes UTF-8. Isolated UI automation
launches occasionally timed out; UI observations are recorded separately from
backend acceptance rather than treated as a source qualification substitute.

The activity acceptance covers real stock debit, native food codes, paired chats,
unknown/separated/resting exclusions, personal privacy, native retention after
city history eviction, configured birth, save/load, batch replay, and moving a
remembered resident's native slot during retirement compaction. It is reproduced
with `python3 tools/verify_activity_events.py` and checked with `--check`.

A 27,000-resident source-world day (1,440 added native minutes) completed in
24.86 seconds with 2,405,662,720 bytes maximum RSS on this host. This is a single
CLI measurement, not a physical-device frame-rate claim. Detailed evidence is in
`content/activity-events-validation.json`, `content/activity-events-native-ui.json`
and the `build/activity-*` logs.

## Committed-source revalidation

The first four sections were rerun against committed ApeSDK revision
`dc4e6d4e35c1a9bdb79ae0a52951631a2e973e98` on 9 November 2026.
The obsolete working-tree exceptions were removed and source qualification,
native compatibility, activity acceptance and Canterbury regression evidence
were refreshed. Existing recorded UI observations remain separate from backend
acceptance; this rerun does not claim a new interactive UI observation.
