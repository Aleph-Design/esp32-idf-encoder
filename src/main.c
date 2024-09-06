/**
 * Created: 20240903
 * Author:  Jan H. Kila van Zuijlen
 * 
 * ESP32 ROTARY ENCODER PORTED TO ESPRESSIF IDE
 * ============================================
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"    
#include "freertos/task.h"
#include "driver/gpio.h"

#define GPIO_ENC_A  GPIO_NUM_26
#define GPIO_ENC_B  GPIO_NUM_27

// a collection of all input pins, used in GPIO configuration
#define GPIO_BIT_MASK ((1ULL <<  GPIO_ENC_A) | (1ULL <<  GPIO_ENC_B))

/**
 * MAX_COUNT: total elements in the stations list MINUS 1
 */
#define MAX_COUNT 9 

#define ESP_INTR_FLAG 0   // Interrupt allocation flag

/**
 * Flag to check whether the rotary encoder shaft has 
 * been rotated
 * This is not needed because we define and initialize
 * this variable inside rotaryHandler()
 */
// volatile bool rotaryMoved = false;
// bool rotaryMoved = false;

/**
 * Define and initialize a queHandle
 */
static QueueHandle_t gpioQueueHandle = NULL;

/**
 * Interrupt handler
 * Just resets the rotaryMoved flag when ever there's
 * a movement on the rotary encoder shaft is detected.
 */
// static void IRAM_ATTR rotaryHandler(void *arg)
static void IRAM_ATTR rotaryHandler(void*)
{
  bool rotaryMoved = true;

  /**
   * just to satisfy the watchdog; AND IT WORKS!
   */
  // uint32_t gpioPIN = (uint32_t) arg;
  xQueueSendFromISR(gpioQueueHandle, &rotaryMoved, NULL);
}

/**
 * The rotary encoder shaft has rotated (the interrupt tells us so) 
 * but what happened?
 * For details and explanation see: https://www.pinteric.com/rotary.html
 */
int8_t checkRotaryEncoder()
{
    // Reset the flag that brought us here (from ISR)
    // We don't do anything usefull with it anymore
    // rotaryMoved = false;

    /**
     * Initial state of lrmem = 0b0011 (3) an impossible movement
     */
    static uint8_t lrmem = 3;
    /**
     * @lrIdx:
     * -  The movement indicator: actually the value received from a
     *    lookup in array TRANS.
     */
    static int lrIdx = 0;
    /**
     * An array that holds a value that tells us what kind of movement
     * has taken place. See comment in 'Some-Explanation'
     */
    static int8_t TRANS[] = {0, -1, 1, 14, 1, 0, 14, -1, -1, 14, 0, 1, 14, 1, -1, 0};

    /**
     * Read the state of BOTH pins ENC_A and ENC_B.
     * The step or state sequence between two indents determines 
     * the direction of rotation:
     * For Clockwise rotation
     * ----------------------
     * MSB  LSB
     * encA encB 
     *   1   1      'rest'- or detent position
     *   0   1
     *   0   0      
     *   1   0
     * 
     * For Anti-Clockwise rotation
     * ---------------------------
     * MSB  LSB
     * encA encB 
     *   1   1      'rest'- or detent position
     *   1   0
     *   0   0      
     *   0   1
     */
    int8_t encA = gpio_get_level(GPIO_ENC_A);
    int8_t encB = gpio_get_level(GPIO_ENC_B);

    /**
     * Here we move the value of the previous state (lrmem) 2 bits to the left
     * then add the value encA (the most significant bit) times 2 => 10 | 00 
     * and add the value of encB (the Least significant bit => 0 | 1)
     * to the previous value of lrmem ANDed with mask 0x03 (0000 0011)
     * Why 0x03; that's the 'rest' or detent state of the encoder.
     * 
     * lrmem is a step or state value in the range 0b0000 to 0b1111
     * lrmem is the index into array TRANS to fetch the lrIdx value.
     */
    lrmem = ((lrmem & 0x03) << 2) + 2 * encA + encB;

    /**
     * Lookup the corresponding lrIdx value in array TRANS with index lrmem
     */
    lrIdx += TRANS[lrmem];

    /** 
     * when the encoder is not in the neutral (detent) state, the
     * sequence is not complete: the function just returns a zero.
     */ 
    if (lrIdx % 4 != 0) return 0;

    /**
     * when the encoder IS in the neutral (detent) state AND lrIdx is
     * positive then we have - one step of clockwise rotation.
     */
    if (lrIdx == 4) {lrIdx = 0; return 1;}

    /**
     * when the encoder IS in the neutral state AND lrIdx is negative
     * the we have - one step of anti-clockwise rotation.
     */
    if (lrIdx == -4) {lrIdx = 0; return -1;}

    /**
     * We detected an impossible rotation - ignore the movement.
     * As said: this is logicallly correct, but practically impossible.
     */ 
    lrIdx = 0;
    return 0;

    vTaskDelay(200 / portTICK_PERIOD_MS);
}

/**
 * Movement counter, either a left or a right turn. This is
 * going to be the index into the available stations list. 
 * The range is: 0 <= counter <= MAX-COUNT
 */
uint8_t rotationCounter = 0;    

/**
 * Display rotationCounter value. 
 * Eventually this is the index in the stations list.
 * It's to be transferred to the task that handles the search
 * for a station.
 */
void displayCounter(uint8_t counter)
{
  printf("Counter: %d\n", rotationCounter); 
}

int8_t rotationValue = 0;

/**
 * rotaryTask
 * - first try: did not set the watchdog in time.
 * - solution: use a queue (to slow down the execution?)
 */
static void rotaryTask(void *pvParam)
{
  bool rotaryMoved;

  while(1){

    if (xQueueReceive(gpioQueueHandle, &rotaryMoved, portMAX_DELAY))
    // if (rotaryMoved)
    {
      // Get the movement; either -1, 0, 1
      rotationValue = checkRotaryEncoder();

      // if there's a sensible movement update counter
      if (rotationValue != 0)
      {
        if (rotationValue < 1)
        {
          if (rotationCounter == 0) rotationCounter = MAX_COUNT + 1;
          rotationCounter += rotationValue;
        } else {
          rotationCounter += rotationValue;
          if (rotationCounter > MAX_COUNT) rotationCounter = 0;
        }
        displayCounter(rotationCounter);
      }
    }
  }
}

/* ----------------------------------------------------------- */

void app_main() 
{
  /**
   * Define and initialize a struct for the GPIO
   */
  gpio_config_t ioConfig = {
    .intr_type    = GPIO_INTR_ANYEDGE,    // interrupt on CHANGE
    .mode         = GPIO_MODE_INPUT,
    .pin_bit_mask = GPIO_BIT_MASK,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .pull_up_en   = GPIO_PULLUP_ENABLE,
  };
  gpio_config(&ioConfig);    // Configure GPIO with give settings

  /**
   * Install GPIO ISR handler service.
   * This allows GPIO interrupt handlers on PER pin basis.
   */
  gpio_install_isr_service(ESP_INTR_FLAG);

  /**
   * hook up the ISR handler 'rotaryHandler()' to the respective
   * GPIO pins.
   * The pin numbers are transferred into the ISR handler = needed?
   * What happens when we don't pass any arguments?
   */
  // gpio_isr_handler_add(GPIO_ENC_A, rotaryHandler, (void*) GPIO_ENC_A);
  // gpio_isr_handler_add(GPIO_ENC_B, rotaryHandler, (void*) GPIO_ENC_B);
  gpio_isr_handler_add(GPIO_ENC_A, rotaryHandler, 0);
  gpio_isr_handler_add(GPIO_ENC_B, rotaryHandler, 0);

  /**
   * Initialize queue handle
   */
  gpioQueueHandle = xQueueCreate(10, sizeof(bool));

  /**
   * We have to display the counter when it's initialized
   * Otherwise it get's lost.
   */
  displayCounter(0);

  /**
   * Create and start 'rotaryTask'
   */
  xTaskCreatePinnedToCore(
        rotaryTask,
        "rotaryTask",
        2048,
        NULL,
        1,
        NULL,
        1
  );
  
} /* --- END MAIN --- */

