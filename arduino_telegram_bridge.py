import requests
import serial
import time
import threading

TOKEN   = "8457967165:AAHHgF2kznpa8OGUAJnlUpaSS0HdE8pjFPQ"
CHAT_ID = "8772375653"

def send_telegram(msg):
    url = f"https://api.telegram.org/bot{TOKEN}/sendMessage"
    try:
        requests.post(url, data={"chat_id": CHAT_ID, "text": msg}, timeout=5)
        print("Telegram sent:", msg)
    except Exception as e:
        print("Telegram error:", e)

def get_updates(offset=None):
    url    = f"https://api.telegram.org/bot{TOKEN}/getUpdates"
    params = {"timeout": 10, "allowed_updates": ["message"]}
    if offset is not None:
        params["offset"] = offset
    try:
        resp = requests.get(url, params=params, timeout=15)
        return resp.json().get("result", [])
    except Exception as e:
        print("getUpdates error:", e)
        return []

TELEGRAM_COMMANDS = {
    "doors open":    "DOORS_OPEN",
    "doors close":   "DOORS_CLOSE",
    "doors free":    "DOORS_FREE",
    "windows open":  "WINDOWS_OPEN",
    "windows close": "WINDOWS_CLOSE",
    "windows free":  "WINDOWS_FREE",
    "fan on":        "FAN_ON",
    "fan off":       "FAN_OFF",
    "fan free":      "FAN_FREE",
    "buzzer on":     "BUZZER_ON",
    "buzzer off":    "BUZZER_OFF",
    "buzzer free":   "BUZZER_FREE",
    "pump on":       "WATERPUMP_ON",
    "pump off":      "WATERPUMP_OFF",
    "pump free":     "WATERPUMP_FREE",
    "status":        "STATUS",
    "led red on":        "LED_RED_ON",
    "led blue on":        "LED_BLUE_ON",
    "led green on":        "LED_GREEN_ON",
    "led yellow on":        "LED_YELLOW_ON",
    "led red off":        "LED_RED_OFF",
    "led blue off":        "LED_BLUE_OFF",
    "led green off":        "LED_GREEN_OFF",
    "led yellow off":        "LED_YELLOW_OFF"
}

HELP_TEXT = (
    "📋 Available commands:\n\n"
    "🚪 Doors:   doors open | doors close | doors free\n"
    "🪟 Windows: windows open | windows close | windows free\n"
    "💨 Fan:     fan on | fan off | fan free\n"
    "🔔 Buzzer:  buzzer on | buzzer off | buzzer free\n"
    "💧 Pump:    pump on | pump off | pump free\n"
    "📊 Info:    status\n"
    "💡 LEDs:\n"
    "   led red on | led red off\n"
    "   led blue on | led blue off\n"
    "   led green on | led green off\n"
    "   led yellow on | led yellow off\n\n"
    "ℹ️  'free' releases manual control and lets automatic mode take over again."
)

ser         = serial.Serial('COM4', 9600, timeout=1)
serial_lock = threading.Lock()
time.sleep(2)

def send_to_arduino(command: str):
    with serial_lock:
        ser.write((command + "\n").encode())
    print(f"→ Arduino: {command}")

last_message = ""
last_time    = 0
COOLDOWN     = 5

ALERT_MESSAGES = {"MOTION", "FIRE", "SMOKE"}

def arduino_listener():
    global last_message, last_time
    print("Listening to Arduino...")
    while True:
        try:
            line = ser.readline().decode(errors='ignore').strip()
            if not line:
                continue
            print("Arduino:", line)
            now = time.time()
            if line != last_message or (now - last_time) > COOLDOWN:
                if line == "MOTION":
                    send_telegram("🚨 Motion detected!")
                elif line == "FIRE":
                    send_telegram("🔥 FIRE ALERT!")
                elif line == "SMOKE":
                    send_telegram("💨 Smoke detected!")
                elif line.startswith("STATUS:"):
                    formatted = line.replace("STATUS:", "").replace("|", "\n")
                    send_telegram(f"📊 System status:\n{formatted}")
                else:
                    send_telegram(f"📡 Arduino: {line}")
                last_message = line
                last_time    = now
        except Exception as e:
            print("Arduino read error:", e)
            time.sleep(1)

def telegram_listener():
    print("Polling Telegram for commands...")
    offset = None
    while True:
        updates = get_updates(offset)
        for update in updates:
            offset  = update["update_id"] + 1
            msg     = update.get("message", {})
            chat_id = str(msg.get("chat", {}).get("id", ""))
            text    = msg.get("text", "").strip().lower()
            if not text:
                continue
            print(f"Telegram [{chat_id}]: {text}")
            if chat_id != CHAT_ID:
                print("  ↳ Ignored (unauthorised chat)")
                continue
            if text in ("help", "/help", "/start"):
                send_telegram(HELP_TEXT)
            elif text in TELEGRAM_COMMANDS:
                arduino_cmd = TELEGRAM_COMMANDS[text]
                send_to_arduino(arduino_cmd)
                send_telegram(f"✅ Sent to Arduino: {arduino_cmd}")
            else:
                send_telegram(f"❓ Unknown command: '{text}'\nSend 'help' to see available commands.")

if __name__ == "__main__":
    send_telegram("🤖 Bridge started. Send 'help' for commands.")
    t1 = threading.Thread(target=arduino_listener,  daemon=True)
    t2 = threading.Thread(target=telegram_listener, daemon=True)
    t1.start()
    t2.start()
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nBridge stopped.")
        ser.close()
