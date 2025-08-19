# Sani Flush 2.0 - Complete Workflow Documentation

## Overview
This document describes the complete workflow of the Sani Flush 2.0 system, including timing calculations and settings integration.

## Settings Configuration

| Setting | Default Value | Unit | Description |
|---------|---------------|------|-------------|
| Flush Relay Time Lapse | 3000 | ms | Duration toilet flush relay stays active |
| Flush Workflow Repeat | 60 | sec | Time between flush cycles |
| Waste Qty Per Flush | 150 | ml | Amount of waste to pump per flush |
| Pic Every N Flushes | 3 | count | Take photos every N flushes |
| Waste Repo Pump Delay | 7 | sec | Delay after flush starts before waste pump activates |
| Camera Pic Delay | 2500 | ms | Delay after flush before taking photos |
| Flush Right After Left Flush Delay | 5 | sec | Delay between left and right toilet flushes |

## Complete Workflow

### 1. System Start
- User presses Start/Stop button (triangle → square)
- System initializes flush flow sequence
- Left toilet flush begins immediately

### 2. Left Toilet Flush Sequence

#### 2.1 Toilet Flush Activation (T=0ms)
- **Action**: Left toilet flush relay (T1) activates
- **Duration**: 3000ms (Flush Relay Time Lapse setting)
- **Visual**: Toilet animation begins (4 stages, 500ms each)
- **Timer**: Starts counting down from 60 seconds

#### 2.2 Waste Repository Activation (T=7000ms)
- **Trigger**: 7 seconds after flush starts (Waste Repo Pump Delay setting)
- **Action**: Left waste pump relay (P1) activates
- **Duration**: Calculated as `(150ml × 1000) ÷ 25ml/sec = 6000ms`
- **Visual**: Waste repo animation (5 stages, 400ms each, cycles until pump stops)

#### 2.3 Camera Photo Capture (T=2500ms after flush completes)
- **Trigger**: Every 3rd flush (Pic Every N Flushes setting)
- **Delay**: 2500ms after flush relay deactivates
- **Action**: Dual camera capture (left01 + left02)
- **Visual**: Camera flash animation

### 3. Right Toilet Flush Sequence

#### 3.1 Right Toilet Activation (T=5000ms from left start)
- **Trigger**: 5 seconds after left toilet starts
- **Action**: Right toilet flush relay (T2) activates
- **Duration**: 3000ms (Flush Relay Time Lapse setting)
- **Visual**: Right toilet animation begins
- **Timer**: Starts counting down from 60 seconds

#### 3.2 Right Waste Repository (T=12000ms from left start)
- **Trigger**: 7 seconds after right flush starts
- **Action**: Right waste pump relay (P2) activates
- **Duration**: 6000ms (same calculation as left)
- **Visual**: Right waste repo animation

#### 3.3 Right Camera Capture
- **Trigger**: Every 3rd flush, 2500ms after flush completes
- **Action**: Dual camera capture (right01 + right02)
- **Flush Counter**: Increments when right flush completes

### 4. Cycle Repeat

#### 4.1 Left Toilet Restart
- **Trigger**: 60 seconds after left flush completes
- **Timer**: Counts down from 60 seconds during wait period
- **Action**: Returns to step 2.1

#### 4.2 Right Toilet Restart  
- **Trigger**: 60 seconds after right flush completes + 5 second delay
- **Timer**: Counts down appropriately
- **Action**: Returns to step 3.1

## Timing Calculations

### Relay Duration Calculations

#### Toilet Flush Relay
```
Duration = Flush Relay Time Lapse setting
Default = 3000ms (3 seconds)
```

#### Waste Pump Relay
```
Duration = (Waste Qty Per Flush × 1000) ÷ Pump Rate
Default = (150ml × 1000) ÷ 25ml/sec = 6000ms (6 seconds)
```

### Timer Display Calculations

#### During Flush
```
Remaining Time = (Flush Workflow Repeat ÷ 1000) - Elapsed Seconds
Display = MM:SS format of remaining time
```

#### Between Flushes
```
Remaining Time = Total Wait Time - Elapsed Since Flush End
Total Wait Time = Flush Workflow Repeat setting (60 seconds)
```

### Photo Capture Logic
```
Left Camera Trigger = (Flush Count + 1) % Pic Every N Flushes == 0
Right Camera Trigger = Flush Count % Pic Every N Flushes == 0
```

## State Management

### Animation States
- **Toilet**: 4 stages, 500ms each
- **Camera**: 5 stages, 100ms each  
- **Waste Repo**: 5 stages, 400ms each (cycles during pump operation)

### Relay States
- **T1/T2**: Toilet flush relays
- **P1/P2**: Waste pump relays
- All relays auto-deactivate after calculated durations

### Timer States
- **Left Timer**: Counts down flush duration, then wait period
- **Right Timer**: Counts down with 5-second offset
- **Display**: Updates every second, shows MM:SS format

## Error Handling
- WiFi disconnection: Logs error, continues operation
- Camera timeout: Logs error, continues workflow
- Settings validation: Min/max bounds enforced
- Relay safety: All relays initialize to OFF state

## Performance Metrics
- **Flush Cycle Time**: ~65 seconds (60s + 5s delay)
- **Total Waste Per Cycle**: 300ml (150ml × 2 toilets)
- **Photos Per Hour**: ~36 photos (every 3rd flush, ~5 flushes/hour)
- **Pump Duty Cycle**: ~18% (12s active per 65s cycle)