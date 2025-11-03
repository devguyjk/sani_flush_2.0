# Sani Flush 2.0 - Workflow Timing Documentation

## Settings (All in Seconds)
- **Flush Workflow Repeat**: 120s (2 minutes) - Duration of each flush cycle
- **Flush Relay Time Lapse**: 3s - How long toilet relay stays active
- **Waste Repo Pump Delay**: 7s - Delay before waste pump activates
- **Camera Pic Delay**: 2.5s - Delay between dual camera captures
- **Right Flush Delay**: 5s - Delay before right toilet starts (converted to 10s in code)

## Workflow Timeline

### Initial Start (T=0)
1. **[BUTTON] START clicked** - User presses start button
2. **[INIT] Workflow started** - Initialize all timers and states
3. **[FLUSH] LEFT START** - Left toilet flush begins immediately
4. **[COUNT] LEFT FLUSH #1** - Left counter increments
5. **Left Timer**: Shows 02:00 countdown (120 seconds)
6. **Right Timer**: Shows 00:10 countdown (10 second delay)

### Left Toilet Cycle (T=0 to T=120s)
- **T=0s**: Left flush starts, relay T1 activates for 3s
- **T=7s**: Left waste repo activates (7s delay), pump P1 runs for 2s
- **T=9s**: Left waste repo completes, waste counter increments
- **T=120s**: Left flush completes, schedules restart

### Right Toilet Cycle (T=10s to T=130s)
- **T=10s**: Right flush starts (10s delay), relay T2 activates for 3s
- **T=17s**: Right waste repo activates (7s delay), pump P2 runs for 2s
- **T=19s**: Right waste repo completes, waste counter increments
- **T=130s**: Right flush completes, schedules restart

### Restart Logic
- **Left Restart**: Every 120s from completion (T=120s, T=240s, T=360s...)
- **Right Restart**: Every 120s + 10s delay from completion (T=130s, T=250s, T=370s...)

## Expected Log Entries

### Startup Sequence
```
[BUTTON] START clicked at T:xxxxx
[INIT] Workflow started at T:xxxxx duration:120s
[FLUSH] LEFT START at T:xxxxx _leftFlushStartTime:xxxxx
[COUNT] LEFT FLUSH #1 at T:xxxxx
[INIT] Starting timers - Left running, Right delayed by 10000ms
[INIT] Timer states - LeftRunning:1 RightRunning:1
```

### Left Flush Cycle
```
5. Activating left waste repo after 7000ms delay
Left waste repo animation and relay started
[COUNT] Waste: 50ml (incremented by 50ml)
Left waste repo animation and relay completed after 2000ms
[WORKFLOW] Left flush completed after 120000ms (120s)
[WORKFLOW] Left restart scheduled at T:xxxxx duration:120s
```

### Right Flush Cycle
```
Starting right toilet flush after delay
[FLUSH] RIGHT START at T:xxxxx _rightFlushStartTime:xxxxx
[COUNT] RIGHT FLUSH #1 at T:xxxxx
5. Activating right waste repo after 7000ms delay
Right waste repo animation and relay started
[COUNT] Waste: 100ml (incremented by 50ml)
Right waste repo animation and relay completed after 2000ms
[WORKFLOW] Right flush completed after 120000ms (120s)
[WORKFLOW] Right restart scheduled at T:xxxxx duration:120s
```

### Restart Checks
```
[TIMER] Left restart check - elapsed:120s required:120s
[RESTART] Left toilet flush after 120s elapsed
[FLUSH] LEFT START at T:xxxxx _leftFlushStartTime:xxxxx
[COUNT] LEFT FLUSH #2 at T:xxxxx
```

## Timer Display Logic
- **During Flush**: Countdown from 120s to 0s
- **Between Flushes**: Countdown from 120s to 0s (wait period)
- **Display Format**: MM:SS (e.g., 02:00, 01:59, 01:58...)

## Critical Timing Conversions
- **Settings**: All values in seconds
- **Millisecond Comparisons**: Settings × 1000 for elapsed time checks
- **Timer Display**: Settings ÷ 60 for minutes, Settings % 60 for seconds
- **Timestamps**: millis() for all logging and state tracking

## State Flags
- **_flushFlowActive**: Overall workflow running
- **_leftFlushActive**: Left toilet currently flushing
- **_rightFlushActive**: Right toilet currently flushing
- **_timerLeftRunning**: Left timer active
- **_timerRightRunning**: Right timer active

## Expected Behavior
1. **Both toilets alternate** with 10s offset
2. **Each flush lasts exactly 120s**
3. **Counters increment at flush START**
4. **Waste increments when pump activates**
5. **Timers show accurate countdown**
6. **Workflow repeats indefinitely**

## Debug Monitoring
- **Every 30s**: Memory and state status
- **Every 60s**: System time debug
- **Every 5min**: Counter validation and recalibration