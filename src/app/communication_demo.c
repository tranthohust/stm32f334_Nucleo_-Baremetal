#include "communication_demo.h"
#include "io_demo.h"
#include "board_peripherals.h"
#include "demo_config.h"
#include "line_parser.h"
#include "STM32F334_CORE.h"
#include "STM32F334_UART.h"
#include "STM32F334_SPI.h"
#include "STM32F334_CAN.h"
#include "STM32F334_I2C.h"
#if FIRMWARE_USE_FREERTOS
#include "rtos_demo.h"
#else
static bool i2c_ready, i2c_test_pending;
static uint8_t i2c_register = DEMO_I2C_REGISTER;
static uint8_t i2c_value;
#endif

static LineParser parser;
static uint32_t last_rx_epoch;
static uint32_t led_tick;
static bool led_auto;
static bool spi_ready;
static bool can_ready;
static bool spi_test_pending;

/* Buffer async phai con song den callback: khong khai bao tren stack
 * trong ham xu ly lenh roi tra ve ngay sau SPI_Submit. */
static const uint8_t spi_tx[] = {0x12U, 0x34U, 0x56U, 0xA5U};
static uint8_t spi_rx[sizeof spi_tx];

static void text(const char *message)
{
    size_t length = 0U;
    while (message[length] != '\0') ++length;
    /* Khong block de doi TX rong. Driver tra so byte nhan vao TX ring;
     * neu terminal/ung dung gay qua tai, xem tx_dropped trong STATS. */
    (void)UART_Write(&huart2, (const uint8_t *)message, length);
}

static void number(uint32_t value)
{
    char digits[10];
    size_t count = 0U;
    do {
        digits[count++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    while (count != 0U) {
        uint8_t digit = (uint8_t)digits[--count];
        (void)UART_Write(&huart2, &digit, 1U);
    }
}

static void hex_byte(uint8_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    uint8_t digits[2] = {(uint8_t)hex[value >> 4U], (uint8_t)hex[value & 15U]};
    (void)UART_Write(&huart2, digits, sizeof digits);
}

static bool equals(const char *line, const char *command)
{
    size_t i = 0U;
    while (line[i] != '\0' && line[i] == command[i]) ++i;
    return line[i] == '\0' && command[i] == '\0';
}

static void spi_complete(SPI_Result result, void *context)
{
    (void)context;
    spi_test_pending = false;
    /* Ham nay duoc SPI_Process goi trong MAIN, khong phai SPI ISR. */
    if (result != SPI_OK) {
        text(result == SPI_TIMEOUT ? "SPI timeout\r\n" : "SPI hardware error\r\n");
        return;
    }
    text("SPI RX:");
    for (size_t i = 0U; i < sizeof spi_rx; ++i) {
        text(" ");
        hex_byte(spi_rx[i]);
    }
    text("\r\n");
}

static void uart_stats(void)
{
    UART_Stats stats = {0};
    UART_GetStats(&huart2, &stats);
    text("RX="); number(stats.rx_bytes);
    text(" TX="); number(stats.tx_bytes);
    text(" RXdrop="); number(stats.rx_dropped);
    text(" TXdrop="); number(stats.tx_dropped);
    text(" RXerr="); number(stats.rx_errors);
    text(" DMAerr="); number(stats.dma_errors);
    text(" badLine="); number(parser.rejected_lines);
    text("\r\n");
}

static void i2c_report(bool accepted, I2C_Result result, uint8_t value)
{
    if (!accepted) {
        text("I2C unavailable/busy\r\n");
        return;
    }
    switch (result) {
    case I2C_OK: text("I2C RX: "); hex_byte(value); text("\r\n"); break;
    case I2C_NACK: text("I2C NACK (check device/address)\r\n"); break;
    case I2C_TIMEOUT: text("I2C timeout (check bus/pull-ups)\r\n"); break;
    case I2C_BUS_ERROR: text("I2C bus error\r\n"); break;
    case I2C_ARBITRATION_LOST: text("I2C arbitration lost\r\n"); break;
    case I2C_OVERRUN: text("I2C overrun\r\n"); break;
    }
}

#if !FIRMWARE_USE_FREERTOS
static void i2c_complete(I2C_Result result, void *context)
{
    (void)context;
    i2c_test_pending = false;
    i2c_report(true, result, i2c_value);
}
#endif

static void i2c_read_command(void)
{
#if FIRMWARE_USE_FREERTOS
    /* Comm task KHONG goi driver I2C. Queue gui gia tri cho worker so huu bus. */
    text(RTOS_QueueI2CRead(DEMO_I2C_ADDRESS, DEMO_I2C_REGISTER) ?
         "I2C request queued\r\n" : "I2C request queue full\r\n");
#else
    I2C_Transaction transaction = {
        .address = DEMO_I2C_ADDRESS, .tx = &i2c_register, .rx = &i2c_value,
        .tx_length = 1U, .rx_length = 1U, .timeout_ms = 100U,
        .callback = i2c_complete, .context = NULL
    };
    if (!i2c_ready || i2c_test_pending || !I2C_Submit(&hi2c1, &transaction))
        text("I2C busy/unavailable\r\n");
    else {
        i2c_test_pending = true;
        text("I2C queued\r\n");
    }
#endif
}

static void i2c_stats_command(void)
{
    I2C_Stats stats = {0};
#if FIRMWARE_USE_FREERTOS
    RTOS_DemoStats snapshot;
    RTOS_GetStats(&snapshot);
    stats = snapshot.i2c; /* Snapshot worker publish, khong truy cap cheo handle. */
#else
    if (!i2c_ready) { text("I2C unavailable\r\n"); return; }
    I2C_GetStats(&hi2c1, &stats);
#endif
    text("I2C OK="); number(stats.completed);
    text(" NACK="); number(stats.nacks);
    text(" timeout="); number(stats.timeouts);
    text(" error="); number(stats.bus_errors + stats.arbitration_lost + stats.overruns);
    text(" queueFull="); number(stats.queue_full);
    text("\r\n");
}

#if FIRMWARE_USE_FREERTOS
static void rtos_stats_command(void)
{
    RTOS_DemoStats stats;
    RTOS_GetStats(&stats);
    text("RTOS req="); number(stats.requests);
    text(" drop="); number(stats.request_dropped);
    text(" resp="); number(stats.responses);
    text(" respDrop="); number(stats.response_dropped);
    text(" IRQwake="); number(stats.irq_wakeups);
    text(stats.producer_enabled ? " producer=ON" : " producer=OFF");
    text(stats.i2c_ready ? " I2C=ready\r\n" : " I2C=unavailable\r\n");
    text("Stack unused WORDS Comm/Worker/Producer: ");
    number(stats.comm_stack_free); text("/"); number(stats.worker_stack_free);
    text("/"); number(stats.producer_stack_free); text("\r\n");
}
#endif

static void command(const char *line, size_t length, void *context)
{
    (void)length;
    (void)context;
    /* Chi den day khi parser nhan DU mot dong. Khong xu ly command trong ISR. */
    if (equals(line, "PING")) {
        text("PONG\r\n");
    } else if (equals(line, "HELP")) {
        text("PING | LED ON | LED OFF | LED TOGGLE | LED AUTO\r\n"
             "STATS | SPI TEST | CAN TEST | CAN STATS\r\n"
             "I2C READ | I2C STATS\r\n");
        text("IO HELP: GPIO READ, TIMER STATS, PWM 50, ADC READ\r\n");
#if FIRMWARE_USE_FREERTOS
        text("RTOS START | RTOS STOP | RTOS STATS\r\n");
#endif
    } else if (equals(line, "LED ON") || equals(line, "LED OFF")) {
        led_auto = false;
        GPIO_WriteToOutputPin(GPIOA, GPIO_PIN_NO_5,
                              equals(line, "LED ON") ? GPIO_PIN_SET : GPIO_PIN_RESET);
        text("OK\r\n");
    } else if (equals(line, "LED TOGGLE")) {
        led_auto = false;
        GPIO_ToggleOutputPin(GPIOA, GPIO_PIN_NO_5);
        text("OK\r\n");
    } else if (equals(line, "LED AUTO")) {
        led_auto = true;
        led_tick = Timebase_Millis();
        text("OK\r\n");
    } else if (equals(line, "STATS")) {
        uart_stats();
    } else if (equals(line, "I2C READ")) {
        i2c_read_command();
    } else if (equals(line, "I2C STATS")) {
        i2c_stats_command();
#if FIRMWARE_USE_FREERTOS
    } else if (equals(line, "RTOS START") || equals(line, "RTOS STOP")) {
        RTOS_SetProducerEnabled(equals(line, "RTOS START"));
        text("OK\r\n");
    } else if (equals(line, "RTOS STATS")) {
        rtos_stats_command();
#endif
    } else if (equals(line, "SPI TEST")) {
        SPI_Transaction transaction = {
            .cs_port = GPIOB, .cs_pin = 6U,
            .tx = spi_tx, .rx = spi_rx, .length = sizeof spi_tx,
            .timeout_ms = 100U, .callback = spi_complete, .context = NULL
        };
        if (!spi_ready || spi_test_pending || !SPI_Submit(&hspi1, &transaction))
            text("SPI busy/unavailable\r\n");
        else {
            spi_test_pending = true;
            text("SPI queued\r\n");
        }
    } else if (equals(line, "CAN TEST")) {
        CAN_Frame frame = {
            .id = 0x123U, .dlc = 4U, .extended = false, .remote = false,
            .data = {0x11U, 0x22U, 0x33U, 0x44U}
        };
        text(can_ready && CAN_Send(&hcan1, &frame) ? "CAN queued\r\n" :
                                              "CAN busy/unavailable\r\n");
    } else if (equals(line, "CAN STATS")) {
        if (!can_ready) {
            text("CAN unavailable\r\n");
            return;
        }
        CAN_Stats stats = {0};
        CAN_GetStats(&hcan1, &stats);
        text("CAN RX="); number(stats.rx_frames);
        text(" RXdrop="); number(stats.rx_queue_overflow);
        text(" TXok="); number(stats.tx_completed);
        text(" TXfail="); number(stats.tx_failed);
        text(" timeout="); number(stats.tx_timeouts);
        text(" busoff="); number(stats.bus_off_events);
        text("\r\n");
    } else if (equals(line, "IO HELP")) {
        text("GPIO READ | GPIO WATCH ON/OFF\r\nTIMER START/STOP/STATS\r\n"
             "PWM 0..100 | PWM START/STOP/STATS\r\n"
             "ADC INIT/READ/STATS | ADC PWM ON/OFF\r\n");
    } else if (!IO_Demo_Command(line)) {
        text("Unknown command; type HELP\r\n");
    }
}

bool CommunicationDemo_Init(void)
{
    Board_LEDInit();
    led_auto = true;
    led_tick = Timebase_Millis();
    LineParser_Init(&parser, command, NULL);
    if (!Board_UARTInit()) return false;
    last_rx_epoch = UART_RxEpoch(&huart2);
    spi_ready = Board_SPIInit();
    can_ready = Board_CANInit();
    IO_Demo_Init();
#if FIRMWARE_USE_FREERTOS
    text("\r\nSTM32F334 FreeRTOS communication demo\r\n");
#else
    i2c_ready = Board_I2CInit();
    text("\r\nSTM32F334 bare-metal communication demo\r\n");
#endif
    text(DEMO_UART_USE_DMA ? "UART RX: circular DMA + HT/TC/IDLE\r\n" :
                            "UART RX: RXNE interrupt\r\n");
    text(can_ready ? "CAN ready\r\n" : "CAN init failed\r\n");
    text("Type HELP then Enter (LF or CRLF).\r\n");
    return true;
}

static void console_process(void)
{
    uint8_t bytes[64];
    /* Snapshot epoch + lay byte trong cung critical section. Khong cho
     * parser ghep byte truoc va sau mot loi RX thanh lenh sai. */
    uint32_t saved = IRQ_Save();
    uint32_t epoch = UART_RxEpoch(&huart2);
    if (epoch != last_rx_epoch) {
        UART_DiscardRx(&huart2);
        last_rx_epoch = epoch;
        LineParser_MarkDiscontinuity(&parser);
    }
    size_t count = UART_Read(&huart2, bytes, sizeof bytes);
    IRQ_Restore(saved);
    LineParser_Feed(&parser, bytes, count);
}

void CommunicationDemo_Process(void)
{
    UART_Process(&huart2);
    IO_Demo_Process();
    if (spi_ready) SPI_Process(&hspi1);
#if FIRMWARE_USE_FREERTOS
    RTOS_I2CResponse response;
    if (RTOS_TakeI2CResult(&response))
        i2c_report(response.accepted, response.result, response.value);
#else
    if (i2c_ready) I2C_Process(&hi2c1);
#endif
    if (can_ready) {
        CAN_Process(&hcan1);
        CAN_Frame frame;
        /* Gioi han cong viec moi vong de UART/SPI khong bi doi vo han. */
        for (unsigned i = 0U; i < 2U && CAN_Receive(&hcan1, &frame); ++i) {
            text("CAN RX id="); number(frame.id); /* In decimal: 0x123 = 291. */
            text(frame.remote ? " RTR" : " data:");
            if (!frame.remote) {
                for (uint8_t j = 0U; j < frame.dlc; ++j) {
                    text(" "); hex_byte(frame.data[j]);
                }
            }
            text("\r\n");
        }
        /* Send=true chi la vao queue. Ket qua nay moi bao frame da phat
         * xong hay that bai; khong duoc nham hai moc thoi gian nay. */
        CAN_TxResult result;
        for (unsigned i = 0U; i < 3U && CAN_GetTxResult(&hcan1, &result); ++i) {
            if (result.status == CAN_TX_OK)
                text("CAN TX OK\r\n");
            else {
                text("CAN TX failed, status=");
                number((uint32_t)result.status);
                text("\r\n");
            }
        }
    }
    console_process();

    uint32_t now = Timebase_Millis();
    if (led_auto && (uint32_t)(now - led_tick) >= 500U) {
        led_tick = now;
        GPIO_ToggleOutputPin(GPIOA, GPIO_PIN_NO_5);
    }
    /* Khong delay busy-loop: CPU phai tiep tuc phuc vu cac service. */
}
