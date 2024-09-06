/*  Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License. */

#include "lib_drive.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

uint8_t wrong_arg = 1;

// Checking if the right number of arguments was inputed
uint8_t Check(char *input, int nrcom)
{
	int cnt = 0;
	for(int i = 0; i < strlen(input); i++)
	{
		if(input[i] == ',')
		{
			cnt++;
		}
	}
	if(cnt == nrcom){
		return 1;
	}

	else{
  	  char error[32] = "WRONG NUMBER OF ARGUMENTS!!!\r\n";
  	  HAL_UART_Transmit(&huart2, (uint8_t *)error, strlen(error), HAL_MAX_DELAY);
  	  wrong_arg = 0;
	  return 0;
	}
}

// Writing the value to a given index in the emulated EEPROM
EE_Status EEPROM_WriteValue(uint16_t index, uint32_t value)
{
    EE_Status ee_status = EE_OK;

    ee_status = EE_WriteVariable32bits(index, value);
    if(ee_status != EE_OK)
    {
        Error_Handler();
    }

    return ee_status;
}

// Reading the value from a given index in the emulated EEPROM
uint32_t EEPROM_ReadValue(uint16_t index)
{
    EE_Status ee_status = EE_OK;
    uint32_t value = 0;

    ee_status = EE_ReadVariable32bits(index, &value);
    if (ee_status != EE_OK)
    {
        Error_Handler();
        return 0xFFFFFFFF;
    }

    return value;
}


void Confirmation(void){
	char confirm[30] = "Command sent and executed!\r\n";
	HAL_UART_Transmit(&huart2, (uint8_t*)confirm, strlen(confirm), HAL_MAX_DELAY);
}

void SPI_TransmitWithDelay(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size, uint32_t Timeout, uint32_t delay)
{
    for (uint16_t i = 0; i < Size; i++)
    {
    // Chip select pin set to low as transmission starts
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    // Data send via SPI to driver
    HAL_SPI_Transmit(hspi, &pData[i], 1, Timeout);
    // Chip select pin set to high as transmission ends
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    }
}

void SPI_TransmitReceiveWithDelay(SPI_HandleTypeDef *hspi, uint8_t *pTxData, uint8_t *pRxData, uint16_t Size, uint32_t Timeout, uint32_t delay)
{
    for (uint16_t i = 0; i < Size; i++)
    {
    // Chip select pin set to low as transmission starts
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    // Data send via SPI to driver
    HAL_SPI_TransmitReceive(hspi, &pTxData[i], &pRxData[i], 1, Timeout);
    // Chip select pin set to high as transmission ends
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    }
}

// Data get downladed from Nucleo G4 emulated EEPROM and send to POWERSTEP01 driver chip over SPI
void Initialisation()
{
	HAL_FLASH_Unlock();
	steps = EEPROM_ReadValue(1);
	coef = EEPROM_ReadValue(2);
	uint8_t transmittedData[7] = {0x18, 0x19, 0x09, 0x0a, 0x0b, 0x0c, 0x16};

	for(int i = 0; i < 7; i++)
	{
		if(i == 0){
			uint8_t data[3]={};
			data[0]=transmittedData[i];
			data[1]=(uint8_t) EEPROM_ReadValue(3);
			data[2]=(uint8_t) EEPROM_ReadValue(4);

			SPI_TransmitWithDelay(&hspi1, data, 3, 1000, byte_delay);

		}
		else{
			uint8_t data[2]={};
			data[0]=transmittedData[i];
			data[1]=(uint8_t) EEPROM_ReadValue(i+4);

			SPI_TransmitWithDelay(&hspi1, data, 2, 1000, byte_delay);
		}
	}
	  HAL_FLASH_Lock();


}


void Run(bool direction, float rotation)
{
    //Speed limitation
	if(rotation >= 0 && rotation <= 4000)
	{
        // Steps/sec converted to Steps/tick
		uint32_t speed = (rotation/(1000000000))*250*(1 << 28);
		uint8_t speed_array[4];
        // Application command with register value + last bit is direction
		speed_array[0] = 0x50 | direction;
        // Speed packed into 3 bytes
		speed_array[1] = (speed >> 16) & 0xF;
		speed_array[2] = (speed >> 8) & 0xFF;
		speed_array[3] = speed & 0xFF;

		SPI_TransmitWithDelay(&hspi1, speed_array, 4, 1000, byte_delay);
	}
}

void Save()
{
	uint8_t transmittedData[7] = {0x18, 0x19, 0x09, 0x0a, 0x0b, 0x0c, 0x16};
	uint8_t dataToStore[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    // Four bytes of data send and received from to driver at via SPI at the same time.
	uint8_t shortedData[3] = {0, 0, 0};

	GetParam(transmittedData[0], shortedData);
	dataToStore[0] = shortedData[0];
	dataToStore[1] = shortedData[1];

	for (int i = 2; i <= 8; i++) {
	GetParam(transmittedData[i-1], shortedData);
	dataToStore[i] = shortedData[0];
	}
	HAL_FLASH_Unlock();
	for(int i=0; i < 8;i++){
		EEPROM_WriteValue(i+3, dataToStore[i]);
	}
	HAL_FLASH_Lock();
}

void Stop()
{
    // Motor is stopped, bridges set to high impedance status
    uint8_t stop[] = {0xa8};
    SPI_TransmitWithDelay(&hspi1, stop, 1, 1000, byte_delay);
}
void GetParam(uint8_t transmittedData, uint8_t *shortedData)
{

	transmittedData |= 0x20;
	uint8_t receivedData[4] = {0, 0, 0, 0};
	uint8_t dataToSend[4] = {transmittedData, 0, 0, 0};
    // Four bytes of data send and recived from to driver at via SPI at the same time.
	SPI_TransmitReceiveWithDelay(&hspi1, dataToSend, receivedData, 4, 1000, byte_delay);
	shortedData[0] = receivedData[1];
	shortedData[1] = receivedData[2];
	shortedData[2] = receivedData[3];
	return shortedData;

//	char message[12];
    // Recived data from driver send back to the terminal via UART
//	sprintf(message,"%2x %2x %2x \r\n", receivedData[1], receivedData[2], receivedData[3]);
//	HAL_UART_Transmit(&huart2, (uint8_t*)message, strlen(message), HAL_MAX_DELAY);
}

void Commands(char* tablica)
  {
    // Extraction of command from array of characters
      char* part;
      char command[30];
      part = strchr(tablica, '(');
      if (part != NULL) {
              size_t length = part - tablica;
              strncpy(command, tablica, length);
              command[length] = '\0';
          } else {
              strncpy(command, tablica, sizeof(command) - 1);
              command[sizeof(command) - 1] = '\0';
          }
    // Setting or getting the set parameters from driver
      if(strcmp(command, "SetParam") == 0 || (strcmp(command, "GetParam") == 0))
      {
              char* start = part + 1;
              char* end = strchr(start, ')');
              uint8_t bytes[8];
              uint8_t cnt = 0;

              while (start < end && cnt < sizeof(bytes)) {
                  unsigned int number;
                  if (sscanf(start, "%2x", &number) == 1) {
                      bytes[cnt] = (uint8_t)number;
                      cnt++;
                  }
                  start = strchr(start, ',');
                  if (start != NULL) {
                      start++;
                  } else {
                      break;
                  }
              }
              if(command[0] == 'G'){

            	  cnt++;
            	  for(cnt; cnt <= 5; cnt++)
            	  {
            		  bytes[cnt] = 0;
            	  }
            	  uint8_t receivedData[3] = {0, 0, 0};

            	  GetParam(bytes[0], receivedData);

            	  char message[12];
            	  sprintf(message,"%2x %2x %2x \r\n", receivedData[0], receivedData[1], receivedData[2]);
            	  HAL_UART_Transmit(&huart2, (uint8_t*)message, strlen(message), HAL_MAX_DELAY);
            	  Confirmation();
              }
              else{
              SPI_TransmitWithDelay(&hspi1, bytes, cnt, 1000, byte_delay);
              Confirmation();
              }


          }
// Reseting the device to factory settings with first slow stop of motor and then bridges set to high impedance
      else if(strcmp(command, "Factory") == 0)
      {
          uint8_t msg2[1] = {0xa8};
          SPI_TransmitWithDelay(&hspi1, msg2, 1, 1000, byte_delay);

      	// Driver reset to factory
      	uint8_t reset_dev[] = {0xc0};

      	// Array's of driver's initial conditions
      	uint8_t g1[] = {0x18, 0x0, 0xc3}; // I_gate,TCC, WD_EN, TBOOST
      	uint8_t g2[] = {0x19, 0x40}; // TDT, TBLANK

      	uint8_t hold[] = {0x09, 0x29};
      	uint8_t run[] = {0x0a, 0x29};
		uint8_t acc[] = {0x0b, 0x29};
		uint8_t dec[] = {0x0c, 0x29};

      	uint8_t step_mode[] = {0x16, 0x07};

      	// Initial conditions transmitted to driver over SPI
      	SPI_TransmitWithDelay(&hspi1, reset_dev, 1, 1000, byte_delay); // ResetDevice
      	SPI_TransmitWithDelay(&hspi1, g1, 3, 1000, byte_delay); // GATECFG1
      	SPI_TransmitWithDelay(&hspi1, g2, 2, 1000, byte_delay); // GATECFG2
      	SPI_TransmitWithDelay(&hspi1, hold, 2, 1000, byte_delay);
      	SPI_TransmitWithDelay(&hspi1, run, 2, 1000, byte_delay);
      	SPI_TransmitWithDelay(&hspi1, acc, 2, 1000, byte_delay);
      	SPI_TransmitWithDelay(&hspi1, dec, 2, 1000, byte_delay);
      	SPI_TransmitWithDelay(&hspi1, step_mode, 2, 1000, byte_delay);

        HAL_FLASH_Unlock();
      	EEPROM_WriteValue(1, 200);
      	EEPROM_WriteValue(2, 12);
        HAL_FLASH_Lock();

        Confirmation();
      }
      else if(strcmp(command, "Save") == 0){
    	  Save();
    	  Confirmation();
      }

      else if(strcmp(command, "Init") == 0)
      {
    	  Initialisation();
    	  Confirmation();
      }
      else if(strcmp(command, "SetSteps") == 0){
    	  HAL_FLASH_Unlock();
    	  uint32_t temp = EEPROM_ReadValue(1);
    	  sscanf(tablica, "SetSteps(%lu)", &steps);
    	  EEPROM_WriteValue(1, steps);
    	  Confirmation();
    	  char set[50] = {};
    	  sprintf(set, "No. of steps changed from %lu to %lu \r\n", temp, steps);
    	  HAL_UART_Transmit(&huart2, (uint8_t*)set, strlen(set), HAL_MAX_DELAY);
    	  HAL_FLASH_Lock();

      }
      else if(strcmp(command, "SetCoef") == 0){
    	  HAL_FLASH_Unlock();
    	  uint32_t temp = EEPROM_ReadValue(2);
    	  sscanf(tablica, "SetCoef(%lu)", &coef);
    	  EEPROM_WriteValue(2, coef);
    	  Confirmation();
    	  char set[50] = {};
    	  sprintf(set, "Coefficient changed from %lu to %lu \r\n", temp, coef);
    	  HAL_UART_Transmit(&huart2, (uint8_t*)set, strlen(set), HAL_MAX_DELAY);
    	  HAL_FLASH_Lock();
      }
      // Start of a motor with preset speed expressed in steps/sec
      else if((strcmp(command, "Run") == 0)/*&&(Check(part, 1))*/)
      {
    	  float number;
    	  char direction;
    	  sscanf(tablica, "Run(%c,%f)", &direction, &number);
    	  if(direction == '1')
    	  {
    		  Run(forward, number);
    		  Confirmation();
    	  }
    	  else if(direction == '0')
    	  {
    		  Run(reverse, number);
    		  Confirmation();
    	  }
    	  else
    	  {
    		   char err[30] = "Wrong value of direction!";
    		   HAL_UART_Transmit(&huart2, (uint8_t*)err, strlen(err), HAL_MAX_DELAY);
    	  }

      }
      // Start of a motor with preset speed in rot/min
      else if((strcmp(command, "Run_RPM") == 0))
            {
    	  	  HAL_FLASH_Unlock();
          	  float number;
          	  char direction;
          	  sscanf(tablica, "Run_RPM(%c,%f)", &direction, &number);
          	  number /= 60.0;
          	  uint8_t nr_steps = EEPROM_ReadValue(1);
          	  number *= nr_steps;
          	  HAL_FLASH_Lock();
          	  if(direction == '1')
          	  {
          		  Run(forward, number);
          		  Confirmation();
          	  }
          	  else if(direction == '0')
          	  {
          		  Run(reverse, number);
          		  Confirmation();
          	  }
          	  else
          	  {
          		   char err[30] = "Wrong value of direction!";
          		   HAL_UART_Transmit(&huart2, (uint8_t*)err, strlen(err), HAL_MAX_DELAY);
          	  }

            }
      //Puts the device in Step-clock mode and imposes DIR direction
      else if(strcmp(command, "StepClock") == 0&&Check(part, 0))
      {
          char tbc[3];
          strncpy(tbc, part + 1, sizeof(tbc) - 1);
          tbc[sizeof(tbc) - 1] = '\0';
          if(tbc[1] == '0')
          {
              uint8_t msg[1] = {0x58};
              SPI_TransmitWithDelay(&hspi1, msg, 1, 1000, byte_delay);
          }
          else
          {
              uint8_t msg[1] = {0x59};
              SPI_TransmitWithDelay(&hspi1, msg, 1, 1000, byte_delay);
          }
          Confirmation();
      }
      //Makes N_STEP (micro)steps in direction (Not performable when motor is running)!!!
      else if (strcmp(command, "Move") == 0&&Check(part, 1))
      {
          char tbc[3];
          strncpy(tbc, part + 1, sizeof(tbc) - 1);
          tbc[sizeof(tbc) - 1] = '\0';
          char *comma_pos = strchr(part, ',');
          if (comma_pos != NULL)
          {
              part = comma_pos + 1;

              uint32_t number;
              sscanf(part, "%lu", &number);

              uint8_t msg[4];
              msg[1] = (number >> 16) & 0x3F;
              msg[2] = (number >> 8) & 0xFF;
              msg[3] = number & 0xFF;

              if(tbc[0] == '0')
              {
                  msg[0] = 0x40;
                  SPI_TransmitWithDelay(&hspi1, msg, 4, 1000, byte_delay);
              }
              else
              {
                  msg[0] = 0x41;
                  SPI_TransmitWithDelay(&hspi1, msg, 4, 1000, byte_delay);
              }
              Confirmation();
          }
      }

      //Brings motor in ABS_POS position (minimum path)
      else if(strcmp(command, "GoTo") == 0&&Check(part, 0))
      {
    	  uint32_t number;
    	  part++;
    	  sscanf(part, "%lu", &number);
    	  uint8_t msg[4];
    	  msg[0] = 0x60;
    	  msg[1] = (number >> 16) & 0x3F;
    	  msg[2] = (number >> 8) & 0xFF;
    	  msg[3] = number & 0xFF;
          SPI_TransmitWithDelay(&hspi1, msg, 4, 1000, byte_delay);
          Confirmation();

      }
      //Brings motor in ABS_POS position forcing DIR direction
      else if(strcmp(command, "GoTo_DIR") == 0&&Check(part, 1))
      {
    	  char tbc[3];
    	  strncpy(tbc, part + 1, sizeof(tbc) - 1);
    	  tbc[sizeof(tbc) - 1] = '\0';
    	  char *comma_pos = strchr(part, ',');
    	  if (comma_pos != NULL)
    	  {
    		  part = comma_pos + 1;
    		  uint32_t number;
    	      sscanf(part, "%lu", &number);
    	      uint8_t msg[4];
    	      msg[1] = (number >> 16) & 0x3F;
    	      msg[2] = (number >> 8) & 0xFF;
    	      msg[3] = number & 0xFF;
    	      if(tbc[0] == '0')
    	      {
    	    	  msg[0] = 0x68;
    	          SPI_TransmitWithDelay(&hspi1, msg, 4, 1000, byte_delay);
    	      }
    	      else
    	      {
    	          msg[0] = 0x69;
    	          SPI_TransmitWithDelay(&hspi1, msg, 4, 1000, byte_delay);
    	      }
    	      Confirmation();
    	  }
      }
      //Performs a motion in DIR direction with speed until SW is closed, the action is executed then a SoftStop takes place
      else if(strcmp(command, "GoUntil") == 0&&Check(part, 2))
      {
          char tbc[5];
          strncpy(tbc, part + 1, sizeof(tbc) - 1);
          tbc[sizeof(tbc) - 1] = '\0';
          part+=4;
          uint32_t number;
          sscanf(part, "%lu", &number);
          uint8_t msg[4];
          msg[1] = (number >> 16) & 0x3F;
          msg[2] = (number >> 8) & 0xFF;
          msg[3] = number & 0xFF;
          if(tbc[0] == '0')
          {
              if(tbc[2] == '0')
              {
                  msg[0] = 0x82;
                  SPI_TransmitWithDelay(&hspi1, msg, 4, 1000, byte_delay);
              }
              else
              {
                  msg[0] = 0x83;
                  SPI_TransmitWithDelay(&hspi1, msg, 4, 1000, byte_delay);
              }
              Confirmation();
          }
          else
          {
              if(tbc[2] == '0')
              {
                  msg[0] = 0x8a;
                  SPI_TransmitWithDelay(&hspi1, msg, 4, 1000, byte_delay);
              }
              else
              {
                  msg[0] = 0x8b;
                  SPI_TransmitWithDelay(&hspi1, msg, 4, 1000, byte_delay);
              }
              Confirmation();
          }
      }
    /*Performs a motion in DIR direction at minimum speed until the SW is released (open),
    the ACT action is executed then a HardStop takes place*/
      else if(strcmp(command, "ReleaseSW") == 0&&Check(part, 1))
      {
          char tbc[5];
          strncpy(tbc, part + 1, sizeof(tbc) - 1);
          tbc[sizeof(tbc) - 1] = '\0';
          if(tbc[1] == '0')
          {
              if(tbc[3] == '0')
              {
                  uint8_t msg[1] = {0x92};
                  SPI_TransmitWithDelay(&hspi1, msg, 1, 1000, byte_delay);
              }
              else
              {
                  uint8_t msg[1] = {0x93};
                  SPI_TransmitWithDelay(&hspi1, msg, 1, 1000, byte_delay);
              }
              Confirmation();
          }
          else
          {
              if(tbc[3] == '0')
              {
                  uint8_t msg[1] = {0x9a};
                  SPI_TransmitWithDelay(&hspi1, msg, 1, 1000, byte_delay);
              }
              else
              {
                  uint8_t msg[1] = {0x9b};
                  SPI_TransmitWithDelay(&hspi1, msg, 1, 1000, byte_delay);
              }
              Confirmation();
          }
      }
      //Brings the motor in HOME position
      else if(strcmp(command, "GoHome") == 0&&Check(part, 0))
      {
          uint8_t msg[1] = {0x70};
          SPI_TransmitWithDelay(&hspi1, msg, 1, 1000, byte_delay);
          Confirmation();
      }
      //Brings the motor in MARK position
      else if(strcmp(command, "GoMark") == 0&&Check(part, 0))
      {
          uint8_t msg[1] = {0x78};
          SPI_TransmitWithDelay(&hspi1, msg, 1, 1000, byte_delay);
          Confirmation();
      }
      //Resets the ABS_POS register (sets HOME position)
      else if(strcmp(command, "ResetPos") == 0&&Check(part, 0))
      {
          uint8_t msg[1] = {0xc8};
          SPI_TransmitWithDelay(&hspi1, msg, 1, 1000, byte_delay);
          Confirmation();
      }
      //Device is reset to power-up conditions
      else if(strcmp(command, "ResetDevice") == 0&&Check(part, 0))
      {
          uint8_t msg[1] = {0xc0};
          SPI_TransmitWithDelay(&hspi1, msg, 1, 1000, byte_delay);
          Confirmation();
      }
      //Stops motor with a deceleration phase
      else if(strcmp(command, "SoftStop") == 0&&Check(part, 0))
      {
          uint8_t msg[1] = {0xb0};
          SPI_TransmitWithDelay(&hspi1, msg, 1, 1000, byte_delay);
          Confirmation();
      }
      // Stops motor immediately
      else if(strcmp(command, "HardStop") == 0&&Check(part, 0))
      {
          uint8_t msg[1] = {0xb8};
          SPI_TransmitWithDelay(&hspi1, msg, 1, 1000, byte_delay);
          Confirmation();
      }
      //Puts the bridges in high impedance status after a deceleration phase
      else if(strcmp(command, "SoftHiZ") == 0&&Check(part, 0))
      {
          uint8_t msg[1] = {0xa0};
          SPI_TransmitWithDelay(&hspi1, msg, 1, 1000, byte_delay);
          Confirmation();
      }
      //Puts the bridges in high impedance status immediately
      else if(strcmp(command, "HardHiZ") == 0&&Check(part, 0))
      {
          uint8_t msg[1] = {0xa8};
          SPI_TransmitWithDelay(&hspi1, msg, 1, 1000, byte_delay);
          Confirmation();
      }
      //Returns the status register value
      else if(strcmp(command, "GetStatus") == 0&&Check(part, 0))
      {
    	  uint8_t receivedData[4] = {0, 0, 0, 0};
    	  uint8_t dataToSend[4] = {0xd0, 0, 0, 0};
    	  // Four bytes of data send and recived from to driver at via SPI at the same time.
    	  SPI_TransmitReceiveWithDelay(&hspi1, dataToSend, receivedData, 4, 1000, byte_delay);

    	  char message[12];
    	  sprintf(message,"%2x %2x %2x \r\n", receivedData[1], receivedData[2], receivedData[3]);
    	  HAL_UART_Transmit(&huart2, (uint8_t*)message, strlen(message), HAL_MAX_DELAY);
    	  Confirmation();
      }
      //Reserved command
      else if(strcmp(command, "RESERVED") == 0&&Check(part, 0))
      {
          uint8_t msg[1] = {0xf8};
          SPI_TransmitWithDelay(&hspi1, msg, 1, 1000, byte_delay);
      }
      else if(wrong_arg)
      {
    	  char msg[20] = "WRONG COMMAND!!!\r\n";
    	  HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
    	  wrong_arg = 1;
      }

  }