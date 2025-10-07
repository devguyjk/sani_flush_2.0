# Sani Flush 2.0 Status Endpoint Payload Parameters

## Core Counters
- `flush_count` - Total flushes since startup
- `daily_flush_count` - Flushes today
- `weekly_flush_count` - Flushes this week
- `monthly_flush_count` - Flushes this month

## System Timing Information
- `system_start_time` - When ESP32 booted/powered on (ISO timestamp)
- `current_time` - Current system time (ISO timestamp)
- `system_uptime_sec` - Seconds since ESP32 boot
- `system_uptime_ms` - Milliseconds since ESP32 boot

## Workflow Timing Information
- `workflow_start_time` - When start/stop button was pressed to begin (ISO timestamp)
- `workflow_active` - Boolean indicating if workflow is currently running
- `workflow_timespan_sec` - Seconds workflow has been running (only active time)
- `workflow_timespan_ms` - Milliseconds workflow has been running (only active time)
- `last_flush_time` - Timestamp of most recent flush
- `time_since_last_flush_sec` - Seconds since last flush
- `time_since_last_flush_ms` - Milliseconds since last flush
- `last_camera_capture_time` - When last camera capture occurred
- `time_since_last_capture_sec` - Seconds since last camera capture
- `estimated_flushes_remaining_today` - Calculated flushes possible until midnight
- `average_flush_interval_sec` - Average time between flushes today
- `projected_end_of_day_flush_count` - Estimated total flushes by end of day

## Image/Camera Status
- `last_image_captured` - Filename of most recent image
- `last_image_time` - When last image was taken
- `total_images_captured` - Total image count
- `images_captured_today` - Images taken today
- `camera_status` - Status of each camera (online/offline/error)
- `last_left_camera_image` - Most recent left side image
- `last_right_camera_image` - Most recent right side image
- `current_image_number` - Current sequential image number for navigation
- `next_image_number` - Next image number (current + 1)
- `previous_image_number` - Previous image number (current - 1)
- `image_url_template` - Template for image URLs: "/image/{image_number}_image"
- `flush_image_count` - Images taken during flush sequences
- `manual_image_count` - Images taken manually

## System Health
- `wifi_status` - Connected/disconnected
- `wifi_signal_strength` - RSSI value
- `free_memory` - Available RAM bytes
- `storage_used` - Used storage percentage
- `cpu_temperature` - ESP32 temperature
- `battery_level` - If battery powered (percentage)

## Operational Status
- `current_mode` - Manual/Auto/Maintenance
- `system_status` - Ready/Busy/Error/Maintenance
- `last_error` - Most recent error message
- `error_count` - Total errors since startup
- `maintenance_mode` - True/false
- `auto_flush_enabled` - True/false

## Settings & Configuration
- `flush_relay_time_lapse_ms` - Flush relay hold time in milliseconds (1000-10000ms)
- `flush_workflow_repeat_sec` - Flush workflow repeat interval in seconds (60-600sec)
- `waste_qty_per_flush_ml` - Waste quantity per flush in milliliters (25-3000ml)
- `pic_every_n_flushes_count` - Take picture every N flushes (1-10 count)
- `waste_repo_pump_delay_sec` - Waste repository pump delay in seconds (1-20sec)
- `camera_pic_delay_ms` - Camera picture delay in milliseconds (500-10000ms)
- `flush_right_after_left_delay_sec` - Right flush after left flush delay in seconds (1-100sec)
- `screen_timeout_sec` - Screen timeout in seconds (10-300sec)
- `system_version` - Firmware version
- `device_id` - Unique device identifier

## Usage Analytics
- `average_flushes_per_day` - Historical average
- `peak_usage_hour` - Hour with most activity
- `busiest_day_of_week` - Most active day
- `total_runtime_hours` - Cumulative operation time

## Network & Connectivity
- `ip_address` - Current IP address
- `mac_address` - Device MAC address
- `server_connection_status` - Upload server connectivity
- `last_server_sync` - Last successful data upload
- `pending_uploads` - Number of queued uploads

## Example JSON Payload Structure

```json
{
  "timestamp": "2024-01-15T10:30:45Z",
  "device_id": "sani_flush_001",
  "system_version": "2.0.1",
  "counters": {
    "flush_count": 1247,
    "daily_flush_count": 23,
    "weekly_flush_count": 156,
    "monthly_flush_count": 678
  },
  "system_timing": {
    "system_start_time": "2024-01-15T08:00:00Z",
    "current_time": "2024-01-15T10:30:45Z",
    "system_uptime_sec": 9045,
    "system_uptime_ms": 9045000
  },
  "workflow_timing": {
    "workflow_start_time": "2024-01-15T08:15:30Z",
    "workflow_active": true,
    "workflow_timespan_sec": 8115,
    "workflow_timespan_ms": 8115000,
    "last_flush_time": "2024-01-15T10:25:12Z",
    "time_since_last_flush_sec": 333,
    "time_since_last_flush_ms": 333000,
    "last_camera_capture_time": "2024-01-15T10:25:12Z",
    "time_since_last_capture_sec": 333,
    "estimated_flushes_remaining_today": 156,
    "average_flush_interval_sec": 352,
    "projected_end_of_day_flush_count": 1403
  },
  "cameras": {
    "last_image_captured": "2024-01-15_102512_flush_1247_LFT_001.png",
    "last_image_time": "2024-01-15T10:25:12Z",
    "total_images_captured": 2494,
    "images_captured_today": 46,
    "camera_status": {
      "left01": "online",
      "left02": "online", 
      "right01": "offline",
      "right02": "online"
    }
  },
  "system_health": {
    "wifi_status": "connected",
    "wifi_signal_strength": -45,
    "free_memory": 234567,
    "cpu_temperature": 42.5,
    "system_status": "ready"
  },
  "settings": {
    "flush_relay_time_lapse_ms": 3000,
    "flush_workflow_repeat_sec": 120,
    "waste_qty_per_flush_ml": 50,
    "pic_every_n_flushes_count": 2,
    "waste_repo_pump_delay_sec": 7,
    "camera_pic_delay_ms": 2500,
    "flush_right_after_left_delay_sec": 5,
    "screen_timeout_sec": 60,
    "auto_flush_enabled": false
  },
  "image_navigation": {
    "current_image_number": 1247,
    "next_image_number": 1248,
    "previous_image_number": 1246,
    "image_url_template": "/image/{image_number}_image",
    "current_image_url": "/image/1247_image",
    "flush_image_count": 2494,
    "manual_image_count": 156
  },
  "network": {
    "ip_address": "192.168.4.100",
    "server_connection_status": "connected",
    "last_server_sync": "2024-01-15T10:25:15Z",
    "pending_uploads": 0
  }
}
```