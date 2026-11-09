/*****************************************************************************

 Project Name       : MDP - Microprocessor Development Project
 Project Code       : HD083D
 Created        : 07-Nov-2019
 Filename       : config.h
 Purpose        : Configuration for different peripherals
 Description        : Base address/configuration for peripherals 
 Author(s)      : Premjith A V
 Email          : premjith@cdac.in

 See LICENSE for license details.
******************************************************************************/

#ifndef CONFIG_H_
#define CONFIG_H_

#define UART_BASE               0x10000100UL
#define ADC_BASE_ADDRESS                0x10001000UL
#define SPI_BASE_ADDR(i)            (((i) == 0 || (i) == 1) ? 0x10000600UL : 0x10200100UL)
#define I2C_BASE_ADDR               0x10000800UL
#define GPIO_0_BASE_ADDRESS         0x10080000UL
#define GPIO_1_BASE_ADDRESS         0x10180000UL
#define TIMER_BASE_ADDRESS          0x10000A00UL
#define PWMC_BASE_ADDR              0x10400000UL    

#define CONCATENATE(X) #X
#define CONCAT(X) CONCATENATE(X)

#define SYS_FREQ    40000000

#endif /* CONFIG_H_ */