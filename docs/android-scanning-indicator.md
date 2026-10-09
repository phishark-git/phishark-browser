# Discreet active-scan indicator

User preference: retain quiet completed results, but make an ongoing analysis
visible so users know to wait. This is an informational indicator, not a new
navigation or form-submission gate. Existing risk warnings and blocks remain.

Impact query: `PhiSharkBridge setDeepPending updateState scanning`. Provider:
existing native generation/verdict and deep-pending events. Consumer: Java UI
for the currently selected tab. Backend/orchestrator payloads, request counts,
callbacks, storage, deadlines, consent and private-mode policy do not change.
Implementation order: Java indicator, documentation, native build, emulator
checks. Deployment order: Android app update only; no server deployment.

The small bottom indicator says “PhiShark kontrol ediyor · Bekleyin”. It appears
after 350 ms of pending work to avoid flicker for fast/cache results. It has no
flashing animation, progress percentage or safety claim. Result/error states
remove it; switching tabs reevaluates the active tab; activity teardown removes
the view and delayed callback. Full details remain available from the app menu.

Mac must implement and test the same UX independently; Android evidence does
not establish iOS behavior. Full browser acceptance remains separate.

## Validation

Pending build and emulator verification.
