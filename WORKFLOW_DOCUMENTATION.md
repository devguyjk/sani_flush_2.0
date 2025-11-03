# Sani Flush 2.0 - Complete Workflow Documentation

## System Overview
The Sani Flush 2.0 system manages dual toilet flushing with integrated camera capture and waste repository management. The system operates on precise timing cycles with configurable parameters.

## Core Components
- **Left Toilet** (T1 relay) - Primary toilet with immediate start
- **Right Toilet** (T2 relay) - Secondary toilet with configurable delay
- **Left Cameras** (2x cameras) - Dual camera setup for left side monitoring
- **Right Cameras** (2x cameras) - Dual camera setup for right side monitoring  
- **Left Waste Repository** (P1 relay) - Waste pump for left side
- **Right Waste Repository** (P2 relay) - Waste pump for right side

## Timing Configuration (From Settings System)
```
Flush Workflow Repeat: 120 seconds (2 minutes)
Right Toilet Flush Delay: 10 seconds after left toilet
Flush Relay Time Lapse: 3000ms (3 seconds relay hold)
Waste Repo Trigger Delay: 7 seconds after flush start
Camera Pic Delay: 25000ms (25 seconds after flush)
Pic Every N Flushes: 2 (camera triggers every 2nd flush)
Waste Qty Per Flush: 50ml
Left Toilet Water Volume: 128oz per flush
Right Toilet Water Volume: 128oz per flush
```

## Complete Workflow Cycle

### Phase 1: Workflow Initialization (T+0s)
```
T+0s:    [BUTTON] START clicked
T+0s:    [INIT] Workflow started - Left timer begins immediately
T+0s:    [FLUSH] Left toilet flush #N starts
T+0s:    [RELAY] T1 relay activated for 3000ms
T+0s:    [COUNT] LEFT FLUSH counter incremented
T+10s:   [INIT] Right timer starts (10s delay from settings)
T+10s:   [FLUSH] Right toilet flush #N starts  
T+10s:   [RELAY] T2 relay activated for 3000ms
T+10s:   [COUNT] RIGHT FLUSH counter incremented
```

### Phase 2: Waste Repository Activation (CRITICAL - Currently Not Working)
**EXPECTED BEHAVIOR (from settings):**
```
T+7s:    [WASTE] Left waste repo should trigger (7s after left flush)
T+7s:    [RELAY] P1 relay should activate for calculated duration
T+7s:    [COUNT] Waste counter should increment by 50ml
T+17s:   [WASTE] Right waste repo should trigger (7s after right flush)
T+17s:   [RELAY] P2 relay should activate for calculated duration  
T+17s:   [COUNT] Waste counter should increment by 50ml
```

**ACTUAL BEHAVIOR (from log analysis):**
```
❌ WASTE REPOSITORY IS NOT FIRING AT ALL
❌ No waste repo trigger logs found in the log file
❌ No P1/P2 relay activation occurring
❌ Waste counter not incrementing during automatic cycles
```

### Phase 3: Camera Capture System (Working Correctly)
```
T+25s:   [CAMERA] Left camera 25s delay completed
T+25s:   [CAMERA] Dual capture triggered (if flush count % 2 == 0)
T+25s:   [QUEUE] Camera 1/2 queued (left01)
T+25s:   [QUEUE] Camera 2/2 queued (left02)
T+25s:   [COUNT] Image counter incremented
T+35s:   [CAMERA] Right camera 25s delay completed
T+35s:   [CAMERA] Dual capture triggered (if flush count % 2 == 0)
T+35s:   [QUEUE] Camera 1/2 queued (right01)
T+35s:   [QUEUE] Camera 2/2 queued (right02)
T+35s:   [COUNT] Image counter incremented
```

### Phase 4: Cycle Completion and Reset
```
T+120s:  [CYCLE] Left timer reaches 00:00
T+120s:  [TIMER] Left timer resets to 02:00
T+120s:  [FLUSH] New left toilet flush cycle begins
T+130s:  [CYCLE] Right timer reaches 00:00  
T+130s:  [TIMER] Right timer resets to 02:00
T+130s:  [FLUSH] New right toilet flush cycle begins
```

## Camera Trigger Logic
The camera system uses a modulo-based trigger:
- **Trigger Condition**: `flushCount % picEveryNFlushes == 0`
- **Current Setting**: Every 2 flushes
- **Examples**:
  - Flush #21: 21 % 2 = 1 → NO camera trigger
  - Flush #22: 22 % 2 = 0 → Camera triggers
  - Flush #23: 23 % 2 = 1 → NO camera trigger

## Critical Issue: Waste Repository Not Functioning

### Expected vs Actual Behavior

**EXPECTED (from settings_system.cpp):**
```cpp
// Should trigger 7 seconds after flush start
if (leftElapsed >= flushSettings.getWasteRepoTriggerDelayMs() && 
    !_animateWasteRepoLeft && !wasteRepoLeftTriggered) {
    _animateWasteRepoLeft = true;
    wasteRepoLeftTriggered = true;
}
```

**ACTUAL (from log analysis):**
- No waste repo trigger logs found
- No P1/P2 relay activation
- Waste counter remains static during automatic cycles
- Only manual waste repo activation works (touch interface)

### Root Cause Analysis
The waste repository system is implemented but not being triggered during automatic flush cycles. The issue appears to be in the `updateFlushFlow()` function where the waste repo trigger logic may not be executing properly.

## System State Tracking

### Counters and Metrics
- **Left Flush Count**: Increments at flush start
- **Right Flush Count**: Increments at flush start  
- **Image Count**: Increments when cameras are queued
- **Waste Count**: Should increment when waste repo activates (NOT WORKING)
- **Total Gallons**: Calculated from flush counts × toilet volumes

### Relay States
- **T1 (Left Toilet)**: 3-second activation per flush
- **T2 (Right Toilet)**: 3-second activation per flush
- **P1 (Left Waste)**: Should activate for calculated duration (NOT WORKING)
- **P2 (Right Waste)**: Should activate for calculated duration (NOT WORKING)

## Timing Precision
The system uses millisecond-precision timing with the following key intervals:
- **Flush Duration**: 3000ms relay hold
- **Cycle Period**: 120000ms (2 minutes)
- **Right Delay**: 10000ms after left start
- **Camera Delay**: 25000ms after flush start
- **Waste Delay**: 7000ms after flush start (NOT WORKING)

## Recommendations for Waste Repository Fix

1. **Debug the updateFlushFlow() function** - Add logging to verify waste repo trigger conditions
2. **Check _leftFlushActive and _rightFlushActive states** - Ensure they're properly set during flush cycles
3. **Verify wasteRepoLeftTriggered/wasteRepoRightTriggered flags** - May be preventing re-triggering
4. **Test manual waste repo activation** - Confirm hardware and relay functionality
5. **Add explicit waste repo logging** - Track when conditions should be met vs when they actually trigger

The waste repository is a critical component for the system's intended functionality and must be resolved for proper operation.