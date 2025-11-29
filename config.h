#ifndef __CONFIG_H__
#define __CONFIG_H__

// WiFi Configuration (使用 2.4G WiFi)
#define WIFI_SSID "openwrt2"
#define WIFI_PASSWORD "1505194659"

// API Configuration
// WhisperLive STT (Real-time Speech-to-Text via WebSocket)
#define WHISPERLIVE_WS_URL "ws://192.168.1.151:9090/"

// DeepSeek API
#define DEEPSEEK_API_URL "https://api.deepseek.com/v1/chat/completions"
#define DEEPSEEK_API_KEY "sk-9725ac0faa5947fd98a4c54649f1e2c6"

// Fish Speech TTS API
#define TTS_API_URL "http://192.168.1.151:8080/v1/tts"
#define TTS_FORMAT "wav"           // Output format: wav, mp3, or pcm
#define TTS_SAMPLE_RATE 16000      // Output sample rate (should match AUDIO_SAMPLE_RATE)
#define TTS_REFERENCE_ID "kill"  // 固定音色 ID

// Button GPIO
#define BUTTON_PIN GPIO_PIN_2

#endif // __CONFIG_H__
