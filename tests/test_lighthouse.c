#include <setjmp.h>
#include <stdlib.h>
#include <reent.h>
#include "../main/lighting.c"

// Execute the real owner task with a deterministic queue and clock. The strip
// calls are spies; this does not simulate RMT or establish physical LED order.
static struct _reent test_reent;
struct _reent *__getreent(void) { return &test_reent; }
static jmp_buf finished;
static int failure;
#define CHECK(condition) do { if (!(condition)) { failure = __LINE__; longjmp(finished, 1); } } while (0)
#define FRAME_WAIT 0 // Delayed-wake tests permit any next 30 fps deadline.

typedef struct {
    TickType_t expected_wait;
    TickType_t advance;
    int command; // -1: timeout; -2: off; -3: zero brightness; -4: 1000 ms lighthouse.
} event_t;
static const event_t *events;
static unsigned event_count, event_index;
static TickType_t now;
static TaskFunction_t owner;
static void *owner_argument;
static int frame_count, fail_frame = -1;
static int positions[160]; // -1 means a solid frame.
static light_rgb_t colors[160];
static light_rgb_t incoming_colors[160];
static light_rgb_t candle_frames[160][GROUP_A_LED_COUNT];
static big_light_settings_t queued;
static bool queue_full;
static uint32_t command_period_ms = 1500;
static light_color_mode_t command_color_mode;
static light_shift_mode_t command_shift_mode;

QueueHandle_t xQueueCreate(unsigned length, unsigned size)
{
    CHECK(length == 4 && size == sizeof(big_light_settings_t));
    return &queued;
}
void vQueueDelete(QueueHandle_t queue) { (void)queue; }
BaseType_t xTaskCreate(TaskFunction_t task, const char *name, unsigned stack,
                      void *argument, unsigned priority, void *handle)
{
    (void)name; (void)stack; (void)priority; (void)handle;
    owner = task;
    owner_argument = argument;
    return pdPASS;
}
TickType_t xTaskGetTickCount(void) { return now; }
BaseType_t xQueueSend(QueueHandle_t queue, const void *item, TickType_t wait)
{
    CHECK(queue == &queued && wait == 0);
    if (queue_full) return 0;
    queued = *(const big_light_settings_t *)item;
    return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t wait)
{
    CHECK(queue == &queued);
    if (event_index == event_count) longjmp(finished, 1);
    event_t event = events[event_index++];
    if (event.expected_wait == FRAME_WAIT) {
        CHECK(wait >= 1 && wait <= 4);
    } else {
        CHECK(wait == event.expected_wait);
    }
    now += event.advance;
    if (event.command == -1) return 0;
    if (event.command == -7 || event.command == -8) {
        *(big_light_settings_t *)item = (big_light_settings_t){
            .effect = LIGHT_EFFECT_BLE_INDICATOR, .on = event.command == -7};
        return pdTRUE;
    }
    big_light_settings_t settings = {
        .on = event.command != -2 && event.command != -5,
        .effect = (event.command == -5 || event.command == -6) ? LIGHT_EFFECT_CANDLE :
                  event.command < 0 ? LIGHT_EFFECT_LIGHT_HOUSE : (light_effect_t)event.command,
        .color = light_color_from_pwm((light_rgb_t){255, 150, 30}),
        .brightness = light_brightness_from_pwm((light_rgb_t){255, 150, 30}),
        .period_ms = event.command == -4 ? 1000 : command_period_ms,
        .color_mode = command_color_mode,
        .shift_mode = command_shift_mode,
        .gradient_end = {0.15f, 0.06f},
        .shift_period_ms = 6000,
    };
    if (event.command == -3 || event.command == -6) settings.brightness = 0;
    *(big_light_settings_t *)item = settings;
    return pdTRUE;
}
esp_err_t group_a_init(void) { return ESP_OK; }
esp_err_t group_a_deinit(void) { return ESP_OK; }
static esp_err_t frame(int position, light_rgb_t pwm)
{
    CHECK(frame_count < 160);
    positions[frame_count] = position;
    colors[frame_count] = pwm;
    return frame_count++ == fail_frame ? ESP_FAIL : ESP_OK;
}
esp_err_t group_a_set_solid(light_rgb_t pwm) { return frame(-1, pwm); }
esp_err_t group_a_set_single(uint8_t position, light_rgb_t pwm)
{
    CHECK(position < 6);
    return frame(position, pwm);
}
esp_err_t group_a_set_pair(uint8_t position, light_rgb_t outgoing, light_rgb_t incoming)
{
    CHECK(position < 6 && frame_count < 160);
    CHECK(outgoing.r + incoming.r == 255);
    CHECK(outgoing.g + incoming.g == 150);
    CHECK(outgoing.b + incoming.b == 30);
    incoming_colors[frame_count] = incoming;
    return frame(position, outgoing);
}
esp_err_t group_a_set_frame(const light_rgb_t pixels[GROUP_A_LED_COUNT])
{
    CHECK(frame_count < 160);
    for (int i = 0; i < GROUP_A_LED_COUNT; ++i) {
        if (command_color_mode == LIGHT_COLOR_MONO && command_shift_mode == LIGHT_SHIFT_STATIC) {
            CHECK(pixels[i].r <= 255 && pixels[i].g <= 150 && pixels[i].b <= 30);
        }
        candle_frames[frame_count][i] = pixels[i];
    }
    return frame(-2, pixels[0]);
}

static int simulate(const event_t *script, unsigned count, TickType_t start)
{
    events = script; event_count = count; event_index = 0; frame_count = 0; now = start;
    failure = 0;
    if (setjmp(finished) == 0) owner(owner_argument);
    return failure;
}

int main(void)
{
    if (setjmp(finished) != 0) return failure;
    CHECK(lighting_init() == ESP_OK);
    big_light_settings_t settings = {.on = true, .effect = LIGHT_EFFECT_LIGHT_HOUSE,
                                     .color = BIG_LIGHT_WHITE, .brightness = 1.0f, .period_ms = 1500};
    CHECK(set_big_light(&settings) == ESP_OK);
    CHECK(queued.effect == LIGHT_EFFECT_LIGHT_HOUSE);
    CHECK(queued.period_ms == 1500);
    settings.period_ms = 0;
    CHECK(set_big_light(&settings) == ESP_ERR_INVALID_ARG);
    settings.period_ms = 59;
    CHECK(set_big_light(&settings) == ESP_ERR_INVALID_ARG);
    settings.period_ms = 60;
    CHECK(set_big_light(&settings) == ESP_OK);
    settings.period_ms = UINT32_MAX; // Must not overflow ms-to-ticks arithmetic.
    CHECK(set_big_light(&settings) == ESP_OK);
    CHECK(lighthouse_period_ticks(UINT32_MAX) == 429496730u);
    settings.effect = LIGHT_EFFECT_SOLID;
    settings.period_ms = 0;
    CHECK(set_big_light(&settings) == ESP_OK);
    settings.period_ms = 1500;
    settings.effect = LIGHT_EFFECT_CANDLE;
    settings.period_ms = 0; // Candle does not require a rotation period.
    CHECK(set_big_light(&settings) == ESP_OK);
    settings.effect = LIGHT_EFFECT_SPARKLES;
    CHECK(set_big_light(&settings) == ESP_OK); // No rotation period required.
    settings.effect = (light_effect_t)3; // Removed effect ID must stay invalid.
    CHECK(set_big_light(&settings) == ESP_ERR_INVALID_ARG);
    settings.effect = (light_effect_t)99;
    CHECK(set_big_light(&settings) == ESP_ERR_INVALID_ARG);
    CHECK(set_big_light(NULL) == ESP_ERR_INVALID_ARG);
    settings.effect = LIGHT_EFFECT_LIGHT_HOUSE;
    settings.period_ms = 1500;
    settings.color.y = 0;
    CHECK(set_big_light(&settings) == ESP_ERR_INVALID_ARG);
    settings.color = BIG_LIGHT_WHITE;
    queue_full = true;
    CHECK(set_big_light(&settings) == ESP_ERR_TIMEOUT);
    queue_full = false;

    // At 100 Hz: 40,30,30 ms repeated gives exactly 30 fps.
    // Five seconds must contain 150 intervals with no rounding drift.
    event_t cadence[151] = {{portMAX_DELAY, 0, 1}};
    for (int i = 1; i <= 150; ++i) {
        TickType_t ticks = i % 3 == 1 ? 4 : 3;
        cadence[i] = (event_t){ticks, ticks, -1};
    }
    command_period_ms = 5000;
    int cadence_result = simulate(cadence, 151, UINT32_MAX - 20);
    if (cadence_result) return cadence_result;
    if (setjmp(finished) != 0) return failure;
    CHECK(now == 479 && frame_count == 151);
    CHECK(positions[150] == 0 && colors[150].r == 255 && incoming_colors[150].r == 0);
    command_period_ms = 1500;

    const event_t rotation[] = {
        {portMAX_DELAY, 0, 1},
        {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1},
        {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1},
        {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1},
        {FRAME_WAIT, 3, -2}, {portMAX_DELAY, 0, 1}, // Off interrupts; restart at 0.
        {FRAME_WAIT, 37, -1}, // Delayed wake renders current phase, not three old frames.
        {FRAME_WAIT, 1, 0}, // Solid interrupts the next frame deadline.
        {portMAX_DELAY, 0, -3}, {portMAX_DELAY, 0, -2},
    };
    int result = simulate(rotation, sizeof(rotation) / sizeof(rotation[0]), 0);
    if (result) return result;
    // Reset jump target after simulate's stack frame has returned.
    if (setjmp(finished) != 0) return failure;
    CHECK(frame_count == 22);
    CHECK(positions[0] == 0 && colors[0].r == 255 && incoming_colors[0].r == 0);
    CHECK(positions[1] == 0 && colors[1].r == 214 && incoming_colors[1].r == 41);
    CHECK(colors[1].g == 126 && incoming_colors[1].g == 24);
    CHECK(colors[1].b == 25 && incoming_colors[1].b == 5);
    CHECK(positions[5] == 2 && colors[5].r == 255 && incoming_colors[5].r == 0);
    CHECK(positions[14] == 5 && colors[14].r == 163 && incoming_colors[14].r == 92);
    CHECK(positions[15] == 0 && colors[15].r == 255 && incoming_colors[15].r == 0);
    CHECK(positions[16] == -1 && colors[16].r == 0 && colors[16].g == 0 && colors[16].b == 0);
    CHECK(positions[17] == 0 && colors[17].r == 255 && positions[18] == 1);
    CHECK(positions[19] == -1 && colors[19].r == 255);
    CHECK(positions[20] == -1 && colors[20].r == 0);
    CHECK(positions[21] == -1 && colors[21].r == 0);

    const event_t wrap[] = {{portMAX_DELAY, 0, 1}, {FRAME_WAIT, 10, -1}};
    result = simulate(wrap, 2, UINT32_MAX - 5);
    if (result) return result;
    if (setjmp(finished) != 0) return failure;
    CHECK(frame_count == 2 && positions[0] == 0 && positions[1] == 0 && incoming_colors[1].r == 41);

    const event_t speed_change[] = {
        {portMAX_DELAY, 0, 1}, {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 3, -4}, {FRAME_WAIT, 10, -1},
    };
    result = simulate(speed_change, 4, 0);
    if (result) return result;
    if (setjmp(finished) != 0) return failure;
    CHECK(frame_count == 4 && positions[2] == 0 && colors[2].r == 255);
    CHECK(positions[3] == 0 && incoming_colors[3].r == 92);

    command_period_ms = 1000;
    const event_t uneven[] = {
        {portMAX_DELAY, 0, 1},
        {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1},
        {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1}, {FRAME_WAIT, 10, -1},
    };
    result = simulate(uneven, 11, 0);
    if (result) return result;
    if (setjmp(finished) != 0) return failure;
    CHECK(now == 100 && frame_count == 11);
    CHECK(positions[10] == 0 && colors[10].r == 255);

    command_period_ms = 61; // Rounds to 70 ms; 30 fps samples skip positions.
    const event_t minimum[] = {
        {portMAX_DELAY, 0, 1}, {FRAME_WAIT, 10, -1},
    };
    result = simulate(minimum, 2, 0);
    if (result) return result;
    if (setjmp(finished) != 0) return failure;
    CHECK(now == 10 && frame_count == 2 && positions[1] == 2);

    // Exact midpoint and wrap fixtures, independent of frame sampling.
    light_rgb_t outgoing, incoming;
    light_rgb_t base = {255, 150, 30};
    CHECK(lighthouse_frame(50, 600, base, &outgoing, &incoming) == 0);
    CHECK(outgoing.r == 191 && incoming.r == 64 && outgoing.g == 112 && incoming.g == 38);
    CHECK(lighthouse_frame(550, 600, base, &outgoing, &incoming) == 5);
    CHECK(outgoing.r == 191 && incoming.r == 64);
    CHECK(lighthouse_frame(600, 600, base, &outgoing, &incoming) == 0);
    CHECK(outgoing.r == 255 && incoming.r == 0);
    for (unsigned phase = 0; phase < 500; ++phase) {
        CHECK(lighthouse_frame(phase, 500, base, &outgoing, &incoming) < 6);
        CHECK(outgoing.r + incoming.r == 255 && outgoing.g + incoming.g == 150 &&
              outgoing.b + incoming.b == 30);
    }

    command_period_ms = 1500;
    fail_frame = 1;
    const event_t failed[] = {{portMAX_DELAY, 0, 1}, {FRAME_WAIT, 10, -1}, {portMAX_DELAY, 10, -2}};
    result = simulate(failed, 3, 0);
    if (result) return result;
    if (setjmp(finished) != 0) return failure;
    CHECK(frame_count == 3 && positions[2] == -1 && colors[2].r == 0);
    fail_frame = -1;
    command_period_ms = 0;
    const event_t candle_script[] = {
        {portMAX_DELAY, 0, 2}, {FRAME_WAIT, 5, -1}, {FRAME_WAIT, 4, -1}, {FRAME_WAIT, 30, -1},
        {FRAME_WAIT, 1, -5}, {portMAX_DELAY, 0, 2}, {FRAME_WAIT, 1, 0},
        {portMAX_DELAY, 0, -6}, {portMAX_DELAY, 0, 2}, {FRAME_WAIT, 1, -4},
    };
    result = simulate(candle_script, 10, UINT32_MAX - 7);
    if (result) return result;
    if (setjmp(finished) != 0) return failure;
    CHECK(frame_count == 10);
    CHECK(positions[0] == -2 && positions[3] == -2);
    CHECK(positions[4] == -1 && colors[4].r == 0); // Candle off.
    CHECK(positions[5] == -2 && positions[6] == -1 && colors[6].r == 255);
    CHECK(positions[7] == -1 && colors[7].r == 0); // Zero brightness suspends.
    CHECK(positions[8] == -2 && positions[9] == 0); // Switch to lighthouse.
    bool changed = false;
    for (int i = 0; i < GROUP_A_LED_COUNT; ++i) {
        if (candle_frames[0][i].r != candle_frames[3][i].r) changed = true;
    }
    CHECK(changed);
    fail_frame = 1;
    const event_t candle_failure[] = {
        {portMAX_DELAY, 0, 2}, {FRAME_WAIT, 5, -1}, {portMAX_DELAY, 0, -5},
    };
    result = simulate(candle_failure, 3, 0);
    if (result) return result;
    if (setjmp(finished) != 0) return failure;
    CHECK(positions[2] == -1 && colors[2].r == 0);
    fail_frame = -1;
    const event_t sparkle_script[] = {
        {portMAX_DELAY, 0, 4}, {FRAME_WAIT, 5, -1}, {FRAME_WAIT, 30, -1},
        {FRAME_WAIT, 1, -2}, {portMAX_DELAY, 0, 4}, {FRAME_WAIT, 1, 0},
        {portMAX_DELAY, 0, 2}, {FRAME_WAIT, 1, 4}, {FRAME_WAIT, 1, -4},
    };
    result = simulate(sparkle_script, 9, UINT32_MAX - 7);
    if (result) return result;
    if (setjmp(finished) != 0) return failure;
    CHECK(frame_count == 9 && positions[0] == -2 && positions[2] == -2);
    CHECK(positions[3] == -1 && colors[3].r == 0);
    CHECK(positions[4] == -2 && positions[5] == -1 && colors[5].r == 255);
    CHECK(positions[6] == -2 && positions[7] == -2 && positions[8] == 0);
    fail_frame = 1;
    const event_t sparkle_failure[] = {
        {portMAX_DELAY, 0, 4}, {FRAME_WAIT, 5, -1}, {portMAX_DELAY, 0, -2},
    };
    result = simulate(sparkle_failure, 3, 0);
    if (result) return result;
    if (setjmp(finished) != 0) return failure;
    CHECK(positions[2] == -1 && colors[2].r == 0);
    fail_frame = -1;
    settings = (big_light_settings_t){.on = true, .effect = LIGHT_EFFECT_SOLID,
        .color = BIG_LIGHT_WHITE, .brightness = 1.0f, .color_mode = LIGHT_COLOR_GRADIENT,
        .gradient_end = {0.15f, 0.06f}, .shift_mode = LIGHT_SHIFT_CYCLE,
        .shift_period_ms = 6000};
    CHECK(set_big_light(&settings) == ESP_OK);
    CHECK(queued.color_mode == LIGHT_COLOR_GRADIENT && queued.shift_period_ms == 6000);
    settings.gradient_end.y = 0;
    CHECK(set_big_light(&settings) == ESP_ERR_INVALID_ARG);
    CHECK(queued.gradient_end.y == 0.06f); // Rejected settings leave the queue untouched.
    settings.gradient_end.y = 0.06f;
    settings.shift_period_ms = 0;
    CHECK(set_big_light(&settings) == ESP_ERR_INVALID_ARG);
    settings.shift_period_ms = 6000;
    settings.color_mode = (light_color_mode_t)99;
    CHECK(set_big_light(&settings) == ESP_ERR_INVALID_ARG);
    settings.color_mode = LIGHT_COLOR_MONO;
    settings.shift_mode = (light_shift_mode_t)99;
    CHECK(set_big_light(&settings) == ESP_ERR_INVALID_ARG);

    // All color/shift combinations compose with every lighting effect.
    command_period_ms = 1500;
    const int effects[] = {0, 1, 2, 4, 5};
    for (volatile int mode = 0; mode <= 1; ++mode) for (volatile int shift = 0; shift <= 2; ++shift) {
        command_color_mode = (light_color_mode_t)mode;
        command_shift_mode = (light_shift_mode_t)shift;
        if (mode == 0 && shift == 0) continue; // Covered by preceding regression cases.
        for (volatile unsigned e = 0; e < sizeof(effects) / sizeof(effects[0]); ++e) {
            bool animated = effects[e] != 0 || shift != 0;
            event_t script[] = {
                {portMAX_DELAY, 0, effects[e]},
                {animated ? FRAME_WAIT : portMAX_DELAY, 1, -2},
                {portMAX_DELAY, 0, effects[e]},
                {animated ? FRAME_WAIT : portMAX_DELAY, 1, -3},
            };
            result = simulate(script, 4, UINT32_MAX - 1);
            if (result) return result;
            if (setjmp(finished) != 0) return failure;
            CHECK(positions[0] == -2 && positions[2] == -2);
            CHECK(positions[1] == -1 && colors[1].r == 0 && colors[1].g == 0 && colors[1].b == 0);
            CHECK(positions[3] == -1 && colors[3].r == 0 && colors[3].g == 0 && colors[3].b == 0);
        }
    }
    command_color_mode = LIGHT_COLOR_GRADIENT;
    command_shift_mode = LIGHT_SHIFT_CYCLE;
    const event_t pattern_script[] = {
        {portMAX_DELAY, 0, 0}, {FRAME_WAIT, 5, -1}, {FRAME_WAIT, 95, -1}, {FRAME_WAIT, 1, 1},
        {FRAME_WAIT, 1, 2}, {FRAME_WAIT, 1, 4}, {FRAME_WAIT, 1, -2},
    };
    result = simulate(pattern_script, 7, UINT32_MAX - 7);
    if (result) return result;
    if (setjmp(finished) != 0) return failure;
    CHECK(frame_count == 7 && positions[6] == -1 && colors[6].r == 0);
    settings = (big_light_settings_t){.on = true, .effect = LIGHT_EFFECT_SOLID,
        .color = light_color_from_pwm((light_rgb_t){255, 150, 30}),
        .brightness = light_brightness_from_pwm((light_rgb_t){255, 150, 30}),
        .color_mode = LIGHT_COLOR_GRADIENT, .gradient_end = {0.15f, 0.06f},
        .shift_mode = LIGHT_SHIFT_CYCLE, .shift_period_ms = 6000};
    light_rgb_t expected_pixels[GROUP_A_LED_COUNT];
    CHECK(color_pattern_render(&settings, 1000, 0, expected_pixels) == ESP_OK);
    for (int i = 0; i < GROUP_A_LED_COUNT; ++i) {
        CHECK(candle_frames[2][i].r == expected_pixels[i].r);
        CHECK(candle_frames[2][i].g == expected_pixels[i].g);
        CHECK(candle_frames[2][i].b == expected_pixels[i].b);
        if (i != 0) CHECK(candle_frames[3][i].r == 0 && candle_frames[3][i].g == 0 && candle_frames[3][i].b == 0);
    }
    fail_frame = 1;
    const event_t pattern_failure[] = {
        {portMAX_DELAY, 0, 0}, {FRAME_WAIT, 5, -1}, {portMAX_DELAY, 0, -2},
    };
    result = simulate(pattern_failure, 3, 0);
    if (result) return result;
    if (setjmp(finished) != 0) return failure;
    CHECK(positions[2] == -1 && colors[2].r == 0);
    command_color_mode = LIGHT_COLOR_MONO;
    command_shift_mode = LIGHT_SHIFT_STATIC;
    command_period_ms = 4000;
    fail_frame = -1;
    const event_t breathing_script[] = {
        {portMAX_DELAY, 0, 5}, {FRAME_WAIT, 100, -1},
        {FRAME_WAIT, 100, -1}, {FRAME_WAIT, 100, -1},
        {FRAME_WAIT, 100, -1}, {FRAME_WAIT, 1, -2},
        {portMAX_DELAY, 0, 5},
    };
    result = simulate(breathing_script, 7, UINT32_MAX - 50);
    if (result) return result;
    if (setjmp(finished) != 0) return failure;
    CHECK(frame_count == 7);
    for (int i = 0; i < GROUP_A_LED_COUNT; ++i) {
        CHECK(candle_frames[0][i].r == 0);
        CHECK(candle_frames[1][i].r == 63);
        CHECK(candle_frames[2][i].r == 255 && candle_frames[2][i].g == 150);
        CHECK(candle_frames[3][i].r == 63);
        CHECK(candle_frames[4][i].r == 0 && candle_frames[6][i].r == 0);
    }
    CHECK(positions[5] == -1 && colors[5].r == 0);
    settings.effect = LIGHT_EFFECT_BREATHING;
    settings.period_ms = 0;
    CHECK(set_big_light(&settings) == ESP_ERR_INVALID_ARG);
    settings.period_ms = 4000;
    CHECK(set_big_light(&settings) == ESP_OK);
    const event_t indicator_script[] = {
        {portMAX_DELAY, 0, 0}, {portMAX_DELAY, 0, -7},
        {FRAME_WAIT, 30, -1}, {FRAME_WAIT, 30, -1},
        {portMAX_DELAY, 0, -2}, {portMAX_DELAY, 0, -8},
        {FRAME_WAIT, 20, -1},
    };
    result = simulate(indicator_script, 7, UINT32_MAX - 10);
    if (result) return result;
    if (setjmp(finished) != 0) return failure;
    CHECK(frame_count == 7);
    CHECK(colors[2].r == 0 && colors[2].g == 0 && colors[2].b == 128);
    CHECK(colors[3].r == 255 && colors[3].g == 150 && colors[3].b == 30);
    CHECK(colors[5].r == 128 && colors[5].g == 0 && colors[5].b == 0);
    CHECK(colors[6].r == 0 && colors[6].g == 0 && colors[6].b == 0);
    queue_full = true;
    CHECK(lighting_ble_indicator(true) == ESP_ERR_TIMEOUT);
    queue_full = false;
    CHECK(lighting_ble_indicator(false) == ESP_OK);
    // Reproduce main.c's exact Breathing + Gradient + Random configuration.
    settings = (big_light_settings_t){.on = true, .effect = LIGHT_EFFECT_BREATHING,
        .period_ms = 10000, .brightness = 1.0f,
        .color = light_color_from_pwm((light_rgb_t){255, 120, 0}),
        .gradient_end = light_color_from_pwm((light_rgb_t){255, 120, 255}),
        .color_mode = LIGHT_COLOR_GRADIENT, .shift_mode = LIGHT_SHIFT_RANDOM,
        .shift_period_ms = 7000};
    CHECK(lighting_validate(&settings) == ESP_OK);
    command_color_mode = LIGHT_COLOR_GRADIENT;
    command_shift_mode = LIGHT_SHIFT_RANDOM;
    frame_count = 0;
    const unsigned samples[] = {0, 2500, 5000, 7500, 10000};
    for (unsigned sample = 0; sample < 5; ++sample) {
        unsigned ms = samples[sample];
        CHECK(patterned_frame(&settings, ms / 10, 1000, 123) == ESP_OK);
        light_rgb_t full[GROUP_A_LED_COUNT];
        CHECK(color_pattern_render(&settings, ms, 123, full) == ESP_OK);
        for (unsigned led = 0; led < GROUP_A_LED_COUNT; ++led) {
            light_rgb_t actual = candle_frames[sample][led];
            if (sample == 0 || sample == 4) {
                CHECK(actual.r == 0 && actual.g == 0 && actual.b == 0);
            } else if (sample == 2) {
                CHECK(actual.r == full[led].r && actual.g == full[led].g && actual.b == full[led].b);
                CHECK(actual.r > 0);
            } else {
                CHECK(fabsf(actual.r - full[led].r * 0.24643995f) <= 0.501f);
                CHECK(fabsf(actual.g - full[led].g * 0.24643995f) <= 0.501f);
                CHECK(fabsf(actual.b - full[led].b * 0.24643995f) <= 0.501f);
            }
        }
    }
    return 0;
}
