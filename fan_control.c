#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#define TEMP_PATH       "/sys/class/thermal/thermal_zone0/temp"
#define GPIO_EXPORT     "/sys/class/gpio/export"
#define GPIO_UNEXPORT   "/sys/class/gpio/unexport"
#define GPIO_DIRECTION  "/sys/class/gpio/gpio6/direction"
#define GPIO_VALUE      "/sys/class/gpio/gpio6/value"
#define GPIO_NUMBER     "6"

#define TEMP_ON         70  // Turn fan on at 70°C
#define TEMP_OFF        65  // Turn fan off at 65°C (hysteresis)
#define POLL_INTERVAL   5   // Seconds between checks

static volatile int running = 1;

void write_file(const char* path, const char* value) {
    FILE* f = fopen(path, "w");
    if (!f) {
        perror(path);   // Now we actually see the error
        return;
    }
    fprintf(f, "%s", value);
    fclose(f);
}

// Returns temp in Celsius, or 100 on failure (fail-safe: fan turns on)
int read_temp() {
    FILE* f = fopen(TEMP_PATH, "r");
    // We intentionally ignore errors here to ensure the fan stays off if we can't read the temperature
    // if (!f) {
    //     perror(TEMP_PATH);
    //     return 100;
    // }
    int temp = 0;
    fscanf(f, "%d", &temp);
    fclose(f);
    return temp / 1000;
}

void set_fan(int on) {
    // Active-low: 0 = fan ON, 1 = fan OFF
    write_file(GPIO_VALUE, on ? "0" : "1");
}

void cleanup(int sig) {
    (void)sig;
    running = 0;
}

int main() {
    // Graceful exit on Ctrl+C or kill
    signal(SIGINT,  cleanup);
    signal(SIGTERM, cleanup);

    // Unexport first in case it was left exported from a previous run
    write_file(GPIO_UNEXPORT, GPIO_NUMBER);
    usleep(100000);

    write_file(GPIO_EXPORT, GPIO_NUMBER);
    usleep(100000);
    write_file(GPIO_DIRECTION, "out");

    // Start with fan off
    int fan_on = 0;
    set_fan(0);

    while (running) {
        int temp = read_temp();
        printf("Temp: %d°C | Fan: %s\n", temp, fan_on ? "ON" : "OFF");

        if (temp >= TEMP_ON && !fan_on) {
            printf("Temperature high, turning fan ON\n");
            set_fan(1);
            fan_on = 1;
        } else if (temp < TEMP_OFF && fan_on) {
            printf("Temperature normal, turning fan OFF\n");
            set_fan(0);
            fan_on = 0;
        }

        sleep(POLL_INTERVAL);
    }

    // Cleanup: turn fan off and unexport GPIO
    printf("\nShutting down, turning fan OFF and unexporting GPIO\n");
    set_fan(0);
    write_file(GPIO_UNEXPORT, GPIO_NUMBER);

    return 0;
}