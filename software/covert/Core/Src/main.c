/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <assert.h>
#include "crypto.h"
#include "rfm95.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
	uint32_t privateKey[8];
	uint8_t publicKey[32];
	uint8_t otherPublicKey[32];
	uint8_t sharedSecret[32];
	volatile uint8_t gotOther;
	volatile uint8_t masterSent;
	volatile uint8_t pairing;
} AsymmetricKeys;

typedef struct {
	volatile uint8_t enabled;
	uint8_t count;
	uint64_t data;
} Record;

typedef struct __attribute__((__packed__)) {
	uint8_t preamble;
	uint8_t deviceID;
	uint32_t sequenceNumber;
	uint64_t data;
	uint16_t _placeholder;
} Packet;

typedef struct __attribute__((__packed__)) {
	uint8_t preamble;
	uint8_t data[32];
} PublicKeyPacket;

typedef struct __attribute__((__packed__)) {
	uint8_t preamble;
	uint8_t data[16];
} KeyExchangePacket;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define MASTER_DEVICE 	0
#define DEVICE_ID 		1
#define RESET 			0
#define NEW_SEQ			0

#define AESKeySize 128/8 //(8 * sizeof(uint32_t));
#define PUBLIC_EXCHANGE_PREAMBLE 0b01010101
#define AES_KEY_EXCHANGE_PREAMBLE 0b10101010
#define VIBE_PREAMBLE 0b11110000
#define DUTY_CYCLE_ON 10
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CRYP_HandleTypeDef hcryp;
uint32_t pKeyAES[4] __ALIGN_END = { 0x00000000,
		0x00000000, 0x00000000, 0x00000000 };
__ALIGN_BEGIN static const uint32_t pInitVectAES[4] __ALIGN_END = { 0x5B841799,
		0xF2DBC132, 0x3961879F, 0x8B3F49C0 };

CRC_HandleTypeDef hcrc;

RNG_HandleTypeDef hrng;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim16;

/* USER CODE BEGIN PV */
FLASH_EraseInitTypeDef EraseInitStruct;
FLASH_EraseInitTypeDef EraseSeqStruct;

AsymmetricKeys aKeys;
Record playback;
Record recording;
Packet outgoing;
rfm95_handle_t radio;
uint32_t deviceSeqs[256] = { 0 };

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_AES_Init(void);
static void MX_RNG_Init(void);
static void MX_CRC_Init(void);
static void MX_TIM16_Init(void);
static void MX_TIM1_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
static void readKeyFromFlash(uint32_t *ptr, FLASH_EraseInitTypeDef *erase);
static void writeKeyToFlash(uint64_t *ptr, FLASH_EraseInitTypeDef *erase);
static uint32_t readSeqFromFlash(FLASH_EraseInitTypeDef *erase);
static void writeSeqToFlash(uint32_t seq, FLASH_EraseInitTypeDef *erase);
static void readingCallback(uint8_t *buffer, uint8_t length);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */
	uint8_t testing = sizeof(Packet);
	assert(
			sizeof(PublicKeyPacket) == 33 && sizeof(KeyExchangePacket) == 17
					&& sizeof(Packet) == 16);
	// Key writing to flash
	EraseInitStruct.Banks = FLASH_BANK_1;
	EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
	EraseInitStruct.Page = (FLASH_PAGE_NB - 1);
	EraseInitStruct.NbPages = 1;

	// Sequence Number storing in flash
	EraseSeqStruct.Banks = FLASH_BANK_1;
	EraseSeqStruct.TypeErase = FLASH_TYPEERASE_PAGES;
	EraseSeqStruct.Page = (FLASH_PAGE_NB - 2);
	EraseSeqStruct.NbPages = 1;

	recording.enabled = 0;
	playback.enabled = 0;

	radio.spi_handle = &hspi1;

	radio.nss_port = RADIO_CS_GPIO_Port;
	radio.nss_pin = RADIO_CS_Pin;
	radio.nrst_port = RADIO_RESET_GPIO_Port;
	radio.nrst_pin = RADIO_RESET_Pin;
	radio.irq_port = RADIO_INT_GPIO_Port;
	radio.irq_pin = RADIO_INT_Pin;

	radio.txDone = true;
	radio.rxDoneCallback = readingCallback;

	aKeys.gotOther = 0;
	aKeys.pairing = 0;
	aKeys.masterSent = 0;
	aKeys.sharedSecret[0] = 0;

	outgoing.data = 0;

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_AES_Init();
	MX_RNG_Init();
	MX_CRC_Init();
	MX_TIM16_Init();
	MX_TIM1_Init();
	MX_SPI1_Init();
	/* USER CODE BEGIN 2 */

	HAL_Delay(100);
	if (!rfm95_init(&radio)) {
		HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);

		while (1) {

		}
	}

	HAL_TIM_Base_Start_IT(&htim16);
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);

	readKeyFromFlash(pKeyAES, &EraseInitStruct);
	// We lost our random key or we want a reset?
	if (RESET || pKeyAES[0] == 0 || pKeyAES[0] == UINT32_MAX) {
		uint64_t tmp[2];
		for (int i = 0; i < 2; i++) {
			if (HAL_RNG_GenerateRandomNumber(&hrng, &tmp[i]) != HAL_OK)
				Error_Handler();
			tmp[i] <<= 32;
			if (HAL_RNG_GenerateRandomNumber(&hrng, &tmp[i]) != HAL_OK)
				Error_Handler();
		}
		writeKeyToFlash(tmp, &EraseInitStruct);
		readKeyFromFlash(pKeyAES, &EraseInitStruct);
	}
	MX_AES_Init();

	// Generate a random sequence number for packets -- assume 2000 is the most packets we'll ever send while devices haven't rebooted
	deviceSeqs[DEVICE_ID] = readSeqFromFlash(&EraseSeqStruct);
	if (NEW_SEQ || deviceSeqs[DEVICE_ID] >= ((UINT32_MAX) >> 1)
			|| deviceSeqs[DEVICE_ID] == 0) {
		uint32_t seq = 0;
		HAL_RNG_GenerateRandomNumber(&hrng, &seq);
		seq >>= 1;
		deviceSeqs[DEVICE_ID] = seq;
	}
	deviceSeqs[DEVICE_ID] += 2000;
	writeSeqToFlash(deviceSeqs[DEVICE_ID], &EraseSeqStruct);

	// Might as well generate a public key in advance
	for (int i = 0; i < 8; i++) {
		HAL_RNG_GenerateRandomNumber(&hrng, &aKeys.privateKey[i]);
	}
	C25519keyGen((uint8_t*) aKeys.privateKey, aKeys.publicKey);

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		if (aKeys.pairing && aKeys.pairing++ <= 5) {
			// send our public key in plaintext.
			PublicKeyPacket tmp;
			tmp.preamble = PUBLIC_EXCHANGE_PREAMBLE;
			memcpy(tmp.data, aKeys.publicKey, sizeof(tmp.data));
			transmitPackage(&tmp, sizeof(PublicKeyPacket));
			// randomize the delay here
			uint32_t randoffset = 0;
			HAL_RNG_GenerateRandomNumber(&hrng, &randoffset);

			HAL_Delay(1000 + (randoffset & 0x7ff));
			continue;
		} else {
			aKeys.pairing = 0;
		}

		if (aKeys.gotOther) {
			C25519keyExchange(aKeys.sharedSecret, (uint8_t*) aKeys.privateKey,
					aKeys.otherPublicKey);
//			rfm95_init(&radio);
			aKeys.gotOther = 0;
			aKeys.masterSent = 1;
		}

		if (aKeys.masterSent && aKeys.masterSent++ <= 10) {
			if (MASTER_DEVICE) {
				// Save the "master's secret key" in old pkeys
				uint32_t oldPkeys[4] = { 0 };
				memcpy(oldPkeys, pKeyAES, AESKeySize);
				// move the shared secret to the AES hardware for encryption
				memcpy(pKeyAES, aKeys.sharedSecret, AESKeySize);
				MX_AES_Init();
				// Create a packet & attack the "master's key"
				KeyExchangePacket tmp;
				tmp.preamble = AES_KEY_EXCHANGE_PREAMBLE;
				memcpy(tmp.data, oldPkeys, AESKeySize);

				// Output the encrypted packet in a buffer
				uint8_t inputBuf[32] = { 0 };
				memcpy(inputBuf, &tmp, sizeof(KeyExchangePacket));
				uint8_t outputBuf[32] = { 0 };

				if (HAL_CRYP_Encrypt(&hcryp, inputBuf, 32, outputBuf, 1)
						== HAL_OK) {
					transmitPackage(outputBuf, 32);
				}
				memcpy(pKeyAES, oldPkeys, AESKeySize);
				MX_AES_Init();
			} else {
				//we're a slave device and we can just sit and wait for master count to ++
			}
			HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
			HAL_Delay(500);
			HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
			HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
			HAL_Delay(500);
			continue;
		} else {
			aKeys.masterSent = 0;
		}

		if (outgoing.data) {
			//encrypt and transmit the outgoing packet
			uint32_t tempin[4] = { 0 };
			uint32_t tempout[4] = { 0 };
			memcpy(tempin, &outgoing, sizeof(Packet));
			HAL_CRYP_Encrypt(&hcryp, (uint8_t*) tempin, 16, (uint8_t*) tempout,
					1);
			outgoing.data = 0;
			for (uint8_t i = 0; i < 3; i++) {
				while (!transmitPackage((uint8_t*) tempout, 16)) {
					HAL_Delay(70);
				}
			}
			continue;
		}

		HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);

//		HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_9);
		HAL_Delay(250);
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };
	RCC_PeriphCLKInitTypeDef PeriphClkInit = { 0 };

	/** Configure the main internal regulator output voltage
	 */
	HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);
	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
	RCC_OscInitStruct.PLL.PLLN = 8;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
	RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}
	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
		Error_Handler();
	}
	/** Initializes the peripherals clocks
	 */
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RNG | RCC_PERIPHCLK_TIM1;
	PeriphClkInit.RngClockSelection = RCC_RNGCLKSOURCE_HSI_DIV8;
	PeriphClkInit.Tim1ClockSelection = RCC_TIM1CLKSOURCE_PCLK1;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
		Error_Handler();
	}
	/** Configure the RNG clock.
	 */
	__HAL_RCC_RNGDIV_CONFIG(RCC_RNGCLK_DIV1);
}

/**
 * @brief AES Initialization Function
 * @param None
 * @retval None
 */
static void MX_AES_Init(void) {

	/* USER CODE BEGIN AES_Init 0 */

	/* USER CODE END AES_Init 0 */

	/* USER CODE BEGIN AES_Init 1 */

	/* USER CODE END AES_Init 1 */
	hcryp.Instance = AES;
	hcryp.Init.DataType = CRYP_DATATYPE_8B;
	hcryp.Init.KeySize = CRYP_KEYSIZE_128B;
	hcryp.Init.pKey = (uint32_t*) pKeyAES;
	hcryp.Init.pInitVect = (uint32_t*) pInitVectAES;
	hcryp.Init.Algorithm = CRYP_AES_CBC;
	hcryp.Init.DataWidthUnit = CRYP_DATAWIDTHUNIT_BYTE;
	hcryp.Init.HeaderWidthUnit = CRYP_HEADERWIDTHUNIT_BYTE;
	hcryp.Init.KeyIVConfigSkip = CRYP_KEYIVCONFIG_ALWAYS;
	if (HAL_CRYP_Init(&hcryp) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN AES_Init 2 */
	/* USER CODE END AES_Init 2 */

}

/**
 * @brief CRC Initialization Function
 * @param None
 * @retval None
 */
static void MX_CRC_Init(void) {

	/* USER CODE BEGIN CRC_Init 0 */

	/* USER CODE END CRC_Init 0 */

	/* USER CODE BEGIN CRC_Init 1 */

	/* USER CODE END CRC_Init 1 */
	hcrc.Instance = CRC;
	hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE;
	hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
	hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
	hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
	hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
	if (HAL_CRC_Init(&hcrc) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN CRC_Init 2 */

	/* USER CODE END CRC_Init 2 */

}

/**
 * @brief RNG Initialization Function
 * @param None
 * @retval None
 */
static void MX_RNG_Init(void) {

	/* USER CODE BEGIN RNG_Init 0 */

	/* USER CODE END RNG_Init 0 */

	/* USER CODE BEGIN RNG_Init 1 */

	/* USER CODE END RNG_Init 1 */
	hrng.Instance = RNG;
	hrng.Init.ClockErrorDetection = RNG_CED_ENABLE;
	if (HAL_RNG_Init(&hrng) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN RNG_Init 2 */

	/* USER CODE END RNG_Init 2 */

}

/**
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI1_Init(void) {

	/* USER CODE BEGIN SPI1_Init 0 */

	/* USER CODE END SPI1_Init 0 */

	/* USER CODE BEGIN SPI1_Init 1 */

	/* USER CODE END SPI1_Init 1 */
	/* SPI1 parameter configuration*/
	hspi1.Instance = SPI1;
	hspi1.Init.Mode = SPI_MODE_MASTER;
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi1.Init.NSS = SPI_NSS_SOFT;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.CRCPolynomial = 7;
	hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
	hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
	if (HAL_SPI_Init(&hspi1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN SPI1_Init 2 */

	/* USER CODE END SPI1_Init 2 */

}

/**
 * @brief TIM1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM1_Init(void) {

	/* USER CODE BEGIN TIM1_Init 0 */

	/* USER CODE END TIM1_Init 0 */

	TIM_MasterConfigTypeDef sMasterConfig = { 0 };
	TIM_OC_InitTypeDef sConfigOC = { 0 };
	TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = { 0 };

	/* USER CODE BEGIN TIM1_Init 1 */

	/* USER CODE END TIM1_Init 1 */
	htim1.Instance = TIM1;
	htim1.Init.Prescaler = 0;
	htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim1.Init.Period = 65535;
	htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim1.Init.RepetitionCounter = 0;
	htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
	sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
	if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1)
			!= HAL_OK) {
		Error_Handler();
	}
	sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
	sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
	sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
	sBreakDeadTimeConfig.DeadTime = 0;
	sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
	sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
	sBreakDeadTimeConfig.BreakFilter = 0;
	sBreakDeadTimeConfig.BreakAFMode = TIM_BREAK_AFMODE_INPUT;
	sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
	sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
	sBreakDeadTimeConfig.Break2Filter = 0;
	sBreakDeadTimeConfig.Break2AFMode = TIM_BREAK_AFMODE_INPUT;
	sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
	if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM1_Init 2 */

	/* USER CODE END TIM1_Init 2 */
	HAL_TIM_MspPostInit(&htim1);

}

/**
 * @brief TIM16 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM16_Init(void) {

	/* USER CODE BEGIN TIM16_Init 0 */

	/* USER CODE END TIM16_Init 0 */

	/* USER CODE BEGIN TIM16_Init 1 */

	/* USER CODE END TIM16_Init 1 */
	htim16.Instance = TIM16;
	htim16.Init.Prescaler = 64;
	htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim16.Init.Period = 25000;
	htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim16.Init.RepetitionCounter = 0;
	htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim16) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM16_Init 2 */

	/* USER CODE END TIM16_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(RADIO_CS_GPIO_Port, RADIO_CS_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOC, RADIO_RESET_Pin | LED2_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : RADIO_INT_Pin */
	GPIO_InitStruct.Pin = RADIO_INT_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(RADIO_INT_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : RADIO_CS_Pin */
	GPIO_InitStruct.Pin = RADIO_CS_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(RADIO_CS_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : RADIO_RESET_Pin LED2_Pin */
	GPIO_InitStruct.Pin = RADIO_RESET_Pin | LED2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pin : PAIR_Pin */
	GPIO_InitStruct.Pin = PAIR_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(PAIR_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : VIBE_BUTTON_Pin */
	GPIO_InitStruct.Pin = VIBE_BUTTON_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(VIBE_BUTTON_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : LED1_Pin */
	GPIO_InitStruct.Pin = LED1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LED1_GPIO_Port, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI0_1_IRQn, 1, 0);
	HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);

	HAL_NVIC_SetPriority(EXTI4_15_IRQn, 1, 0);
	HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

}

/* USER CODE BEGIN 4 */

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin) {
//	To enable instant replay
//	if (GPIO_Pin == VIBE_BUTTON_Pin) {
//		TIM1->CCR1 = (DUTY_CYCLE_ON * UINT16_MAX) / 10;
//		HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
//	}

	if (recording.enabled || aKeys.pairing) {
		return;
	}
	if (GPIO_Pin == PAIR_Pin) {
		// in actual pairing
		aKeys.pairing = 1;

	} else if (GPIO_Pin == VIBE_BUTTON_Pin) {
		// on vibe button:
		recording.enabled = 1;
		recording.count = 1;
		recording.data = 1;
	}
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == RADIO_INT_Pin) {
		rfm95_handleInterrupt();
	}
//	To enable instant replay
//	if (GPIO_Pin == VIBE_BUTTON_Pin) {
//		TIM1->CCR1 = 0;
//		HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
//	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (aKeys.pairing) {
		HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);
	}
//	if (aKeys.masterSent) {
//		HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
//	}

	if (htim == &htim16 && (recording.enabled || playback.enabled)) {

		if (playback.enabled) {
			uint64_t shifted = (playback.data >> (playback.count++));
			uint8_t state = shifted & 1;

			TIM1->CCR1 = state ? (DUTY_CYCLE_ON * UINT16_MAX) / 10 : 0;
			HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin,
					(state ? GPIO_PIN_SET : GPIO_PIN_RESET));

			if (playback.count >= sizeof(playback.data) * 8) {
				playback.enabled = 0;
				HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
				TIM1->CCR1 = 0;
			}
		}

		if (recording.enabled) {
			uint64_t read = (
					HAL_GPIO_ReadPin(VIBE_BUTTON_GPIO_Port, VIBE_BUTTON_Pin)
							== GPIO_PIN_RESET ? 1 : 0);
			uint64_t state = read << (recording.count++);
			recording.data |= state;
			if (recording.count >= sizeof(recording.data) * 8) {
				// packet should be formed & sent here

				outgoing.deviceID = DEVICE_ID;
				outgoing.preamble = VIBE_PREAMBLE;
				outgoing.sequenceNumber = ++deviceSeqs[DEVICE_ID];
				outgoing.data = recording.data;

				// replay on local device:
//				playback.data = recording.data;
//				playback.enabled = 1;
//				playback.count = 0;

				recording.enabled = 0;
				if (state) {
					recording.enabled = 1;
					recording.count = 1;
					recording.data = 1;
				}

			}
		}
	}
}

static void readingCallback(uint8_t *buffer, uint8_t length) {
	if (aKeys.pairing && length == sizeof(PublicKeyPacket)) {
		PublicKeyPacket tmp;
		tmp.preamble = 0;
		memcpy(&tmp, buffer, length);
		if (tmp.preamble == PUBLIC_EXCHANGE_PREAMBLE) {
			memcpy(aKeys.otherPublicKey, tmp.data, 32);
			aKeys.gotOther = 1;
		}
	} else if (!MASTER_DEVICE && length == 32 && aKeys.masterSent) {
		// try to decrypt with shared secret
		uint32_t oldPkeys[4] = { 0 };

		memcpy(oldPkeys, pKeyAES, AESKeySize);
		memcpy(pKeyAES, aKeys.sharedSecret, AESKeySize);
		MX_AES_Init();

		uint8_t decryptRes[32] = { 0 };
		if (HAL_CRYP_Decrypt(&hcryp, buffer, length, decryptRes, 1) == HAL_OK) {
			KeyExchangePacket tmp;
			tmp.preamble = 0;
			memcpy(&tmp, decryptRes, sizeof(decryptRes));
			if (tmp.preamble == AES_KEY_EXCHANGE_PREAMBLE) {
				writeKeyToFlash((uint64_t*) tmp.data, &EraseInitStruct);

				memcpy(pKeyAES, tmp.data, AESKeySize);
				// We don't necessarily have to have this here -- we can let it keep writing
				aKeys.masterSent = 0;
			}
		} else {
			memcpy(pKeyAES, oldPkeys, AESKeySize);
		}
		MX_AES_Init();
	} else if (length == sizeof(Packet)) {
		Packet tmp;
		tmp.preamble = 0;
		if (HAL_CRYP_Decrypt(&hcryp, buffer, length, &tmp, 1) == HAL_OK
				&& tmp.preamble == VIBE_PREAMBLE
				&& tmp.sequenceNumber > deviceSeqs[tmp.deviceID]) {

			playback.data = tmp.data;
			playback.enabled = 1;
			playback.count = 0;
			deviceSeqs[tmp.deviceID] = tmp.sequenceNumber;

		}
	}
}

static void readKeyFromFlash(uint32_t *ptr, FLASH_EraseInitTypeDef *erase) {

	uint32_t addr = 0x08000000 + FLASH_PAGE_SIZE * erase->Page;
	for (int i = 0; i < AESKeySize / sizeof(uint32_t); i++) {
		ptr[i] = ((uint32_t*) addr)[i];
	}
}

static void writeKeyToFlash(uint64_t *ptr, FLASH_EraseInitTypeDef *erase) {
//801f800
	uint32_t addr = 0x08000000 + FLASH_PAGE_SIZE * erase->Page;

	uint32_t pgerr = 0;
	HAL_FLASH_Unlock();
	HAL_FLASHEx_Erase(erase, &pgerr);
	uint64_t alignedtmp[2] = { 0 };
	memcpy(alignedtmp, ptr, sizeof(alignedtmp));
	for (int i = 0; i < 2; i++) {
		uint64_t val = alignedtmp[i];
		uint32_t location = addr + (sizeof(uint64_t)) * i;
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, location, val);
	}
	HAL_FLASH_Lock();
}

static uint32_t readSeqFromFlash(FLASH_EraseInitTypeDef *erase) {
	uint32_t addr = 0x08000000 + FLASH_PAGE_SIZE * erase->Page;
	return *((uint32_t*) addr);
}
static void writeSeqToFlash(uint32_t seq, FLASH_EraseInitTypeDef *erase) {
//801f000
	uint32_t addr = 0x08000000 + FLASH_PAGE_SIZE * erase->Page;
	uint32_t pgerr = 0;
	HAL_FLASH_Unlock();
	HAL_FLASHEx_Erase(erase, &pgerr);
	HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, seq);
	HAL_FLASH_Lock();
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
