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

Source `6f9f1da1`: 8 shared tests, 19 OAuth assertions and 5 branding tests passed.
Chromium Java/JNI compilation and x64 APK/AAB packaging passed in 4m06.40s.
Existing upstream Android XR `UsedByNative` warnings remained nonfatal. No native
policy or transport code was changed, so no new policy test was introduced.

On the API 35 x64 emulator, the small bottom indicator was observed during a
pending load and again during the three-second deep response of the synthetic
`duplicate-history-slow` fixture. It disappeared after completion, without a
low-risk/partial-result badge. Changing query started another analysis and showed
the indicator; returning to the new-tab page removed it. Across these two checks,
the fixture counted exactly 2 preflight and 2 deep POSTs, despite its five
same-document events, with zero privacy rejections. This is local fixture evidence,
not production server retention or complete mobile acceptance.

APK SHA-256: `496eab1154ef022c13b0a60eaecf87e61546256112c480c409883f3869b213ab`.
AAB SHA-256: `88ace863043f3524e3d930a1910f6c3fc629fde223d05ac88f0f3c556614e0ac`.
In-place installation succeeded, and the installed APK hash matched the final
preserved artifact. Original command-line settings were restored and fixture
mode was verified off after the test; app data was not cleared.

Graphify browser, component and workspace graphs were refreshed; the original
query returned 82 nodes after refresh (budget-truncated). Browser: 543 nodes /
1,104 raw edges; component: 948 edges; workspace: 13,097 nodes / 28,805 edges,
with no dangling aggregate endpoints. The component excludes 145 external AST
references; four conceptual nodes lack source files. Document semantics, older
node IDs and mixed branch snapshots remain limitations of the impact check.
No API, server deployment or iOS implementation change is claimed.
