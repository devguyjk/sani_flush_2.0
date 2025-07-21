# Timer and Flush Bar Workflow

This document explains the workflow for the timer and flush bar functionality in the Sani Flush 2.0 system.

## Timer Workflow

### Left Timer (Primary Timer)

| Line | Function | Description |
|------|----------|-------------|
| 1168-1169 | `updateTimers()` | Main function that updates both left and right timers |
| 1172-1173 | `if (_timerLeftRunning)` | Checks if left timer is active |
| 1175-1176 | `if (_leftFlushActive)` | Checks if left toilet is currently flushing |
| 1179-1180 | `unsigned long elapsed = (_currentTime - _leftFlushStartTime) / 1000` | Calculates elapsed time in seconds since flush started |
| 1181-1182 | `unsigned long remaining = (FLUSH_DURATION_MS / 1000) - elapsed` | Calculates remaining time in seconds |
| 1183-1187 | `if (remaining > 0)` | If time remains, update minutes and seconds |
| 1191-1195 | `else` | If no time remains, reset timer to 00:00 |
| 1199-1200 | `else` | If not flushing, count down from time lapse period |
| 1202-1203 | `unsigned long elapsed = (_currentTime - _timerLeftStartTime) / 1000` | Calculate elapsed time since last flush completed |
| 1204-1205 | `unsigned long totalTime = _flushTotalTimeLapseMin * 60` | Calculate total time lapse in seconds |
| 1206-1211 | `if (elapsed < totalTime)` | If still in time lapse period, update countdown |
| 1215-1219 | `else` | If time lapse complete, reset timer to 00:00 |
| 1254-1258 | `if (_timerLeftSeconds != _lastLeftSeconds)` | Only redraw timer if seconds have changed |

### Right Timer (Secondary Timer)

| Line | Function | Description |
|------|----------|-------------|
| 1224-1225 | `if (_timerRightRunning)` | Checks if right timer is active |
| 1227-1228 | `if (_rightFlushActive)` | Checks if right toilet is currently flushing |
| 1231-1232 | `unsigned long elapsed = (_currentTime - _rightFlushStartTime) / 1000` | Calculates elapsed time in seconds since flush started |
| 1233-1234 | `unsigned long remaining = (FLUSH_DURATION_MS / 1000) - elapsed` | Calculates remaining time in seconds |
| 1235-1239 | `if (remaining > 0)` | If time remains, update minutes and seconds |
| 1243-1247 | `else` | If no time remains, reset timer to 00:00 |
| 1249-1250 | `else if (_currentTime >= _timerRightStartTime)` | If between flushes, count down to next flush |
| 1252-1255 | `unsigned long remaining = (_timerRightStartTime - _currentTime) / 1000` | Calculate time until next flush |
| 1260-1264 | `else` | If in delay period before first flush, show 00:00 |
| 1260-1264 | `if (_timerRightSeconds != _lastRightSeconds)` | Only redraw timer if seconds have changed |

## Flush Bar Workflow

### Left Flush Bar

| Line | Function | Description |
|------|----------|-------------|
| 343-344 | `updateFlushBar()` | Main function to update left flush bar |
| 347-350 | Bar dimensions setup | Sets position and size matching toilet width |
| 353-354 | `tft.fillRect(barX, barY, barWidth, barHeight, TFT_WHITE)` | Always start with clean white bar |
| 355-356 | `tft.drawRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, ILI9341_BLACK)` | Draw black outline |
| 358-361 | `if (!_leftFlushActive)` | If not flushing, leave bar white and return |
| 364-366 | `unsigned long elapsed = _currentTime - _leftFlushStartTime` | Calculate elapsed time since flush started |
| 367-368 | `float progress = (float)elapsed / (float)FLUSH_DURATION_MS` | Calculate progress as percentage (0.0-1.0) |
| 371-372 | `int blueWidth = (int)(barWidth * (1.0 - progress))` | Calculate blue portion width (remaining time) |
| 375-377 | `if (blueWidth > 0)` | Draw blue portion if any time remains |
| 380-381 | `tft.drawRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, ILI9341_BLACK)` | Redraw outline |

### Right Flush Bar

| Line | Function | Description |
|------|----------|-------------|
| 384-385 | `drawRightFlushBar()` | Main function to update right flush bar |
| 388-391 | Bar dimensions setup | Sets position and size matching toilet width |
| 394-395 | `tft.fillRect(barX, barY, barWidth, barHeight, TFT_WHITE)` | Always start with clean white bar |
| 396-397 | `tft.drawRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, ILI9341_BLACK)` | Draw black outline |
| 399-402 | `if (!_rightFlushActive)` | If not flushing, leave bar white and return |
| 405-407 | `unsigned long elapsed = _currentTime - _rightFlushStartTime` | Calculate elapsed time since flush started |
| 408-409 | `float progress = (float)elapsed / (float)FLUSH_DURATION_MS` | Calculate progress as percentage (0.0-1.0) |
| 412-413 | `int blueWidth = (int)(barWidth * (1.0 - progress))` | Calculate blue portion width (remaining time) |
| 416-418 | `if (blueWidth > 0)` | Draw blue portion if any time remains |
| 421-422 | `tft.drawRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, ILI9341_BLACK)` | Redraw outline |

## Flush Flow and Counter Update

| Line | Function | Description |
|------|----------|-------------|
| 1022-1023 | `updateFlushFlow()` | Main function that manages the flush cycle |
| 1028-1029 | `if (_leftFlushActive)` | Check if left toilet is flushing |
| 1031-1032 | `unsigned long leftElapsed = _currentTime - _leftFlushStartTime` | Calculate elapsed time for left toilet |
| 1035-1040 | Waste repo trigger | Trigger waste repo animation after delay |
| 1043-1044 | `if (leftElapsed >= FLUSH_DURATION_MS)` | Check if left flush is complete (60s) |
| 1046-1048 | `_leftFlushActive = false` | Deactivate left flush |
| 1051-1052 | `_timerLeftMinutes = 0; _timerLeftSeconds = 0` | Reset left timer to 00:00 |
| 1066-1067 | `_timerLeftStartTime = _currentTime` | Set start time for next cycle |
| 1074-1075 | `if (_rightFlushActive)` | Check if right toilet is flushing |
| 1077-1078 | `unsigned long rightElapsed = _currentTime - _rightFlushStartTime` | Calculate elapsed time for right toilet |
| 1081-1086 | Waste repo trigger | Trigger waste repo animation after delay |
| 1089-1090 | `if (rightElapsed >= FLUSH_DURATION_MS)` | Check if right flush is complete (60s) |
| 1092-1094 | `_rightFlushActive = false` | Deactivate right flush |
| 1097-1099 | `_flushCount++` | Increment flush count when right toilet completes |
| 1100-1101 | `updateFlushCount(1)` | Update 4-digit display |
| 1104-1105 | `_timerRightMinutes = 0; _timerRightSeconds = 0` | Reset right timer to 00:00 |
| 1119-1120 | `_timerRightStartTime = _currentTime + (_flushTotalTimeLapseMin * 60000) + RIGHT_TOILET_FLUSH_DELAY_MS` | Set start time for next cycle |

## Details Display Update

| Line | Function | Description |
|------|----------|-------------|
| 425-426 | `drawFlowDetails()` | Function to draw the details section |
| 442-444 | Flush Count display | Shows current flush count |
| 447-449 | Waste Consumed | Shows total waste in ml (flush count × dose per flush) |
| 452-454 | Gallons Flushed | Shows total gallons (flush count × 2) |
| 457-459 | Settings header | Displays settings section header |
| 462-464 | Flush Duration | Shows flush duration (60s) |
| 467-469 | Waste per Flush | Shows waste dose per flush in ml |
| 472-474 | Pump Delay | Shows waste pump delay in seconds |

## Key Improvements Made

1. **Left Timer Reset**: Fixed the left timer to properly reset to 00:00 after reaching the flush duration (60 seconds)
2. **Flush Bar Display**: 
   - Completely rewrote the flush bar logic to ensure clean display
   - Always starts with a clean white bar
   - Blue portion represents remaining time (shrinks as time passes)
   - Bars are exactly aligned with toilets (same width and position)
3. **Details Display**:
   - Updated to show waste in ml instead of oz
   - Details are updated after right toilet completes flush
4. **Counter Update**:
   - 4-digit display is incremented when right toilet completes its flush cycle