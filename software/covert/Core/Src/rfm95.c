#include "rfm95.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/**
 * Private Function Definitions
 */
//static void rfm95_reset();
//static bool rfm95_read(rfm95_register_t reg, uint8_t *buffer);
//static bool rfm95_write(rfm95_register_t reg, uint8_t value);

///////////////////////////////////////////////////////////////////////////////

/**
 * Public functions
 */

/**
 * Initializes device and sets Handle
 */
bool rfm95_init(rfm95_handle_t *handle_pointer) {
	handle = handle_pointer;

	assert(handle->spi_handle->Init.Mode == SPI_MODE_MASTER);
	assert(handle->spi_handle->Init.Direction == SPI_DIRECTION_2LINES);
	assert(handle->spi_handle->Init.DataSize == SPI_DATASIZE_8BIT);
	assert(handle->spi_handle->Init.CLKPolarity == SPI_POLARITY_LOW);
	assert(handle->spi_handle->Init.CLKPhase == SPI_PHASE_1EDGE);

	rfm95_reset();

	// Check for correct version.
	uint8_t version = 0;
	if (!rfm95_read(RFM95_REGISTER_VERSION, &version))
		return false;

	if (version != RFM9x_VER)
		return false;

	// Module must be placed in sleep mode before switching to lora.
	if (!rfm95_write(RFM95_REGISTER_OP_MODE, RFM95_REGISTER_OP_MODE_SLEEP))
		return false;
	if (!rfm95_write(RFM95_REGISTER_OP_MODE, RFM95_REGISTER_OP_MODE_LORA))
		return false;

	// Set module power to 17dbm.
	if (!rfm95_setPower(20))
		return false;

	// RX timeout set to 37 symbols.
	if (!rfm95_write(RFM95_REGISTER_SYMB_TIMEOUT_LSB, 255))
		return false;

	// Preamble set to 8 + 4.25 = 12.25 symbols.
	if (!rfm95_write(RFM95_REGISTER_PREAMBLE_MSB, 0x00))
		return false;
	if (!rfm95_write(RFM95_REGISTER_PREAMBLE_LSB, 0x08))
		return false;

	// Set IQ inversion.
	if (!rfm95_write(RFM95_REGISTER_INVERT_IQ_1,
	RFM95_REGISTER_INVERT_IQ_1_ON_TXONLY))
		return false;
	if (!rfm95_write(RFM95_REGISTER_INVERT_IQ_2,
	RFM95_REGISTER_INVERT_IQ_2_OFF))
		return false;

	// Set up TX and RX FIFO base addresses.
	if (!rfm95_write(RFM95_REGISTER_FIFO_TX_BASE_ADDR, 0x80))
		return false;
	if (!rfm95_write(RFM95_REGISTER_FIFO_RX_BASE_ADDR, 0x00))
		return false;

	if (!rfm95_write(RFM95_REGISTER_FR_MSB, lora_frequency[0]))
		return false;
	if (!rfm95_write(RFM95_REGISTER_FR_MID, lora_frequency[1]))
		return false;
	if (!rfm95_write(RFM95_REGISTER_FR_LSB, lora_frequency[2]))
		return false;
//82
	if (!rfm95_write(RFM95_REGISTER_MODEM_CONFIG_1, 0x82))
		return false;
	if (!rfm95_write(RFM95_REGISTER_MODEM_CONFIG_2, 0x90 | 0b111))
		return false;  //change to 0x94 for enabling CRC
	if (!rfm95_write(RFM95_REGISTER_MODEM_CONFIG_3, 0x00))
		return false;

	rfm95_write(RFM95_REGISTER_OP_MODE,
	RFM95_REGISTER_OP_MODE_LORA_RXCONTINUOUS | 0x80);

	return true;
}

/**
 * Sets power for transmission, 17 by default
 */
bool rfm95_setPower(int8_t power) {
	rfm95_register_pa_config_t pa_config = { 0 };
	uint8_t pa_dac_config = 0;

	if (power >= 2 && power <= 17) {
		pa_config.max_power = 7;
		pa_config.pa_select = 1;
		pa_config.output_power = (power - 2);
		pa_dac_config = RFM95_REGISTER_PA_DAC_LOW_POWER;

	} else if (power == 20) {
		pa_config.max_power = 7;
		pa_config.pa_select = 1;
		pa_config.output_power = 15;
		pa_dac_config = RFM95_REGISTER_PA_DAC_HIGH_POWER;
	}

	if (!rfm95_write(RFM95_REGISTER_PA_CONFIG, pa_config.buffer))
		return false;
	if (!rfm95_write(RFM95_REGISTER_PA_DAC, pa_dac_config))
		return false;

	return true;
}

/**
 * Transmits payload after adding preamble
 */
bool transmitPackage(uint8_t *payload, size_t payloadLength) {

	if (!handle->txDone) {
//		rfm95_write(RFM95_REGISTER_OP_MODE,
//							RFM95_REGISTER_OP_MODE_LORA_RXCONTINUOUS | 0x80);
		return false;
	}
	handle->txDone = false;

	uint8_t regopmode = 0;
	do {
		rfm95_read(RFM95_REGISTER_OP_MODE, &regopmode);
		if (!rfm95_write(RFM95_REGISTER_OP_MODE,
		RFM95_REGISTER_OP_MODE_LORA_STANDBY))
			return false;
		HAL_Delay(1);
	} while (regopmode != RFM95_REGISTER_OP_MODE_LORA_STANDBY);

	if (!rfm95_write(RFM95_REGISTER_PAYLOAD_LENGTH, payloadLength))
		return false;

	// Set SPI pointer to start of TX section in FIFO
	if (!rfm95_write(RFM95_REGISTER_FIFO_ADDR_PTR, 0x80))
		return false;

	// Write payload to FIFO.
	for (size_t i = 0; i < payloadLength; i++) {
		rfm95_write(RFM95_REGISTER_FIFO_ACCESS, payload[i]);
	}

	if (!rfm95_write(RFM95_REGISTER_DIO_MAPPING_1,
	RFM95_REGISTER_DIO_MAPPING_1_IRQ_TXDONE))
		return false;
	if (!rfm95_write(RFM95_REGISTER_OP_MODE, RFM95_REGISTER_OP_MODE_LORA_TX))
		return false;

	// todo check this

//	while (HAL_GPIO_ReadPin(handle->irq_port, handle->irq_pin) == GPIO_PIN_RESET) {
//	}
//
//	rfm95_write(RFM95_REGISTER_OP_MODE,
//			RFM95_REGISTER_OP_MODE_LORA_RXCONTINUOUS | 0x80);
	//
//	if (!rfm95_write(RFM95_REGISTER_OP_MODE, RFM95_REGISTER_OP_MODE_SLEEP)) return false;

//	rfm95_write(RFM95_REGISTER_OP_MODE,
//				RFM95_REGISTER_OP_MODE_LORA_RXCONTINUOUS | 0x80);

	return true;

}

/**
 * Reads payload stored in receivedPacketData global variable and stores it in buffer.
 * also stores receivedPacketLength in packetLength
 */
//bool receivePackage(uint8_t **buffer, uint8_t *packetLength) {
//	if (receivedPacketData == NULL)
//		return false;
//
//	packetLength = &receivedPacketLength;
//
//	if (*buffer != NULL) {
//		free(buffer);
//		*buffer = NULL;
//	}
//
//	*buffer = (uint8_t*) calloc(receivedPacketLength, sizeof(uint8_t));
//	memcpy(*buffer, receivedPacketData, receivedPacketLength);
//
//	return true;
//}
/**
 * Generic function for handling interrupt, for tx and rx
 */
void rfm95_handleInterrupt() {
	uint8_t irqFlags;
	rfm95_read(RFM95_REGISTER_IRQ_FLAGS, &irqFlags);
	rfm95_write(RFM95_REGISTER_IRQ_FLAGS, irqFlags);

	if ((irqFlags & 0x20) == 0) {
//		++packetError;
		if ((irqFlags & 0x40) != 0) {
//			--packetError;
			// read packet length
			uint8_t packetLength;

			// reading from RX_NVBYTES, since implicit header mode is off
			// check line 706 in the arduino library
			rfm95_read(0x13, &packetLength);

			// set FIFO address to current RX address
			uint8_t currentAddr;

			rfm95_read(0x10, &currentAddr);
			rfm95_write(RFM95_REGISTER_FIFO_ADDR_PTR, currentAddr);

			uint8_t *buffer = (uint8_t*) calloc(packetLength, sizeof(uint8_t));

			for (size_t i = 0; i < packetLength; i++) {
				rfm95_read(RFM95_REGISTER_FIFO_ACCESS, &buffer[i]);
			}

//            if (!handle -> rxDoneCallback && isPacketValid(buffer, packetLength)) {
//
//                if (receivedPacketData != NULL)
//                {
//                	free(receivedPacketData);
//                	receivedPacketData = NULL;
//                }
//
//                receivedPacketLength = packetLength;
//                receivedPacketData = (uint8_t *) calloc(packetLength, sizeof(uint8_t));
//            	memcpy(receivedPacketData, buffer + 4, packetLength);
//            }

			if (handle->rxDoneCallback) {
				handle->rxDoneCallback(buffer, packetLength);
			}
			// maybe clear the fifo by doing this?
//			rfm95_write(RFM95_REGISTER_OP_MODE, RFM95_REGISTER_OP_MODE_SLEEP);
//			HAL_Delay(1);
//			rfm95_write(RFM95_REGISTER_OP_MODE,
//								RFM95_REGISTER_OP_MODE_LORA_RXCONTINUOUS | 0x80);

			//line 401? receive()
			//writeRegister(REG_DIO_MAPPING_1, 0x00); // DIO0 => RXDONE
			rfm95_write(RFM95_REGISTER_DIO_MAPPING_1, 0x00);
			rfm95_write(RFM95_REGISTER_OP_MODE,
								RFM95_REGISTER_OP_MODE_LORA_RXCONTINUOUS | 0x80);

			rfm95_write(RFM95_REGISTER_FIFO_RX_BASE_ADDR, 0x00);
			//RegSeqConfig1?
			free(buffer);

		}
		if ((irqFlags & 0x08) != 0) {
			handle->txDone = true;
			rfm95_write(RFM95_REGISTER_OP_MODE,
					RFM95_REGISTER_OP_MODE_LORA_RXCONTINUOUS | 0x80);
			rfm95_write(RFM95_REGISTER_DIO_MAPPING_1, 0x00);

		}
	}
}

///////////////////////////////////////////////////////////////////////////////

/**
 * Private Functions
 */

/**
 * Reads from register given by reg and stores value in buffer
 */
bool rfm95_read(rfm95_register_t reg, uint8_t *buffer) {
	HAL_GPIO_WritePin(handle->nss_port, handle->nss_pin, GPIO_PIN_RESET);

	uint8_t transmit_buffer = (uint8_t) reg & 0x7fu;

	if (HAL_SPI_Transmit(handle->spi_handle, &transmit_buffer, 1,
	RFM95_SPI_TIMEOUT) != HAL_OK)
		return false;
	if (HAL_SPI_Receive(handle->spi_handle, buffer, 1, RFM95_SPI_TIMEOUT)
			!= HAL_OK)
		return false;

	HAL_GPIO_WritePin(handle->nss_port, handle->nss_pin, GPIO_PIN_SET);

	return true;
}

/**
 * Writes value to register given by reg
 */
bool rfm95_write(rfm95_register_t reg, uint8_t value) {
	HAL_GPIO_WritePin(handle->nss_port, handle->nss_pin, GPIO_PIN_RESET);

	uint8_t transmit_buffer[2] = { ((uint8_t) reg | 0x80u), value };

	if (HAL_SPI_Transmit(handle->spi_handle, transmit_buffer, 2,
	RFM95_SPI_TIMEOUT) != HAL_OK)
		return false;

	HAL_GPIO_WritePin(handle->nss_port, handle->nss_pin, GPIO_PIN_SET);

	return true;
}

/**
 * Resets Device for initialization
 */
void rfm95_reset() {
	HAL_GPIO_WritePin(handle->nrst_port, handle->nrst_pin, GPIO_PIN_RESET);
	HAL_Delay(1);
	HAL_GPIO_WritePin(handle->nrst_port, handle->nrst_pin, GPIO_PIN_SET);
	HAL_Delay(5);
}

