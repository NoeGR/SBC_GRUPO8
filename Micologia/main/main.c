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
#include "wifi.c"
#include "respuestas_bot.c"
#include "credentials.h"


// Pines y sensores
#define RIEGO_PIN          GPIO_NUM_23  
#define HUMEDAD_SENSOR_PIN ADC1_CHANNEL_7  
#define TURBIDEZ_SENSOR_PIN ADC1_CHANNEL_6  // (GPIO 34).
#define DHT11_PIN          GPIO_NUM_19  
#define DHT_TYPE DHT_TYPE_DHT11 
#define WATER_SENSOR GPIO_NUM_21

#define INTERVALO_TRANSMISION 300 // Intervalo (segundos) entre transmisiones a ThingsBoard.

static const char *TAG = "Micologia";

static esp_adc_cal_characteristics_t adc1_chars;

bool wifi_activado = false;  


float humedadValor = 0.0, turbidezValor = 100, temperaturaValor = 0;

bool riegoActivo = false;
bool riegoManual = false;  
float humedad_umbral = 65.0; 
int tiempo_espera_manual = 5;
int nivelAgua=0;

void init_sensor_agua(){
	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode= GPIO_MODE_INPUT;
	io_conf.pin_bit_mask = (1ULL << WATER_SENSOR);
	io_conf.pull_down_en=GPIO_PULLDOWN_DISABLE;
	io_conf.pull_up_en=GPIO_PULLUP_ENABLE;
	gpio_config(&io_conf);
}
void leer_nivel_agua(){
	 nivelAgua=gpio_get_level(WATER_SENSOR);
	if(!nivelAgua){
		printf("El recipiente de agua esta vacio\n");
	}else{
	printf("El recipiente aun contiene agua\n");
	}
	 
}
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
    leer_nivel_agua();
    actualizar_lcd(humedadValor, temperaturaValor, turbidezValor);
    controlar_riego();
}


 void principal(void *pvParameters) {
    time_t ultimaTransmision = time(NULL);
    char comando[50], respuesta[200];
	
    while (1) {
        medir_ambiente(); 
      
        	if (difftime(time(NULL), ultimaTransmision) > INTERVALO_TRANSMISION) {
	            ultimaTransmision = time(NULL);
	            // Enviar datos a ThingsBoard
	            mqtt_mandar_datos(humedadValor, turbidezValor, temperaturaValor, riegoActivo, nivelAgua);
	            printf("Datos enviados a ThingsBoard.\n");
	
	            // Entrar en Deep Sleep
	            printf("Preparando para entrar en Deep Sleep por %d segundos...\n", INTERVALO_TRANSMISION);
				https_telegram_sendMessage("Entrando en Deep Sleep...");	           
	            esp_wifi_stop();  
	            esp_sleep_enable_timer_wakeup(INTERVALO_TRANSMISION * 1000000ULL);
	            printf("Entrando en Deep Sleep...\n");
		        esp_deep_sleep_start();  
	        }
	        if(humedadValor >= humedad_umbral && (temperaturaValor >=20 && temperaturaValor <=30)){
	         
	            printf("Preparando para entrar en Deep Sleep por %d segundos...\n", INTERVALO_TRANSMISION);
	            https_telegram_sendMessage("Entrando en Deep Sleep...");
	            esp_wifi_stop();  
	            esp_sleep_enable_timer_wakeup(1800 * 1000000ULL);
	            printf("Entrando en Deep Sleep...\n");
		        esp_deep_sleep_start();  
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

 

        vTaskDelay(500 / portTICK_PERIOD_MS);  
    }
}


void app_main(void) {
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
    init_sensor_agua();
    lcd_init();
    gpio_set_direction(RIEGO_PIN, GPIO_MODE_OUTPUT);

    // Iniciar conexiones
    init_wifi_system();
    iniciar_mqtt();  
    iniciar_http(); 
   
    xTaskCreate(principal, "Flujo Principal", 4096, NULL, 9, NULL);
}
