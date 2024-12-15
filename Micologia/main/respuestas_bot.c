#include <stdio.h>
#include <string.h>

int respuestas_bot(char pregunta[], char *mensaje, float datos[]) {
    int opcion = 0;
    char aux[200] = {0};

    // Comando: /datos
    if (strcmp(pregunta, "/datos") == 0) {
        sprintf(aux, "Humedad: %.0f %%\nTemperatura ambiente: %.0fC\nTurbidez del Agua: %.0f%% claridad\nEstado del Riego: %s",
                datos[0], datos[1],  datos[2],(datos[3] == 1.0) ? "Activo" : "Inactivo");
        opcion = 1;  // Mostrar datos generales
    }
    // Comando: /riego_manual
    else if (strcmp(pregunta, "/riego_manual") == 0) {
        sprintf(aux, "Activando riego manual...");
        opcion = 2;  // Activar riego manual
    }
    // Comando: /estado
    else if (strcmp(pregunta, "/estado") == 0) {
        sprintf(aux, "Estado del sistema:\nHumedad: %.0f%%\nTemperatura: %.0fC\nTurbidez: %.0f%% claridad \nRiego: %s",
                datos[0], datos[1], datos[2], (datos[3] == 1.0) ? "Activo" : "Inactivo");
        opcion = 3;  // Consultar estado general
    }
    // Comando: /config_riego
    else if (strncmp(pregunta, "/config_riego", 13) == 0) { // Compara solo el prefijo
    float nuevo_umbral;
    if (sscanf(pregunta, "/config_riego %f", &nuevo_umbral) == 1) {
        sprintf(aux, "Umbral de riego configurado a %0.1f %% de humedad.", nuevo_umbral);
        datos[4] = nuevo_umbral;  
        opcion = 4;  // Configurar el umbral de riego
    } else {
        sprintf(aux, "Error: formato incorrecto. Use /config_riego <valor>");
    }
}
    // Comando: /parar_riego
    else if (strcmp(pregunta, "/parar_riego") == 0) {
        sprintf(aux, "Desactivando riego manual...");
        opcion = 5;  // Desactivar riego manual
    }
    else {
        sprintf(aux, "Comando no reconocido.");
        opcion = 0;  
    }

    strncpy(mensaje, aux, 200);
    return opcion;
}

void resp_riego(int estado, char *mensaje, float humedad) {
    char aux[100];

    if (estado == 1)
        sprintf(aux, "Riego activado. Humedad actual: %0.1f %%", humedad);
    else
        sprintf(aux, "Riego detenido. Humedad actual: %0.1f %%", humedad);

    strncpy(mensaje, aux, 100);
}
