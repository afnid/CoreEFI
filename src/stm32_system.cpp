#ifdef STM32

#include "stm32_main.h"

/**
 * @brief  System Clock Configuration
 *         The system Clock is configured as follow :
 *            System Clock source            = PLL (HSI)
 *            SYSCLK(Hz)                     = 32000000
 *            HCLK(Hz)                       = 32000000
 *            AHB Prescaler                  = 1
 *            APB1 Prescaler                 = 1
 *            APB2 Prescaler                 = 1
 *            HSI Frequency(Hz)              = 16000000
 *            PLLMUL                         = 6
 *            PLLDIV                         = 3
 *            Flash Latency(WS)              = 1
 * @retval None
 */
void STRCC::init(void) {
#ifdef STML1
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };

	/* Enable HSE Oscillator and Activate PLL with HSE as source */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
	RCC_OscInitStruct.PLL.PLLDIV = RCC_PLL_DIV3;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		/* Initialization Error */
		while (1)
			;
	}

	/* Set Voltage scale1 as MCU will run at 32MHz */
	__HAL_RCC_PWR_CLK_ENABLE()
	;
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/* Poll VOSF bit of in PWR_CSR. Wait until it is reset to 0 */
	while (__HAL_PWR_GET_FLAG(PWR_FLAG_VOS) != RESET) {
	};

	/* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
	 clocks dividers */
	RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
		while (1)
			;
	}
#endif
}

void STWDG::init() {
	bzero(&wwdg, sizeof(wwdg));
	wwdg.Init.Window = 0x7e;
	wwdg.Init.Counter = 0x7e;
	wwdg.Init.Prescaler = WWDG_PRESCALER_1;
#ifdef WWDG_EWI_ENABLE
	wwdg.Init.EWIMode = WWDG_EWI_ENABLE;
#endif
	HAL_WWDG_Init(&wwdg);

	__HAL_RCC_WWDG_CLK_ENABLE();
}

void STWDG::refresh() {
#ifdef WWDG_EWI_ENABLE
	HAL_WWDG_Refresh(&wwdg);
#else
	HAL_WWDG_Refresh(&wwdg, 0);
#endif
}

void System::irq_enable(IRQn_Type irq, uint8_t pri, uint8_t sub) {
	HAL_NVIC_SetPriority(irq, pri, sub);
	HAL_NVIC_EnableIRQ(irq);
}

void System::irq_disable(IRQn_Type irq) {
	HAL_NVIC_DisableIRQ(irq);
}

void System::blip(GPIO::PinId pt, uint16_t ms) {
	GPIO::setPin(pt, true);
	HAL_Delay(ms);
	GPIO::setPin(pt, false);
	HAL_Delay(ms);
	wdg.refresh();
}

void System::blink(GPIO::PinId pt, uint8_t count) {
	blink(pt, count, 2);
}

void System::blink(GPIO::PinId pt, uint8_t count, uint8_t repeats, uint16_t flash, uint16_t sleep) {
	for (uint8_t i = 0; i < repeats; i++) {
		for (uint8_t j = 0; j < count; j++)
			blip(pt, flash);

		HAL_Delay(sleep);
		wdg.refresh();
	}
}

#endif
