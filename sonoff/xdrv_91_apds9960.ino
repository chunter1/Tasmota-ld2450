/*
 Copyright (c) 2017 Shawn Hymel/Sparkfun and Theo Arends.  All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 - Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
 - Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

//#define USE_APDS9960    // uncomment to enable the sensor
// !!!!!!  turn off conflicting drivers !!!!
#if defined(USE_SHT) || defined(USE_VEML6070)
 #warning I will turn off conflicting drivers (SHT and VEML6070) !!!
 #undef USE_SHT          // SHT-Driver blocks gesture sensor
 #undef USE_VEML6070     // address conflict on the I2C-bus
#endif

#ifdef USE_I2C
#ifdef USE_APDS9960

enum GestureCommands {
  CMND_GESTURE };

const char kGestureCommands[] PROGMEM =
  "Gesture" ;

/*********************************************************************************************\
 * APDS9960
 *
 * Source: Shawn Hymel (SparkFun Electronics)
 * Adaption for TASMOTA: Christian Baars
\*********************************************************************************************/

#define APDS9960_I2C_ADDR         0x39

#define APDS9960_CHIPID_1         0xAB
#define APDS9960_CHIPID_2         0x9C

/* Gesture parameters */
#define GESTURE_THRESHOLD_OUT   10
#define GESTURE_SENSITIVITY_1   50
#define GESTURE_SENSITIVITY_2   20



uint8_t APDS9960addr;
uint8_t APDS9960type = 0;
char APDS9960stype[7];
char currentGesture[6];
bool gesture_mode = true;


volatile uint8_t recovery_loop_counter = 0;  //count number of stateloops to switch the sensor off, if needed
#define APDS9960_LONG_RECOVERY           50 //long pause after sensor overload in loops
#define APDS9960_MAX_GESTURE_CYCLES      50  //how many FIFO-reads are allowed to prevent crash
bool APDS9960_overload = false;

#ifdef USE_WEBSERVER
const char HTTP_APDS_9960_SNS[] PROGMEM = "%s"
  "{s}" "Red" "{m}%s{e}"
  "{s}" "Green" "{m}%s{e}"
  "{s}" "Blue" "{m}%s{e}"
  "{s}" "Ambient" "{m}%s " D_UNIT_LUX "{e}"
  "{s}" "Proximity"  "{m}%s{e}";      // {s} = <tr><th>, {m} = </th><td>, {e} = </td></tr>
#endif  // USE_WEBSERVER


/*********************************************************************************************\
 * APDS9960
 *
 * Programmer : APDS9960 Datasheet and Sparkfun
\*********************************************************************************************/


/* Misc parameters */
#define FIFO_PAUSE_TIME         30      // Wait period (ms) between FIFO reads

/* APDS-9960 register addresses */
#define APDS9960_ENABLE         0x80
#define APDS9960_ATIME          0x81
#define APDS9960_WTIME          0x83
#define APDS9960_AILTL          0x84
#define APDS9960_AILTH          0x85
#define APDS9960_AIHTL          0x86
#define APDS9960_AIHTH          0x87
#define APDS9960_PILT           0x89
#define APDS9960_PIHT           0x8B
#define APDS9960_PERS           0x8C
#define APDS9960_CONFIG1        0x8D
#define APDS9960_PPULSE         0x8E
#define APDS9960_CONTROL        0x8F
#define APDS9960_CONFIG2        0x90
#define APDS9960_ID             0x92
#define APDS9960_STATUS         0x93
#define APDS9960_CDATAL         0x94
#define APDS9960_CDATAH         0x95
#define APDS9960_RDATAL         0x96
#define APDS9960_RDATAH         0x97
#define APDS9960_GDATAL         0x98
#define APDS9960_GDATAH         0x99
#define APDS9960_BDATAL         0x9A
#define APDS9960_BDATAH         0x9B
#define APDS9960_PDATA          0x9C
#define APDS9960_POFFSET_UR     0x9D
#define APDS9960_POFFSET_DL     0x9E
#define APDS9960_CONFIG3        0x9F
#define APDS9960_GPENTH         0xA0
#define APDS9960_GEXTH          0xA1
#define APDS9960_GCONF1         0xA2
#define APDS9960_GCONF2         0xA3
#define APDS9960_GOFFSET_U      0xA4
#define APDS9960_GOFFSET_D      0xA5
#define APDS9960_GOFFSET_L      0xA7
#define APDS9960_GOFFSET_R      0xA9
#define APDS9960_GPULSE         0xA6
#define APDS9960_GCONF3         0xAA
#define APDS9960_GCONF4         0xAB
#define APDS9960_GFLVL          0xAE
#define APDS9960_GSTATUS        0xAF
#define APDS9960_IFORCE         0xE4
#define APDS9960_PICLEAR        0xE5
#define APDS9960_CICLEAR        0xE6
#define APDS9960_AICLEAR        0xE7
#define APDS9960_GFIFO_U        0xFC
#define APDS9960_GFIFO_D        0xFD
#define APDS9960_GFIFO_L        0xFE
#define APDS9960_GFIFO_R        0xFF

/* Bit fields */
#define APDS9960_PON            0b00000001
#define APDS9960_AEN            0b00000010
#define APDS9960_PEN            0b00000100
#define APDS9960_WEN            0b00001000
#define APSD9960_AIEN           0b00010000
#define APDS9960_PIEN           0b00100000
#define APDS9960_GEN            0b01000000
#define APDS9960_GVALID         0b00000001

/* On/Off definitions */
#define OFF                     0
#define ON                      1

/* Acceptable parameters for setMode */
#define POWER                   0
#define AMBIENT_LIGHT           1
#define PROXIMITY               2
#define WAIT                    3
#define AMBIENT_LIGHT_INT       4
#define PROXIMITY_INT           5
#define GESTURE                 6
#define ALL                     7

/* LED Drive values */
#define LED_DRIVE_100MA         0
#define LED_DRIVE_50MA          1
#define LED_DRIVE_25MA          2
#define LED_DRIVE_12_5MA        3

/* Proximity Gain (PGAIN) values */
#define PGAIN_1X                0
#define PGAIN_2X                1
#define PGAIN_4X                2
#define PGAIN_8X                3

/* ALS Gain (AGAIN) values */
#define AGAIN_1X                0
#define AGAIN_4X                1
#define AGAIN_16X               2
#define AGAIN_64X               3

/* Gesture Gain (GGAIN) values */
#define GGAIN_1X                0
#define GGAIN_2X                1
#define GGAIN_4X                2
#define GGAIN_8X                3

/* LED Boost values */
#define LED_BOOST_100           0
#define LED_BOOST_150           1
#define LED_BOOST_200           2
#define LED_BOOST_300           3

/* Gesture wait time values */
#define GWTIME_0MS              0
#define GWTIME_2_8MS            1
#define GWTIME_5_6MS            2
#define GWTIME_8_4MS            3
#define GWTIME_14_0MS           4
#define GWTIME_22_4MS           5
#define GWTIME_30_8MS           6
#define GWTIME_39_2MS           7

/* Default values */
#define DEFAULT_ATIME           219     // 103ms
#define DEFAULT_WTIME           246     // 27ms
#define DEFAULT_PROX_PPULSE     0x87    // 16us, 8 pulses
#define DEFAULT_GESTURE_PPULSE  0x89    // 16us, 10 pulses
#define DEFAULT_POFFSET_UR      0       // 0 offset
#define DEFAULT_POFFSET_DL      0       // 0 offset
#define DEFAULT_CONFIG1         0x60    // No 12x wait (WTIME) factor
#define DEFAULT_LDRIVE          LED_DRIVE_100MA
#define DEFAULT_PGAIN           PGAIN_4X
#define DEFAULT_AGAIN           AGAIN_4X
#define DEFAULT_PILT            0       // Low proximity threshold
#define DEFAULT_PIHT            50      // High proximity threshold
#define DEFAULT_AILT            0xFFFF  // Force interrupt for calibration
#define DEFAULT_AIHT            0
#define DEFAULT_PERS            0x11    // 2 consecutive prox or ALS for int.
#define DEFAULT_CONFIG2         0x01    // No saturation interrupts or LED boost
#define DEFAULT_CONFIG3         0       // Enable all photodiodes, no SAI
#define DEFAULT_GPENTH          40      // Threshold for entering gesture mode
#define DEFAULT_GEXTH           30      // Threshold for exiting gesture mode
#define DEFAULT_GCONF1          0x40    // 4 gesture events for int., 1 for exit
#define DEFAULT_GGAIN           GGAIN_4X
#define DEFAULT_GLDRIVE         LED_DRIVE_100MA
#define DEFAULT_GWTIME          GWTIME_2_8MS
#define DEFAULT_GOFFSET         0       // No offset scaling for gesture mode
#define DEFAULT_GPULSE          0xC9    // 32us, 10 pulses
#define DEFAULT_GCONF3          0       // All photodiodes active during gesture
#define DEFAULT_GIEN            0       // Disable gesture interrupts

#define ERROR                   0xFF

/* Direction definitions */
enum {
  DIR_NONE,
  DIR_LEFT,
  DIR_RIGHT,
  DIR_UP,
  DIR_DOWN,
  DIR_ALL
};

/* State definitions*/
enum {
  APDS9960_NA_STATE,
  APDS9960_ALL_STATE
};


/* Container for gesture data */
typedef struct gesture_data_type {
    uint8_t u_data[32];
    uint8_t d_data[32];
    uint8_t l_data[32];
    uint8_t r_data[32];
    uint8_t index;
    uint8_t total_gestures;
    uint8_t in_threshold;
    uint8_t out_threshold;
} gesture_data_type;

/*Members*/
 gesture_data_type gesture_data_;
 int16_t gesture_ud_delta_ = 0;
 int16_t gesture_lr_delta_ = 0;
 int16_t gesture_ud_count_ = 0;
 int16_t gesture_lr_count_ = 0;
 int16_t gesture_state_ = 0;
 int16_t gesture_motion_ = DIR_NONE;



 /*******************************************************************************
  * Helper functions
  ******************************************************************************/

 /**
  * @brief Writes a single byte to the I2C device (no register)
  *
  * @param[in] val the 1-byte value to write to the I2C device
  * @return True if successful write operation. False otherwise.
  */
 bool wireWriteByte(uint8_t val)
 {
     Wire.beginTransmission(APDS9960_I2C_ADDR);
     Wire.write(val);
     if( Wire.endTransmission() != 0 ) {
         return false;
     }

     return true;
 }
 /**
 * @brief Reads a block (array) of bytes from the I2C device and register
 *
 * @param[in] reg the register to read from
 * @param[out] val pointer to the beginning of the data
 * @param[in] len number of bytes to read
 * @return Number of bytes read. -1 on read error.
 */
int8_t wireReadDataBlock(   uint8_t reg,
                                        uint8_t *val,
                                        uint16_t len)
{
    unsigned char i = 0;

    /* Indicate which register we want to read from */
    if (!wireWriteByte(reg)) {
        return -1;
    }

    /* Read block data */
    Wire.requestFrom(APDS9960_I2C_ADDR, len);
    while (Wire.available()) {
        if (i >= len) {
            return -1;
        }
        val[i] = Wire.read();
        i++;
    }

    return i;
}

 /*******************************************************************************
  * Getters and setters for register values
  ******************************************************************************/

 /**
  * @brief Returns the lower threshold for proximity detection
  *
  * @return lower threshold
  */
 uint8_t getProxIntLowThresh()
 {
     uint8_t val;

     /* Read value from PILT register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_PILT) ;
     return val;
 }

 /**
  * @brief Sets the lower threshold for proximity detection
  *
  * @param[in] threshold the lower proximity threshold
  */
  void setProxIntLowThresh(uint8_t threshold)
  {
        I2cWrite8(APDS9960_I2C_ADDR, APDS9960_PILT, threshold);
  }

 /**
  * @brief Returns the high threshold for proximity detection
  *
  * @return high threshold
  */
 uint8_t getProxIntHighThresh()
 {
     uint8_t val;

     /* Read value from PIHT register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_PIHT) ;
     return val;
 }

 /**
  * @brief Sets the high threshold for proximity detection
  *
  * @param[in] threshold the high proximity threshold
  */

 void setProxIntHighThresh(uint8_t threshold)
   {
         I2cWrite8(APDS9960_I2C_ADDR, APDS9960_PIHT, threshold);
   }


 /**
  * @brief Returns LED drive strength for proximity and ALS
  *
  * Value    LED Current
  *   0        100 mA
  *   1         50 mA
  *   2         25 mA
  *   3         12.5 mA
  *
  * @return the value of the LED drive strength. 0xFF on failure.
  */
 uint8_t getLEDDrive()
 {
     uint8_t val;

     /* Read value from CONTROL register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_CONTROL) ;
     /* Shift and mask out LED drive bits */
     val = (val >> 6) & 0b00000011;

     return val;
 }

 /**
  * @brief Sets the LED drive strength for proximity and ALS
  *
  * Value    LED Current
  *   0        100 mA
  *   1         50 mA
  *   2         25 mA
  *   3         12.5 mA
  *
  * @param[in] drive the value (0-3) for the LED drive strength
  */
  void setLEDDrive(uint8_t drive)
  {
     uint8_t val;

      /* Read value from CONTROL register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_CONTROL);

      /* Set bits in register to given value */
      drive &= 0b00000011;
      drive = drive << 6;
      val &= 0b00111111;
      val |= drive;

      /* Write register value back into CONTROL register */
      I2cWrite8(APDS9960_I2C_ADDR, APDS9960_CONTROL, val);
  }


 /**
  * @brief Returns receiver gain for proximity detection
  *
  * Value    Gain
  *   0       1x
  *   1       2x
  *   2       4x
  *   3       8x
  *
  * @return the value of the proximity gain. 0xFF on failure.
  */
 uint8_t getProximityGain()
 {
     uint8_t val;

     /* Read value from CONTROL register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_CONTROL) ;
     /* Shift and mask out PDRIVE bits */
     val = (val >> 2) & 0b00000011;

     return val;
 }

 /**
  * @brief Sets the receiver gain for proximity detection
  *
  * Value    Gain
  *   0       1x
  *   1       2x
  *   2       4x
  *   3       8x
  *
  * @param[in] drive the value (0-3) for the gain
  */
  void setProximityGain(uint8_t drive)
 {
     uint8_t val;

     /* Read value from CONTROL register */
   val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_CONTROL);

     /* Set bits in register to given value */
     drive &= 0b00000011;
     drive = drive << 2;
     val &= 0b11110011;
     val |= drive;

     /* Write register value back into CONTROL register */
     I2cWrite8(APDS9960_I2C_ADDR, APDS9960_CONTROL, val);
 }


 /**
  * @brief Returns receiver gain for the ambient light sensor (ALS)
  *
  * Value    Gain
  *   0        1x
  *   1        4x
  *   2       16x
  *   3       64x
  *
  * @return the value of the ALS gain. 0xFF on failure.
  */

 /**
  * @brief Sets the receiver gain for the ambient light sensor (ALS)
  *
  * Value    Gain
  *   0        1x
  *   1        4x
  *   2       16x
  *   3       64x
  *
  * @param[in] drive the value (0-3) for the gain
  */
  void setAmbientLightGain(uint8_t drive)
  {
      uint8_t val;

      /* Read value from CONTROL register */
      val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_CONTROL);

      /* Set bits in register to given value */
      drive &= 0b00000011;
      val &= 0b11111100;
      val |= drive;

      /* Write register value back into CONTROL register */
      I2cWrite8(APDS9960_I2C_ADDR, APDS9960_CONTROL, val);
  }

 /**
  * @brief Get the current LED boost value
  *
  * Value  Boost Current
  *   0        100%
  *   1        150%
  *   2        200%
  *   3        300%
  *
  * @return The LED boost value. 0xFF on failure.
  */
 uint8_t getLEDBoost()
 {
     uint8_t val;

     /* Read value from CONFIG2 register */
   val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_CONFIG2) ;

     /* Shift and mask out LED_BOOST bits */
     val = (val >> 4) & 0b00000011;

     return val;
 }

 /**
  * @brief Sets the LED current boost value
  *
  * Value  Boost Current
  *   0        100%
  *   1        150%
  *   2        200%
  *   3        300%
  *
  * @param[in] drive the value (0-3) for current boost (100-300%)
  */
 void setLEDBoost(uint8_t boost)
 {
     uint8_t val;

     /* Read value from CONFIG2 register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_CONFIG2) ;
     /* Set bits in register to given value */
     boost &= 0b00000011;
     boost = boost << 4;
     val &= 0b11001111;
     val |= boost;

     /* Write register value back into CONFIG2 register */
    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_CONFIG2, val) ;
 }

 /**
  * @brief Gets proximity gain compensation enable
  *
  * @return 1 if compensation is enabled. 0 if not. 0xFF on error.
  */
 uint8_t getProxGainCompEnable()
 {
     uint8_t val;

     /* Read value from CONFIG3 register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_CONFIG3) ;

     /* Shift and mask out PCMP bits */
     val = (val >> 5) & 0b00000001;

     return val;
 }

 /**
  * @brief Sets the proximity gain compensation enable
  *
  * @param[in] enable 1 to enable compensation. 0 to disable compensation.
  */
  void setProxGainCompEnable(uint8_t enable)
 {
     uint8_t val;

     /* Read value from CONFIG3 register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_CONFIG3) ;

     /* Set bits in register to given value */
     enable &= 0b00000001;
     enable = enable << 5;
     val &= 0b11011111;
     val |= enable;

     /* Write register value back into CONFIG3 register */
     I2cWrite8(APDS9960_I2C_ADDR, APDS9960_CONFIG3, val) ;
 }

 /**
  * @brief Gets the current mask for enabled/disabled proximity photodiodes
  *
  * 1 = disabled, 0 = enabled
  * Bit    Photodiode
  *  3       UP
  *  2       DOWN
  *  1       LEFT
  *  0       RIGHT
  *
  * @return Current proximity mask for photodiodes. 0xFF on error.
  */
 uint8_t getProxPhotoMask()
 {
     uint8_t val;

     /* Read value from CONFIG3 register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_CONFIG3) ;

     /* Mask out photodiode enable mask bits */
     val &= 0b00001111;

     return val;
 }

 /**
  * @brief Sets the mask for enabling/disabling proximity photodiodes
  *
  * 1 = disabled, 0 = enabled
  * Bit    Photodiode
  *  3       UP
  *  2       DOWN
  *  1       LEFT
  *  0       RIGHT
  *
  * @param[in] mask 4-bit mask value
  */
 void setProxPhotoMask(uint8_t mask)
 {
     uint8_t val;

     /* Read value from CONFIG3 register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_CONFIG3) ;

     /* Set bits in register to given value */
     mask &= 0b00001111;
     val &= 0b11110000;
     val |= mask;

     /* Write register value back into CONFIG3 register */
     I2cWrite8(APDS9960_I2C_ADDR, APDS9960_CONFIG3, val) ;
 }

 /**
  * @brief Gets the entry proximity threshold for gesture sensing
  *
  * @return Current entry proximity threshold.
  */
 uint8_t getGestureEnterThresh()
 {
     uint8_t val;

     /* Read value from GPENTH register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_GPENTH) ;

     return val;
 }

 /**
  * @brief Sets the entry proximity threshold for gesture sensing
  *
  * @param[in] threshold proximity value needed to start gesture mode
  */
 void setGestureEnterThresh(uint8_t threshold)
 {
    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_GPENTH, threshold) ;

 }

 /**
  * @brief Gets the exit proximity threshold for gesture sensing
  *
  * @return Current exit proximity threshold.
  */
 uint8_t getGestureExitThresh()
 {
     uint8_t val;

     /* Read value from GEXTH register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_GEXTH) ;

     return val;
 }

 /**
  * @brief Sets the exit proximity threshold for gesture sensing
  *
  * @param[in] threshold proximity value needed to end gesture mode
  */
 void setGestureExitThresh(uint8_t threshold)
 {
     I2cWrite8(APDS9960_I2C_ADDR, APDS9960_GEXTH, threshold) ;
 }

 /**
  * @brief Gets the gain of the photodiode during gesture mode
  *
  * Value    Gain
  *   0       1x
  *   1       2x
  *   2       4x
  *   3       8x
  *
  * @return the current photodiode gain. 0xFF on error.
  */
 uint8_t getGestureGain()
 {
     uint8_t val;

     /* Read value from GCONF2 register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_GCONF2) ;

     /* Shift and mask out GGAIN bits */
     val = (val >> 5) & 0b00000011;

     return val;
 }

 /**
  * @brief Sets the gain of the photodiode during gesture mode
  *
  * Value    Gain
  *   0       1x
  *   1       2x
  *   2       4x
  *   3       8x
  *
  * @param[in] gain the value for the photodiode gain
  */
 void setGestureGain(uint8_t gain)
 {
     uint8_t val;

     /* Read value from GCONF2 register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_GCONF2) ;

     /* Set bits in register to given value */
     gain &= 0b00000011;
     gain = gain << 5;
     val &= 0b10011111;
     val |= gain;

     /* Write register value back into GCONF2 register */
     I2cWrite8(APDS9960_I2C_ADDR, APDS9960_GCONF2, val) ;
 }

 /**
  * @brief Gets the drive current of the LED during gesture mode
  *
  * Value    LED Current
  *   0        100 mA
  *   1         50 mA
  *   2         25 mA
  *   3         12.5 mA
  *
  * @return the LED drive current value. 0xFF on error.
  */
 uint8_t getGestureLEDDrive()
 {
     uint8_t val;

     /* Read value from GCONF2 register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_GCONF2) ;

     /* Shift and mask out GLDRIVE bits */
     val = (val >> 3) & 0b00000011;

     return val;
 }

 /**
  * @brief Sets the LED drive current during gesture mode
  *
  * Value    LED Current
  *   0        100 mA
  *   1         50 mA
  *   2         25 mA
  *   3         12.5 mA
  *
  * @param[in] drive the value for the LED drive current
  */
 void setGestureLEDDrive(uint8_t drive)
 {
     uint8_t val;

     /* Read value from GCONF2 register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_GCONF2) ;

     /* Set bits in register to given value */
     drive &= 0b00000011;
     drive = drive << 3;
     val &= 0b11100111;
     val |= drive;

     /* Write register value back into GCONF2 register */
     I2cWrite8(APDS9960_I2C_ADDR, APDS9960_GCONF2, val) ;
 }

 /**
  * @brief Gets the time in low power mode between gesture detections
  *
  * Value    Wait time
  *   0          0 ms
  *   1          2.8 ms
  *   2          5.6 ms
  *   3          8.4 ms
  *   4         14.0 ms
  *   5         22.4 ms
  *   6         30.8 ms
  *   7         39.2 ms
  *
  * @return the current wait time between gestures. 0xFF on error.
  */
 uint8_t getGestureWaitTime()
 {
     uint8_t val;

     /* Read value from GCONF2 register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_GCONF2) ;

     /* Mask out GWTIME bits */
     val &= 0b00000111;

     return val;
 }

 /**
  * @brief Sets the time in low power mode between gesture detections
  *
  * Value    Wait time
  *   0          0 ms
  *   1          2.8 ms
  *   2          5.6 ms
  *   3          8.4 ms
  *   4         14.0 ms
  *   5         22.4 ms
  *   6         30.8 ms
  *   7         39.2 ms
  *
  * @param[in] the value for the wait time
  */
 void setGestureWaitTime(uint8_t time)
 {
     uint8_t val;

     /* Read value from GCONF2 register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_GCONF2) ;

     /* Set bits in register to given value */
     time &= 0b00000111;
     val &= 0b11111000;
     val |= time;

     /* Write register value back into GCONF2 register */
    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_GCONF2, val) ;
 }

 /**
  * @brief Gets the low threshold for ambient light interrupts
  *
  * @param[out] threshold current low threshold stored on the APDS-9960
  */
 void getLightIntLowThreshold(uint16_t &threshold)
 {
     uint8_t val_byte;
     threshold = 0;

     /* Read value from ambient light low threshold, low byte register */
     val_byte = I2cRead8(APDS9960_I2C_ADDR, APDS9960_AILTL) ;
     threshold = val_byte;

     /* Read value from ambient light low threshold, high byte register */
     I2cWrite8(APDS9960_I2C_ADDR, APDS9960_AILTH, val_byte) ;
     threshold = threshold + ((uint16_t)val_byte << 8);
 }

 /**
  * @brief Sets the low threshold for ambient light interrupts
  *
  * @param[in] threshold low threshold value for interrupt to trigger
  */

   void setLightIntLowThreshold(uint16_t threshold)
   {
       uint8_t val_low;
       uint8_t val_high;

       /* Break 16-bit threshold into 2 8-bit values */
       val_low = threshold & 0x00FF;
       val_high = (threshold & 0xFF00) >> 8;

       /* Write low byte */
       I2cWrite8(APDS9960_I2C_ADDR, APDS9960_AILTL, val_low) ;

       /* Write high byte */
       I2cWrite8(APDS9960_I2C_ADDR, APDS9960_AILTH, val_high) ;

   }


 /**
  * @brief Gets the high threshold for ambient light interrupts
  *
  * @param[out] threshold current low threshold stored on the APDS-9960
  */
 void getLightIntHighThreshold(uint16_t &threshold)
 {
     uint8_t val_byte;
     threshold = 0;

     /* Read value from ambient light high threshold, low byte register */
     val_byte = I2cRead8(APDS9960_I2C_ADDR, APDS9960_AIHTL);
     threshold = val_byte;

     /* Read value from ambient light high threshold, high byte register */
     I2cWrite8(APDS9960_I2C_ADDR, APDS9960_AIHTH, val_byte) ;
     threshold = threshold + ((uint16_t)val_byte << 8);
 }

 /**
  * @brief Sets the high threshold for ambient light interrupts
  *
  * @param[in] threshold high threshold value for interrupt to trigger
  */
  void setLightIntHighThreshold(uint16_t threshold)
  {
      uint8_t val_low;
      uint8_t val_high;

      /* Break 16-bit threshold into 2 8-bit values */
      val_low = threshold & 0x00FF;
      val_high = (threshold & 0xFF00) >> 8;

      /* Write low byte */
      I2cWrite8(APDS9960_I2C_ADDR, APDS9960_AIHTL, val_low);

      /* Write high byte */
      I2cWrite8(APDS9960_I2C_ADDR, APDS9960_AIHTH, val_high) ;
  }


 /**
  * @brief Gets the low threshold for proximity interrupts
  *
  * @param[out] threshold current low threshold stored on the APDS-9960
  */
 void getProximityIntLowThreshold(uint8_t &threshold)
 {
     threshold = 0;

     /* Read value from proximity low threshold register */
     threshold = I2cRead8(APDS9960_I2C_ADDR, APDS9960_PILT);

 }

 /**
  * @brief Sets the low threshold for proximity interrupts
  *
  * @param[in] threshold low threshold value for interrupt to trigger
  */
 void setProximityIntLowThreshold(uint8_t threshold)
 {

     /* Write threshold value to register */
     I2cWrite8(APDS9960_I2C_ADDR, APDS9960_PILT, threshold) ;
 }



 /**
  * @brief Gets the high threshold for proximity interrupts
  *
  * @param[out] threshold current low threshold stored on the APDS-9960
  */
 void getProximityIntHighThreshold(uint8_t &threshold)
 {
     threshold = 0;

     /* Read value from proximity low threshold register */
     threshold = I2cRead8(APDS9960_I2C_ADDR, APDS9960_PIHT) ;

 }

 /**
  * @brief Sets the high threshold for proximity interrupts
  *
  * @param[in] threshold high threshold value for interrupt to trigger
  */
 void setProximityIntHighThreshold(uint8_t threshold)
 {

     /* Write threshold value to register */
   I2cWrite8(APDS9960_I2C_ADDR, APDS9960_PIHT, threshold) ;
 }

 /**
  * @brief Gets if ambient light interrupts are enabled or not
  *
  * @return 1 if interrupts are enabled, 0 if not. 0xFF on error.
  */
 uint8_t getAmbientLightIntEnable()
 {
     uint8_t val;

     /* Read value from ENABLE register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_ENABLE) ;

     /* Shift and mask out AIEN bit */
     val = (val >> 4) & 0b00000001;

     return val;
 }

 /**
  * @brief Turns ambient light interrupts on or off
  *
  * @param[in] enable 1 to enable interrupts, 0 to turn them off
  */
 void setAmbientLightIntEnable(uint8_t enable)
 {
     uint8_t val;

     /* Read value from ENABLE register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_ENABLE);

     /* Set bits in register to given value */
     enable &= 0b00000001;
     enable = enable << 4;
     val &= 0b11101111;
     val |= enable;

     /* Write register value back into ENABLE register */
     I2cWrite8(APDS9960_I2C_ADDR, APDS9960_ENABLE, val) ;
 }

 /**
  * @brief Gets if proximity interrupts are enabled or not
  *
  * @return 1 if interrupts are enabled, 0 if not. 0xFF on error.
  */
 uint8_t getProximityIntEnable()
 {
     uint8_t val;

     /* Read value from ENABLE register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_ENABLE) ;

     /* Shift and mask out PIEN bit */
     val = (val >> 5) & 0b00000001;

     return val;
 }

 /**
  * @brief Turns proximity interrupts on or off
  *
  * @param[in] enable 1 to enable interrupts, 0 to turn them off
  */
 void setProximityIntEnable(uint8_t enable)
 {
     uint8_t val;

     /* Read value from ENABLE register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_ENABLE) ;

     /* Set bits in register to given value */
     enable &= 0b00000001;
     enable = enable << 5;
     val &= 0b11011111;
     val |= enable;

     /* Write register value back into ENABLE register */
     I2cWrite8(APDS9960_I2C_ADDR, APDS9960_ENABLE, val) ;
 }

 /**
  * @brief Gets if gesture interrupts are enabled or not
  *
  * @return 1 if interrupts are enabled, 0 if not. 0xFF on error.
  */
 uint8_t getGestureIntEnable()
 {
     uint8_t val;

     /* Read value from GCONF4 register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_GCONF4) ;

     /* Shift and mask out GIEN bit */
     val = (val >> 1) & 0b00000001;

     return val;
 }

 /**
  * @brief Turns gesture-related interrupts on or off
  *
  * @param[in] enable 1 to enable interrupts, 0 to turn them off
  */
 void setGestureIntEnable(uint8_t enable)
 {
     uint8_t val;

     /* Read value from GCONF4 register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_GCONF4) ;

     /* Set bits in register to given value */
     enable &= 0b00000001;
     enable = enable << 1;
     val &= 0b11111101;
     val |= enable;

     /* Write register value back into GCONF4 register */
     I2cWrite8(APDS9960_I2C_ADDR, APDS9960_GCONF4, val) ;
 }

 /**
  * @brief Clears the ambient light interrupt
  *
  */
 void clearAmbientLightInt()
 {
     uint8_t throwaway;
     throwaway = I2cRead8(APDS9960_I2C_ADDR, APDS9960_AICLEAR);
 }

 /**
  * @brief Clears the proximity interrupt
  *
  */
 void clearProximityInt()
 {
     uint8_t throwaway;
     throwaway = I2cRead8(APDS9960_I2C_ADDR, APDS9960_PICLEAR) ;

 }

 /**
  * @brief Tells if the gesture state machine is currently running
  *
  * @return 1 if gesture state machine is running, 0 if not. 0xFF on error.
  */
 uint8_t getGestureMode()
 {
     uint8_t val;

     /* Read value from GCONF4 register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_GCONF4) ;

     /* Mask out GMODE bit */
     val &= 0b00000001;

     return val;
 }

 /**
  * @brief Tells the state machine to either enter or exit gesture state machine
  *
  * @param[in] mode 1 to enter gesture state machine, 0 to exit.
  */
 void setGestureMode(uint8_t mode)
 {
     uint8_t val;

     /* Read value from GCONF4 register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_GCONF4) ;

     /* Set bits in register to given value */
     mode &= 0b00000001;
     val &= 0b11111110;
     val |= mode;

     /* Write register value back into GCONF4 register */
     I2cWrite8(APDS9960_I2C_ADDR, APDS9960_GCONF4, val) ;
 }


bool APDS9960_init()
{
    /* Set default values for ambient light and proximity registers */

    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_ATIME, DEFAULT_ATIME)  ;

    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_WTIME, DEFAULT_WTIME) ;

    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_PPULSE, DEFAULT_PROX_PPULSE)  ;

    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_POFFSET_UR, DEFAULT_POFFSET_UR)  ;

    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_POFFSET_DL, DEFAULT_POFFSET_DL)  ;

    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_CONFIG1, DEFAULT_CONFIG1)  ;

    setLEDDrive(DEFAULT_LDRIVE);

    setProximityGain(DEFAULT_PGAIN);

    setAmbientLightGain(DEFAULT_AGAIN);

    setProxIntLowThresh(DEFAULT_PILT) ;

    setProxIntHighThresh(DEFAULT_PIHT);

    setLightIntLowThreshold(DEFAULT_AILT) ;

    setLightIntHighThreshold(DEFAULT_AIHT) ;

    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_PERS, DEFAULT_PERS)  ;

    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_CONFIG2, DEFAULT_CONFIG2)  ;

    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_CONFIG3, DEFAULT_CONFIG3)  ;

    /* Set default values for gesture sense registers */
    setGestureEnterThresh(DEFAULT_GPENTH);

    setGestureExitThresh(DEFAULT_GEXTH) ;

    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_GCONF1, DEFAULT_GCONF1)  ;

    setGestureGain(DEFAULT_GGAIN) ;

    setGestureLEDDrive(DEFAULT_GLDRIVE) ;

    setGestureWaitTime(DEFAULT_GWTIME) ;

    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_GOFFSET_U, DEFAULT_GOFFSET)  ;

    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_GOFFSET_D, DEFAULT_GOFFSET)  ;

    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_GOFFSET_L, DEFAULT_GOFFSET)  ;

    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_GOFFSET_R, DEFAULT_GOFFSET)  ;

    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_GPULSE, DEFAULT_GPULSE)  ;

    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_GCONF3, DEFAULT_GCONF3)  ;

    setGestureIntEnable(DEFAULT_GIEN);

    return true;
}
/*******************************************************************************
 * Public methods for controlling the APDS-9960
 ******************************************************************************/

/**
 * @brief Reads and returns the contents of the ENABLE register
 *
 * @return Contents of the ENABLE register. 0xFF if error.
 */
uint8_t getMode()
{
    uint8_t enable_value;

    /* Read current ENABLE register */
     enable_value = I2cRead8(APDS9960_I2C_ADDR, APDS9960_ENABLE) ;

    return enable_value;
}

/**
 * @brief Enables or disables a feature in the APDS-9960
 *
 * @param[in] mode which feature to enable
 * @param[in] enable ON (1) or OFF (0)
 */
void setMode(uint8_t mode, uint8_t enable)
{
    uint8_t reg_val;

    /* Read current ENABLE register */
    reg_val = getMode();


    /* Change bit(s) in ENABLE register */
    enable = enable & 0x01;
    if( mode >= 0 && mode <= 6 ) {
        if (enable) {
            reg_val |= (1 << mode);
        } else {
            reg_val &= ~(1 << mode);
        }
    } else if( mode == ALL ) {
        if (enable) {
            reg_val = 0x7F;
        } else {
            reg_val = 0x00;
        }
    }

    /* Write value back to ENABLE register */
      I2cWrite8(APDS9960_I2C_ADDR, APDS9960_ENABLE, reg_val) ;
}

/**
 * @brief Starts the light (R/G/B/Ambient) sensor on the APDS-9960
 *
 * @param[in] interrupts true to enable hardware interrupt on high or low light
 */
void enableLightSensor(bool interrupts)
{

    /* Set default gain, interrupts, enable power, and enable sensor */
    setAmbientLightGain(DEFAULT_AGAIN);
    if( interrupts ) {
        setAmbientLightIntEnable(1) ;

    }
    else {
        setAmbientLightIntEnable(0);
    }
    enablePower() ;
    setMode(AMBIENT_LIGHT, 1) ;
}

/**
 * @brief Ends the light sensor on the APDS-9960
 *
 */
void disableLightSensor()
{
    setAmbientLightIntEnable(0) ;
    setMode(AMBIENT_LIGHT, 0) ;
}

/**
 * @brief Starts the proximity sensor on the APDS-9960
 *
 * @param[in] interrupts true to enable hardware external interrupt on proximity
 */
void enableProximitySensor(bool interrupts)
{
    /* Set default gain, LED, interrupts, enable power, and enable sensor */
    setProximityGain(DEFAULT_PGAIN);
    setLEDDrive(DEFAULT_LDRIVE) ;
    if( interrupts ) {
        setProximityIntEnable(1) ;
    } else {
        setProximityIntEnable(0) ;
    }
    enablePower();
    setMode(PROXIMITY, 1) ;
}

/**
 * @brief Ends the proximity sensor on the APDS-9960
 *
 */
void disableProximitySensor()
{
	setProximityIntEnable(0) ;
  setMode(PROXIMITY, 0) ;
}

/**
 * @brief Starts the gesture recognition engine on the APDS-9960
 *
 * @param[in] interrupts true to enable hardware external interrupt on gesture
 */
void enableGestureSensor(bool interrupts)
{

    /* Enable gesture mode
       Set ENABLE to 0 (power off)
       Set WTIME to 0xFF
       Set AUX to LED_BOOST_300
       Enable PON, WEN, PEN, GEN in ENABLE
    */
    resetGestureParameters();
    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_WTIME, 0xFF) ;
    I2cWrite8(APDS9960_I2C_ADDR, APDS9960_PPULSE, DEFAULT_GESTURE_PPULSE) ;
    setLEDBoost(LED_BOOST_100);  // tip from jonn26 - 100 for 300
    if( interrupts ) {
        setGestureIntEnable(1) ;
    } else {
        setGestureIntEnable(0) ;
    }
    setGestureMode(1);
    enablePower() ;
    setMode(WAIT, 1) ;
    setMode(PROXIMITY, 1) ;
    setMode(GESTURE, 1);
}

/**
 * @brief Ends the gesture recognition engine on the APDS-9960
 *
 */
void disableGestureSensor()
{
    resetGestureParameters();
    setGestureIntEnable(0) ;
    setGestureMode(0) ;
    setMode(GESTURE, 0) ;
}

/**
 * @brief Determines if there is a gesture available for reading
 *
 * @return True if gesture available. False otherwise.
 */
bool isGestureAvailable()
{
    uint8_t val;

    /* Read value from GSTATUS register */
     val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_GSTATUS) ;

    /* Shift and mask out GVALID bit */
    val &= APDS9960_GVALID;

    /* Return true/false based on GVALID bit */
    if( val == 1) {
        return true;
    } else {
        return false;
    }
}

/**
 * @brief Processes a gesture event and returns best guessed gesture
 *
 * @return Number corresponding to gesture. -1 on error.
 */
int16_t readGesture()
{
    uint8_t fifo_level = 0;
    uint8_t bytes_read = 0;
    uint8_t fifo_data[128];
    uint8_t gstatus;
    uint16_t motion;
    uint16_t i;
    uint8_t gesture_loop_counter = 0; // don't loop forever later

    /* Make sure that power and gesture is on and data is valid */
    if( !isGestureAvailable() || !(getMode() & 0b01000001) ) {
        return DIR_NONE;
    }

    /* Keep looping as long as gesture data is valid */
    while(1) {
        if (gesture_loop_counter == APDS9960_MAX_GESTURE_CYCLES){ // We will escape after a few loops
          disableGestureSensor();   // stop the sensor to prevent problems with power consumption/blocking  and return to the main loop
          APDS9960_overload = true; // we report this as "long"-gesture
          char log[LOGSZ];
          snprintf_P(log, sizeof(log), PSTR("Sensor overload"));
          AddLog_P(LOG_LEVEL_DEBUG, log);
        }
        gesture_loop_counter += 1;
        /* Wait some time to collect next batch of FIFO data */
        delay(FIFO_PAUSE_TIME);

        /* Get the contents of the STATUS register. Is data still valid? */
       gstatus = I2cRead8(APDS9960_I2C_ADDR, APDS9960_GSTATUS);

        /* If we have valid data, read in FIFO */
        if( (gstatus & APDS9960_GVALID) == APDS9960_GVALID ) {

            /* Read the current FIFO level */
            fifo_level = I2cRead8(APDS9960_I2C_ADDR,APDS9960_GFLVL) ;

#if DEBUG
            Serial.print("FIFO Level: ");
            Serial.println(fifo_level);
#endif

            /* If there's stuff in the FIFO, read it into our data block */
            if( fifo_level > 0) {
                bytes_read = wireReadDataBlock(  APDS9960_GFIFO_U,
                                                (uint8_t*)fifo_data,
                                                (fifo_level * 4) );
                if( bytes_read == -1 ) {
                    return ERROR;
                }
#if DEBUG
                Serial.print("FIFO Dump: ");
                for ( i = 0; i < bytes_read; i++ ) {
                    Serial.print(fifo_data[i]);
                    Serial.print(" ");
                }
                Serial.println();
#endif

                /* If at least 1 set of data, sort the data into U/D/L/R */
                if( bytes_read >= 4 ) {
                    for( i = 0; i < bytes_read; i += 4 ) {
                        gesture_data_.u_data[gesture_data_.index] = \
                                                            fifo_data[i + 0];
                        gesture_data_.d_data[gesture_data_.index] = \
                                                            fifo_data[i + 1];
                        gesture_data_.l_data[gesture_data_.index] = \
                                                            fifo_data[i + 2];
                        gesture_data_.r_data[gesture_data_.index] = \
                                                            fifo_data[i + 3];
                        gesture_data_.index++;
                        gesture_data_.total_gestures++;
                    }

#if DEBUG
                Serial.print("Up Data: ");
                for ( i = 0; i < gesture_data_.total_gestures; i++ ) {
                    Serial.print(gesture_data_.u_data[i]);
                    Serial.print(" ");
                }
                Serial.println();
#endif

                    /* Filter and process gesture data. Decode near/far state */
                    if( processGestureData() ) {
                        if( decodeGesture() ) {
                            //***TODO: U-Turn Gestures
#if DEBUG
                            //Serial.println(gesture_motion_);
#endif
                        }
                    }

                    /* Reset data */
                    gesture_data_.index = 0;
                    gesture_data_.total_gestures = 0;
                }
            }
        } else {

            /* Determine best guessed gesture and clean up */
            delay(FIFO_PAUSE_TIME);
            decodeGesture();
            motion = gesture_motion_;
#if DEBUG
            Serial.print("END: ");
            Serial.println(gesture_motion_);
#endif
            resetGestureParameters();
            return motion;
        }
    }
}

/**
 * Turn the APDS-9960 on
 *
 */
void enablePower()
{
    setMode(POWER, 1) ;
}

/**
 * Turn the APDS-9960 off
 *
 */
void disablePower()
{
    setMode(POWER, 0) ;
}

/*******************************************************************************
 * Ambient light and color sensor controls
 ******************************************************************************/

/**
 * @brief Reads the ambient (clear) light level as a 16-bit value
 *
 * @param[out] val value of the light sensor.
 */
void readAmbientLight(uint16_t &val)
{
    uint8_t val_byte;
    val = 0;

    /* Read value from clear channel, low byte register */
    val_byte = I2cRead8(APDS9960_I2C_ADDR, APDS9960_CDATAL);
    val = val_byte;

    /* Read value from clear channel, high byte register */
    val_byte = I2cRead8(APDS9960_I2C_ADDR, APDS9960_CDATAH);
    val = val + ((uint16_t)val_byte << 8);
}

/**
 * @brief Reads the red light level as a 16-bit value
 *
 * @param[out] val value of the light sensor.
 */
void readRedLight(uint16_t &val)
{
    uint8_t val_byte;
    val = 0;

    /* Read value from clear channel, low byte register */
    val_byte = I2cRead8(APDS9960_I2C_ADDR, APDS9960_RDATAL) ;
    val = val_byte;

    /* Read value from clear channel, high byte register */
    val_byte = I2cRead8(APDS9960_I2C_ADDR, APDS9960_RDATAH) ;
    val = val + ((uint16_t)val_byte << 8);
}

/**
 * @brief Reads the green light level as a 16-bit value
 *
 * @param[out] val value of the light sensor.
 */
void readGreenLight(uint16_t &val)
{
    uint8_t val_byte;
    val = 0;

    /* Read value from clear channel, low byte register */
    val_byte = I2cRead8(APDS9960_I2C_ADDR, APDS9960_GDATAL) ;
    val = val_byte;

    /* Read value from clear channel, high byte register */
    val_byte = I2cRead8(APDS9960_I2C_ADDR, APDS9960_GDATAH) ;
    val = val + ((uint16_t)val_byte << 8);
}

/**
 * @brief Reads the red light level as a 16-bit value
 *
 * @param[out] val value of the light sensor.
 */
void readBlueLight(uint16_t &val)
{
    uint8_t val_byte;
    val = 0;

    /* Read value from clear channel, low byte register */
    val_byte = I2cRead8(APDS9960_I2C_ADDR, APDS9960_BDATAL) ;
    val = val_byte;

    /* Read value from clear channel, high byte register */
    val_byte = I2cRead8(APDS9960_I2C_ADDR, APDS9960_BDATAH) ;
    val = val + ((uint16_t)val_byte << 8);
}

/*******************************************************************************
 * Proximity sensor controls
 ******************************************************************************/

/**
 * @brief Reads the proximity level as an 8-bit value
 *
 * @param[out] val value of the proximity sensor.
 */
void readProximity(uint8_t &val)
{
    val = 0;

    /* Read value from proximity data register */
    val = I2cRead8(APDS9960_I2C_ADDR, APDS9960_PDATA) ;
}

/*******************************************************************************
 * High-level gesture controls
 ******************************************************************************/

/**
 * @brief Resets all the parameters in the gesture data member
 */
void resetGestureParameters()
{
    gesture_data_.index = 0;
    gesture_data_.total_gestures = 0;

    gesture_ud_delta_ = 0;
    gesture_lr_delta_ = 0;

    gesture_ud_count_ = 0;
    gesture_lr_count_ = 0;

    gesture_state_ = 0;
    gesture_motion_ = DIR_NONE;
}

/**
 * @brief Processes the raw gesture data to determine swipe direction
 *
 * @return True if near or far state seen. False otherwise.
 */
bool processGestureData()
{
    uint8_t u_first = 0;
    uint8_t d_first = 0;
    uint8_t l_first = 0;
    uint8_t r_first = 0;
    uint8_t u_last = 0;
    uint8_t d_last = 0;
    uint8_t l_last = 0;
    uint8_t r_last = 0;
    uint16_t ud_ratio_first;
    uint16_t lr_ratio_first;
    uint16_t ud_ratio_last;
    uint16_t lr_ratio_last;
    uint16_t ud_delta;
    uint16_t lr_delta;
    uint16_t i;

    /* If we have less than 4 total gestures, that's not enough */
    if( gesture_data_.total_gestures <= 4 ) {
        return false;
    }

    /* Check to make sure our data isn't out of bounds */
    if( (gesture_data_.total_gestures <= 32) && \
        (gesture_data_.total_gestures > 0) ) {

        /* Find the first value in U/D/L/R above the threshold */
        for( i = 0; i < gesture_data_.total_gestures; i++ ) {
            if( (gesture_data_.u_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.d_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.l_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.r_data[i] > GESTURE_THRESHOLD_OUT) ) {

                u_first = gesture_data_.u_data[i];
                d_first = gesture_data_.d_data[i];
                l_first = gesture_data_.l_data[i];
                r_first = gesture_data_.r_data[i];
                break;
            }
        }

        /* If one of the _first values is 0, then there is no good data */
        if( (u_first == 0) || (d_first == 0) || \
            (l_first == 0) || (r_first == 0) ) {

            return false;
        }
        /* Find the last value in U/D/L/R above the threshold */
        for( i = gesture_data_.total_gestures - 1; i >= 0; i-- ) {
#if DEBUG
            Serial.print(F("Finding last: "));
            Serial.print(F("U:"));
            Serial.print(gesture_data_.u_data[i]);
            Serial.print(F(" D:"));
            Serial.print(gesture_data_.d_data[i]);
            Serial.print(F(" L:"));
            Serial.print(gesture_data_.l_data[i]);
            Serial.print(F(" R:"));
            Serial.println(gesture_data_.r_data[i]);
#endif
            if( (gesture_data_.u_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.d_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.l_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.r_data[i] > GESTURE_THRESHOLD_OUT) ) {

                u_last = gesture_data_.u_data[i];
                d_last = gesture_data_.d_data[i];
                l_last = gesture_data_.l_data[i];
                r_last = gesture_data_.r_data[i];
                break;
            }
        }
    }

    /* Calculate the first vs. last ratio of up/down and left/right */
    ud_ratio_first = ((u_first - d_first) * 100) / (u_first + d_first);
    lr_ratio_first = ((l_first - r_first) * 100) / (l_first + r_first);
    ud_ratio_last = ((u_last - d_last) * 100) / (u_last + d_last);
    lr_ratio_last = ((l_last - r_last) * 100) / (l_last + r_last);

#if DEBUG
    Serial.print(F("Last Values: "));
    Serial.print(F("U:"));
    Serial.print(u_last);
    Serial.print(F(" D:"));
    Serial.print(d_last);
    Serial.print(F(" L:"));
    Serial.print(l_last);
    Serial.print(F(" R:"));
    Serial.println(r_last);

    Serial.print(F("Ratios: "));
    Serial.print(F("UD Fi: "));
    Serial.print(ud_ratio_first);
    Serial.print(F(" UD La: "));
    Serial.print(ud_ratio_last);
    Serial.print(F(" LR Fi: "));
    Serial.print(lr_ratio_first);
    Serial.print(F(" LR La: "));
    Serial.println(lr_ratio_last);
#endif

    /* Determine the difference between the first and last ratios */
    ud_delta = ud_ratio_last - ud_ratio_first;
    lr_delta = lr_ratio_last - lr_ratio_first;

#if DEBUG
    Serial.print("Deltas: ");
    Serial.print("UD: ");
    Serial.print(ud_delta);
    Serial.print(" LR: ");
    Serial.println(lr_delta);
#endif

    /* Accumulate the UD and LR delta values */
    gesture_ud_delta_ += ud_delta;
    gesture_lr_delta_ += lr_delta;

#if DEBUG
    Serial.print("Accumulations: ");
    Serial.print("UD: ");
    Serial.print(gesture_ud_delta_);
    Serial.print(" LR: ");
    Serial.println(gesture_lr_delta_);
#endif

    /* Determine U/D gesture */
    if( gesture_ud_delta_ >= GESTURE_SENSITIVITY_1 ) {
        gesture_ud_count_ = 1;
    } else if( gesture_ud_delta_ <= -GESTURE_SENSITIVITY_1 ) {
        gesture_ud_count_ = -1;
    } else {
        gesture_ud_count_ = 0;
    }

    /* Determine L/R gesture */
    if( gesture_lr_delta_ >= GESTURE_SENSITIVITY_1 ) {
        gesture_lr_count_ = 1;
    } else if( gesture_lr_delta_ <= -GESTURE_SENSITIVITY_1 ) {
        gesture_lr_count_ = -1;
    } else {
        gesture_lr_count_ = 0;
    }

#if DEBUG
    Serial.print("UD_CT: ");
    Serial.print(gesture_ud_count_);
    Serial.print(" LR_CT: ");
    Serial.print(gesture_lr_count_);
    Serial.println("----------");
#endif

    return false;
}

/**
 * @brief Determines swipe direction or near/far state
 *
 * @return True if near/far event. False otherwise.
 */
bool decodeGesture()
{

    /* Determine swipe direction */
    if( (gesture_ud_count_ == -1) && (gesture_lr_count_ == 0) ) {
        gesture_motion_ = DIR_UP;
    } else if( (gesture_ud_count_ == 1) && (gesture_lr_count_ == 0) ) {
        gesture_motion_ = DIR_DOWN;
    } else if( (gesture_ud_count_ == 0) && (gesture_lr_count_ == 1) ) {
        gesture_motion_ = DIR_RIGHT;
    } else if( (gesture_ud_count_ == 0) && (gesture_lr_count_ == -1) ) {
        gesture_motion_ = DIR_LEFT;
    } else if( (gesture_ud_count_ == -1) && (gesture_lr_count_ == 1) ) {
        if( abs(gesture_ud_delta_) > abs(gesture_lr_delta_) ) {
            gesture_motion_ = DIR_UP;
        } else {
            gesture_motion_ = DIR_RIGHT;
        }
    } else if( (gesture_ud_count_ == 1) && (gesture_lr_count_ == -1) ) {
        if( abs(gesture_ud_delta_) > abs(gesture_lr_delta_) ) {
            gesture_motion_ = DIR_DOWN;
        } else {
            gesture_motion_ = DIR_LEFT;
        }
    } else if( (gesture_ud_count_ == -1) && (gesture_lr_count_ == -1) ) {
        if( abs(gesture_ud_delta_) > abs(gesture_lr_delta_) ) {
            gesture_motion_ = DIR_UP;
        } else {
            gesture_motion_ = DIR_LEFT;
        }
    } else if( (gesture_ud_count_ == 1) && (gesture_lr_count_ == 1) ) {
        if( abs(gesture_ud_delta_) > abs(gesture_lr_delta_) ) {
            gesture_motion_ = DIR_DOWN;
        } else {
            gesture_motion_ = DIR_RIGHT;
        }
    } else {
        return false;
    }

    return true;
}

void handleGesture() {
    if (isGestureAvailable() ) {
    char log[LOGSZ];
    switch (readGesture()) {
      case DIR_UP:
        snprintf_P(log, sizeof(log), PSTR("UP"));
        snprintf_P(currentGesture, sizeof(currentGesture), PSTR("Up"));
        break;
      case DIR_DOWN:
        snprintf_P(log, sizeof(log), PSTR("DOWN"));
        snprintf_P(currentGesture, sizeof(currentGesture), PSTR("Down"));
        break;
      case DIR_LEFT:
        snprintf_P(log, sizeof(log), PSTR("LEFT"));
          snprintf_P(currentGesture, sizeof(currentGesture), PSTR("Left"));
        break;
      case DIR_RIGHT:
        snprintf_P(log, sizeof(log), PSTR("RIGHT"));
          snprintf_P(currentGesture, sizeof(currentGesture), PSTR("Right"));
        break;
      default:
      if(APDS9960_overload)
      {
        snprintf_P(log, sizeof(log), PSTR("LONG"));
        snprintf_P(currentGesture, sizeof(currentGesture), PSTR("Long"));
      }
      else{
        snprintf_P(log, sizeof(log), PSTR("NONE"));
        snprintf_P(currentGesture, sizeof(currentGesture), PSTR("None"));
      }
    }
    AddLog_P(LOG_LEVEL_DEBUG, log);

    mqtt_data[0] = '\0';
    if (MqttShowSensor()) {
      MqttPublishPrefixTopic_P(TELE, PSTR(D_RSLT_SENSOR), Settings.flag.mqtt_sensor_retain);
#ifdef USE_RULES
      RulesTeleperiod();  // Allow rule based HA messages
#endif  // USE_RULES
    }
  }
}


void APDS9960_loop()
{
  if (recovery_loop_counter > 0){
    recovery_loop_counter -= 1;
  }
  if (recovery_loop_counter == 1 && APDS9960_overload){  //restart sensor just before the end of recovery from long press
    enableGestureSensor(false);
    APDS9960_overload = false;
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"Gesture\":\"On\"}"));
    MqttPublishPrefixTopic_P(RESULT_OR_TELE, mqtt_data); // only after the long break we report, that we are online again
    gesture_mode = true;
  }

  if (gesture_mode) {
      if (recovery_loop_counter == 0){
      handleGesture();

        if (APDS9960_overload)
        {
        disableGestureSensor();
        recovery_loop_counter = APDS9960_LONG_RECOVERY;  // long pause after overload/long press - number of stateloops
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"Gesture\":\"Off\"}"));
        MqttPublishPrefixTopic_P(RESULT_OR_TELE, mqtt_data);
        gesture_mode = false;
        }
      }
  }
}


bool APDS9960_detect(void)
{

  if (APDS9960type) {
    return true;
  }

  boolean success = false;
  char log[LOGSZ];
  APDS9960type = I2cRead8(APDS9960_I2C_ADDR, APDS9960_ID);

  if (APDS9960type ==  APDS9960_CHIPID_1 || APDS9960type ==  APDS9960_CHIPID_2)
  {
    strcpy_P(APDS9960stype, PSTR("APDS9960"));
    snprintf_P(log, sizeof(log), PSTR("I2C: %s found at address 0x%x"), APDS9960stype, APDS9960_I2C_ADDR);
    AddLog_P(LOG_LEVEL_DEBUG, log);
    if (APDS9960_init()){
      success = true;
      snprintf_P(log, sizeof(log), PSTR("APDS9960 initialized"));
      AddLog_P(LOG_LEVEL_DEBUG, log);
      enableGestureSensor(false);
      }
  }
  else
  {
   snprintf_P(log, sizeof(log), PSTR("APDS9960 not found at address 0x%x"), APDS9960_I2C_ADDR);
   AddLog_P(LOG_LEVEL_DEBUG, log);
  }
  currentGesture[0] = '\0';
  return success;
}




/*********************************************************************************************\
 * Presentation
\*********************************************************************************************/

void APDS9960_show(boolean json)
{
  if (!APDS9960type) {
    return;
  }
  if (!gesture_mode)
    {
      char red_chr[10];
      char green_chr[10];
      char blue_chr[10];
      char ambient_chr[10];
      char prox_chr[10];
      uint16_t val;
      uint8_t val_prox;


      readRedLight(val);
      sprintf (red_chr, "%u", val);
      readGreenLight(val);
      sprintf (green_chr, "%u", val);
      readBlueLight(val);
      sprintf (blue_chr, "%u", val );
      readAmbientLight(val);
      sprintf (ambient_chr, "%u", val );

      readProximity(val_prox);
      sprintf (prox_chr, "%u", val_prox );

      if (json) {
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"%s\":{\"Red\":%s, \"Green\":%s, \"Blue\":%s, \"Ambient\":%s, \"Proximity\":%s}"),
      mqtt_data, APDS9960stype, red_chr, green_chr, blue_chr, ambient_chr, prox_chr);
      }

      #ifdef USE_WEBSERVER
      else{
      snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_APDS_9960_SNS, mqtt_data, red_chr, green_chr, blue_chr, ambient_chr, prox_chr );
        }
      #endif  // USE_WEBSERVER
    }
    else{
      if (json && (currentGesture[0] != '\0' )) {
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"%s\":{\"%s\":1}"), mqtt_data, APDS9960stype, currentGesture);
        currentGesture[0] = '\0';
      }
}
}
/*********************************************************************************************\
 * Commands
\*********************************************************************************************/

boolean apds9960_command()
{
  char command [CMDSZ];
  boolean serviced = true;

  int command_code = GetCommandCode(command, sizeof(command), XdrvMailbox.topic, kGestureCommands);
  if (-1 == command_code) {
    serviced = false;  // Unknown command
    return serviced;
  }

  if (CMND_GESTURE == command_code) {
      switch (XdrvMailbox.payload) {
      case 0: // Off
          disableGestureSensor();
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"Gesture\":\"Off\"}"));
          gesture_mode = false;
          enableLightSensor(false);
          enableProximitySensor(false);
          break;
      case 1: // On
        if (APDS9960type) {
          disableLightSensor();
          disableProximitySensor();
          enableGestureSensor(false);
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"Gesture\":\"on\"}"));
          gesture_mode = true;
          }
      default: // get status
          if(gesture_mode){
            snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"Gesture\":\"on\"}"));
            }
            else{
              snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"Gesture\":\"Off\"}"));
            }
      }
  }
  else {
    serviced = false;  // Unknown command
  }
  return serviced;
}
/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

#define XDRV_91

boolean Xdrv91(byte function)
{
  boolean result = false;

  if (i2c_flg) {
    switch (function) {
        case FUNC_INIT:
        APDS9960_detect();
        break;
        case FUNC_COMMAND:
        result = apds9960_command();
        break;
    }
  }
  return result;
}

#define XSNS_91

boolean Xsns91(byte function)
{
  boolean result = false;

  if (APDS9960type) {
    switch (function) {
      case FUNC_EVERY_50_MSECOND:
        APDS9960_loop();
        break;
      case FUNC_JSON_APPEND:
        APDS9960_show(1);
        break;
#ifdef USE_WEBSERVER
      case FUNC_WEB_APPEND:
        APDS9960_show(0);
        break;
#endif  // USE_WEBSERVER
    }
  }
  return result;
}
#endif  // USE_APDS9960
#endif  // USE_I2C