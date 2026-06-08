"""PlatformIO pre-build: load .env into CPPDEFINES (secrets stay out of git)."""
Import("env")

from pathlib import Path

ALLOWED_KEYS = frozenset({
    "MQTT_BROKER",
    "MQTT_PORT",
    "MQTT_TOPIC",
    "DEVICE_NAME",
    "DEVICE_TYPE",
    "LED_PIN",
})

INT_KEYS = frozenset({"MQTT_PORT", "DEVICE_TYPE", "LED_PIN"})


def parse_line(line):
    line = line.strip()
    if not line or line.startswith("#") or "=" not in line:
        return None
    key, _, value = line.partition("=")
    key = key.strip()
    # Strip inline comments and surrounding quotes
    if "#" in value:
        value = value.split("#", 1)[0]
    value = value.strip().strip('"').strip("'")
    if not key or key not in ALLOWED_KEYS:
        return None
    return key, value


env_path = Path(env["PROJECT_DIR"]) / ".env"
if not env_path.is_file():
    print("WARN: .env not found — copy .env.example to .env and fill in your values")
else:
    defines = []
    for line in env_path.read_text(encoding="utf-8").splitlines():
        parsed = parse_line(line)
        if parsed is None:
            continue
        key, value = parsed
        if key in INT_KEYS:
            defines.append((key, int(value)))
        else:
            defines.append((key, f'\\"{value}\\"'))
    if defines:
        env.Append(CPPDEFINES=defines)
        print("Loaded from .env:", ", ".join(k for k, _ in defines))
