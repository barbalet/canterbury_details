# Accessibility and input qualification

Supported input is native macOS keyboard and pointer/trackpad. Controller support
is not included. Native labels, roles, focus and text selection are exposed through
AppKit. The release gate records actual keyboard/accessibility-tree interaction
separately from pure synthetic navigation tests. It does not claim a physical
screen-reader, Braille display, switch device or game controller certification.

| Workflow | Keyboard route | Qualification |
| --- | --- | --- |
| Management | Command-M; Tab fields/categories; Command-P/K/F/J/B | Native focus/typed requests and result checks; C/CLI parity |
| Text editing | Edit menu, Command-A/C/V/X/Z | Native AppKit responder chain |
| Status pages | Category popup, Command-R, Command-[ / ] | Bounded copied read model, scrollable report |
| People | Searchable resident table, arrows/Return | Stable ID resolved on worker; snapshots don't tick |
| Full roster | Click/focus then arrows, Home/End, Return | Pure tested navigation at 1–30,000 entries; selected ID retained |
| Map | Arrows, F, +/−; explicit Space run/pause | Read-only camera except explicitly assigned run command |
| Save/load | Command-S/O and standard native file chooser | Worker I/O, atomic publish; canceled chooser has no model effects |
| Readable text | View Larger/Smaller; management Larger/Smaller | Resident/inspector 100–150%; form/report 10–24/26 points |

The cream window uses an explicit light appearance for dark readable labels even
under macOS dark mode; the sidebar retains a separate high-contrast dark appearance.
Roster +/× symbols and textual located/unplaced descriptions work without color.
Selection has a visible outline and an accessible stable-ID/location value. Source
unknowns and intervention results are words rather than color-only warnings.

Management forms and records scroll; Tab follows the form's logical order, and
native text fields scroll their focused area into view. No timed/reflex interaction
is required. No custom motion/transition animation is used, so reduced-motion
settings do not change simulation behavior; the benchmark reports the host setting
without changing it. Screen zoom and other OS assistive settings are user-owned.

Synthetic acceptance enumerates roster boundaries and RNG purity. Native evidence
records the tested app workflow, clock before/after paused navigation, form preview,
queue/cancel/query, text resizing and readonly management pages. Navigation/text/
inspection consume no model minutes or RNG; Run/Step/Apply are explicit operations.
