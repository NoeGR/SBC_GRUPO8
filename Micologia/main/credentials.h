#ifndef CREDENTIALS_H
#define CREDENTIALS_H


#define PROYECTO    "SBC22-T-05 -> Huerto Hidropónico"

// Credenciales del WiFi.
#define WIFI_NAME       "PWifi"
#define WIFI_PASSWORD   "gamboa12"

// Credenciales de ThingsBoard.
#define THINGSBOARD_SERVER  "mqtt://demo.thingsboard.io"
#define THINGSBOARD_TOKEN   "UQGpPHWI0Tfn1x1C6BYH"

// Credenciales de Telegram y el bot.
#define TELEGRAM_SERVER "https://api.telegram.org"
#define BOT_TOKEN       "7521369560:AAGLCFVOYd03m_Ij5WRrwhP22FGr6Uk1TbM"
#define BOT_CHAT_ID     -1002336930953  // Id del chat dónde recibiremos los mensajes del bot.
#define BOT_URL         TELEGRAM_SERVER "/bot" BOT_TOKEN   // A falta del método.

#endif