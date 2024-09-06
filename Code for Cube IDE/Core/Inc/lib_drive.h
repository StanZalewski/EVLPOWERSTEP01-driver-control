/*  Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License. */

#ifndef INC_LIB_DRIVE_H_
#include <stdbool.h>
#include <stdint.h>
#include <main.h>

#include "eeprom_emul.h"
#include "eeprom_emul_conf_template.h"
#include "eeprom_emul_types.h"
#include "flash_interface.h"

#define INC_LIB_DRIVE_H_

#define forward true
#define reverse false
#define byte_delay 1

extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart2;
extern char tab[50];
extern uint32_t coef;
extern uint32_t steps;
uint8_t Check(char *input, int nrcom);
EE_Status EEPROM_WriteValue(uint16_t index, uint32_t value);
uint32_t EEPROM_ReadValue(uint16_t index);

void GetParam(uint8_t transmittedData, uint8_t *shortedData);
void SPI_TransmitWithDelay(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size, uint32_t Timeout, uint32_t delay);
void Confirmation(void);

void Initialisation();
void Stop();
void Run(bool direction, float rotation);
void Commands(char* tablica);

#endif
