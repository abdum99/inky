#include "../lib/eink/eink.h"
#include "../lib/Utils/Debug.h"

#include <math.h>

#include <stdlib.h>     //exit()

#include <mosquitto.h>
#include <mqtt_protocol.h>

#define OSIRIS_HOST "osiris.local"
#define OSIRIS_MQTT_PORT 1883
#define KEEP_ALIVE 60

#define INKY_TOPIC "/inky/bmp"
#define INKY_QOS 2 // QOS 2: delivered exactly once

#define ONE_MINUTE (60 * 1000)

IT8951_Dev_Info Dev_Info;
struct mosquitto *mqtt = NULL; // mosquitto client
static int RECV_COUNT = 0;

void on_message(struct mosquitto *mqtt, void *obj, const struct mosquitto_message *message) {
    Debug("WOOOAH GOT A MESSAGE. HOLY SHIT\n");
    Debug("LO\n");
    Debug("Topic: %s\n", message->topic);
    fflush(stdout);

    FILE *fptr;
    fptr = fopen("../pic/img.bmp", "wb+");
    fwrite(message->payload, message->payloadlen, sizeof(uint8_t), fptr);
    fclose(fptr);

    //Show a bmp file
    //1bp use A2 mode by default, before used it, refresh the screen with WHITE
    eInk_BMP(&Dev_Info, BitsPerPixel_4);
}

void on_connect(struct mosquitto *mosq, void *obj, int rc) {
	printf("ID: %d\n", * (int *) obj);
	if(rc) {
		printf("Error with result code: %d\n", rc);
		exit(-1);
	}
	mosquitto_subscribe(mosq, NULL, "test/t1", 0);
}

// NEED TO FREE MQTT !!!
static int init_mqtt() {
    Debug("Initting MQTT\n");
    mosquitto_lib_init();
    mqtt = mosquitto_new(NULL, true, NULL);

    return 0;
}

static int connect_mqtt() {
    mosquitto_message_callback_set(mqtt, on_message);
    // mosquitto_connect_callback_set(mqtt, on_connect);
    int status = mosquitto_connect(mqtt, OSIRIS_HOST, OSIRIS_MQTT_PORT, KEEP_ALIVE);
    if (status != MOSQ_ERR_SUCCESS) {
        Debug("COULD NOT CONNECT\n");
        Debug("STATUS: %s\n", mosquitto_strerror(status));
    }
    Debug("Connected to Osiris MQTT Broker\n");

    sleep(5);

    status = mosquitto_subscribe(mqtt, &RECV_COUNT, INKY_TOPIC, 2);
    if (status != MOSQ_ERR_SUCCESS) {
        Debug("COULD NOT SUBSRIBE!\n");
        Debug("STATUS: %s\n", mosquitto_strerror(status));
    }
    Debug("Subscribed to %s\n", INKY_TOPIC);
    return 0;
}

int main(int argc, char *argv[])
{
    if (!eInk_Init(&Dev_Info)) {
        return -1;
    };

    init_mqtt();
    connect_mqtt();

	mosquitto_loop_start(mqtt);
	printf("Press Enter to quit...\n");
	getchar();
	mosquitto_loop_stop(mqtt, true);

    eInk_Shutdown();
    
    return 0;
}
