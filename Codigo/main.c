#include "LPC17xx.h"
#include "lpc_types.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_gpdma.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_exti.h"
#include "lpc17xx_uart.h"

#define ADC_RATE    8000
#define LISTSIZE    12000
#define TIMEOUT     7000
#define NUM_LISTS   3

void configADC(void);
void configDAC(void);
void configGPIO(void);
void configEINT0(void);
void configUART(void);
void configEINT1(void);
void configDMA(__IO uint16_t listADC[]);
void configNVIC(void);

uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max);
void cleanListADC(void);
void moveListDAC(void);
void buttonDebounce(void);
void sendSamplesUART(void);

/* Buffers y contadores */
__IO uint16_t listADC[LISTSIZE] = {0};
volatile uint32_t samples_count = 0;   /* contador seguro (volatile) */

/* Flags */
uint8_t RECORDING = 0;
volatile uint8_t FINISHED = 0;

GPDMA_LLI_Type LLI_Array[NUM_LISTS];
GPDMA_Channel_CFG_Type dmaCFG;

/* -------------------------------- MAIN -------------------------------- */
int main(void)
{
    configGPIO();
    configEINT0();
    configEINT1();
    configADC();
    configDAC();
    configUART();
    configNVIC();

    while (1)
    {
        if (FINISHED)
        {
            /* marcaremos FINISHED = 0 antes de enviar para evitar reentradas */
            FINISHED = 0;
            sendSamplesUART();
        }
    }

    return 0;
}

/* ------------------------- funciones utilitarias ----------------------- */
void cleanListADC(void)
{
    for (uint32_t i = 0; i < LISTSIZE; i++) listADC[i] = 0;
}

void moveListDAC(void)
{
    for (uint32_t i = 0; i < LISTSIZE; i++)
    {
        listADC[i] = listADC[i] << 6;
    }
}

void buttonDebounce(void)
{
    for (volatile uint32_t i = 0; i < 50000; i++) {}
}

uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/* --------------------------- configuraciones -------------------------- */
void configGPIO(void)
{
    PINSEL_CFG_Type pinCFG;
    /* P0.23 AD0.0 */
    pinCFG.Funcnum = PINSEL_FUNC_1; pinCFG.OpenDrain = PINSEL_PINMODE_NORMAL;
    pinCFG.Pinmode = PINSEL_PINMODE_TRISTATE; pinCFG.Pinnum = PINSEL_PIN_23; pinCFG.Portnum = PINSEL_PORT_0;
    PINSEL_ConfigPin(&pinCFG);

    /* P0.24 AD0.1 */
    pinCFG.Pinnum = PINSEL_PIN_24; PINSEL_ConfigPin(&pinCFG);

    /* P2.10 EINT0 */
    pinCFG.Funcnum = PINSEL_FUNC_1; pinCFG.Pinmode = PINSEL_PINMODE_PULLDOWN;
    pinCFG.Pinnum = PINSEL_PIN_10; pinCFG.Portnum = PINSEL_PORT_2; PINSEL_ConfigPin(&pinCFG);

    /* P2.11 EINT1 */
    pinCFG.Pinnum = PINSEL_PIN_11; PINSEL_ConfigPin(&pinCFG);

    /* P0.26 DAC */
    pinCFG.Funcnum = PINSEL_FUNC_2; pinCFG.Pinmode = PINSEL_PINMODE_TRISTATE;
    pinCFG.Pinnum = PINSEL_PIN_26; pinCFG.Portnum = PINSEL_PORT_0; PINSEL_ConfigPin(&pinCFG);

    /* LEDs */
    LPC_GPIO0->FIODIR |= (1 << 22);
    LPC_GPIO3->FIODIR |= (1 << 25);
    LPC_GPIO3->FIODIR |= (1 << 26);
    LPC_GPIO0->FIOSET |= (1 << 22);
    LPC_GPIO3->FIOSET |= (1 << 25);
    LPC_GPIO3->FIOSET |= (1 << 26);

    /* UART2 pins P0.10 (TX) / P0.11 (RX) */
    pinCFG.Funcnum = 1; pinCFG.OpenDrain = 0; pinCFG.Pinmode = 0;
    pinCFG.Pinnum = 10; pinCFG.Portnum = 0; PINSEL_ConfigPin(&pinCFG);
    pinCFG.Pinnum = 11; PINSEL_ConfigPin(&pinCFG);
}

void configADC(void)
{
    ADC_Init(LPC_ADC, ADC_RATE);
    ADC_ChannelCmd(LPC_ADC, 0, ENABLE);
    ADC_ChannelCmd(LPC_ADC, 1, ENABLE);
    ADC_BurstCmd(LPC_ADC, ENABLE);
    /* habilita interrupcion por finalizacion del canal 0 */
    ADC_IntConfig(LPC_ADC, ADC_ADINTEN0, ENABLE);
    /* NOTA: no arrancamos NVIC del ADC hasta que pulsemos EINT0 */
    ADC_StartCmd(LPC_ADC, ADC_START_CONTINUOUS);
}

void configDAC(void)
{
    DAC_CONVERTER_CFG_Type dacCFG;
    dacCFG.CNT_ENA = SET;
    dacCFG.DMA_ENA = SET;
    DAC_SetDMATimeOut(LPC_DAC, TIMEOUT);
    DAC_ConfigDAConverterControl(LPC_DAC, &dacCFG);
    DAC_Init(LPC_DAC);
}

void configDMA(__IO uint16_t listADC[])
{
    for (int i = 0; i < NUM_LISTS; i++)
    {
        LLI_Array[i].DstAddr = (uint32_t)&(LPC_DAC->DACR);
        LLI_Array[i].SrcAddr = (uint32_t)(listADC + i * 4095);

        if (i == (NUM_LISTS - 1))
            LLI_Array[i].NextLLI = (uint32_t)&LLI_Array[0];
        else
            LLI_Array[i].NextLLI = (uint32_t)&LLI_Array[i + 1];

        LLI_Array[i].Control = 4095
                             | (1 << 18)  /* source width 16 bit */
                             | (1 << 22)  /* dest width = word 32 bits */
                             | (1 << 26)  /* source increment */
                             ;
    }

    dmaCFG.ChannelNum = 0;
    dmaCFG.TransferSize = 4095;
    dmaCFG.TransferWidth = 0;
    dmaCFG.TransferType = GPDMA_TRANSFERTYPE_M2P;
    dmaCFG.SrcConn = 0;
    dmaCFG.DstConn = GPDMA_CONN_DAC;
    dmaCFG.SrcMemAddr = (uint32_t)listADC;
    dmaCFG.DstMemAddr = 0;
    dmaCFG.DMALLI = (uint32_t)&LLI_Array[0];

    GPDMA_Init();
    GPDMA_Setup(&dmaCFG);
    GPDMA_ChannelCmd(0, ENABLE);
}

void configEINT0(void)
{
    EXTI_InitTypeDef exti;
    exti.EXTI_Mode = EXTI_MODE_EDGE_SENSITIVE;
    exti.EXTI_polarity = EXTI_POLARITY_HIGH_ACTIVE_OR_RISING_EDGE;
    exti.EXTI_Line = EXTI_EINT0;
    EXTI_Config(&exti);
}

void configEINT1(void)
{
    EXTI_InitTypeDef exti;
    exti.EXTI_Mode = EXTI_MODE_EDGE_SENSITIVE;
    exti.EXTI_polarity = EXTI_POLARITY_HIGH_ACTIVE_OR_RISING_EDGE;
    exti.EXTI_Line = EXTI_EINT1;
    EXTI_Config(&exti);
}

void configNVIC(void)
{
    /* Habilita ADC_IRQn cuando se quiera grabar; por ahora lo dejamos habilitado
       (la interrupcion de ADC estaba siendo habilitada en configADC con ADC_IntConfig) */
    NVIC_EnableIRQ(ADC_IRQn);

    EXTI_ClearEXTIFlag(EXTI_EINT0);
    NVIC_EnableIRQ(EINT0_IRQn);

    EXTI_ClearEXTIFlag(EXTI_EINT1);
    NVIC_EnableIRQ(EINT1_IRQn);

    /* GPDMA inicialmente deshabilitado */
    GPDMA_ChannelCmd(0, DISABLE);
}

void configUART(void)
{
    UART_CFG_Type      UARTConfigStruct;
    UART_FIFO_CFG_Type UARTFIFOConfigStruct;


    UART_ConfigStructInit(&UARTConfigStruct); /* 9600-8N1 por defecto */
    UART_Init(LPC_UART2, &UARTConfigStruct);

    UART_FIFOConfigStructInit(&UARTFIFOConfigStruct);
    UART_FIFOConfig(LPC_UART2, &UARTFIFOConfigStruct);

    UART_TxCmd(LPC_UART2, ENABLE);
    UART_IntConfig(LPC_UART2, UART_INTCFG_RBR, DISABLE);
    UART_IntConfig(LPC_UART2, UART_INTCFG_RLS, DISABLE);
}

/* --------------------------- INTERRUPT HANDLERS ----------------------- */
void ADC_IRQHandler(void)
{
    static uint32_t ADCVAL = 0;
    static uint32_t ADCVALMAP = 0;

    if (RECORDING)
    {
        /* grabamos mientras no superemos LISTSIZE */
        if (samples_count < LISTSIZE)
        {
            LPC_GPIO3->FIOCLR |= (1 << 26); /* Led azul ON */
            listADC[samples_count] = ((LPC_ADC->ADDR0) >> 6) & 0x3FF;
            samples_count++;
        }

        if (samples_count >= LISTSIZE)
        {
            /* terminamos la grabacion */
            LPC_GPIO0->FIOSET |= (1 << 22);
            LPC_GPIO3->FIOSET |= (1 << 25);
            LPC_GPIO3->FIOSET |= (1 << 26);

            RECORDING = 0;
            FINISHED = 1;

            /* deshabilitar NVIC del ADC para evitar sobrescrituras mientras procesamos */
            NVIC_DisableIRQ(ADC_IRQn);

            /* preparar datos para DAC */
            moveListDAC();

        }
    }
    else /* no recording: ajustar DAC timeout según potenciometro */
    {
        ADCVAL = ((LPC_ADC->ADDR1) >> 6) & 0x3FF;
        ADCVALMAP = map(ADCVAL, 0, 1024, 5000, 20000);
        DAC_SetDMATimeOut(LPC_DAC, ADCVALMAP);
    }

    /* limpiar flag de ADC (lectura repetida para limpiar) */
    LPC_ADC->ADGDR &= LPC_ADC->ADGDR;
}

void EINT0_IRQHandler(void)
{
    buttonDebounce();

    RECORDING = 1;
    samples_count = 0;
    cleanListADC();

    /* apagar canales DMA y preparar */
    GPDMA_ChannelCmd(0, DISABLE);

    NVIC_EnableIRQ(ADC_IRQn);
    EXTI_ClearEXTIFlag(EXTI_EINT0);
}

void EINT1_IRQHandler(void)
{
    static uint8_t PLAY = 0;
    buttonDebounce();

    if (PLAY)
    {
        LPC_GPIO3->FIOSET |= (1 << 25);
        LPC_GPIO3->FIOSET |= (1 << 26);
        LPC_GPIO0->FIOCLR |= (1 << 22);
        GPDMA_ChannelCmd(0, DISABLE);
        DAC_UpdateValue(LPC_DAC, 0);
        PLAY = 0;
    }
    else
    {
        LPC_GPIO0->FIOSET |= (1 << 22);
        LPC_GPIO3->FIOSET |= (1 << 26);
        LPC_GPIO3->FIOCLR |= (1 << 25);
        configDMA(listADC);
        PLAY = 1;
    }

    EXTI_ClearEXTIFlag(EXTI_EINT1);
}

void sendSamplesUART(void)
{
    /* Envío binario: 2 bytes por muestra (MSB, LSB) con UART_Send BLOCKING */
    uint8_t buf[2];

    for (uint32_t i = 0; i < LISTSIZE; i++)
    {
        uint16_t sample = listADC[i] >> 6; /* 10 bits: 0..1023 */
        buf[0] = (uint8_t)((sample >> 8) & 0x03); /* dos bits altos */
        buf[1] = (uint8_t)(sample & 0xFF);
        UART_Send(LPC_UART2, buf, 2, BLOCKING);
    }

    /* una vez enviado, re-habilitamos el ADC si queremos grabar otra vez */
    samples_count = 0;
    /* volver a permitir ADC IRQ si se desea */
    NVIC_EnableIRQ(ADC_IRQn);
}
