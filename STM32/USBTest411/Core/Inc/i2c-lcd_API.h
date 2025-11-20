/*
 * i2c-lcd_API.h
 *
 *  Created on: Nov 17, 2025
 *      Author: Jeiel
 */

#ifndef INC_I2C_LCD_API_H_
#define INC_I2C_LCD_API_H_

#include "stm32f4xx_hal.h"


//------------------- LCD Initialization Service ----------------------
void GROVE_LCD_Init(I2C_HandleTypeDef *hi2c);


void GROVE_LCD_Clear(void);
void GROVE_LCD_Home(void);
void GROVE_LCD_SetCursor(uint8_t Col, uint8_t Row);
void GROVE_LCD_WriteChar(char Ch);
void GROVE_LCD_WriteString(char* Str);

void GROVE_LCD_DisplayON(void);
void GROVE_LCD_DisplayOFF(void);
void GROVE_LCD_CursorON(void);
void GROVE_LCD_CursorOFF(void);
void GROVE_LCD_BlinkON(void);
void GROVE_LCD_BlinkOFF(void);

// Backlight.
void GROVE_LCD_setBacklightRGB(uint8_t r, uint8_t g, uint8_t b);





#endif /* INC_I2C_LCD_API_H_ */
