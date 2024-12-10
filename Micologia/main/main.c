#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "dht.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_sleep.h"
//#include "ADS1115.h"
#include "display.h"

#include "conexion_mqtt_thingsboard.c"
#include "conexion_http_telegram.c"
#include "respuestas_bot.c"
#include "credentials.h"


// Pines y sensores
#define RIEGO_PIN          GPIO_NUM_23  
#define HUMEDAD_SENSOR_PIN ADC1_CHANNEL_7  
#define TURBIDEZ_SENSOR_PIN ADC1_CHANNEL_6  // (GPIO 34).
#define DHT11_PIN          GPIO_NUM_19  
#define DHT_TYPE DHT_TYPE_DHT11 

#define INTERVALO_TRANSMISION 30 // Intervalo (segundos) entre transmisiones a ThingsBoard.

static const char *TAG = "Micologia";

static esp_adc_cal_characteristics_t adc1_chars;

bool wifi_activado = false;  


float humedadValor = 0.0, turbidezValor = 100, temperaturaValor = 0;

bool riegoActivo = false;
bool riegoManual = false;  
float humedad_umbral = 65.0; 
int tiempo_espera_manual = 5;

//enum estadoSist {TRANSMITIR, RECIBIR};  // Modos de trabajo del sistema.
//enum estadoSist modo = TRANSMITIR;

void configure_adc() {
    adc1_config_width(ADC_WIDTH_BIT_12); 
    adc1_config_channel_atten(HUMEDAD_SENSOR_PIN, ADC_ATTEN_DB_12); 
    adc1_config_channel_atten(TURBIDEZ_SENSOR_PIN, ADC_ATTEN_DB_12); 
}
int read_analog_value(adc1_channel_t channel) {
    return adc1_get_raw(channel);
}

void medir_humedad(void) {
   float humedadRaw = (float)read_analog_value(HUMEDAD_SENSOR_PIN);
    humedadValor = ((4095-humedadRaw) * 100)/4095;  
    printf("Humedad del suelo: %.2f %%\n", humedadValor);
        
}

void medir_turbidez(void) {
    float sum=0; 
    for(int i=0; i<10;i++){
     turbidezValor = (float)read_analog_value(TURBIDEZ_SENSOR_PIN);
     sum+=turbidezValor;
     vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    sum=sum/10;
    turbidezValor = (sum*100)/4095;
    printf("Claridad del agua: %.0f\n", turbidezValor);
}


void medir_temperatura(void) {
	int16_t tempAux = 0;
	int16_t humidity = 0;
    if (dht_read_data(DHT_TYPE, DHT11_PIN, &humidity, &tempAux) == ESP_OK) {
            temperaturaValor = tempAux / 10.0;

          printf("Temperatura: %.0fC\n", temperaturaValor);


        } else {
            ESP_LOGE(TAG, "Error al leer el sensor DHT11");
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); 
}

void enviar_alerta_humedad_baja(float humedad) {
    char alerta[200];
    snprintf(alerta, sizeof(alerta), "⚠️ Alerta: La humedad está por debajo del umbral!\nHumedad actual: %.2f %%\nUmbral configurado: %.2f %%", humedad, humedad_umbral);
    https_telegram_sendMessage(alerta);
}

void controlar_riego() {
    static time_t inicio_espera_manual = 0;
    static bool alerta_humedad_baja_enviada = false;

    if (riegoManual) {
        if (riegoActivo) {
            // Si la bomba está encendida y la humedad supera el umbral, volver al modo automático
            if (humedadValor >= humedad_umbral) {
                riegoManual = false;
                //guardar_estado_riego(riegoManual);
                gpio_set_level(RIEGO_PIN, 0);  // Apagar la bomba
                riegoActivo = false;
                printf("Riego manual desactivado. Volviendo a modo automático.\n");
                https_telegram_sendMessage("Riego manual desactivado. Volviendo a modo automático.");
            }
        } else {
            // Si la bomba está apagada y la humedad está por debajo del umbral, iniciar el temporizador
            if (humedadValor < humedad_umbral && inicio_espera_manual == 0) {
                inicio_espera_manual = time(NULL);
            }

            // Si ha pasado el tiempo de espera sin acción, pasar al modo automático y encender la bomba
            if (inicio_espera_manual > 0 && (time(NULL) - inicio_espera_manual) >= tiempo_espera_manual) {
                riegoManual = false;
                gpio_set_level(RIEGO_PIN, 1);  // Encender la bomba
                riegoActivo = true;
                inicio_espera_manual = 0;
                char mensaje[200];
                snprintf(mensaje, sizeof(mensaje),
                         "Tiempo de espera agotado.\nHumedad: %.2f %%.\nActivando riego automatico.\n",
                         humedadValor);
                printf("%s\n", mensaje);
                https_telegram_sendMessage(mensaje);
            }
        }
    } else {
        // Modo automático: controlar riego según el umbral de humedad
        if (humedadValor < humedad_umbral && !riegoActivo) {
            gpio_set_level(RIEGO_PIN, 1);  // Encender la bomba
            riegoActivo = true;
            printf("Riego automatico activado. Humedad: %.2f %%\n", humedadValor);
            https_telegram_sendMessage("Riego automático activado debido a humedad baja.");
        } else if (humedadValor >= humedad_umbral && riegoActivo) {
            gpio_set_level(RIEGO_PIN, 0);  // Apagar la bomba
            riegoActivo = false;
            printf("Riego automatico desactivado. Humedad: %.2f %%\n", humedadValor);
            https_telegram_sendMessage("Riego automático desactivado debido a humedad suficiente.");
        }
    }

    if (humedadValor < humedad_umbral) {
        if (!alerta_humedad_baja_enviada) {
            enviar_alerta_humedad_baja(humedadValor);
            alerta_humedad_baja_enviada = true;
        }
    } else if (alerta_humedad_baja_enviada && humedadValor >= humedad_umbral) {
        // Enviar mensaje cuando la humedad se estabilice
        char mensaje[200];
        snprintf(mensaje, sizeof(mensaje),
                 "✅ Humedad estabilizada.\nHumedad actual: %.2f %%.\nUmbral configurado: %.2f %%",
                 humedadValor, humedad_umbral);
        https_telegram_sendMessage(mensaje);
        alerta_humedad_baja_enviada = false;
    }
}

void medir_ambiente(void) {
    medir_humedad();
    medir_turbidez();
    medir_temperatura();
    actualizar_lcd(humedadValor, temperaturaValor, turbidezValor);
    //guardar_ultima_lectura(humedadValor, turbidezValor, temperaturaValor);
    controlar_riego();
}


// Flujo principal
/*void principal(void *pvParameters) {
    clock_t ultimaTransmision = clock();  // Controlará el tiempo de transmisión a ThingsBoard.
    clock_t ultimaComprobacionTelegram = clock();
    const int INTERVALO_TELEGRAM = 10;
    char comando[50], respuesta[200];  
	//bool alerta_enviada = false; 
	 
    while (1) {
        medir_ambiente();
        
		if (humedadValor < humedad_umbral) {
            if (!alerta_enviada) {
                enviar_alerta_humedad_baja(humedadValor);
                alerta_enviada = true; // Marcar como enviada para evitar múltiples mensajes.
            }
        } else {
            alerta_enviada = false; // Resetear la alerta si la humedad vuelve a estar por encima del umbral.
        }
        
		        if ((clock() - ultimaTransmision) / CLOCKS_PER_SEC > INTERVALO_TRANSMISION) {
		            ultimaTransmision = clock();
		
		            // Enviar datos a ThingsBoard
		            mqtt_mandar_datos(humedadValor, turbidezValor, temperaturaValor, riegoActivo);
		            printf("Datos enviados a ThingsBoard.\n");
		             printf("Apagando Wi-Fi para ahorrar energia...\n");
		            esp_wifi_stop();
    				wifi_activado = false;
		        }
		        
		        if ((clock() - ultimaTransmision) / CLOCKS_PER_SEC > (INTERVALO_TRANSMISION - 5)) { // Reactivar 5 segundos antes
			            if (!wifi_activado) {
			                printf("Reactivando Wi-Fi para la proxima transmision...\n");
			                esp_wifi_start();
			                 wifi_activado = true;
			            }
			        }
		
		        // Comprobación periódica para Telegram
		        if ((clock() - ultimaComprobacionTelegram) / CLOCKS_PER_SEC > INTERVALO_TELEGRAM) {
		            ultimaComprobacionTelegram = clock();
		
		            // Activar Wi-Fi temporalmente para comandos de Telegram
		            if (!wifi_activado) {
		                printf("Activando Wi-Fi para comprobar comandos de Telegram...\n");
		                esp_wifi_start();
		                wifi_activado = true;  
		            }
			        
                https_telegram_getUpdates(comando);

                if (strlen(comando) > 0) {
                    float datos[5] = {humedadValor, temperaturaValor, turbidezValor, riegoActivo, [4]=humedad_umbral};
                    int opc = respuestas_bot(comando, respuesta, datos);

                    https_telegram_sendMessage(respuesta);

                    switch (opc) {
                        case 2:  // Activar riego manual.
                            gpio_set_level(RIEGO_PIN, 1);
                            riegoManual = true;  // Cambia a modo manual
                            riegoActivo = true;
                            //guardar_estado_riego(riegoManual); 
                            break;
                        case 4:  // Configurar umbral de humedad.
                            humedad_umbral = datos[4];
                            riegoManual = false; 
                            //guardar_umbral_humedad(humedad_umbral);
                            break;
                        case 5:  // Detener riego manual.
                            gpio_set_level(RIEGO_PIN, 0);
                            riegoManual = true;  // Permanece en modo manual
                            riegoActivo = false;
                            //guardar_estado_riego(riegoManual);
                            break;
                        default:
                            break;
                    }
                }
                 // Apagar Wi-Fi después de procesar comandos si no es tiempo de transmisión
            if ((clock() - ultimaTransmision) / CLOCKS_PER_SEC < (INTERVALO_TRANSMISION - 5)) {
                printf("Apagando Wi-Fi despues de comprobar Telegram...\n");
                esp_wifi_stop();
                wifi_activado = false;  
            }
        }

        vTaskDelay(500 / portTICK_PERIOD_MS);  
    }
}*/

 void principal(void *pvParameters) {
    //clock_t ultimaTransmision = clock();  // Controlará el tiempo de transmisión a ThingsBoard.
    time_t ultimaTransmision = time(NULL);
    char comando[50], respuesta[200];

    while (1) {
        medir_ambiente();  // Leer sensores y controlar riego.

        // Transmisión de datos a ThingsBoard
        //clock() - ultimaTransmision) / CLOCKS_PER_SEC > INTERVALO_TRANSMISION
		            //ultimaTransmision = clock();
        	if (difftime(time(NULL), ultimaTransmision) > INTERVALO_TRANSMISION) {
	            ultimaTransmision = time(NULL);
	            // Enviar datos a ThingsBoard
	            mqtt_mandar_datos(humedadValor, turbidezValor, temperaturaValor, riegoActivo);
	            printf("Datos enviados a ThingsBoard.\n");
	
	            // Entrar en Deep Sleep
	            printf("Preparando para entrar en Deep Sleep por %d segundos...\n", INTERVALO_TRANSMISION);
				https_telegram_sendMessage("Entrando en Deep Sleep...");	           
	            esp_wifi_stop();  // Asegúrate de apagar el Wi-Fi antes de Deep Sleep
	            esp_sleep_enable_timer_wakeup(INTERVALO_TRANSMISION * 1000000ULL);
	            printf("Entrando en Deep Sleep...\n");
		        esp_deep_sleep_start();  // Inicia Deep Sleep
	        }
	        if(humedadValor >= humedad_umbral && (temperaturaValor >=20 && temperaturaValor <=30)){
	         // Entrar en Deep Sleep
	            printf("Preparando para entrar en Deep Sleep por %d segundos...\n", INTERVALO_TRANSMISION);
	            https_telegram_sendMessage("Entrando en Deep Sleep...");
	            esp_wifi_stop();  // Asegúrate de apagar el Wi-Fi antes de Deep Sleep
	            esp_sleep_enable_timer_wakeup(120 * 1000000ULL);
	            printf("Entrando en Deep Sleep...\n");
		        esp_deep_sleep_start();  // Inicia Deep Sleep
	        }
	        
            // Procesar comandos de Telegram
            printf("Verificando comandos de Telegram...\n");
            https_telegram_getUpdates(comando);

            if (strlen(comando) > 0) {
                float datos[5] = {humedadValor, temperaturaValor, turbidezValor, riegoActivo, humedad_umbral};
                int opc = respuestas_bot(comando, respuesta, datos);

                https_telegram_sendMessage(respuesta);

                switch (opc) {
                    case 2:  // Activar riego manual.
                        gpio_set_level(RIEGO_PIN, 1);
                        riegoManual = true;  // Cambia a modo manual
                        riegoActivo = true;
                        break;

                    case 4:  // Configurar umbral de humedad.
                        humedad_umbral = datos[4];
                        guardar_umbral_humedad(humedad_umbral);  // Guardar en NVS
                        break;

                    case 5:  // Detener riego manual.
                        gpio_set_level(RIEGO_PIN, 0);
                        riegoManual = false;  // Cambia a modo automático
                        riegoActivo = false;
                        break;

                    default:
                        break;
                }
            }

 

        vTaskDelay(500 / portTICK_PERIOD_MS);  // Pequeña espera antes del próximo ciclo.
    }
}


void app_main(void) {
	// Verificar la causa del despertar
    /*esp_sleep_wakeup_cause_t causa_despertar = esp_sleep_get_wakeup_cause();
    if (causa_despertar == ESP_SLEEP_WAKEUP_TIMER) {
        printf("Despertado por temporizador. Reiniciando ciclo...\n");
    } else {
        printf("Reinicio normal o causa desconocida.\n");
    }

    // Inicializar NVS
    inicializar_nvs();

    // Cargar configuraciones persistentes
    humedad_umbral = cargar_umbral_humedad();
    riegoManual = cargar_estado_riego();
    cargar_ultima_lectura(&humedadValor, &turbidezValor, &temperaturaValor);*/
     esp_sleep_wakeup_cause_t causa_despertar = esp_sleep_get_wakeup_cause();
    
    if (causa_despertar == ESP_SLEEP_WAKEUP_TIMER) {
        printf("Despertado por temporizador. Reiniciando ciclo...\n");
    } else {
        printf("Reinicio normal o causa desconocida.\n");
    }
    // Inicializar NVS
    inicializar_nvs();

    // Cargar el umbral de humedad desde NVS
    humedad_umbral = cargar_umbral_humedad();
	
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    // Configuración del ADC
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
    configure_adc();

    lcd_init();
    
    
    // Configuración de GPIO
    gpio_set_direction(RIEGO_PIN, GPIO_MODE_OUTPUT);


    // Iniciar conexiones
    iniciar_mqtt();  
    iniciar_http(); 

    // Inicia la tarea principal
    xTaskCreate(principal, "Flujo Principal", 4096, NULL, 9, NULL);
}
