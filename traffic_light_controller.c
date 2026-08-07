#include <stdint.h>
#include "sleep.h"  // Vitis BSP: sleep(seconds), usleep(usec)


/* --------- MMIO (PS GPIO + PL peripherals) --------- */
#define LED_Data        (*((volatile uint32_t*)0x41210000))
#define Switch_Data     (*((volatile uint32_t*)0x41220000))
#define Button_Data     (*((volatile uint32_t*)0x41200000))
#define Bank0_Input     (*((volatile uint32_t*)0xE000A060))
#define Bank1_Input     (*((volatile uint32_t*)0xE000A064))
#define Bank2_Input     (*((volatile uint32_t*)0xE000A068))
#define Bank3_Input     (*((volatile uint32_t*)0xE000A06C))
#define Bank0_Output    (*((volatile uint32_t*)0xE000A040))
#define Bank1_Output    (*((volatile uint32_t*)0xE000A044))
#define Bank2_Output    (*((volatile uint32_t*)0xE000A048))
#define Bank3_Output    (*((volatile uint32_t*)0xE000A04C))
#define Bank0_Dir       (*((volatile uint32_t*)0xE000A204))
#define Bank1_Dir       (*((volatile uint32_t*)0xE000A244))
#define Bank2_Dir       (*((volatile uint32_t*)0xE000A284))
#define Bank3_Dir       (*((volatile uint32_t*)0xE000A2C4))
#define Bank0_Enable    (*((volatile uint32_t*)0xE000A208))
#define Bank1_Enable    (*((volatile uint32_t*)0xE000A248))
#define Bank2_Enable    (*((volatile uint32_t*)0xE000A288))
#define Bank3_Enable    (*((volatile uint32_t*)0xE000A2C8))


/* PMODC pins */
#define JC1  (1u<<15)
#define JC2  (1u<<16)
#define JC3  (1u<<17)
#define JC4  (1u<<18)
#define JC7  (1u<<19)
#define JC8  (1u<<20)
#define JC9  (1u<<21)
#define JC10 (1u<<22)
/* Tri-color LED on Bank0 */
#define LED12_B (1u<<16)
#define LED12_R (1u<<17)
#define LED12_G (1u<<18)
/* TX and RX pins (one-hot) */
#define TX_G  JC1
#define TX_Y  JC2
#define TX_R  JC3
#define RX_G  JC7
#define RX_Y  JC8
#define RX_R  JC9
/* --------- Helpers --------- */
static inline void gpio_set_output(uint32_t mask, volatile uint32_t* dir, volatile uint32_t* en) {
    *dir |= mask;
    *en  |= mask;
}
static inline void gpio_set_input(uint32_t mask, volatile uint32_t* dir, volatile uint32_t* en) {
    *dir &= ~mask;
    *en  &= ~mask;
}


static inline void leds_off(void) {
    Bank0_Output &= ~(LED12_B | LED12_R | LED12_G);
}
static inline void led_red(void) {
    Bank0_Output &= ~(LED12_B | LED12_G);
    Bank0_Output |=  LED12_R;
}
static inline void led_green(void) {
    Bank0_Output &= ~(LED12_B | LED12_R);
    Bank0_Output |=  LED12_G;
}
static inline void led_yellow(void) {
    Bank0_Output &= ~LED12_B;
    Bank0_Output |= (LED12_R | LED12_G);
}
/* TX drive (one-hot) */
static inline void tx_off(void) { Bank2_Output &= ~(TX_G | TX_Y | TX_R); }
static inline void tx_g(void)   { tx_off(); Bank2_Output |= TX_G; }
static inline void tx_y(void)   { tx_off(); Bank2_Output |= TX_Y; }
static inline void tx_r(void)   { tx_off(); Bank2_Output |= TX_R; }
/* Read SW0 */
static inline uint32_t sw0(void) { return (Switch_Data & 1u) ? 1u : 0u; }
/* Configure ACTIVE sender */
static inline void config_sender(void) {
    gpio_set_output(TX_G | TX_Y | TX_R, &Bank2_Dir, &Bank2_Enable);
    gpio_set_input(RX_G | RX_Y | RX_R | JC10, &Bank2_Dir, &Bank2_Enable);
    tx_off();
}
/* Configure PASSIVE receiver */
static inline void config_receiver(void) {
    gpio_set_input(TX_G | TX_Y | TX_R | JC4, &Bank2_Dir, &Bank2_Enable);
    tx_off();
}
/* Detect quiet bus for 'ms_quiet' milliseconds */
static int bus_is_quiet_ms(uint32_t ms_quiet) {
    uint32_t ok_ms = 0;
    while (ok_ms < ms_quiet) {
        if (Bank2_Input & (RX_G | RX_Y | RX_R)) {
            ok_ms = 0;
        } else {
            ok_ms += 1;
        }
        usleep(1000);
    }
    return 1;
}
/* ========================= main ========================= */
int main(void) {
    // Initialize LED
    gpio_set_output(LED12_B | LED12_R | LED12_G, &Bank0_Dir, &Bank0_Enable);
    led_red();


    // Default: tri-state TX (both boards start passive)
    config_receiver();
    led_red();
    int i_am_active = 0;
    while (1) {
        // Check SW0 dynamically: EW board flips SW0=1 → claim ACTIVE
        if (!i_am_active && sw0()) {
            config_sender();
            i_am_active = 1;
        }
        if (i_am_active) {
            // ACTIVE cycle matching your flowchart
            // Step 1: NS=G, EW=R, 2.5s
            led_green();  // NS green
            tx_g();
            sleep(2.5);
            // Step 2: NS=Y, EW=R, 2.5s
            led_yellow(); // NS yellow
            tx_y();
            sleep(2.5);
            // Step 3: NS=R, EW=R, 1.5s
            led_red(); 
            tx_r();
            sleep(1.5);
            // Step 4: NS=R, EW=G, 2.5s
            led_green();  // EW green
            tx_g();
            sleep(2.5);
            // Step 5: NS=R, EW=Y, 2.5s
            led_yellow(); // EW yellow
            tx_y();
            sleep(2.5);
            // Step 6: NS=R, EW=R, 1.5s
            led_red();
            tx_r();
            sleep(1.5);
            // Release bus, switch to passive
            tx_off();
            config_receiver();
            led_red();
            i_am_active = 0;
        } else {
            // Passive: enforce red while partner is active
            while (Bank2_Input & (RX_G | RX_Y | RX_R)) {
                led_red();
                usleep(1000);
            }
        }
    }
}