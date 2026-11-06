#include "rtos_demo.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "rtos_hooks.h"
#include "board_peripherals.h"
#include "communication_demo.h"
#include "demo_config.h"

#define REQUEST_COUNT 4U
#define RESPONSE_COUNT 4U
#define COMM_STACK_WORDS 384U
#define WORKER_STACK_WORDS 256U
#define PRODUCER_STACK_WORDS 192U
_Static_assert(configCPU_CLOCK_HZ == DEMO_CORE_HZ, "RTOS/board CPU clock mismatch");
_Static_assert(pdMS_TO_TICKS(1U) != 0U, "Worker needs at least one tick per ms");

typedef struct { uint8_t address, reg; } I2C_Request;
typedef struct { bool done; I2C_Result result; } I2C_Completion;

static StaticTask_t comm_tcb, worker_tcb, producer_tcb;
static StackType_t comm_stack[COMM_STACK_WORDS] __attribute__((aligned(8)));
static StackType_t worker_stack[WORKER_STACK_WORDS] __attribute__((aligned(8)));
static StackType_t producer_stack[PRODUCER_STACK_WORDS] __attribute__((aligned(8)));
static TaskHandle_t comm_task, worker_task, producer_task;
static StaticQueue_t request_storage, response_storage;
static uint8_t request_items[REQUEST_COUNT * sizeof(I2C_Request)];
static uint8_t response_items[RESPONSE_COUNT * sizeof(RTOS_I2CResponse)];
static QueueHandle_t request_queue, response_queue;
static StaticSemaphore_t stats_mutex_storage;
static SemaphoreHandle_t stats_mutex;
static RTOS_DemoStats counters;
static volatile bool i2c_irq_bridge_ready;
static volatile uint32_t irq_wakeups;

static void stats_lock(void)
{
    configASSERT(xSemaphoreTake(stats_mutex, portMAX_DELAY) == pdTRUE);
}

static void stats_unlock(void)
{
    configASSERT(xSemaphoreGive(stats_mutex) == pdTRUE);
}

bool RTOS_QueueI2CRead(uint8_t address, uint8_t reg)
{
    if (address < 0x08U || address > 0x77U) return false;
    I2C_Request request = {address, reg};
    /* Queue COPY HAI GIA TRI, khong giu con tro vao stack cua producer. */
    bool queued = xQueueSend(request_queue, &request, 0U) == pdPASS;
    stats_lock();
    if (queued) ++counters.requests;
    else ++counters.request_dropped;
    stats_unlock();
    return queued;
}

bool RTOS_TakeI2CResult(RTOS_I2CResponse *response)
{
    return response != NULL && xQueueReceive(response_queue, response, 0U) == pdPASS;
}

void RTOS_SetProducerEnabled(bool enabled)
{
    stats_lock();
    counters.producer_enabled = enabled;
    stats_unlock();
}

void RTOS_GetStats(RTOS_DemoStats *out)
{
    if (out == NULL) return;
    /* Mutex giu snapshot cac field nhat quan; khong UART/driver/queue wait
     * trong doan khoa. Mutex co priority inheritance, khac semaphore nhi phan. */
    stats_lock();
    *out = counters;
    stats_unlock();
    out->irq_wakeups = irq_wakeups;
    /* High water la so WORD chua tung dung, khong phai byte hay con trong hien tai. */
    out->comm_stack_free = uxTaskGetStackHighWaterMark(comm_task);
    out->worker_stack_free = uxTaskGetStackHighWaterMark(worker_task);
    out->producer_stack_free = uxTaskGetStackHighWaterMark(producer_task);
}

void RTOS_I2CNotifyFromISR(void)
{
    /* IRQ co the den trong Init truoc khi worker san sang. Driver van xu ly,
     * nhung chi bridge vao kernel sau khi scheduler va worker da hoat dong. */
    if (!i2c_irq_bridge_ready) return;
    BaseType_t wake = pdFALSE;
    ++irq_wakeups;
    vTaskNotifyGiveFromISR(worker_task, &wake);
    portYIELD_FROM_ISR(wake);
}

static void i2c_complete(I2C_Result result, void *context)
{
    I2C_Completion *completion = context;
    /* Callback cua I2C_Process: chay trong WORKER, khong chay trong ISR.
     * Notification tu ISR chi bao 'kiem tra lai state', khong bao da xong goi. */
    completion->result = result;
    completion->done = true;
}

static void publish_response(const RTOS_I2CResponse *response)
{
    I2C_Stats i2c_stats = {0};
    I2C_GetStats(&hi2c1, &i2c_stats); /* Chi worker truy cap driver I2C. */
    bool published = xQueueSend(response_queue, response, 0U) == pdPASS;
    stats_lock();
    counters.i2c = i2c_stats;
    if (published) ++counters.responses;
    else ++counters.response_dropped;
    stats_unlock();
}

static void i2c_worker(void *argument)
{
    (void)argument;
    bool ready = Board_I2CInit();
    stats_lock();
    counters.i2c_ready = ready;
    stats_unlock();
    i2c_irq_bridge_ready = ready;
    for (;;) {
        I2C_Request request;
        /* Khong co viec thi Blocked; CPU danh cho task khac/Idle. */
        if (xQueueReceive(request_queue, &request, portMAX_DELAY) != pdPASS) continue;
        uint8_t register_byte = request.reg;
        uint8_t value = 0U;
        I2C_Completion completion = {false, I2C_BUS_ERROR};
        I2C_Transaction transaction = {
            .address = request.address, .tx = &register_byte, .rx = &value,
            .tx_length = 1U, .rx_length = 1U, .timeout_ms = 100U,
            .callback = i2c_complete, .context = &completion
        };
        /* Cac bien tren stack van song vi worker KHONG roi vong cho callback
         * truoc khi transfer hoan tat. Khong Submit con tro stack roi return. */
        bool accepted = ready && I2C_Submit(&hi2c1, &transaction);
        if (accepted) {
            (void)ulTaskNotifyTake(pdTRUE, 0U); /* Bo wake cu; Process doc status thuc. */
            while (!completion.done) {
                I2C_Process(&hi2c1);
                if (!completion.done)
                    (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1U));
            }
        }
        RTOS_I2CResponse response = {
            .address = request.address, .reg = request.reg, .value = value,
            .accepted = accepted, .result = completion.result
        };
        publish_response(&response);
    }
}

static void producer(void *argument)
{
    (void)argument;
    TickType_t previous = xTaskGetTickCount();
    for (;;) {
        /* DelayUntil giu chu ky tinh tu moc truoc, giam drift so voi Delay. */
        (void)xTaskDelayUntil(&previous, pdMS_TO_TICKS(2000U));
        stats_lock();
        bool enabled = counters.producer_enabled;
        stats_unlock();
        if (enabled)
            (void)RTOS_QueueI2CRead(DEMO_I2C_ADDRESS, DEMO_I2C_REGISTER);
    }
}

static void communications(void *argument)
{
    (void)argument;
    /* Chi task nay goi UART/SPI/CAN, IO/TIM/ADC va callback ung dung.
     * Init sau scheduler: IRQ bridge khong the pend switch truoc scheduler. */
    if (!CommunicationDemo_Init()) RTOS_Fatal(RTOS_FAULT_STARTUP);
    for (;;) {
        CommunicationDemo_Process();
        /* Khong quay while(1) khong block o priority2: Worker/Producer priority1
         * se khong duoc chay neu task nay khong nhuong CPU. */
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
}

void RTOS_Start(void)
{
    request_queue = xQueueCreateStatic(REQUEST_COUNT, sizeof(I2C_Request),
                                       request_items, &request_storage);
    response_queue = xQueueCreateStatic(RESPONSE_COUNT, sizeof(RTOS_I2CResponse),
                                        response_items, &response_storage);
    stats_mutex = xSemaphoreCreateMutexStatic(&stats_mutex_storage);
    configASSERT(request_queue != NULL && response_queue != NULL && stats_mutex != NULL);
    vQueueAddToRegistry(request_queue, "I2C requests");
    vQueueAddToRegistry(response_queue, "I2C responses");
    vQueueAddToRegistry(stats_mutex, "Stats mutex");
    comm_task = xTaskCreateStatic(communications, "Comm", COMM_STACK_WORDS,
                                  NULL, 2U, comm_stack, &comm_tcb);
    worker_task = xTaskCreateStatic(i2c_worker, "I2CWorker", WORKER_STACK_WORDS,
                                    NULL, 1U, worker_stack, &worker_tcb);
    producer_task = xTaskCreateStatic(producer, "Producer", PRODUCER_STACK_WORDS,
                                      NULL, 1U, producer_stack, &producer_tcb);
    configASSERT(comm_task != NULL && worker_task != NULL && producer_task != NULL);
    vTaskStartScheduler();
    RTOS_Fatal(RTOS_FAULT_STARTUP); /* Scheduler tren MCU khong duoc return. */
}
