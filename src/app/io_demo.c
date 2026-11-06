#include "io_demo.h"
#include "board_peripherals.h"
#include "STM32F334_CORE.h"
#include "demo_config.h"

static volatile uint32_t gpio_edges;
static uint32_t seen_edges, changed_ms, presses, timer_callbacks;
static uint8_t stable_input;
static bool gpio_settling, gpio_watch;
static bool timer_ready, pwm_ready;
static bool adc_follow_pwm;

static void output(const char *s)
{
    size_t n = 0U;
    while (s[n] != '\0') ++n;
    (void)UART_Write(&huart2, (const uint8_t *)s, n);
}

static void number(uint32_t value)
{
    uint8_t digits[10], result[10];
    size_t n = 0U;
    do { digits[n++] = (uint8_t)('0' + value % 10U); value /= 10U; } while (value != 0U);
    for (size_t i = 0U; i < n; ++i) result[i] = digits[n - 1U - i];
    (void)UART_Write(&huart2, result, n);
}

static bool equals(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static void timer_elapsed(uint32_t count, void *context)
{
    (void)context;
    timer_callbacks += count; /* Callback duoc TIM_Process goi, khong phai ISR. */
}

static void adc_complete(ADC_Result result, uint16_t value, void *context)
{
    (void)context;
    if (result == ADC_TIMEOUT) {
        output("ADC timeout; use ADC INIT to recalibrate\r\n");
        return;
    }
    if (result != ADC_OK) { output("ADC overrun\r\n"); return; }
    output("ADC raw="); number(value);
    output(" approx mV="); number(((uint32_t)value * DEMO_ADC_VREF_MV + 2047U) / 4095U);
    if (adc_follow_pwm && pwm_ready) {
        uint16_t duty = (uint16_t)(((uint32_t)value * 1000U + 2047U) / 4095U);
        (void)TIM_PWM_SetDuty(&hpwm2, duty);
        if (!hpwm2.Running) (void)TIM_PWM_Start(&hpwm2);
        output(" PWM permille="); number(duty);
    }
    output("\r\n");
}

void IO_Demo_Init(void)
{
    gpio_edges = seen_edges = presses = timer_callbacks = 0U;
    gpio_settling = gpio_watch = adc_follow_pwm = false;
    Board_GPIOInputInit();
    stable_input = GPIO_ReadFromInputPin(GPIOB, 0U);
    changed_ms = Timebase_Millis();
    timer_ready = Board_TimerInit();
    pwm_ready = Board_PWMInit(); /* PWM dung, muc low den lenh PWM START. */
    (void)Board_ADCInit();
    /* Timer IRQ chi dem, khong in tu dong 10 lan/giay hay doi ownership LED. */
    if (timer_ready) (void)TIM_Start_IT(&htim3, timer_elapsed, NULL);
}

void IO_Demo_GPIOIRQ(void)
{
    if ((EXIT->PR1 & 1U) != 0U) {
        GPIO_IRQHandling(0U);
        ++gpio_edges;
    }
}

void IO_Demo_Process(void)
{
    if (timer_ready) TIM_Process(&htim3);
    if (hadc1.Initialized) ADC_Process(&hadc1);
    uint32_t key = IRQ_Save();
    uint32_t now = Timebase_Millis();
    uint32_t edges = gpio_edges;
    bool report = false;
    uint8_t input = stable_input;
    if (edges != seen_edges) {
        seen_edges = edges;
        changed_ms = now;
        gpio_settling = true;
    }
    if (gpio_settling && (uint32_t)(now - changed_ms) >= 30U && (EXIT->PR1 & 1U) == 0U) {
        input = GPIO_ReadFromInputPin(GPIOB, 0U);
        gpio_settling = false;
        if (input != stable_input) {
            stable_input = input;
            if (input == 0U) ++presses;
            report = gpio_watch;
        }
    }
    IRQ_Restore(key);
    if (report) output(input == 0U ? "PB0 pressed\r\n" : "PB0 released\r\n");
}

bool IO_Demo_Command(const char *line)
{
    if (equals(line, "GPIO READ")) {
        output("PB0="); number(GPIO_ReadFromInputPin(GPIOB, 0U));
        output(" edges="); number(gpio_edges);
        output(" debounced presses="); number(presses); output("\r\n");
    } else if (equals(line, "GPIO WATCH ON") || equals(line, "GPIO WATCH OFF")) {
        gpio_watch = equals(line, "GPIO WATCH ON"); output("OK\r\n");
    } else if (equals(line, "TIMER START")) {
        output(timer_ready && TIM_Start_IT(&htim3, timer_elapsed, NULL) ?
               "Timer started\r\n" : "Timer already running/unavailable\r\n");
    } else if (equals(line, "TIMER STOP")) {
        output(timer_ready && TIM_Stop(&htim3) ? "Timer stopped\r\n" : "Timer unavailable\r\n");
    } else if (equals(line, "TIMER STATS")) {
        output("TIM3 updates="); number(TIM_GetUpdates(&htim3));
        output(" processed="); number(timer_callbacks); output(" period=100ms\r\n");
    } else if (equals(line, "PWM START")) {
        output(pwm_ready && TIM_PWM_Start(&hpwm2) ? "PWM started 1kHz\r\n" :
                                                        "PWM already running/unavailable\r\n");
    } else if (equals(line, "PWM STOP")) {
        output(pwm_ready && TIM_Stop(&hpwm2) ? "PWM stopped low\r\n" : "PWM unavailable\r\n");
    } else if (equals(line, "PWM STATS")) {
        output("TIM2_CH1 PA0 1kHz duty permille="); number(hpwm2.DutyPermille);
        output(hpwm2.Running ? " running\r\n" : " stopped\r\n");
    } else if (line[0] == 'P' && line[1] == 'W' && line[2] == 'M' && line[3] == ' ') {
        /* Decimal 0..100, khong dung atoi/sscanf/libc hay chap nhan prefix rac. */
        const char *p = line + 4;
        unsigned percent = 0U, digits = 0U;
        while (*p >= '0' && *p <= '9' && digits < 3U) {
            percent = percent * 10U + (unsigned)(*p++ - '0'); ++digits;
        }
        if (*p != '\0' || digits == 0U || percent > 100U || !pwm_ready ||
            !TIM_PWM_SetDuty(&hpwm2, (uint16_t)(percent * 10U)))
            output("Use PWM 0..100\r\n");
        else output("PWM duty updated; PWM START enables output\r\n");
    } else if (equals(line, "ADC INIT")) {
        output(Board_ADCInit() ? "ADC calibrated/ready\r\n" : "ADC already ready/busy/init failed\r\n");
    } else if (equals(line, "ADC READ")) {
        output(ADC_Start_IT(&hadc1, 50U, adc_complete, NULL) ? "ADC started\r\n" :
                                                               "ADC busy/unavailable; try ADC INIT\r\n");
    } else if (equals(line, "ADC PWM ON") || equals(line, "ADC PWM OFF")) {
        adc_follow_pwm = equals(line, "ADC PWM ON");
        output("OK; ADC READ takes one sample and optionally updates PWM\r\n");
    } else if (equals(line, "ADC STATS")) {
        output(hadc1.Initialized ? "ADC ready samples=" : "ADC not ready samples=");
        number(hadc1.Samples); output(" errors="); number(hadc1.Errors); output("\r\n");
    } else return false;
    return true;
}
