#ifndef BIQU_SKR_H
#define BIQU_SKR_H

#include "../Pins_STM32.h"

// List of assignable pins and their mapping from names to MPU ports. This is indexed by logical pin number.
// The names must match user input that has been converted to lowercase and had _ and - characters stripped out.
// Aliases are separate by the , character.
// If a pin name is prefixed by ! then this means the pin is hardware inverted. The same pin may have names for both the inverted and non-inverted cases,
// for example the inverted heater pins on the expansion connector are available as non-inverted servo pins on a DFueX.
#if STM32H7
constexpr PinEntry PinTable_BIQU_SKR_SE_BX_v2_0[] =
{
    //Thermistors
    {PH_4, "e0temp,th0"},
    {PA_3, "e1temp,th1"},
    {PH_5, "e2temp,thb"},

    //Endstops
    {PB_11, "xstop,x-"},
    {PD_13, "xstopmax,x+"},
    {PB_12, "ystop,y-"},
    {PB_13, "ystopmax,y+"},
    {PD_12, "zstop,z-"},
    {PD_11, "zstopmax,z+"},
	{PH_2, "probe"},
	
    //Heaters and Fans (Big and Small Mosfets}
    {PA_4,  "bed,hbed" },
    {PC_4,  "e0heat,he0" },
    {PC_5,  "e1heat,he1" },
    {PA_5,  "fan0,fan" },
    {PA_6,  "fan1" },
    {PA_7,  "fan2" },

    //Servos
    {PA_2,  "servo0" },
	

	//Extension 1
	{PC_9, "PC9"},
	{PF_9, "PF9"},
	{PC_4, "PC4"},
	{PG_11, "PG11"},
	{PG_14, "PG14"},
	{PC_1, "PC1"},
	{PF_8, "PF8"},
	{PF_10, "PF10"},
	{PC_5, "PC5"},
	{PG_13, "PG13"},
	{PD_3, "PD3"},
	{PF_7, "PF7"},

	//Extension 2
	{PD_0, "PD0"},
	{PD_2, "PD2"},
	{PD_0, "PD5"},
	{PE_0, "PE0"},
	{PE_2, "PE2"},
	{PE_4, "PE4"},

	//Wifi
	{PA_0, "wifi-tx,TX4"},
	{PA_1, "wifi-rx,RX4"},
    //TFT
	{PA_9, "tft-tx,TX1"},
	{PA_10, "tft-rx,RX1"},
};

constexpr BoardDefaults biqu_skr_se_bx_v2_0_Defaults = {
    {0xf1832a2},                 	            // Signatures
    SD_SPI3_B,                                  // SD Card access
    {   //CLK, MISO, MOSI
        {NoPin, NoPin, NoPin},                  //SPI0
        {NoPin, NoPin, NoPin},                  //SPI1
        {PC_10, PC_11, PC_12},                  //SPI2
        {NoPin, NoPin, NoPin},                  //SPI3
        {NoPin, NoPin, NoPin},                  //SPI4
        {NoPin, NoPin, NoPin},                  //SPI5
        {PE_2,  PE_5,  PE_6},                   //SPI6
        {NoPin, NoPin, NoPin},                  //SPI7
        {NoPin, NoPin, NoPin},                  //SPI8
    },
	5,											// Number of drivers
    {PG_14, PB_4, PG_9, PC_15, PD_2},        	//enablePins
    {PG_13, PB_3, PD_7, PC_14, PA_8},	        //stepPins
    {PG_12, PD_3, PD_6, PC_13, PC_9},    	    //dirPins
#if HAS_SMART_DRIVERS
    {PG_10, PD_4, PD_5, PI_8, PC_8},            //uartPins
    5,                                      	// Smart drivers
#endif
    0,                                       	//digiPot Factor
#if HAS_VOLTAGE_MONITOR
    NoPin,
#endif
    PI_11,
#if HAS_SBC_INTERFACE
    PH_12, PH_10, SSP4,
#endif
};

constexpr PinEntry PinTable_BTT_SKR_3[] =
{
    //Thermistors
    {PA_1, "bedtemp,tb"},
    {PA_2, "e0temp,th0"},
    {PA_3, "e1temp,th1"},

    //Endstops
    {PC_1, "xstop,x-stop"},
    {PC_3, "ystop,y-stop"},
    {PC_0, "zstop,z-stop"},
    {PC_2, "e0stop,e0det"},
    {PA_0, "e1stop,e1det"},

    //Servos
    {PE_5,  "servo0" },

    //Probe
    {PC_13, "probe"},

    //Heaters and Fans (Big and Small Mosfets}
    {PD_7,  "bed,hbed" },
    {PB_3,  "e0heat,heat0" },
    {PB_4,  "e1heat,heat1" },
    {PB_7,  "fan0,fan" },
    {PB_6,  "fan1" },
    {PB_5,  "fan2" },

    //Neopixel
    {PE_6, "Neopixel"},

    //PSON
    {PE_4, "PSON"},

    //PWRDET
    {PC_15, "PWRDET"},

    //Status LED
    {PA_13, "LED"},
	
    //EXP1
    {PC_5, "BEEP"},
    {PB_0, "BTN_ENC"},
    {PB_1, "LCD_EN"},
    {PE_8, "LCD_RS"},
    {PE_9, "LCD_D4"},
    {PE_10, "LCD_D5"},
    {PE_11, "LCD_D6"},
    {PE_12, "LCD_D7"},

    //EXP2
    {PA_6, "LCD_MISO"},
    {PA_5, "LCD_SCK"},
    {PE_7, "BTN_EN1"},
    {PA_4, "LCD_SS"},
    {PB_2, "BTN_EN2"},
    {PA_7, "LCD_MOSI"},
    {PC_4, "LCD_CD"},
};

constexpr BoardDefaults btt_skr_3_Defaults = {
    {0xaa36a0c4},                  // Signatures
    SD_SDIO,                                  // SD Card access
    {   //CLK, MISO, MOSI
        {PA_5, PA_6, PA_7},                     //SPI0
        {PB_13, PB_14, PB_15},                  //SPI1
        {NoPin, NoPin, NoPin},                  //SPI2
        {NoPin, NoPin, NoPin},                  //SPI3
        {NoPin, NoPin, NoPin},                  //SPI4
        {NoPin, NoPin, NoPin},                  //SPI5
        {NoPin, NoPin, NoPin},                  //SPI6
        {NoPin, NoPin, NoPin},                  //SPI7
        {NoPin, NoPin, NoPin},                  //SPI8
    },
    5,                            // Number of drivers
    {PD_6, PD_1, PE_0, PC_7, PD_13}, // enablePins
    {PD_4, PA_15, PE_2, PD_15, PD_11},  // stepPins
    {PD_3, PA_8, PE_3, PD_14, PD_10},    // dirPins
#if TMC_SOFT_UART
    {PD_5, PD_0, PE_1, PC_6, PD_12},    // uartpins
    5,                            // Smart drivers
#endif
    0,                            // digiPot Factor
#if HAS_VOLTAGE_MONITOR
    NoPin,
#endif
    NoPin,
#if HAS_SBC_INTERFACE
    NoPin, NoPin, SSPNONE,
#endif
};

#else
constexpr PinEntry PinTable_BIQU_SKR_PRO_v1_1[] =
{
    //Thermistors
    {PF_3, "e0temp,t0"},
    {PF_4, "e1temp,t1"},
    {PF_5, "e2temp,t2"},
    {PF_6, "bedtemp,t3"},

    //Endstops
    {PB_10, "xstop,x-stop"},
    {PE_12, "ystop,y-stop"},
    {PG_8, "zstop,z-stop"},
    {PE_15, "e0stop,e0det"},
    {PE_10, "e1stop,e1det"},
    {PG_5, "e2stop,e2det"},
    {PA_2, "probe"},

    //Heaters and Fans (Big and Small Mosfets}
    {PD_12,  "bed,hbed" },
    {PB_1,  "e0heat,he0" },
    {PD_14,  "e1heat,he1" },
    {PB_0,  "e2heat,he2" },
    {PC_8,  "fan0,fan" },
    {PE_5,  "fan1" },
    {PE_6,  "fan2" },

    //Servos
    {PA_1,  "servo0" },

    //EXP1
    {PG_4, "BEEP"},
    {PA_8, "BTNENC"},
    {PD_11, "LCD_EN"},
    {PD_10, "LCD_RS"},
    {PG_2, "LCD_D4"},
    {PG_3, "LCD_D5"},
    {PG_6, "LCD_D6"},
    {PG_7, "LCD_D7"},

    //EXP2
    {PB_14, "LCD_MISO"},
    {PB_13, "LCD_SCK"},
    {PG_10, "BTN_EN1"},
    {PB_12, "LCD_SS"},
    {PF_11, "BTN_EN2"},
    {PB_15, "LCD_MOSI"},
    {PF_12, "LCD_CD"},
    {PF_13, "KILL"},

    //Wifi
    {PG_0, "wifi1"},
    {PG_1, "wifi2"},
    {PC_7, "wifi3"},
    {PC_6, "wifi4"},
    {PF_14, "wifi5"},
    {PF_15, "wifi6"},

    //SPI
    {PA_15, "X-CS"},
    {PB_9, "Z-CS"},
    {PB_8, "Y-CS"},
    {PB_3, "E0-CS"},
    {PG_15, "E1-CS"},
    {PG_12, "E2-CS"},

};

constexpr BoardDefaults biquskr_pro_1_1_Defaults = {
    {0x768a39d6, 0x50da391},                    // Signatures
    SD_SPI1_A,                                  // SD Card access
    {   //CLK, MISO, MOSI
        {PA_5, PA_6, PB_5},                     //SPI0
        {PB_13, PB_14, PB_15},                  //SPI1
        {PC_10, PC_11, PC_12},                  //SPI2
        {NoPin, NoPin, NoPin},                  //SPI3
        {NoPin, NoPin, NoPin},                  //SPI4
        {NoPin, NoPin, NoPin},                  //SPI5
    },
    6,                                          // Number of drivers
    {PF_2, PD_7,  PC_0, PC_3,  PA_3, PF_0},     //enablePins
    {PE_9, PE_11, PE_13, PE_14,  PD_15, PD_13}, //stepPins
    {PF_1, PE_8, PC_2, PA_0,  PE_7, PG_9},      //dirPins
#if HAS_SMART_DRIVERS
    {PC_13, PE_3, PE_1, PD_4, PD_1, PD_6},      //uartPins
    6,                                          // Smart drivers
#endif
    0,                                          //digiPot Factor
#if HAS_VOLTAGE_MONITOR
    NoPin,
#endif
    NoPin,
#if HAS_SBC_INTERFACE
    PF_12, PB_12, SSP2,
#endif
};


constexpr PinEntry PinTable_BIQU_GTR_v1_0[] =
{
    //Thermistors
    {PC_1, "e0temp,t0"},
    {PC_2, "e1temp,t1"},
    {PC_3, "e2temp,t2"},
    {PC_0, "bedtemp,t3"},
    {PA_3, "e3temp,TempM1"},
    {PF_9, "e4temp,TempM2"},
    {PF_10, "e5temp,TempM3"},
    {PF_7, "e6temp,TempM4"},
    {PF_5, "e7temp,TempM5"},

    //Endstops
    {PF_2, "xstop,x-stop"},
    {PC_13, "ystop,y-stop"},
    {PE_0, "zstop,z-stop"},
    {PG_14, "e0stop,e0det"},
    {PG_9, "e1stop,e1det"},
    {PD_3, "e2stop,e2det"},
    {PI_4, "e3stop,M1Stop"},
    {PF_4, "e4stop,M2Stop"},
    {PF_6, "e5stop,M3Stop"},
    {PI_7, "e6stop,M4Stop"},
    {PF_12, "e7stop,M5Stop"},
    {PH_11, "probe"},
    {PI_11, "EI1"},
    {PH_6, "EI2"},

    //Heaters and Fans (Big and Small Mosfets}
    {PA_2,  "bed,hbed" },
    {PB_1,  "e0heat,heat0" },
    {PA_1,  "e1heat,heat1" },
    {PB_0,  "e2heat,heat2" },
    {PD_15,  "e3heat,heatM1" },
    {PD_13,  "e4heat,heatM2" },
    {PD_12,  "e5heat,heatM3" },
    {PE_13,  "e6heat,heatM4" },
    {PI_6,  "e7heat,heatM5" },
    {PE_5,  "fan0" },
    {PE_6,  "fan1" },
    {PC_8,  "fan2" },
    {PI_5,  "fanM1" },
    {PE_9,  "fanM2" },
    {PE_11,  "fanM3" },
    {PC_9,  "fanM4" },
    {PE_14,  "fanM5" },

    //Servos
    {PB_11,  "servo0" },

    //EXP1
    {PC_11, "BEEP"},
    {PA_15, "BTNENC"},
    {PC_10, "LCD_EN"},
    {PA_8, "LCD_RS"},
    {PG_8, "LCD_D4"},
    {PG_7, "LCD_D5"},
    {PG_6, "LCD_D6"},
    {PG_5, "LCD_D7"},

    //EXP2
    {PB_14, "LCD_MISO"},
    {PB_13, "LCD_SCK"},
    {PD_10, "BTN_EN1"},
    {PB_12, "LCD_SS"},
    {PH_10, "BTN_EN2"},
    {PB_15, "LCD_MOSI"},
    {PB_10, "LCD_CD"},

    //Neopixel
    {PF_13, "Neopixel"},

    //Wifi
    {PF_11, "wifi1"},
    {PC_7, "wifi3"},
    {PC_6, "wifi4"},
    {PC_5, "wifi5"},

    //SPI
    {PC_14, "X-CS"},
    {PE_1, "Y-CS"},
    {PB_5, "Z-CS"},
    {PG_10, "E0-CS"},
    {PD_4, "E1-CS"},
    {PC_12, "E2-CS"},
    {PB_6, "MISO"},
    {PG_15, "MOSI"},
    {PB_3, "SCK"},

    //Extra M5
    {PI_2, "KMOSI"},
    {PI_1, "KSCK"},
    {PH_2, "KCS"},
    {PF_13, "RGB_LED"},
};

constexpr BoardDefaults biqu_gtr_1_0_Defaults = {
    {0x94a2cc03},                               // Signatures
    SD_SPI1_B,                                  // SD Card access
    {   //CLK, MISO, MOSI
        {PA_5, PA_6, PA_7},                     //SPI0
        {PB_13, PB_14, PB_15},                  //SPI1
        {NoPin, NoPin, NoPin},                  //SPI2
        {NoPin, NoPin, NoPin},                  //SPI3
        {NoPin, NoPin, NoPin},                  //SPI4
        {NoPin, NoPin, NoPin},                  //SPI5
    },
    6+5,                                        // Number of drivers GTR + M5
    {PF_1, PE_4,  PB_9, PG_13,  PD_7, PD_2,PF_8,PG_2,PF_4,PE_8,PI_0},    //enablePins including M5
    {PC_15, PE_3, PB_8, PG_12,  PD_6, PD_1,PF_3,PD_14,PE_12,PG_0,PH_12},    //stepPins Including M5
    {PF_0, PE_2, PB_7, PG_11,  PD_5, PD_0,PG_3,PD_11,PE_10,PG_1,PH_15}, //dirPins Including M5
#if HAS_SMART_DRIVERS
    {PC_14, PE_1, PB_5, PG_10, PD_4, PC_12,PG_4,PE_15,PE_7,PF_15,PH_14},  //uartpins including M5
    6,                                      // Smart drivers or 11 with M5
#endif
    0,                                       //digiPot Factor
#if HAS_VOLTAGE_MONITOR
    NoPin,
#endif
    NoPin,
#if HAS_SBC_INTERFACE
    PB_10, PB_12, SSP2,
#endif
};

constexpr PinEntry PinTable_BTT_RRF_E3_v1_1[] =
{
    //Thermistors
    {PA_0, "e0temp,th0"},
    {PA_1, "bedtemp,tb"},
    {PA_2, "e1temp,th1"},
    {PA_3, "PT100,th2"},

    //Endstops
    {PC_0, "xstop,x-stop"},
    {PC_1, "ystop,y-stop"},
    {PC_2, "zstop,z-stop"},
    {PC_3, "e0stop,e0det"},
    {PB_10, "e1stop"},
    {PB_11, "x2stop"},
    {PE_3, "IO"},

    //Servos
    {PB_0,  "servo0" },

    //Probe
    {PC_5, "probe"},

    //Heaters and Fans (Big and Small Mosfets}
    {PB_4,  "bed,hbed" },
    {PB_3,  "e0heat,heat0" },
    {PE_4,  "e1heat" },
    {PB_5,  "fan0,fan" },
    {PB_6,  "fan1" },
    {PE_5,  "fan2" },
    {PE_6,  "fan3" },

    //EXP1
    {PB_1, "LCD_D6"},
    {PB_2, "LCD_D4"},
    {PE_7, "LCD_EN"},
    {PE_8, "BEEP"},
    {PE_9,"BTN_ENC"},
    {PE_10, "LCD_D5"},
    {PE_11,"LCD_D7"},

    //Neopixel
    {PB_7, "Neopixel"},

    //TFT
    {PA_9, "TX1,tft-tx"},
    {PA_10, "RX1,tft-rx"},

    //UART
    {PB_8, "SDA1"},
    {PB_9, "SCL1"},

    //PSON
    {PE_1, "PSON"},

    //PWRDET
    {PE_0, "PWRDET"},

    //Status LED
    {PE_2, "LED"},

};

constexpr BoardDefaults btt_rrf_e3_1_1_Defaults = {
    {0x94a2cc03, 0xb173b733},                   // Signatures
    SD_SDIO,                                    // SD Card access
    {   //CLK, MISO, MOSI
        {NoPin, NoPin, NoPin},                     //SPI0
        {PB_13, PB_14, PB_15},                  //SPI1
        {NoPin, NoPin, NoPin},                  //SPI2
        {NoPin, NoPin, NoPin},                  //SPI3
        {NoPin, NoPin, NoPin},                  //SPI4
        {NoPin, NoPin, NoPin},                  //SPI5
    },
    6,                                          // Number of drivers
    {PD_7, PD_3, PD_14, PD_10, PC_13, PE_15},   // enablePins
    {PD_5, PD_0, PC_6, PD_12, PC_15, PE_13},    // stepPins
    {PD_4, PA_15, PC_7, PD_13, PA_8, PE_12},    // dirPins
#if TMC_SOFT_UART
    {PD_6, PD_1, PD_15, PD_11, PC_14, PE_14},   // uartpins
    4,                                          // Smart drivers
#endif
    0,                                          // digiPot Factor
#if HAS_VOLTAGE_MONITOR
    NoPin,
#endif
    NoPin,
#if HAS_SBC_INTERFACE
    NoPin, NoPin, SSPNONE,
#endif
};

constexpr PinEntry PinTable_BTT_SKR_2[] =
{
    //Thermistors
    {PA_1, "bedtemp,tb"},
    {PA_2, "e0temp,th0"},
    {PA_3, "e1temp,th1"},

    //Endstops
    {PC_1, "xstop,x-stop"},
    {PC_3, "ystop,y-stop"},
    {PC_0, "zstop,z-stop"},
    {PC_2, "e0stop,e0det"},
    {PA_0, "e1stop,e1det"},

    //Servos
    {PE_5,  "servo0" },

    //Probe
    {PE_4, "probe"},

    //Heaters and Fans (Big and Small Mosfets}
    {PD_7,  "bed,hbed" },
    {PB_3,  "e0heat,heat0" },
    {PB_4,  "e1heat,heat1" },
    {PB_7,  "fan0,fan" },
    {PB_6,  "fan1" },
    {PB_5,  "fan2" },

    //EXP1
    {PE_13, "LCD_D7"},
    {PE_11, "LCD_D5"},
    {PE_9, "LCD_RS"},
    {PC_5, "BEEP"},
    {PB_0, "BTN_ENC"},
    {PE_10, "LCD_D4"},
    {PE_12, "LCD_D6"},
    {PB_1, "LCD_EN"},

    //EXP2
    {PC_4, "LCD_CD"},
    {PB_2, "BTN_EN2"},
    {PE_7, "BTN_EN1"},
    {PA_6, "LCD_MISO"},
    {PA_5, "LCD_SCK"},
    {PA_4, "LCD_SS"},
    {PA_7, "LCD_MOSI"},

    //Neopixel
    {PE_6, "Neopixel"},

    //TFT
    {PA_9, "TX1,tft-tx"},
    {PA_10, "RX1,tft-rx"},

    //SPI
    {PE_0, "X-CS"},
    {PD_3, "Y-CS"},
    {PD_0, "Z-CS"},
    {PC_6, "E0-CS"},
    {PD_12, "E1-CS"},
    {PA_14, "MISO"},
    {PE_14, "MOSI"},
    {PE_15, "SCK"},

    //I2C
    {PB_8, "SCL1"},
    {PB_9, "SDA1"},

    //PSON
    {PE_8, "PSON"},

    //PWRDET
    {PC_15, "PWRDET"},

    //Status LED
    {PA_13, "LED"},

    //Safe power
    {PC_13, "SP"},
};

constexpr BoardDefaults btt_skr_2_Defaults = {
    {0xb75b00a7, 0x35f4602c},               // Signatures
    SD_SDIO,                                // SD Card access
    {   //CLK, MISO, MOSI
        {PA_5, PA_6, PA_7},                 //SPI0
        {PB_13, PB_14, PB_15},              //SPI1
        {NoPin, NoPin, NoPin},              //SPI2
        {NoPin, NoPin, NoPin},              //SPI3
        {NoPin, NoPin, NoPin},              //SPI4
        {NoPin, NoPin, NoPin},              //SPI5
    },
    5,                                      // Number of drivers
    {PE_3, PD_6, PD_1, PC_7, PD_13},        // enablePins
    {PE_2, PD_5, PA_15, PD_15, PD_11},      // stepPins
    {PE_1, PD_4, PA_8, PD_14, PD_10},       // dirPins
#if TMC_SOFT_UART
    {PE_0, PD_3, PD_0, PC_6, PD_12},        // uartpins
    5,                                      // Smart drivers
#endif
    0,                                      // digiPot Factor
#if HAS_VOLTAGE_MONITOR
    NoPin,
#endif
    PC_13,
#if HAS_SBC_INTERFACE
    PB_11, PB_12, SSP2,
#endif
};

constexpr PinEntry PinTable_BTT_OCTOPUS[] =
{
    //Thermistors
    {PF_3, "bedtemp,tb"},
    {PF_4, "e0temp,th0"},
    {PF_5, "e1temp,th1"},
    {PF_6, "e2temp,th2"},
    {PF_7, "e3temp,th3"},

    //Endstops
    {PG_6, "xstop,x-stop"},
    {PG_9, "ystop,y-stop"},
    {PG_10, "zstop,z-stop"},
    {PG_11, "e0stop,e0det"},
    {PG_12, "e1stop,e1det"},
    {PG_13, "e2stop,e2det"},
    {PG_14, "e3stop,e3det"},
    {PG_15, "e4stop,e4det"},

    //Servos
    {PB_6,  "servo0" },

    //Probe
    {PB_7, "probe"},

    //Heaters and Fans (Big and Small Mosfets}
    {PA_1,  "bed,hbed" },
    {PA_2,  "e0heat,heat0" },
    {PA_3,  "e1heat,heat1" },
    {PB_10, "e2heat,heat2" },
    {PB_11, "e3heat,heat3" },
    {PA_8,  "fan0,fan" },
    {PE_5,  "fan1" },
    {PD_12, "fan2" },
    {PD_13, "fan3" },
    {PD_14, "fan4" },
    {PD_15, "fan5" },

    //EXP1
    {PE_15, "LCD_D7"},
    {PE_13, "LCD_D5"},
    {PE_10, "LCD_RS"},
    {PE_7, "BTN_ENC"},
    {PE_8, "BEEP"},
    {PE_9, "LCD_EN"},
    {PE_12, "LCD_D4"},
    {PE_14, "LCD_D6"},

    //EXP2
    {PC_15, "LCD_CD"},
    {PB_2, "BTN_EN2"},
    {PB_1, "BTN_EN1"},
    {PA_6, "LCD_MISO"},
    {PA_5, "LCD_SCK"},
    {PA_4, "LCD_SS"},
    {PA_7, "LCD_MOSI"},

    //Neopixel
    {PB_0, "Neopixel"},

    //TFT
    {PA_9, "TX1,tft-tx"},
    {PA_10, "RX1,tft-rx"},

    //SPI
    {PC_4, "X-CS"},
    {PD_11, "Y-CS"},
    {PC_6, "Z-CS"},
    {PC_7, "E0-CS"},
    {PF_2, "E1-CS"},
    {PE_4, "E2-CS"},
    {PE_1, "E3-CS"},
    {PD_3, "E4-CS"},
    {PA_6, "MISO"},
    {PA_7, "MOSI"},
    {PA_5, "SCK"},

    //I2C
    {PB_8, "SCL1"},
    {PB_9, "SDA1"},

    //PSON
    {PE_11, "PSON"},

    //PWRDET
    {PC_0, "PWRDET"},

    //Status LED
    {PA_13, "LED"},
};

constexpr BoardDefaults btt_octopus_Defaults = {
    {0x5e29d842},                               // Signatures
    SD_SDIO,                                    // SD Card access
    {   //CLK, MISO, MOSI
        {PA_5, PA_6, PA_7},                     //SPI0
        {PB_13, PC_2, PC_3},                    //SPI1
        {PB_3,  PB_4,  PB_5},                   //SPI2
        {NoPin, NoPin, NoPin},                  //SPI3
        {NoPin, NoPin, NoPin},                  //SPI4
        {NoPin, NoPin, NoPin},                  //SPI5
    },
    8,                                          // Number of drivers
    {PF_14, PF_15, PG_5, PA_0, PG_2, PF_1, PD_4, PE_0}, // enablePins
    {PF_13, PG_0, PF_11, PG_4, PF_9, PC_13, PE_2, PE_6},// stepPins
    {PF_12, PG_1, PG_3, PC_1, PF_10, PF_0, PE_3,
#if STARTUP_DELAY
    NoPin
#else
    PA_14
#endif 
    },	// dirPins
#if TMC_SOFT_UART
    {PC_4, PD_11, PC_6, PC_7, PF_2, PE_4, PE_1, PD_3},  // uartpins
    8,                                          // Smart drivers
#endif
    0,                                          // digiPot Factor
#if HAS_VOLTAGE_MONITOR
    NoPin,
#endif
    NoPin,
#if HAS_SBC_INTERFACE
    PD_10, PB_12, SSP2,
#endif
};
constexpr PinEntry PinTable_BTT_OCTOPUSPRO[] =
{
    //Thermistors
    {PF_3, "bedtemp,tb"},
    {PF_4, "e0temp,th0"},
    {PF_5, "e1temp,th1"},
    {PF_6, "e2temp,th2"},
    {PF_7, "e3temp,th3"},

    //Endstops
    {PG_6, "xstop,x-stop"},
    {PG_9, "ystop,y-stop"},
    {PG_10, "zstop,z-stop"},
    {PG_11, "e0stop,e0det"},
    {PG_12, "e1stop,e1det"},
    {PG_13, "e2stop,e2det"},
    {PG_14, "e3stop,e3det"},
    {PG_15, "e4stop,e4det"},

    //Servos
    {PB_6,  "servo0" },

    //Probe
    {PB_7, "probe"},
    {PC_5, "probe1"},

    //Heaters and Fans (Big and Small Mosfets}
    {PA_1,  "bed,hbed" },
    {PA_2,  "e0heat,heat0" },
    {PA_3,  "e1heat,heat1" },
    {PB_10, "e2heat,heat2" },
    {PB_11, "e3heat,heat3" },
    {PA_8,  "fan0,fan" },
    {PE_5,  "fan1" },
    {PD_12, "fan2" },
    {PD_13, "fan3" },
    {PD_14, "fan4" },
    {PD_15, "fan5" },

    //EXP1
    {PE_15, "LCD_D7"},
    {PE_13, "LCD_D5"},
    {PE_10, "LCD_RS"},
    {PE_7, "BTN_ENC"},
    {PE_8, "BEEP"},
    {PE_9, "LCD_EN"},
    {PE_12, "LCD_D4"},
    {PE_14, "LCD_D6"},

    //EXP2
    {PC_15, "LCD_CD"},
    {PB_2, "BTN_EN2"},
    {PB_1, "BTN_EN1"},
    {PA_6, "LCD_MISO"},
    {PA_5, "LCD_SCK"},
    {PA_4, "LCD_SS"},
    {PA_7, "LCD_MOSI"},

    //Neopixel
    {PB_0, "Neopixel"},

    //TFT
    {PA_9, "TX1,tft-tx"},
    {PA_10, "RX1,tft-rx"},

    //SPI
    {PC_4, "X-CS"},
    {PD_11, "Y-CS"},
    {PC_6, "Z-CS"},
    {PC_7, "E0-CS"},
    {PF_2, "E1-CS"},
    {PE_4, "E2-CS"},
    {PE_1, "E3-CS"},
    {PD_3, "E4-CS"},
    {PA_6, "MISO"},
    {PA_7, "MOSI"},
    {PA_5, "SCK"},

    //I2C
    {PB_8, "SCL1"},
    {PB_9, "SDA1"},

    //PSON
    {PE_11, "PSON"},

    //PWRDET
    {PC_0, "PWRDET"},

    //Status LED
    {PA_13, "LED"},
};

constexpr BoardDefaults btt_octopuspro_Defaults = {
    {0x5e29d842},                               // Signatures
    SD_SDIO,                                    // SD Card access
    {   //CLK, MISO, MOSI
        {PA_5, PA_6, PA_7},                     //SPI0
        {PB_13, PC_2, PC_3},                    //SPI1
        {PB_3,  PB_4,  PB_5},                   //SPI2
        {NoPin, NoPin, NoPin},                  //SPI3
        {NoPin, NoPin, NoPin},                  //SPI4
        {NoPin, NoPin, NoPin},                  //SPI5
    },
    8,                                          // Number of drivers
    {PF_14, PF_15, PG_5, PA_0, PG_2, PF_1, PD_4, PE_0}, // enablePins
    {PF_13, PG_0, PF_11, PG_4, PF_9, PC_13, PE_2, PE_6},// stepPins
    {PF_12, PG_1, PG_3, PC_1, PF_10, PF_0, PE_3,
#if STARTUP_DELAY
    NoPin
#else
    PA_14
#endif 
    },  // dirPins
#if TMC_SOFT_UART
    {PC_4, PD_11, PC_6, PC_7, PF_2, PE_4, PE_1, PD_3},  // uartpins
    8,                                          // Smart drivers
#endif
    0,                                          // digiPot Factor
#if HAS_VOLTAGE_MONITOR
    NoPin,
#endif
    NoPin,
#if HAS_SBC_INTERFACE
    PD_10, PB_12, SSP2,
#endif
};
#endif
#endif