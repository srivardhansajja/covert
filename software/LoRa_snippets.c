// import statements
#include <rfm95.h>



// initializing device and handle struct
rfm95_handle_t var;
rfm95_handle_t* handle = &var;

handle->spi_handle = &hspi1;

handle->nss_port = NSS_GPI_GPIO_Port;
handle->nss_pin = NSS_GPI_Pin;
handle->nrst_port = RESET_GPIO_Port;
handle->nrst_pin = RESET_Pin;
handle->irq_port = GPIOB;
handle->irq_pin = GPIO_PIN_5;

handle->txDone = true;
handle->rxDoneCallback = 0;

bool initSuccess = rfm95_init(handle);

rfm95_setPower(17);   // optional, default power = 17



// transmitting data
char radiopacket[16] = "445EvanWidloskiX";
uint8_t *data = radiopacket;
bool transmitStatus = transmitPackage(data, 16);



// receiving data, data will be stored in buffer after
//   execution of this snippet, if valid data is ever received
uint8_t* buffer = NULL;
uint8_t* length = 0;
bool receiveStatus = receivePackage(&buffer, length);



// Callback function for interrupt
void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == GPIO_PIN_5) {
		rfm95_handleInterrupt();
	}
}