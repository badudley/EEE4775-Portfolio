/*
 * Application 4 — Synchronization Quest (Part B of Quest 1)
 * Theme: ODYSSEY-1, a deep-space relay station
 *
 * ============================================================
 *  ROLE MAP (why each primitive does the job it does)
 * ============================================================
 *
 *  BINARY SEMAPHORE  "uplink_sem"
 *     Role: ISR -> task hand-off. The ground-station antenna hardware
 *     line (simulated by a GPIO edge) fires exactly one event; the
 *     uplink task just needs to know "a packet arrived," not how many
 *     are queued (the debounce gate already collapses bursts). A
 *     binary semaphore is the textbook fit: ISR-safe give, one waiter,
 *     no counting semantics needed.
 *
 *  COUNTING SEMAPHORE  "thruster_pool" (N = 3)
 *     Role: models ODYSSEY-1's three physical RCS (Reaction Control
 *     System) thruster valves. Four subsystems — Attitude Control,
 *     Station-Keeping, Collision-Avoidance, and Docking-Alignment —
 *     compete for those three valves. The count is chosen to match
 *     real hardware (3 valves exist, full stop), so the 4th consumer
 *     is *expected* to sometimes back off. That backoff is the point:
 *     it demonstrates the semaphore enforcing a hard resource ceiling.
 *
 *  MUTEX  "telemetry_mux"
 *     Role: protects telemetry_seq, a shared downlink packet-sequence
 *     counter that two sensor tasks (Attitude-Sensor, Thermal-Sensor)
 *     both increment. This is a classic read-modify-write shared
 *     variable guarded by a single owner at a time — the mutex's
 *     ownership + priority-inheritance semantics are the reason it
 *     beats a binary semaphore here (see README).
 *
 * ============================================================
 *  INDUCED FAILURE — chosen: "remove the mutex"
 * ============================================================
 *  INDUCE_FAILURE = 0 : telemetry_mux guards the increment (correct).
 *  INDUCE_FAILURE = 1 : the take/give calls around the increment are
 *     skipped. Two tasks now perform an unguarded read-modify-write
 *     on telemetry_seq. A monitor task compares the number of
 *     attempted increments (tracked with an atomic counter, which is
 *     independent of the bug under test) against the final value of
 *     telemetry_seq. Under INDUCE_FAILURE=1 these diverge — lost
 *     updates — because a "PATCH" (packet count) can be silently
 *     dropped mid-air by two tasks racing on ODYSSEY-1's downlink
 *     sequence number. See README for prediction vs. observed logs.
 *
 * ============================================================
 *  LOCK MODE  (priority-inversion demo)
 * ============================================================
 *  Renamed for theme, same mechanics as the scaffold:
 *    H = fault_handler_task   ("attitude fault handler", prio 15)
 *    M = star_tracker_task    ("star-tracker calibration", prio 10)
 *    L = orbit_calc_task      ("orbit-determination burn", prio 5)
 *  H and L share nav_lock, a resource representing exclusive access to
 *  the onboard navigation solution. USE_PI_MUTEX selects the lock type.
 *
 *   USE_PI_MUTEX = 1 -> MUTEX, priority inheritance ON. When H blocks,
 *                       L inherits prio 15, so M (star-tracker calib)
 *                       cannot preempt L. H's wait is bounded by L's
 *                       remaining orbit-determination burn.
 *   USE_PI_MUTEX = 0 -> BINARY SEMAPHORE used as a lock, no ownership,
 *                       no inheritance. M preempts L mid-burn, so H
 *                       waits for the orbit burn AND the star-tracker
 *                       calibration to finish — unbounded inversion.
 *
 * ============================================================
 * Scaffold Code - AI usage (unchanged from provided starter):
 *   Addition of the USE_PI_MUTEX compile-time switch and the H/M/L
 *     priority-inversion harness (lock plumbing + timestamp telemetry)
 *   Logic to allow for switching the lock primitive between an
 *     inheriting mutex and a non-inheriting binary semaphore
 *   Commenting of code including human readable summaries
 * ============================================================
 */

#ifndef USE_PI_MUTEX
#define USE_PI_MUTEX 1
#endif

#ifndef INDUCE_FAILURE
#define INDUCE_FAILURE 0
#endif

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "esp_task_wdt.h"

#define BUTTON_GPIO GPIO_NUM_18

#define CONFIG_LOG_DEFAULT_LEVEL_INFO 1
#define CONFIG_LOG_MAXIMUM_LEVEL  5

static const char *TAG = "app4";

/* ---------- Synchronization primitives ---------- */
static SemaphoreHandle_t uplink_sem;     /* binary — ISR -> uplink task */
static SemaphoreHandle_t thruster_pool;  /* counting — N=3 RCS valves */
static SemaphoreHandle_t telemetry_mux;  /* mutex — protect telemetry_seq */

/* Shared state guarded by telemetry_mux (or NOT, if INDUCE_FAILURE=1) */
static volatile uint32_t telemetry_seq = 0;

/* Independent-of-the-bug attempt counter, used only to detect lost
 * updates. Incremented atomically with a GCC builtin so the failure
 * we're studying is isolated to telemetry_seq, not to this counter. */
static volatile uint32_t telemetry_attempts = 0;

/* ---------- ISR: ground antenna carrier-detect edge ---------- */
static volatile int64_t last_edge_us;
static void IRAM_ATTR ground_uplink_isr(void *arg)
{
    int64_t now = esp_timer_get_time();
    if (now - last_edge_us < 200) return;       /* debounce */
    last_edge_us = now;

    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(uplink_sem, &woken);
    portYIELD_FROM_ISR(woken);
}

/* ---------- Uplink task: waits on the binary sem ---------- */
static void uplink_task(void *arg)
{
    uint32_t cmd_count = 0;
    for (;;) {
        if (xSemaphoreTake(uplink_sem, portMAX_DELAY) == pdTRUE) {
            cmd_count++;
            ESP_LOGI(TAG, "[UPLINK] ground command received (#%u) — decoding packet",
                     (unsigned)cmd_count);
            ESP_LOGI(TAG, "[UPLINK] command #%u executed — safe-mode check OK",
                     (unsigned)cmd_count);
        }
    }
}

/* ---------- Thruster-pool task: takes from the counting sem ----------
 * Four subsystems contend for 3 physical RCS valves. */
static const char *subsystem_name(int id)
{
    switch (id) {
        case 1: return "Attitude-Control";
        case 2: return "Station-Keeping";
        case 3: return "Collision-Avoidance";
        case 4: return "Docking-Alignment";
        default: return "Unknown";
    }
}

static void thruster_task(void *arg)
{
    int id = (int)(uintptr_t)arg;
    const char *name = subsystem_name(id);
    for (;;) {
        if (xSemaphoreTake(thruster_pool, pdMS_TO_TICKS(1000)) == pdTRUE) {
            ESP_LOGI(TAG, "[thruster] %s claimed an RCS valve — firing burn...", name);
            vTaskDelay(pdMS_TO_TICKS(500 + (id * 200)));   /* simulated burn */
            xSemaphoreGive(thruster_pool);
            ESP_LOGI(TAG, "[thruster] %s released its RCS valve", name);
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            ESP_LOGW(TAG, "[thruster] %s found no free valve in 1s — deferred burn", name);
        }
    }
}

/* ---------- Two sensor tasks racing on telemetry_seq ---------- */
static const char *sensor_name(int id)
{
    return (id == 1) ? "Attitude-Sensor" : "Thermal-Sensor";
}

static void telemetry_writer_task(void *arg)
{
    int id = (int)(uintptr_t)arg;
    const char *name = sensor_name(id);
    for (;;) {
#if INDUCE_FAILURE
        /* --- INDUCED FAILURE: mutex removed --- */
        __sync_fetch_and_add(&telemetry_attempts, 1);
        uint32_t old = telemetry_seq;
        /* A tiny scheduling window here is enough for another writer
         * to interleave its own read-modify-write. */
        telemetry_seq = old + 1;
        ESP_LOGI(TAG, "[%s] telemetry_seq %u -> %u (UNGUARDED)",
                 name, (unsigned)old, (unsigned)telemetry_seq);
#else
        if (xSemaphoreTake(telemetry_mux, portMAX_DELAY) == pdTRUE) {
            __sync_fetch_and_add(&telemetry_attempts, 1);
            uint32_t old = telemetry_seq;
            telemetry_seq = old + 1;
            ESP_LOGI(TAG, "[%s] telemetry_seq %u -> %u", name,
                     (unsigned)old, (unsigned)telemetry_seq);
            xSemaphoreGive(telemetry_mux);
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(150 + (id * 73)));    /* irregular */
    }
}

/* ---------- Monitor: proves (or disproves) lost updates ---------- */
static void telemetry_monitor_task(void *arg)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        uint32_t attempts = telemetry_attempts;
        uint32_t final_val = telemetry_seq;
        int32_t lost = (int32_t)attempts - (int32_t)final_val;
        ESP_LOGW(TAG,
            "[MONITOR] attempts=%u  telemetry_seq=%u  lost_updates=%ld  (INDUCE_FAILURE=%d)",
            (unsigned)attempts, (unsigned)final_val, (long)lost, INDUCE_FAILURE);
    }
}

/* ============================================================
 *  Priority-inversion demo  (H = fault_handler, M = star_tracker,
 *  L = orbit_calc)  sharing nav_lock
 * ============================================================
 */
#if USE_PI_MUTEX
#define PI_LOCK_CREATE() xSemaphoreCreateMutex()
#define PI_LOCK_NAME     "MUTEX (priority inheritance ON)"
#else
#define PI_LOCK_CREATE() xSemaphoreCreateBinary()
#define PI_LOCK_NAME     "BINARY SEM (no inheritance)"
#endif

static SemaphoreHandle_t nav_lock;

/* Stagger knobs — control the ordering, not the durations. */
#define PI_H_DELAY_MS  50      /* H tries the lock 50 ms after start (after L holds it) */
#define PI_M_DELAY_MS  100     /* M becomes ready 100 ms after start */

/* Work knobs — fixed-iteration CPU burns. TUNE on Wokwi using the logged
 * wall-clock durations: aim for L ~500 ms and M ~1000 ms when each runs alone. */
#define PI_L_ITERS  20000000UL
#define PI_M_ITERS  40000000UL

static volatile uint32_t pi_sink;       /* defeats dead-code elimination */
static void pi_burn(uint32_t iters)
{
    uint32_t x = pi_sink ? pi_sink : 1u;
    for (uint32_t i = 0; i < iters; i++) { x ^= (x << 5); x += i; }
    pi_sink = x;
}

static void orbit_calc_task(void *arg)   /* L */
{
    /* L is created last in app_main, so it grabs the lock immediately. */
    xSemaphoreTake(nav_lock, portMAX_DELAY);
    int64_t t_acq = esp_timer_get_time();
    ESP_LOGI(TAG, "[PI][L=orbit_calc] took nav_lock @ %lld us — running orbit-determination burn",
             (long long)t_acq);
    pi_burn(PI_L_ITERS);
    int64_t t_rel = esp_timer_get_time();
    xSemaphoreGive(nav_lock);
    ESP_LOGI(TAG, "[PI][L=orbit_calc] released nav_lock @ %lld us (held %lld us wall-clock)",
             (long long)t_rel, (long long)(t_rel - t_acq));
    vTaskDelete(NULL);
}

static void star_tracker_task(void *arg)   /* M */
{
    vTaskDelay(pdMS_TO_TICKS(PI_M_DELAY_MS));
    int64_t t0 = esp_timer_get_time();
    ESP_LOGI(TAG, "[PI][M=star_tracker] ready @ %lld us — calibrating (takes no lock)",
             (long long)t0);
    pi_burn(PI_M_ITERS);
    int64_t t1 = esp_timer_get_time();
    ESP_LOGI(TAG, "[PI][M=star_tracker] done  @ %lld us (ran %lld us wall-clock)",
             (long long)t1, (long long)(t1 - t0));
    vTaskDelete(NULL);
}

static void fault_handler_task(void *arg)   /* H */
{
    vTaskDelay(pdMS_TO_TICKS(PI_H_DELAY_MS));
    int64_t t_block = esp_timer_get_time();
    ESP_LOGI(TAG, "[PI][H=fault_handler] attitude fault raised @ %lld us — needs nav_lock, blocking",
             (long long)t_block);
    xSemaphoreTake(nav_lock, portMAX_DELAY);
    int64_t t_acq = esp_timer_get_time();
    int64_t wait = t_acq - t_block;
    ESP_LOGW(TAG, "[PI][H=fault_handler] ACQUIRED @ %lld us — waited %lld us (~%lld ms)  [lock=%s]",
             (long long)t_acq, (long long)wait, (long long)(wait / 1000), PI_LOCK_NAME);
    xSemaphoreGive(nav_lock);
    vTaskDelete(NULL);
}

static void start_inversion_demo(void)
{
    nav_lock = PI_LOCK_CREATE();
#if !USE_PI_MUTEX
    /* A binary semaphore is created empty; prime it once so it starts "unlocked". */
    xSemaphoreGive(nav_lock);
#endif
    ESP_LOGI(TAG, "[PI] inversion demo lock = %s", PI_LOCK_NAME);

    /* Create H and M first (they delay before acting), then L LAST so L wins the
     * lock the instant it is created instead of starving app_main while it burns. */
    xTaskCreatePinnedToCore(fault_handler_task, "H", 4096, NULL, 15, NULL, APP_CPU_NUM);
    xTaskCreatePinnedToCore(star_tracker_task,  "M", 4096, NULL, 10, NULL, APP_CPU_NUM);
    xTaskCreatePinnedToCore(orbit_calc_task,    "L", 4096, NULL,  5, NULL, APP_CPU_NUM);
}

/* ---------- app_main ---------- */
void app_main(void)
{
    esp_task_wdt_reconfigure(&(esp_task_wdt_config_t){.timeout_ms = 10000, .idle_core_mask = 0, .trigger_panic = false });
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "==== ODYSSEY-1 [App 4] starting — sync quest ====");
    ESP_LOGI(TAG, "Lock mode: %s (USE_PI_MUTEX=%d)", PI_LOCK_NAME, USE_PI_MUTEX);
    ESP_LOGI(TAG, "Induced-failure mode: INDUCE_FAILURE=%d (0=mutex guards telemetry_seq, 1=unguarded)",
             INDUCE_FAILURE);

    uplink_sem     = xSemaphoreCreateBinary();
    thruster_pool  = xSemaphoreCreateCounting(3, 3);
    telemetry_mux  = xSemaphoreCreateMutex();

    /* ISR + uplink responder — ground station antenna carrier-detect line */
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&cfg);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, ground_uplink_isr, NULL);

    xTaskCreatePinnedToCore(uplink_task, "uplink", 4096, NULL, 12, NULL, APP_CPU_NUM);

    /* Thruster pool — 4 subsystems contending for 3 RCS valves */
    for (int i = 1; i <= 4; i++) {
        xTaskCreatePinnedToCore(thruster_task, "thruster", 4096,
                                (void*)(uintptr_t)i, 5, NULL, APP_CPU_NUM);
    }

    /* Telemetry writers — both update telemetry_seq (guarded unless INDUCE_FAILURE) */
    xTaskCreatePinnedToCore(telemetry_writer_task, "attitude_sensor", 4096, (void*)1, 8, NULL, APP_CPU_NUM);
    xTaskCreatePinnedToCore(telemetry_writer_task, "thermal_sensor",  4096, (void*)2, 8, NULL, APP_CPU_NUM);
    xTaskCreatePinnedToCore(telemetry_monitor_task, "tlm_monitor",    4096, NULL,     3, NULL, APP_CPU_NUM);

    /* Priority-inversion demo (H/M/L). For the cleanest H-wait numbers, you can
     * temporarily comment out the thruster/telemetry task creation above so
     * Core 1 carries only this demo. */
    start_inversion_demo();
}