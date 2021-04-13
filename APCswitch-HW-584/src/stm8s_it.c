#include "stm8s_it.h"
#include "gpio.h"

//******************************************************************************
//            D U M M Y   I N T E R R U P T H A N D L E R S                    *
//******************************************************************************
INTERRUPT_HANDLER_TRAP(TRAP_IRQHandler){}

INTERRUPT_HANDLER(TLI_IRQHandler, 0){}

INTERRUPT_HANDLER(AWU_IRQHandler, 1){}

INTERRUPT_HANDLER(CLK_IRQHandler, 2){}

INTERRUPT_HANDLER(EXTI_PORTA_IRQHandler, 3){}

INTERRUPT_HANDLER(EXTI_PORTB_IRQHandler, 4){}

INTERRUPT_HANDLER(EXTI_PORTC_IRQHandler, 5){}

INTERRUPT_HANDLER(EXTI_PORTD_IRQHandler, 6){}

INTERRUPT_HANDLER(EXTI_PORTE_IRQHandler, 7){}

INTERRUPT_HANDLER(EXTI_PORTF_IRQHandler, 8){}

INTERRUPT_HANDLER(TIM1_UPD_OVF_TRG_BRK_IRQHandler, 11){}

INTERRUPT_HANDLER(TIM1_CAP_COM_IRQHandler, 12){}

INTERRUPT_HANDLER(TIM2_CAP_COM_IRQHandler, 14){}

INTERRUPT_HANDLER(TIM3_UPD_OVF_BRK_IRQHandler, 15){}

INTERRUPT_HANDLER(TIM3_CAP_COM_IRQHandler, 16){}

INTERRUPT_HANDLER(UART1_TX_IRQHandler, 17){}

INTERRUPT_HANDLER(UART1_RX_IRQHandler, 18){}

