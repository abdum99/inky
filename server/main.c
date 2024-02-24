#include "../lib/eink/eink.h"
#include "../lib/Utils/Debug.h"
#include "../lib/Config/DEV_Config.h"


#include <math.h>

#include <signal.h>

#include <stdlib.h>  // exit()
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>  // sleep()
#include <pthread.h>
#include <sys/time.h>

#include <mosquitto.h>
#include <mqtt_protocol.h>

#define OSIRIS_HOST "10.0.0.14"
#define OSIRIS_MQTT_PORT 1883
#define KEEP_ALIVE 60

#define INKY_TOPIC "/inky/bmp"
#define RESPONSE_TOPIC "res/inky/bmp"
#define INKY_QOS 2 // QOS 2: delivered exactly once
#define SUCCESS_RESPONSE "success"
#define FAIL_RESPONSE "fail"

#define IMG_FILE "./pic/img.bmp"

#define ONE_MINUTE (60 * 1000)

pthread_mutex_t device_lock;

IT8951_Dev_Info Dev_Info;
struct mosquitto *mosq = NULL; // mosquitto client
static int RECV_COUNT = 0;

char *response_topic;
uint8_t *correlation_data;
uint16_t correlation_data_len;

int restart_eink() {
    //Init the BCM2835 Device
    Debug("trying to init the DEV module...\n");
    if (DEV_Module_Init() != 0) {
        Debug("failed to init the DEV module\n");
    }

    Debug("trying to shutdown the eInk...\n");
    eInk_Shutdown();
    Debug("eInk should have shutdown now\n");

    Debug("trying to init eInk again...\n");
    if (!eInk_Init(&Dev_Info)) {
        Debug("Failed to init eInk\n");
        return -1;
    };
    Debug("RECOVERY SUCCESS\n");
    return 0;
}

// VERY BOLD OF ME, but SIGSEGV will only happen when drawing
// so we hold the lock and should unlock 
void sigsegv_handler (int signum) {
    // int acquired = pthread_mutex_trylock(&device_lock)
    Debug("Received a SIGSEGV! signum=%d\n", signum);
    Debug("Gonna send a strongly-worded response to osiris...\n");
    int res = mosquitto_publish(
        mosq,
        NULL,
        response_topic,
        sizeof FAIL_RESPONSE,
        FAIL_RESPONSE,
        INKY_QOS,
        true
    );
    if (res == MOSQ_ERR_SUCCESS) {
        Debug("Success");
    } else {
        Debug("couldnt send message to osiris, res %s\n", mosquitto_strerror(res));
    }

    Debug("Attempting to restart the module...\n");
    res = restart_eink();
    if (res != 0) {
        Debug("sighs, we tried. needs a manual restart\n");
        Debug("or better ... a crash...\n");
        Debug("ig see you on the other side!\n");
    }
    Debug("Yayyy, restarted\n");
    pthread_mutex_unlock(&device_lock);
}

int extract_mqtt_properties(
    const mosquitto_property *proplist
) {
    // read MQTTv5 properties
    const mosquitto_property *prop;
    for(prop = proplist; prop != NULL; prop = mosquitto_property_next(prop)) {
        const mosquitto_property *res;

        if(mosquitto_property_identifier(prop) == MQTT_PROP_RESPONSE_TOPIC) {
            res = mosquitto_property_read_string(
                prop,
                MQTT_PROP_RESPONSE_TOPIC,
                &response_topic,
                false
            );
            if (res == NULL) {
                Debug("response topic could not be read. returning");
                return -1;
            }

        } else if (mosquitto_property_identifier(prop) == MQTT_PROP_CORRELATION_DATA) {
            res = mosquitto_property_read_binary(
                prop,
                MQTT_PROP_CORRELATION_DATA,
                (void **)&correlation_data,
                &correlation_data_len,
                false
            );
        }
    }

    return 0;
}

void on_message_v5(
    struct mosquitto *mosq,
    void *obj,
    const struct mosquitto_message *message,
    const mosquitto_property *proplist
) {
    pthread_mutex_lock(&device_lock);
    Debug("Topic: %s\n", message->topic);
    Debug("payloadlen: %u\n", message->payloadlen);

    // timing
    struct timeval stop, start;

    // ============ START TIMING =============
    gettimeofday(&start, NULL);
    // ***************************************
    int res = extract_mqtt_properties(proplist);
    if (res != 0) {
        Debug("returning...\n");
        pthread_mutex_unlock(&device_lock);
        return;
    }
    // ***************************************
    gettimeofday(&stop, NULL);
    printf(
        "TIME::extracting mqtt props: %lu ms\n",
        ((stop.tv_sec - start.tv_sec) * 1000000 +
        (stop.tv_usec - start.tv_usec))
        / 1000
    ); 
    // ============ STOP TIMING =============


    // ============ START TIMING =============
    gettimeofday(&start, NULL);
    // ***************************************
    FILE *fptr = fopen(IMG_FILE, "wb+");
    if (!fptr) {
        Debug("Unable to open file!\n");
        pthread_mutex_unlock(&device_lock);
        return;
    }
    fwrite(message->payload, message->payloadlen, sizeof(uint8_t), fptr);
    fclose(fptr);
    // ***************************************
    gettimeofday(&stop, NULL);
    printf(
        "TIME::writing file: %lu ms\n",
        ((stop.tv_sec - start.tv_sec) * 1000000 +
        (stop.tv_usec - start.tv_usec))
        / 1000
    ); 
    // ============ STOP TIMING =============

    Debug("Received image in %s\n", IMG_FILE);

    // ============ START TIMING =============
    gettimeofday(&start, NULL);
    // ***************************************
    //Show a bmp file
    eInk_BMP(&Dev_Info, BitsPerPixel_4);
    // ***************************************
    gettimeofday(&stop, NULL);
    printf(
        "TIME::drawing BMP: %lu ms\n",
        ((stop.tv_sec - start.tv_sec) * 1000000 +
        (stop.tv_usec - start.tv_usec))
        / 1000
    ); 
    // ============ STOP TIMING =============
    pthread_mutex_unlock(&device_lock);
    res = mosquitto_publish(
        mosq,
        NULL,
        RESPONSE_TOPIC,
        sizeof FAIL_RESPONSE,
        (const void*)FAIL_RESPONSE,
        INKY_QOS,
        true
    );
    if (res == MOSQ_ERR_SUCCESS) {
        Debug("Success");
    } else {
        Debug("couldnt send message to osiris, res %s\n", mosquitto_strerror(res));
    }
}

void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
    Debug("Topic: %s\n", message->topic);
    Debug("payloadlen: %u\n", message->payloadlen);

    FILE *fptr = fopen(IMG_FILE, "wb+");
    if (!fptr) {
        Debug("Unable to open file!\n");
        return;
    }
    fwrite(message->payload, message->payloadlen, sizeof(uint8_t), fptr);
    fclose(fptr);
    Debug("Received image in %s\n", IMG_FILE);

    //Show a bmp file
    eInk_BMP(&Dev_Info, BitsPerPixel_4);
}

void on_connect(struct mosquitto *mosq, void *obj, int rc) {
    Debug("Connected to Osiris MQTT Broker\n");
	Debug("ID: %d\n", * (int *) obj);
	if(rc) {
		Debug("Error with result code: %d\n", rc);
		exit(-1);
	}
}

void on_log(struct mosquitto *mosq, void *obj, int level, const char *msg) {
    Debug("MOSQ: %d: %s\n", level, msg);
}

// NEED TO FREE MQTT !!!
static int init_mqtt() {
    Debug("Initting MQTT\n");
    mosquitto_lib_init();
    mosq = mosquitto_new(NULL, true, NULL);
    mosquitto_int_option(mosq, MOSQ_OPT_PROTOCOL_VERSION, MQTT_PROTOCOL_V5);

    return 0;
}

static int connect_mqtt() {
    mosquitto_log_callback_set(mosq, on_log);
    mosquitto_message_v5_callback_set(mosq, on_message_v5);
    mosquitto_connect_callback_set(mosq, on_connect);
    while (true) {
        int status = mosquitto_connect(mosq, OSIRIS_HOST, OSIRIS_MQTT_PORT, KEEP_ALIVE);
        if (status == MOSQ_ERR_SUCCESS) {
            break;
        }
        Debug("COULD NOT CONNECT\n");
        Debug("STATUS: %s\n", mosquitto_strerror(status));
        if (status == MOSQ_ERR_ERRNO) {
            Debug("%s\n", strerror(errno));
        }
        sleep(5);
        Debug("Reconnecting...");
    }

    int status = mosquitto_subscribe(mosq, &RECV_COUNT, INKY_TOPIC, 2);
    if (status != MOSQ_ERR_SUCCESS) {
        Debug("COULD NOT SUBSRIBE!\n");
        Debug("STATUS: %s\n", mosquitto_strerror(status));
    }
    Debug("Subscribed to %s\n", INKY_TOPIC);

    return 0;
}

int main(int argc, char *argv[])
{
    if (getuid() != 0) {
        Debug("Program must be run as root for now :(\n");
        return -1;
    }
    signal(SIGSEGV, sigsegv_handler);
    if (!eInk_Init(&Dev_Info)) {
        return -1;
    };

    init_mqtt();
    connect_mqtt();

	mosquitto_loop_start(mosq);

    while (1) {
        printf("Type \"quit\" to quit...\n");

        char input[32];
        fgets(input, sizeof input, stdin);
        if (strncmp(input, "quit\n", sizeof input) == 0) {
            printf("Exiting...\n");
            mosquitto_loop_stop(mosq, true);
            eInk_Shutdown();
            return 0;
        }
    }
    
    return 0;
}
