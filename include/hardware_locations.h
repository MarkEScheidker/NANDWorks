#ifndef HARDWARE_LOCATIONS
#define HARDWARE_LOCATIONS

#include <time.h>
#include <fstream>

#define PROFILE_TIME true

#define FORCE_INLINE __attribute__((always_inline))

// #define START_TIME (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tvInitialns))
#define START_TIME {\
*(interval_timer+2) = 0xffff;\
*(interval_timer+3) = 0xffff;\
*(interval_timer+1) = 0x04;\
}

// #define END_TIME (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tvFinalns))
#define END_TIME {\
*(interval_timer+4) = 0x01;\
*(interval_timer+1) = 0x08;\
}

// #define PRINT_TIME time_info_file<<"  took "<<(tvFinalns.tv_sec-tvInitialns.tv_sec)*1000000000L+(tvFinalns.tv_nsec-tvInitialns.tv_nsec)<<" nanoseconds\n"
#define PRINT_TIME time_info_file<<"  took "<<(0xffffffff-(((*(interval_timer+4))&0xffff)+((*(interval_timer+5))<<16)))*10<<" nanoseconds\n"

// #define GET_TIME_ELAPSED ((tvFinalns.tv_sec-tvInitialns.tv_sec)*1000000000L+(tvFinalns.tv_usec-tvInitialns.tv_usec))

#define GET_TIME_ELAPSED ((0xffffffff-(*(interval_timer+4)+0xffff*(*(interval_timer+5)))*10)

namespace hw {
static constexpr int LW_BRIDGE_BASE = 0xff200000;
static constexpr int LW_BRIDGE_SPAN = 0x5000;

// red LEDs for debugging
static constexpr int RLED_OFFSET = 0x0;

/**
The computer system used here is DE1_SoC that runs at 100Mhz (10 ns period)
 connection to the NAND is made in parallel port 1 on JP1
.. the address of which is at 0xff200060
.. so here we will create masks for the data to be read/written
*/
static constexpr int JP1_location_OFFSET = 0x060;

/**
this is the offset of timer0 from LW_Bridge_base
.. on FPGA
*/
static constexpr int INTERVAL_TIMER_OFFSET = 0x2000;

/**
value 1 in direction register means output
.. 0 means input
.. following is just the address of the register
*/
static constexpr int JP1_direction_OFFSET = 0x0064;

static constexpr int PUSH_KEY_LOCATION_OFFSET = 0x0050;

static constexpr int DQ_mask = 0x000000ff;    ///<connected at D7D6..D0

static constexpr int WP_shift = 8;
static constexpr int WP_mask = (0x1<<WP_shift);	///< connected at D8

static constexpr int CLE_shift = 11;
static constexpr int CLE_mask = (0x1<<CLE_shift); ///< connected at D11

static constexpr int ALE_shift = 10;
static constexpr int ALE_mask = (0x1<<ALE_shift); ///< connected at D10

static constexpr int RE_shift = 13;
static constexpr int RE_mask = (0x1<<RE_shift); ///< connected at D13

static constexpr int WE_shift = 9;
static constexpr int WE_mask = (0x1<<WE_shift); ///< connected at D9

static constexpr int CE_shift = 12;
static constexpr int CE_mask = (0x1<<CE_shift); ///< connected at D12

static constexpr int RB_shift = 14;
static constexpr int RB_mask = (0x1<<RB_shift); ///< connected to D14

static constexpr int DQS_shift = 15;
static constexpr int DQS_mask = (0x01<<DQS_shift);	///< for synchronous D15

static constexpr int DQSc_shift = 16;
static constexpr int DQSc_mask = (0x01<<DQSc_shift);	///< for synchronous D16

#define SAMPLE_TIME asm("nop");asm("nop")
#define HOLD_TIME {asm("nop");}
// 100ns
#define tWW asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");asm("nop")
#define tWB {asm("nop"); asm("nop");}
#define tRR {uint8_t ii=0;for(ii=0;ii<4;ii++) asm("nop");}
#define tRHW tWB
#define tCCS tWB
#define tADL tWB
#define tWHR {uint8_t ii=0;for(ii=0;ii<12;ii++) asm("nop");} // .. tWHR = 120ns
} // namespace hw


#endif