#pragma once

#include <stdbool.h>
#include "stm32g0xx_hal.h"

#ifndef RFM95_SPI_TIMEOUT
#define RFM95_SPI_TIMEOUT 10
#endif

#ifndef RFM95_WAKEUP_TIMEOUT
#define RFM95_WAKEUP_TIMEOUT 10
#endif

#ifndef RFM95_SEND_TIMEOUT
#define RFM95_SEND_TIMEOUT 1000
#endif


/**
 * Constants for RFM95 register values
 */
#define RFM9x_VER 0x12

#define RFM95_REGISTER_OP_MODE_SLEEP                            0x00
#define RFM95_REGISTER_OP_MODE_LORA_RXCONTINUOUS                0x05
#define RFM95_REGISTER_OP_MODE_LORA_RXSINGLE                    0x06
#define RFM95_REGISTER_OP_MODE_LORA                             0x80
#define RFM95_REGISTER_OP_MODE_LORA_STANDBY                     0x81
#define RFM95_REGISTER_OP_MODE_LORA_TX                          0x83

#define RFM95_REGISTER_PA_DAC_LOW_POWER                         0x84
#define RFM95_REGISTER_PA_DAC_HIGH_POWER                        0x87

#define RFM95_REGISTER_MODEM_CONFIG_3_LDR_OPTIM_AGC_AUTO_ON     0x0C

#define RFM95_REGISTER_DIO_MAPPING_1_IRQ_TXDONE                 0x40
#define RFM95_REGISTER_DIO_MAPPING_1_IRQ_RXDONE                 0x00

#define RFM95_REGISTER_INVERT_IQ_1_ON_TXONLY                    0x27
#define RFM95_REGISTER_INVERT_IQ_1_OFF                          0x26
#define RFM95_REGISTER_INVERT_IQ_2_ON                           0x19
#define RFM95_REGISTER_INVERT_IQ_2_OFF                          0x1D


/**
 * Registers addresses for RFM95.
 */
typedef enum
{
	RFM95_REGISTER_FIFO_ACCESS = 0x00,
	RFM95_REGISTER_OP_MODE = 0x01,
	RFM95_REGISTER_FR_MSB = 0x06,
	RFM95_REGISTER_FR_MID = 0x07,
	RFM95_REGISTER_FR_LSB = 0x08,
	RFM95_REGISTER_PA_CONFIG = 0x09,
	RFM95_REGISTER_FIFO_ADDR_PTR = 0x0D,
	RFM95_REGISTER_FIFO_TX_BASE_ADDR = 0x0E,
	RFM95_REGISTER_FIFO_RX_BASE_ADDR = 0x0F,
	RFM95_REGISTER_IRQ_FLAGS = 0x12,
	RFM95_REGISTER_MODEM_CONFIG_1 = 0x1D,
	RFM95_REGISTER_MODEM_CONFIG_2 = 0x1E,
	RFM95_REGISTER_SYMB_TIMEOUT_LSB = 0x1F,
	RFM95_REGISTER_PREAMBLE_MSB = 0x20,
	RFM95_REGISTER_PREAMBLE_LSB = 0x21,
	RFM95_REGISTER_PAYLOAD_LENGTH = 0x22,
	RFM95_REGISTER_MODEM_CONFIG_3 = 0x26,
	RFM95_REGISTER_INVERT_IQ_1 = 0x33,
	RFM95_REGISTER_SYNC_WORD = 0x39,
	RFM95_REGISTER_INVERT_IQ_2 = 0x3B,
	RFM95_REGISTER_DIO_MAPPING_1 = 0x40,
	RFM95_REGISTER_DIO_MAPPING_2 = 0x41,
	RFM95_REGISTER_VERSION = 0x42,
	RFM95_REGISTER_PA_DAC = 0x4D
} rfm95_register_t;


/**
 * Structure defining power of RFM95(W) transceiver.
 */
typedef struct
{
	union {
		struct {
			uint8_t output_power : 4;
			uint8_t max_power : 3;
			uint8_t pa_select : 1;
		};
		uint8_t buffer;
	};
} rfm95_register_pa_config_t;


/**
 * Structure defining a handle describing an RFM95(W) transceiver.
 */
typedef   int (*FP)(uint8_t *buf , uint8_t len);

typedef struct {

	SPI_HandleTypeDef *spi_handle; //The handle to the SPI bus for the device.

	GPIO_TypeDef *nss_port;   //The port of the NSS pin.
	uint16_t nss_pin;         // The NSS pin.

	GPIO_TypeDef *nrst_port;  //The port of the RST pin.
	uint16_t nrst_pin;        //The RST pin.

	GPIO_TypeDef *irq_port;   //The port of the IRQ / DIO0 pin.
	uint16_t irq_pin;         //The IRQ / DIO0 pin.

	GPIO_TypeDef *dio5_port;  // The port of the IRQ / DIO0 pin.
	uint16_t dio5_pin;        //The IRQ / DIO0 pin.

	volatile uint8_t txDone;
	volatile FP rxDoneCallback;

} rfm95_handle_t;


/**
 *  Global Variables
 */
static const unsigned char lora_frequency[3] = {0xE4, 0xC0, 0x26}; //14991398, 915.0 MHz
rfm95_handle_t *handle;
//uint32_t packetError = 0;

/**
 *  Global Functions
 */
bool rfm95_init(rfm95_handle_t *handle_pointer);
bool rfm95_setPower(int8_t power);
void rfm95_handleInterrupt();

bool transmitPackage(uint8_t *payload, size_t payloadLength);
bool receivePackage(uint8_t **buffer, uint8_t *packetLength);
bool rfm95_write(rfm95_register_t reg, uint8_t value);
bool rfm95_read(rfm95_register_t reg, uint8_t *buffer);
void rfm95_reset();


